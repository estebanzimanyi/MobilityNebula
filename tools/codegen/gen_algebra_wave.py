#!/usr/bin/env python3
"""Autogenerate a set/span/spanset/box-algebra codegen wave from real MEOS
signatures.

Reads tools/codegen/_meos_headers.txt for the authoritative signature of every
target function (return type + ordered param types), classifies each operand as
a container (Span/Set/SpanSet/STBox/TBox text literal) or a scalar
(int/bigint/float), and emits:
  - a codegen_nebula.py spec (--out-spec)
  - one systest .test per op with valid per-subtype literals (--out-tests-dir),
    expected block left blank for record_tests.py.

The container operand is the primary input_type; the other operand is an extra
arg. box_first is set from the ACTUAL parameter order (names are unreliable:
union_float_span is (Span*, double) but union_float_set is (double, Set*)).

Usage:
  python3 tools/codegen/gen_algebra_wave.py --names <file|-> [--dry-run]
        [--out-spec spec.json] [--out-tests-dir nes-systests/function/meos]
"""
from __future__ import annotations
import argparse, json, re, sys

HEADERS = "tools/codegen/_meos_headers.txt"

# scalar C type -> (nebula cpp, source-column SQL type, sample literal a / b)
SCALARS = {
    "int":         ("int32_t", "INT32",   "3",   "7"),
    "int64":       ("int64_t", "INT64",   "3",   "7"),
    "double":      ("double",  "FLOAT64", "3.5", "7.5"),
}
# scalar parsed from a quoted text field -> (parser, parser_extra, sample a, sample b).
# date_in -> DateADT and timestamptz_in -> TimestampTz return plain values (no free).
SCALARS_TEXT = {
    "DateADT":     ("date_in", "", "2020-01-03", "2020-06-15"),
    "TimestampTz": ("timestamptz_in", ", -1",
                    "2020-01-03 00:00:00+00", "2020-06-15 12:00:00+00"),
}
# container kind + subtype -> (input_type key / box parser, box_type, return *_text)
SUBSPAN = {"int": "intspan", "bigint": "bigintspan", "float": "floatspan",
           "date": "datespan", "timestamptz": "tstzspan"}
SUBSET  = {"int": "intset",  "bigint": "bigintset",  "float": "floatset",
           "date": "dateset", "timestamptz": "tstzset"}
SUBSS   = {"int": "intspanset", "bigint": "bigintspanset", "float": "floatspanset",
           "date": "datespanset", "timestamptz": "tstzspanset"}
# per (input_type key) -> a pair of valid text literals for the .test
LITERALS = {
    "intspan": ("[1, 5)", "[3, 9)"), "bigintspan": ("[1, 5)", "[3, 9)"),
    "floatspan": ("[1.5, 5.5)", "[3.5, 9.5)"),
    "intset": ("{1, 3, 5}", "{3, 5, 9}"), "bigintset": ("{1, 3, 5}", "{3, 5, 9}"),
    "floatset": ("{1.5, 3.5, 5.5}", "{3.5, 5.5, 9.5}"),
    "intspanset": ("{[1, 5), [10, 15)}", "{[3, 7)}"),
    "bigintspanset": ("{[1, 5), [10, 15)}", "{[3, 7)}"),
    "floatspanset": ("{[1.5, 5.5), [10.5, 15.5)}", "{[3.5, 7.5)}"),
    "datespan": ("[2020-01-01, 2020-01-10)", "[2020-01-05, 2020-01-20)"),
    "dateset": ("{2020-01-01, 2020-01-05, 2020-01-10}", "{2020-01-03, 2020-01-08}"),
    "datespanset": ("{[2020-01-01, 2020-01-10)}", "{[2020-01-05, 2020-01-20)}"),
    "tstzspan": ("[2020-01-01 00:00:00+00, 2020-01-10 00:00:00+00)",
                 "[2020-01-05 00:00:00+00, 2020-01-20 00:00:00+00)"),
    "tstzset": ("{2020-01-01 00:00:00+00, 2020-01-05 00:00:00+00}",
                "{2020-01-03 00:00:00+00, 2020-01-08 00:00:00+00}"),
    "tstzspanset": ("{[2020-01-01 00:00:00+00, 2020-01-10 00:00:00+00)}",
                    "{[2020-01-05 00:00:00+00, 2020-01-20 00:00:00+00)}"),
    "stbox_text": ("STBOX X((1,1),(5,5))", "STBOX X((3,3),(7,7))"),
    "tbox_text": ("TBOXFLOAT XT([1, 5],[2020-01-01, 2020-01-05])",
                  "TBOXFLOAT XT([3, 7],[2020-01-03, 2020-01-07])"),
}
BOX_PARSER = {"intspan": "intspan_in", "bigintspan": "bigintspan_in", "floatspan": "floatspan_in",
              "intset": "intset_in", "bigintset": "bigintset_in", "floatset": "floatset_in",
              "intspanset": "intspanset_in", "bigintspanset": "bigintspanset_in",
              "floatspanset": "floatspanset_in", "stbox_text": "stbox_in", "tbox_text": "tbox_in"}
BOX_CTYPE  = {"intspan": "Span", "bigintspan": "Span", "floatspan": "Span",
              "intset": "Set", "bigintset": "Set", "floatset": "Set",
              "intspanset": "SpanSet", "bigintspanset": "SpanSet", "floatspanset": "SpanSet",
              "stbox_text": "STBox", "tbox_text": "TBox"}
RET_TEXT   = {("Span", k): v for k, v in {"int":"intspan_text","bigint":"bigintspan_text","float":"floatspan_text"}.items()}
RET_TEXT.update({("Set", k): v for k, v in {"int":"intset_text","bigint":"bigintset_text","float":"floatset_text"}.items()})
RET_TEXT.update({("SpanSet", k): v for k, v in {"int":"intspanset_text","bigint":"bigintspanset_text","float":"floatspanset_text"}.items()})

CONTAINER_C = {"Span": "span", "Set": "set", "SpanSet": "spanset", "STBox": "stbox", "TBox": "tbox"}

# Unary accessors (lower/upper/width/start_value/end_value/inc/num_*): a single
# Span/Set/SpanSet operand -> scalar. The container input_type is the leading name
# token (intspan_lower -> intspan); the type-generic forms (span_lower_inc,
# set_num_values, spanset_num_spans) carry no subtype, so exercise them with the
# int-subtype literal. The C return type maps straight to a scalar sink.
GENERIC_CONTAINER = {"span": "intspan", "set": "intset", "spanset": "intspanset"}
SCALAR_RET = {"int": "int", "int64": "int64", "double": "double", "bool": "bool",
              "DateADT": "int", "TimestampTz": "int64"}


def load_sigs():
    txt = re.sub(r"\s+", " ", open(HEADERS).read())
    sigs = {}
    for d in txt.split(";"):
        d = d.strip()
        # re.search (not match): the \s+ collapse glues a preceding block comment
        # (no ';' inside) onto the first declaration after it, so the chunk can
        # start with '/*...*/' rather than 'extern' (e.g. bigintset_end_value, the
        # first accessor after its section header). Each chunk still holds at most
        # one declaration since they are split on ';'.
        m = re.search(r"extern\s+(.+?)\s+(\*?)(\w+)\s*\((.*)\)", d)
        if not m:
            continue
        ret = (m.group(1) + " " + m.group(2)).strip()
        name, params = m.group(3), m.group(4).strip()
        plist = []
        if params and params != "void":
            for p in params.split(","):
                p = p.strip()
                pm = re.match(r"(?:const\s+)?(\w+)\s*(\*?)\s*\w+", p)
                if pm:
                    plist.append((pm.group(1), bool(pm.group(2))))
        sigs[name] = (ret, plist)
    return sigs


def ctype_of(base, ptr):
    if ptr:
        return {"Span": "Span", "Set": "Set", "SpanSet": "SpanSet", "STBox": "STBox", "TBox": "TBox"}.get(base)
    return None  # scalar handled separately


def subtype_of(name):
    for s in ("bigint", "int", "float", "timestamptz", "date"):
        if re.search(r"_" + s + r"(_|$)", name):
            return s
    return None


def classify(name, ret, plist):
    """Return (spec_entry, test_meta) or (None, reason)."""
    # ---- unary accessor: one Span/Set/SpanSet operand -> scalar ----
    if len(plist) == 1:
        base, ptr = plist[0]
        if not ptr or base not in ("Span", "Set", "SpanSet"):
            return None, f"unary param {base}{'*' if ptr else ''} not a span/set/spanset"
        ckey = name.split("_", 1)[0]
        input_type = GENERIC_CONTAINER.get(ckey, ckey)
        if input_type not in LITERALS:
            return None, f"no literal for container {input_type}"
        rkind = SCALAR_RET.get(ret.strip())
        if rkind is None:
            return None, f"unary return {ret.strip()} unmapped"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[],
                     comment_one_liner=f"{name} ({ret.strip()}) — unary {input_type} accessor.")
        tmeta = dict(cols=[("a", "VARSIZED")],
                     rows=[(LITERALS[input_type][0],), (LITERALS[input_type][1],)],
                     token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    if len(plist) != 2:
        return None, f"arity {len(plist)} (not 2)"
    # identify container params (pointer to Span/Set/SpanSet/STBox/TBox) vs scalars
    kinds = []
    for base, ptr in plist:
        cc = ctype_of(base, ptr)
        if cc:
            kinds.append(("box", cc))
        elif base in ("int", "int64", "double", "DateADT", "TimestampTz"):
            kinds.append(("scalar", base))
        else:
            return None, f"unsupported param {base}{'*' if ptr else ''}"
    sub = subtype_of(name)
    # ---- container + scalar (set-algebra & predicates ⊕ value) ----
    box_idx = [i for i, k in enumerate(kinds) if k[0] == "box"]
    sca_idx = [i for i, k in enumerate(kinds) if k[0] == "scalar"]
    if len(box_idx) == 1 and len(sca_idx) == 1:
        bi, si = box_idx[0], sca_idx[0]
        cc = kinds[bi][1]
        if cc in ("STBox", "TBox"):
            return None, "box+scalar not expected"
        if sub is None:
            return None, "no subtype token"
        keymap = {"Span": SUBSPAN, "Set": SUBSET, "SpanSet": SUBSS}[cc]
        if sub not in keymap:
            return None, f"subtype {sub} unsupported"
        input_type = keymap[sub]
        scbase = kinds[si][1]
        rkind = ret_kind(ret, sub)
        if rkind is None:
            return None, f"return {ret} unmapped"
        box_first = (bi == 1)  # container is the 2nd param -> swap so call=(scalar,container)
        if scbase in SCALARS:
            cpp, col_sql, sa, sb = SCALARS[scbase]
            extra = dict(kind="scalar", cpp=cpp)
        else:  # date/tstz: a scalar VALUE parsed from a quoted text field
            parser, pextra, sa, sb = SCALARS_TEXT[scbase]
            col_sql = "VARSIZED"
            extra = dict(kind="scalar_text", ctype=scbase, parser=parser,
                         parser_extra=pextra, header="meos.h")
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[extra],
                     comment_one_liner=f"{name} ({ret.strip()}) — generic set-algebra/predicate, container⊕value.")
        if box_first:
            entry["box_first"] = True
        tmeta = dict(cols=[("a", "VARSIZED"), ("b", col_sql)],
                     rows=[(LITERALS[input_type][0], sa), (LITERALS[input_type][1], sb)],
                     token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    # ---- container + container (box∩box algebra OR box/box predicate) ----
    if len(box_idx) == 2:
        c0 = kinds[0][1]; c1 = kinds[1][1]
        if c0 in ("STBox", "TBox") or c1 in ("STBox", "TBox"):
            if c0 != c1:
                return None, f"mixed boxes {c0}/{c1}"
            input_type = "stbox_text" if c0 == "STBox" else "tbox_text"
            rkind = ret_kind(ret, None)          # bool/int predicate -> "int"; box result -> *_text_out
            if rkind is None:
                return None, f"box-box return {ret} unmapped"
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=input_type, return_kind=rkind,
                         extra_args=[dict(kind="box", box_type=c0, parser=BOX_PARSER[input_type], header="meos.h")],
                         comment_one_liner=f"{name} ({ret.strip()}) — {c0} box-box operator over two literals.")
            l0, l1 = LITERALS[input_type]
            tmeta = dict(cols=[("a", "VARSIZED"), ("b", "VARSIZED")],
                         rows=[(l0, l1), (l1, l0)], token=name.upper(), sink=sink_of(rkind))
            return entry, tmeta
        # type-generic Span/Set/SpanSet over two literals (e.g. contains_span_spanset);
        # one MEOS symbol covers all subtypes, so int-subtype literals exercise it.
        INTKEY = {"Span": "intspan", "Set": "intset", "SpanSet": "intspanset"}
        if c0 not in INTKEY or c1 not in INTKEY:
            return None, f"unhandled box-box {c0}/{c1}"
        rkind = ret_kind(ret, "int")
        if rkind is None:
            return None, f"box-box return {ret} unmapped"
        prim, sec = INTKEY[c0], INTKEY[c1]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=prim, return_kind=rkind,
                     extra_args=[dict(kind="box", box_type=c1, parser=BOX_PARSER[sec], header="meos.h")],
                     comment_one_liner=f"{name} ({ret.strip()}) — {c0}/{c1} operator over two literals.")
        tmeta = dict(cols=[("a", "VARSIZED"), ("b", "VARSIZED")],
                     rows=[(LITERALS[prim][0], LITERALS[sec][0]),
                           (LITERALS[prim][1], LITERALS[sec][1])],
                     token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    return None, "unhandled operand mix"


def sink_of(rkind):
    if rkind == "int":
        return "INT32"
    if rkind == "int64":
        return "INT64"
    if rkind == "double":
        return "FLOAT64"
    if rkind == "bool":
        return "BOOLEAN"
    return "VARSIZED"


def ret_kind(ret, sub):
    ret = ret.strip()
    if ret in ("bool", "int"):
        return "int"
    if ret == "double":
        return "double"
    base = ret.replace("*", "").strip()
    if base in ("Span", "Set", "SpanSet"):
        return RET_TEXT.get((base, sub))
    if base == "STBox":
        return "stbox_text_out"
    if base == "TBox":
        return "tbox_text_out"
    return None


def camel(name):
    return "".join(p.capitalize() for p in name.split("_"))


def make_test(name, tmeta):
    src = re.sub(r"[^a-z0-9]", "", name)[:18] + "s"
    cols = ", ".join(f"{c} {t}" for c, t in [("id", "UINT64")] + tmeta["cols"])
    rows = []
    for i, r in enumerate(tmeta["rows"], 1):
        cells = [str(i)]
        for (c, t), v in zip(tmeta["cols"], r):
            cells.append(f"'{v}'" if t == "VARSIZED" else str(v))
        rows.append("|".join(cells))
    argcols = ", ".join(c for c, _ in tmeta["cols"])
    sink = tmeta.get("sink", "VARSIZED")
    body = (
        f"# name: MEOS_{tmeta['token']}\n"
        f"# description: {name} — autogenerated algebra wave; result recorded.\n"
        f"# groups: [Function, MEOS, Algebra, Codegen]\n"
        f"CREATE LOGICAL SOURCE {src}({cols});\n"
        f"CREATE PHYSICAL SOURCE FOR {src} TYPE File SET('|' AS PARSER.FIELD_DELIMITER);\n"
        f"ATTACH INLINE\n" + "\n".join(rows) + "\n\n"
        f"CREATE SINK {src}_out({src}.id UINT64, r {sink}) TYPE File;\n"
        f"SELECT id, {tmeta['token']}({argcols}) AS r FROM {src} INTO {src}_out;\n"
        f"----\n")
    return body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--names", required=True)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--out-spec")
    ap.add_argument("--out-tests-dir")
    a = ap.parse_args()
    names = [l.strip() for l in (sys.stdin if a.names == "-" else open(a.names)) if l.strip()]
    sigs = load_sigs()
    ops, tests, skipped = [], {}, []
    for n in names:
        if n not in sigs:
            skipped.append((n, "no signature")); continue
        ret, plist = sigs[n]
        entry, meta = classify(n, ret, plist)
        if entry is None:
            skipped.append((n, meta)); continue
        ops.append(entry)
        tests[n] = make_test(n, meta)
    print(f"# mapped {len(ops)} ops, skipped {len(skipped)}")
    for n, why in skipped:
        print(f"#   SKIP {n}: {why}")
    if a.dry_run:
        for o in ops:
            print(f"  {o['meos_call']:30s} in={o['input_type']:14s} ret={o['return_kind']:18s} "
                  f"box_first={o.get('box_first', False)} "
                  f"extra={o['extra_args'][0].get('kind') if o['extra_args'] else 'unary'}")
        return
    if a.out_spec:
        json.dump({"_comment": "autogenerated algebra wave", "operators": ops}, open(a.out_spec, "w"), indent=2)
        print(f"# wrote {a.out_spec}")
    if a.out_tests_dir:
        for n, body in tests.items():
            open(f"{a.out_tests_dir}/{n}.test", "w").write(body)
        print(f"# wrote {len(tests)} .test files to {a.out_tests_dir}")


if __name__ == "__main__":
    main()
