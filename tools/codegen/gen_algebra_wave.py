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
    "textset": ('{"AAA", "BBB", "CCC"}', '{"BBB", "DDD"}'),
    "cbuffer": ("Cbuffer(Point(1 1),1.0)", "Cbuffer(Point(2 2),0.5)"),
    "pose": ("Pose(Point(1 1), 0.5)", "Pose(Point(2 2), 1.0)"),
    "npoint": ("NPoint(1, 0.5)", "NPoint(1, 0.7)"),
    "nsegment": ("NSegment(1, 0.2, 0.8)", "NSegment(1, 0.5, 0.7)"),
    "stbox_text": ("STBOX X((1,1),(5,5))", "STBOX X((3,3),(7,7))"),
    "tbox_text": ("TBOXFLOAT XT([1, 5],[2020-01-01, 2020-01-05])",
                  "TBOXFLOAT XT([3, 7],[2020-01-03, 2020-01-07])"),
    # Object-set text literals: a brace-list of DOUBLE-QUOTED element WKTs (the
    # canonical *set_out form; *set_in re-parses it). The outer single-quote of
    # the .test VARSIZED field is stripped by the generic text-literal input, the
    # interior double-quotes are preserved and delimit the set elements.
    "cbufferset": ('{"Cbuffer(Point(1 1),0.5)", "Cbuffer(Point(2 2),0.5)"}',
                   '{"Cbuffer(Point(3 3),1.0)", "Cbuffer(Point(4 4),0.5)"}'),
    "npointset":  ('{"Npoint(1,0.5)", "Npoint(2,0.5)"}',
                   '{"Npoint(1,0.7)", "Npoint(3,0.5)"}'),
    "poseset":    ('{"Pose(Point(1 1),0.5)", "Pose(Point(2 2),1.0)"}',
                   '{"Pose(Point(3 3),0.3)", "Pose(Point(4 4),0.5)"}'),
    "geoset":     ('{"Point(1 1)", "Point(2 2)"}',
                   '{"Point(3 3)", "Point(4 4)"}'),
    # A bare geometry primary. SRID-prefixed + multi-point so srid/num_points are
    # meaningful; parsed via geom_in (codegen GENERIC_INPUTS "geom").
    "geom":       ("SRID=4326;Linestring(1 1, 2 2)", "SRID=4326;Linestring(3 3, 4 4, 5 5)"),
    "interval":   ("1 day", "2 days 3 hours"),
}
# Interval* extra arg: parsed from an ISO text literal via interval_in with typmod
# -1 (a `box`-kind extra, already wired in codegen_nebula) and freed by the
# assembler. interval_out serializes an Interval result.
IVAL_EXTRA = dict(kind="box", box_type="Interval", parser="interval_in",
                  parser_extra=", -1", header="meos.h")
# object/geo/text set ± value algebra & predicates. The value type (the non-`set`
# name token) -> (object-set primary input_type, value extra-arg, *set_text return
# serializer for algebra results). geo/text reuse the existing geom/text extra
# kinds; cbuffer/npoint/pose parse+free as a `box`-kind extra (object parser).
SETVAL = {
    "geo":     ("geoset",     dict(kind="geom"),                                                              "geoset_text"),
    "cbuffer": ("cbufferset", dict(kind="box", box_type="Cbuffer", parser="cbuffer_in", header="meos_cbuffer.h"), "cbufferset_text"),
    "npoint":  ("npointset",  dict(kind="box", box_type="Npoint",  parser="npoint_in",  header="meos_npoint.h"),  "npointset_text"),
    "pose":    ("poseset",    dict(kind="box", box_type="Pose",    parser="pose_in",    header="meos_pose.h"),    "poseset_text"),
    "text":    ("textset",    dict(kind="text"),                                                               "textset_text"),
}
# A value that IS an element of the set's row1/row2 literal (aligned to LITERALS):
# makes contains true and keeps intersection / set-first minus non-empty.
SETVAL_LIT_IN = {
    "geo":     ("Point(1 1)", "Point(3 3)"),
    "cbuffer": ("Cbuffer(Point(1 1),0.5)", "Cbuffer(Point(3 3),1.0)"),
    "npoint":  ("Npoint(1,0.5)", "Npoint(1,0.7)"),
    "pose":    ("Pose(Point(1 1),0.5)", "Pose(Point(3 3),0.3)"),
    "text":    ("AAA", "BBB"),
}
# A value NOT in the set: keeps a value-first minus ({value} \ set) non-empty.
SETVAL_LIT_OUT = {
    "geo":     ("Point(9 9)", "Point(8 8)"),
    "cbuffer": ("Cbuffer(Point(9 9),0.5)", "Cbuffer(Point(8 8),0.5)"),
    "npoint":  ("Npoint(9,0.5)", "Npoint(8,0.5)"),
    "pose":    ("Pose(Point(9 9),0.5)", "Pose(Point(8 8),0.5)"),
    "text":    ("ZZZ", "YYY"),
}
SETVAL_OPS = ("contains", "contained", "left", "right", "overleft", "overright",
              "union", "intersection", "minus")
# span constructors: name -> (base-scalar primary, value SQL, value cpp, *span_text
# return, lower/upper sample for row1, lower/upper for row2). lower_inc/upper_inc
# are passed true/false. date = days-since-2000, tstz = µs-since-2000.
SPAN_MAKE = {
    "intspan_make":    ("int_base", "INT32", "int32_t", "intspan_text", "1", "5", "2", "8"),
    "bigintspan_make": ("bigint_base", "INT64", "int64_t", "bigintspan_text", "1", "5", "2", "8"),
    "floatspan_make":  ("float_base", "FLOAT64", "double", "floatspan_text", "1.5", "5.5", "2.5", "8.5"),
    "datespan_make":   ("date_base", "INT32", "int32_t", "datespan_text", "100", "200", "150", "300"),
    "tstzspan_make":   ("timestamptz_base", "INT64", "int64_t", "tstzspan_text", "1000000", "5000000", "2000000", "8000000"),
}
# instant + object constructors: name -> dict(prim input, primary literal pair
# `plit` (geom/object VARSIZED) OR base-scalar `psql`/`pa`/`pb`, scalar `extras`
# [(cpp, sql, a, b)], return serializer). ts = µs-since-2000 (2020-01-01 ≈ 6.3e14).
_TS = ("int64_t", "INT64", "631152000000000", "631238400000000")
MAKE_SPEC = {
    "tfloatinst_make":  dict(prim="float_base", psql="FLOAT64", pa="5.5", pb="8.5", extras=[_TS], ret="tfloat_out"),
    "tintinst_make":    dict(prim="int_base", psql="INT32", pa="5", pb="8", extras=[_TS], ret="tint_out"),
    "tpointinst_make":  dict(prim="geom", plit=("SRID=4326;Point(1 1)", "SRID=4326;Point(2 2)"), extras=[_TS], ret="tspatial_text"),
    "tgeoinst_make":    dict(prim="geom", plit=("SRID=4326;Point(1 1)", "SRID=4326;Point(2 2)"), extras=[_TS], ret="tspatial_text"),
    "tnpointinst_make": dict(prim="npoint", plit=("NPoint(1, 0.5)", "NPoint(1, 0.7)"), extras=[_TS], ret="tspatial_text"),
    "npoint_make":      dict(prim="bigint_base", psql="INT64", pa="1", pb="2", extras=[("double", "FLOAT64", "0.5", "0.7")], ret="npoint_value_out"),
    "cbuffer_make":     dict(prim="geom", plit=("Point(1 1)", "Point(2 2)"), extras=[("double", "FLOAT64", "1.0", "0.5")], ret="cbuffer_value_out"),
    "pose_make_2d":     dict(prim="float_base", psql="FLOAT64", pa="1.0", pb="2.0",
                             extras=[("double", "FLOAT64", "1.0", "2.0"), ("double", "FLOAT64", "0.5", "1.0"), ("int32_t", "INT32", "0", "0")], ret="pose_value_out"),
    "pose_make_point2d":dict(prim="geom", plit=("Point(1 1)", "Point(2 2)"), extras=[("double", "FLOAT64", "0.5", "1.0")], ret="pose_value_out"),
}
BOX_PARSER = {"intspan": "intspan_in", "bigintspan": "bigintspan_in", "floatspan": "floatspan_in",
              "datespan": "datespan_in", "tstzspan": "tstzspan_in",
              "intset": "intset_in", "bigintset": "bigintset_in", "floatset": "floatset_in",
              "dateset": "dateset_in", "tstzset": "tstzset_in",
              "intspanset": "intspanset_in", "bigintspanset": "bigintspanset_in",
              "floatspanset": "floatspanset_in", "datespanset": "datespanset_in",
              "tstzspanset": "tstzspanset_in", "stbox_text": "stbox_in", "tbox_text": "tbox_in"}
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
              "DateADT": "int", "TimestampTz": "int64",
              "int32": "int", "int32_t": "int", "uint32": "int"}
# TBox/STBox single-operand accessors map straight onto the box-text primary
# input (the box-box cmp wave already proved tbox_in/stbox_in as a primary).
BOX_INPUT = {"TBox": "tbox_text", "STBox": "stbox_text"}

# Arity-1 X_to_box/span conversion constructors (int_to_tbox, span_to_tbox,
# tnumber_to_tbox, tbox_to_intspan, …). The single operand picks the primary
# input builder; the result is a VARSIZED box/span serialized via *_out.
#   base scalar value -> the *_base value-reader input (frees:False)
CONV_BASE_IN  = {"int": "int_base", "double": "float_base", "TimestampTz": "timestamptz_base"}
CONV_BASE_COL = {"int_base": ("INT32", "5", "8"),
                 "float_base": ("FLOAT64", "5.5", "8.5"),
                 "timestamptz_base": ("INT64", "1609459200", "1609545600")}
#   numeric Span/Set/SpanSet operand -> a float-subtype text-literal primary
CONV_CONTAINER_IN = {"Span": "floatspan", "Set": "floatset", "SpanSet": "floatspanset"}
#   Span result subtype (from the to_<X>span suffix) -> its *_out serializer key
CONV_SPAN_RET = {"intspan": "intspan_text", "floatspan": "floatspan_text",
                 "tstzspan": "tstzspan_text", "datespan": "datespan_text"}
#   SpanSet result subtype (from the to_<X>spanset suffix) -> its *_out key
CONV_SPANSET_RET = {"intspanset": "intspanset_text", "floatspanset": "floatspanset_text",
                    "tstzspanset": "tstzspanset_text", "datespanset": "datespanset_text"}

# Name-gated unary VARSIZED accessors `T *f(const C *)` returning a span/set/
# spanset. Name-gated (NOT signature-derived) because the header collapses a
# `Span **` array return (spanset_spans, set_spans — array out, deferred) to the
# same parsed ('Span *', [SpanSet]) shape as a single-span accessor, so only the
# name distinguishes them. value -> (operand C type, primary input, *_out key).
UNARY_VARSIZED = {
    "spanset_span":       ("SpanSet", "floatspanset", "floatspan_text"),
    "spanset_start_span": ("SpanSet", "floatspanset", "floatspan_text"),
    "spanset_end_span":   ("SpanSet", "floatspanset", "floatspan_text"),
    "set_copy":           ("Set",     "floatset",     "floatset_text"),
    "spanset_copy":       ("SpanSet", "floatspanset", "floatspanset_text"),
}
# Name-gated unary VARSIZED accessors over a single temporal instant returning a
# span/spanset whose name has NO '_to_' (so the conv branch misses them). A
# tfloat instant is built (value, ts); the result serialises via its *_out.
TEMPORAL_UNARY_VARSIZED = {
    "temporal_time":      "tstzspanset_text",     # the time spanset of a temporal
    "tnumber_valuespans": "floatspanset_text",    # the value spanset of a tnumber
}
# `_round` rounds an operand to `maxdd` decimals and returns the SAME type. The
# operand base type -> (primary input_type, same-type *_out return_kind). float_
# round (a plain double) is handled inline. Object returns reuse the Wave-J
# object value_out serializers; geo/nsegment deferred (no primary/serializer).
ROUND_RET = {
    "Span":     ("floatspan",    "floatspan_text"),
    "SpanSet":  ("floatspanset", "floatspanset_text"),
    "Set":      ("floatset",     "floatset_text"),
    "STBox":    ("stbox_text",   "stbox_text_out"),
    "TBox":     ("tbox_text",    "tbox_text_out"),
    "Temporal": ("tfloat",       "tfloat_out"),
    "Cbuffer":  ("cbuffer",      "cbuffer_value_out"),
    "Pose":     ("pose",         "pose_value_out"),
    "Npoint":   ("npoint",       "npoint_value_out"),
    "GSERIALIZED": ("geom",      "geo_value_out"),
}
#   per-op box literal override (tbox_to_intspan needs an INT box, not the
#   default TBOXFLOAT) — keyed by op name.
CONV_BOX_LIT = {"tbox_to_intspan": ("TBOXINT XT([1, 5],[2020-01-01, 2020-01-05])",
                                    "TBOXINT XT([3, 7],[2020-01-03, 2020-01-07])")}

# Out-param accessors `bool f(const Box*, T *result)`: the out-param C type ->
# (cpp decl, generic return_kind). The validity-flag return is discarded.
OUT_PARAM_RET = {"double": ("double", "double"), "int": ("int", "int"),
                 "TimestampTz": ("TimestampTz", "int64"), "bool": ("bool", "bool"),
                 "int64": ("int64", "int64"), "DateADT": ("DateADT", "int")}
# value_n primary operand -> the GENERIC_INPUTS builder key + its test column.
#   temporal subtypes build a single instant; set subtypes parse a text literal.
VALUE_N_TEMPORAL = {"tint": ("INT32", "5", "8"), "tfloat": ("FLOAT64", "5.5", "8.5"),
                    "tbool": ("BOOLEAN", "true", "false"), "ttext": ("VARSIZED", "ABC", "DEF")}
VALUE_N_SET = {"intset", "bigintset", "floatset", "dateset", "tstzset", "textset"}
# Object-element sets: a Set* of heap objects whose start/end_value and value_n
# return the element via the *_value_out serializers (a brace-list literal input).
# geoset parses via geomset_in (no geoset_in symbol); see codegen_nebula registry.
OBJECT_SETS = {"cbufferset", "npointset", "poseset", "geoset"}
# value_n out-params that are a heap pointer (T **result) -> the *_out serializer
# key (VARSIZED return), as opposed to the scalar OUT_PARAM_RET out-params.
VALUE_N_VARSIZED = {"text": "text_value_out", "GSERIALIZED": "geo_value_out",
                    "Pose": "pose_value_out", "Cbuffer": "cbuffer_value_out",
                    "Npoint": "npoint_value_out"}
# per-op box literal override for accessors that need a specific box flavour:
# tboxint_* need an INT box; stbox time accessors need an STBOX carrying T.
_STBOX_XT = ("STBOX XT(((1,1),(5,5)),[2020-01-01, 2020-01-05])",
             "STBOX XT(((3,3),(7,7)),[2020-01-03, 2020-01-07])")
_TBOXINT  = ("TBOXINT XT([1, 5],[2020-01-01, 2020-01-05])",
             "TBOXINT XT([3, 7],[2020-01-03, 2020-01-07])")
ACCESSOR_BOX_LIT = {"tboxint_xmin": _TBOXINT, "tboxint_xmax": _TBOXINT,
                    "stbox_tmin": _STBOX_XT, "stbox_tmax": _STBOX_XT,
                    "stbox_tmin_inc": _STBOX_XT, "stbox_tmax_inc": _STBOX_XT}

# Object scalar types compared/related to another value of the same type
# (cbuffer/pose/npoint/nsegment cmp/eq/ne/lt/le/gt/ge/same and the cbuffer
# spatial rels). Both operands parse from a quoted text literal; bool/int sink.
# value -> (input_type key, *_in parser, header).
OBJECT_TYPES = {"Cbuffer": ("cbuffer", "cbuffer_in", "meos_cbuffer.h"),
                "Pose": ("pose", "pose_in", "meos_pose.h"),
                "Npoint": ("npoint", "npoint_in", "meos_npoint.h"),
                "Nsegment": ("nsegment", "nsegment_in", "meos_npoint.h")}

# Temporal-instant ⊗ scalar ops returning a Temporal* (arithmetic / at-value /
# minus / shift / scale -> same numeric subtype; temporal comparison -> tbool;
# tdistance -> tfloat). The temporal operand is built from a single (value, ts)
# instant; the result is serialized to WKT via its subtype *_out (clean: a tint/
# tfloat/tbool WKT is 'value@timestamp...', no interior '('). value samples + the
# scalar samples differ per subtype. ts samples are epoch seconds (proven-good).
# subtype -> (value SQL col, value cpp, value sample a, value sample b).
TEMPORAL_INPUTS = {"tint": ("INT32", "int32_t", "5", "8"),
                   "tfloat": ("FLOAT64", "double", "5.5", "8.5"),
                   "ttext": ("VARSIZED", "text", "ABC", "DEF"),
                   "tbool": ("BOOLEAN", "bool", "true", "false")}
# base C type -> (SQL col, cpp, sample a, sample b). 'text' is a text* literal
# (held in a constant XYZ, distinct from the ttext values so minus_value keeps a
# non-empty result and textcat is non-trivial).
TSCALAR_COL = {"int": ("INT32", "int32_t", "3", "2"),
               "double": ("FLOAT64", "double", "3.5", "2.5"),
               "text": ("VARSIZED", "text", "XYZ", "XYZ"),
               "bool": ("BOOLEAN", "bool", "false", "true")}
TCMP = ("tlt", "tle", "tgt", "tge", "teq", "tne")

# always_/ever_ reductions: a temporal ⊗ a base value -> bool. The temporal is
# built from a single multi-column instant (mirrors codegen_nebula GENERIC_INPUTS
# field layouts); the base is a bool scalar (BOOLEAN source field) or an object
# text literal (npoint/pose/cbuffer). ts (UINT64) is appended automatically.
# subtype -> [(col, SQL, sample a, sample b), ...] for the value columns.
ALWAYS_TINPUT = {
    "tbool":    [("value", "BOOLEAN", "true", "false")],
    "tnpoint":  [("rid", "INT64", "1", "1"), ("frac", "FLOAT64", "0.5", "0.7")],
    "tpose":    [("x", "FLOAT64", "1.0", "2.0"), ("y", "FLOAT64", "1.0", "2.0"),
                 ("theta", "FLOAT64", "0.5", "1.0")],
    "tcbuffer": [("lon", "FLOAT64", "1.0", "2.0"), ("lat", "FLOAT64", "1.0", "2.0"),
                 ("radius", "FLOAT64", "1.0", "0.5")],
    "ttext":    [("value", "VARSIZED", "ABC", "DEF")],
    "tgeo":     [("lon", "FLOAT64", "1.0", "2.0"), ("lat", "FLOAT64", "1.0", "2.0")],
}
# temporal subtype token -> codegen GENERIC_INPUTS builder key (when they differ).
ALWAYS_INPUT_TYPE = {"tgeo": "tgeompoint"}
# Temporal-returning result families: a temporal comparison / spatial relation
# yields a tbool; tdistance yields a tfloat. (Everything else temporal-returning —
# e.g. a tgeo restriction — serializes to geo WKT with '(' and is deferred.)
TBOOL_PREFIX = {"teq", "tne", "tlt", "tle", "tgt", "tge",
                "tcontains", "tcovers", "tdisjoint", "tintersects", "ttouches", "tdwithin"}

# Temporal restriction (temporal_at_*/minus_* and tnumber_at_*/minus_*): a temporal
# ⊗ a time/value box keeps or drops instants. Exercised with a single tint instant
# (value 5/8 @ 2021-01-01/02 = epoch 1609459200/1609545600) -> result tint (tint_out).
# at_ literal CONTAINS both instants (kept), minus_ literal contains NEITHER (kept
# unchanged) -> both rows record the input instant. Keyed by the last name token.
# token -> (parser, ctype, at-literal, minus-literal).
RESTRICT_BASE = {
    "tstzspan":    ("tstzspan_in", "Span", "[2020-01-01, 2022-01-01)", "[2025-01-01, 2026-01-01)"),
    "tstzspanset": ("tstzspanset_in", "SpanSet", "{[2020-01-01, 2022-01-01)}", "{[2025-01-01, 2026-01-01)}"),
    "tstzset":     ("tstzset_in", "Set", "{2021-01-01, 2021-01-02}", "{2025-01-01, 2025-01-02}"),
    "span":        ("intspan_in", "Span", "[1, 10)", "[100, 200)"),
    "spanset":     ("intspanset_in", "SpanSet", "{[1, 10)}", "{[100, 200)}"),
    "values":      ("intset_in", "Set", "{5, 8}", "{100, 200}"),
    "tbox":        ("tbox_in", "TBox", "TBOXINT XT([1, 10],[2020-01-01, 2022-01-01])",
                    "TBOXINT XT([100, 200],[2025-01-01, 2026-01-01])"),
}

# Two-temporal ops (teq/tne/tlt/tle/tgt/tge_temporal_temporal etc.): build BOTH
# operands from per-event instant columns (no hex) at the SAME timestamp so the
# pointwise op is defined, and serialize the Temporal result by family. The
# second instant uses *2-suffixed columns via the codegen "temporal2" extra-arg.
TWO_TEMPORAL = {
    "tint": dict(
        input_type="tint",
        make_cols=[("value", "INT32"), ("ts", "UINT64"), ("value2", "INT32"), ("ts2", "UINT64")],
        rows=[("5", "1609459200", "7", "1609459200"), ("8", "1609545600", "2", "1609545600")],
        t2_fields=[("value2", "int32_t"), ("ts2", "uint64_t")],
        header="meos.h",
        t2_build=('                std::string {var}W = fmt::format("{{}}@{{}}", value2, MEOS::Meos::convertEpochToTimestamp(ts2));\n'
                  '                Temporal* {var} = tint_in({var}W.c_str());\n'
                  '                if (!{var}) {{ free(temp); return {z}; }}\n')),
    "tgeompoint": dict(
        input_type="tgeompoint",
        make_cols=[("lon", "FLOAT64"), ("lat", "FLOAT64"), ("ts", "UINT64"),
                   ("lon2", "FLOAT64"), ("lat2", "FLOAT64"), ("ts2", "UINT64")],
        rows=[("1.0", "1.0", "1609459200", "2.0", "2.0", "1609459200"),
              ("3.0", "3.0", "1609545600", "4.0", "4.0", "1609545600")],
        t2_fields=[("lon2", "double"), ("lat2", "double"), ("ts2", "uint64_t")],
        header="meos_geo.h",
        t2_build=('                if (!(lon2 >= -180.0 && lon2 <= 180.0 && lat2 >= -90.0 && lat2 <= 90.0)) {{ free(temp); return {z}; }}\n'
                  '                std::string {var}W = fmt::format("SRID=4326;Point({{}} {{}})@{{}}", lon2, lat2, MEOS::Meos::convertEpochToTimestamp(ts2));\n'
                  '                Temporal* {var} = tgeompoint_in({var}W.c_str());\n'
                  '                if (!{var}) {{ free(temp); return {z}; }}\n')),
    "tcbuffer": dict(
        input_type="tcbuffer",
        make_cols=[("lon", "FLOAT64"), ("lat", "FLOAT64"), ("radius", "FLOAT64"), ("ts", "UINT64"),
                   ("lon2", "FLOAT64"), ("lat2", "FLOAT64"), ("radius2", "FLOAT64"), ("ts2", "UINT64")],
        rows=[("1.0", "1.0", "1.0", "1609459200", "2.0", "2.0", "0.5", "1609459200"),
              ("3.0", "3.0", "1.0", "1609545600", "4.0", "4.0", "0.5", "1609545600")],
        t2_fields=[("lon2", "double"), ("lat2", "double"), ("radius2", "double"), ("ts2", "uint64_t")],
        header="meos_cbuffer.h",
        t2_build=('                std::string {var}W = fmt::format("Cbuffer(Point({{}} {{}}),{{}})@{{}}", lon2, lat2, radius2, MEOS::Meos::convertEpochToTimestamp(ts2));\n'
                  '                Temporal* {var} = tcbuffer_in({var}W.c_str());\n'
                  '                if (!{var}) {{ free(temp); return {z}; }}\n')),
    "ttext": dict(
        input_type="ttext",
        make_cols=[("value", "VARSIZED"), ("ts", "UINT64"), ("value2", "VARSIZED"), ("ts2", "UINT64")],
        rows=[("ABC", "1609459200", "DEF", "1609459200"), ("GHI", "1609545600", "JKL", "1609545600")],
        t2_fields=[("value2", "VariableSizedData"), ("ts2", "uint64_t")],
        header="meos.h",
        t2_build=('                std::string {var}S(value2Ptr, value2Size);\n'
                  '                while (!{var}S.empty() && ({var}S.front()==\'\\\'\' || {var}S.front()==\'"\')) {var}S.erase({var}S.begin());\n'
                  '                while (!{var}S.empty() && ({var}S.back()==\'\\\'\' || {var}S.back()==\'"\')) {var}S.pop_back();\n'
                  '                std::string {var}W = fmt::format("\\"{{}}\\"@{{}}", {var}S, MEOS::Meos::convertEpochToTimestamp(ts2));\n'
                  '                Temporal* {var} = ttext_in({var}W.c_str());\n'
                  '                if (!{var}) {{ free(temp); return {z}; }}\n')),
    "tnpoint": dict(
        input_type="tnpoint",
        make_cols=[("rid", "INT64"), ("frac", "FLOAT64"), ("ts", "UINT64"),
                   ("rid2", "INT64"), ("frac2", "FLOAT64"), ("ts2", "UINT64")],
        rows=[("1", "0.5", "1609459200", "1", "0.7", "1609459200"),
              ("1", "0.3", "1609545600", "1", "0.6", "1609545600")],
        t2_fields=[("rid2", "int64_t"), ("frac2", "double"), ("ts2", "uint64_t")],
        header="meos_npoint.h",
        t2_build=('                std::string {var}W = fmt::format("NPoint({{}},{{}})@{{}}", rid2, frac2, MEOS::Meos::convertEpochToTimestamp(ts2));\n'
                  '                Temporal* {var} = tnpoint_in({var}W.c_str());\n'
                  '                if (!{var}) {{ free(temp); return {z}; }}\n')),
    "tpose": dict(
        input_type="tpose",
        make_cols=[("x", "FLOAT64"), ("y", "FLOAT64"), ("theta", "FLOAT64"), ("ts", "UINT64"),
                   ("x2", "FLOAT64"), ("y2", "FLOAT64"), ("theta2", "FLOAT64"), ("ts2", "UINT64")],
        rows=[("1.0", "1.0", "0.5", "1609459200", "2.0", "2.0", "1.0", "1609459200"),
              ("3.0", "3.0", "0.3", "1609545600", "4.0", "4.0", "0.6", "1609545600")],
        t2_fields=[("x2", "double"), ("y2", "double"), ("theta2", "double"), ("ts2", "uint64_t")],
        header="meos_pose.h",
        t2_build=('                std::string {var}W = fmt::format("Pose(Point({{}} {{}}),{{}})@{{}}", x2, y2, theta2, MEOS::Meos::convertEpochToTimestamp(ts2));\n'
                  '                Temporal* {var} = tpose_in({var}W.c_str());\n'
                  '                if (!{var}) {{ free(temp); return {z}; }}\n')),
}
# base operand -> (extra-arg builder tag, column SQL, sample a, sample b). The tag
# is "scalar" (bool source field), "text" (text* via text_in), or a *_in parser.
ALWAYS_BASE = {
    "bool":    ("scalar", "BOOLEAN", "true", "false"),
    "text":    ("text", "VARSIZED", "ABC", "DEF"),
    "Npoint":  ("npoint_in", "VARSIZED", "NPoint(1, 0.5)", "NPoint(1, 0.7)"),
    "Pose":    ("pose_in", "VARSIZED", "Pose(Point(1 1), 0.5)", "Pose(Point(2 2), 1.0)"),
    "Cbuffer": ("cbuffer_in", "VARSIZED", "Cbuffer(Point(1 1),1.0)", "Cbuffer(Point(2 2),0.5)"),
    "GSERIALIZED": ("geom", "VARSIZED", "SRID=4326;Point(1 1)", "SRID=4326;Point(2 2)"),
}
ALWAYS_BASE_HDR = {"Npoint": "meos_npoint.h", "Pose": "meos_pose.h", "Cbuffer": "meos_cbuffer.h"}


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
                # (\*{0,2}): capture single AND double pointers (a T **result
                # out-param would otherwise mis-parse, the type swallowing a char).
                pm = re.match(r"(?:const\s+)?(\w+)\s*(\*{0,2})\s*\w+", p)
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
    rbase = ret.replace("*", "").strip()
    # ---- span_make(lower, upper, lower_inc, upper_inc) -> Span: a base-scalar
    #      primary (lower) + a same-type scalar (upper) + two bool flags. ----
    if name in SPAN_MAKE:
        prim, vsql, vcpp, ret_k, lo0, hi0, lo1, hi1 = SPAN_MAKE[name]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=prim, return_kind=ret_k,
                     extra_args=[dict(kind="scalar", cpp=vcpp), dict(kind="scalar", cpp="bool"),
                                 dict(kind="scalar", cpp="bool")],
                     comment_one_liner=f"{name} ({ret.strip()}) — span constructor.")
        tmeta = dict(cols=[("value", vsql), ("arg0", vsql), ("arg1", "BOOLEAN"), ("arg2", "BOOLEAN")],
                     rows=[(lo0, hi0, "true", "false"), (lo1, hi1, "true", "false")],
                     token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- instant / object constructors (value[+more] -> TInstant / Npoint /
    #      Cbuffer / Pose): a base-scalar or geom/object primary + scalar extras. ----
    if name in MAKE_SPEC:
        s = MAKE_SPEC[name]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=s["prim"], return_kind=s["ret"],
                     extra_args=[dict(kind="scalar", cpp=c) for c, _, _, _ in s["extras"]],
                     comment_one_liner=f"{name} ({ret.strip()}) — constructor.")
        # the result type's header is not pulled in by a base/geom primary
        _rhdr = {"npoint_value_out": "meos_npoint.h", "cbuffer_value_out": "meos_cbuffer.h",
                 "pose_value_out": "meos_pose.h", "tspatial_text": "meos_geo.h"}.get(s["ret"])
        if _rhdr:
            entry["extra_headers"] = [_rhdr]
        if "plit" in s:                                 # geom/object VARSIZED literal primary
            pcol, pa, pb = ("a", "VARSIZED"), s["plit"][0], s["plit"][1]
        else:                                           # base-scalar primary
            pcol, pa, pb = ("value", s["psql"]), s["pa"], s["pb"]
        ecols = [(f"arg{i}", esql) for i, (_, esql, _, _) in enumerate(s["extras"])]
        rowa = tuple([pa] + [a for _, _, a, _ in s["extras"]])
        rowb = tuple([pb] + [b for _, _, _, b in s["extras"]])
        tmeta = dict(cols=[pcol] + ecols, rows=[rowa, rowb], token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- name-gated unary VARSIZED accessor: T *f(const C *) -> span/set/spanset ----
    if name in UNARY_VARSIZED:
        _oc, input_type, rkind = UNARY_VARSIZED[name]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[],
                     comment_one_liner=f"{name} ({ret.strip()}) — unary {input_type} -> {rbase}.")
        l0, l1 = LITERALS[input_type]
        tmeta = dict(cols=[("a", "VARSIZED")], rows=[(l0,), (l1,)],
                     token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- arity-1 X_to_box/span/spanset conversion constructor. Requires '_to_'
    #      in the name: that safely excludes the array-returning '_spans' siblings
    #      (Span ** collapses to the same parsed shape as a single Span). ----
    if len(plist) == 1 and "_to_" in name and rbase in ("TBox", "STBox", "Span", "SpanSet"):
        ob, op = plist[0]
        if not op and ob in CONV_BASE_IN:               # base scalar value
            input_type = CONV_BASE_IN[ob]
        elif op and ob == "Temporal":                   # tnumber instant
            input_type = "tfloat"
        elif op and ob in CONV_CONTAINER_IN:            # numeric span/set/spanset
            # prefer the operand's OWN subtype (the name prefix, e.g.
            # intspan_to_floatspan -> intspan in); fall back to the float subtype
            # only for type-generic conversions whose prefix is not a literal.
            pfx = name.rsplit("_to_", 1)[0]
            input_type = pfx if pfx in LITERALS else CONV_CONTAINER_IN[ob]
        elif op and ob in BOX_INPUT:                    # box -> span
            input_type = BOX_INPUT[ob]
        elif op and ob in OBJECT_TYPES:                 # cbuffer/pose/npoint/nsegment
            input_type = OBJECT_TYPES[ob][0]            # -> family-gated subdir
        else:
            return None, f"conv input {ob}{'*' if op else ''} unsupported"
        sfx = name.rsplit("_to_", 1)[1]
        if rbase == "TBox":
            rkind = "tbox_text_out"
        elif rbase == "STBox":
            rkind = "stbox_text_out"
        elif rbase == "SpanSet":                        # subtype from suffix, else float
            rkind = CONV_SPANSET_RET.get(sfx, "floatspanset_text")
        else:                                           # Span: subtype from suffix,
            rkind = CONV_SPAN_RET.get(sfx, "floatspan_text")  # else generic -> float
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[],
                     comment_one_liner=f"{name} ({ret.strip()}) — {ob} -> {rbase} conversion.")
        if input_type in CONV_BASE_COL:                 # base scalar: one value column
            col_sql, va, vb = CONV_BASE_COL[input_type]
            tmeta = dict(cols=[("value", col_sql)], rows=[(va,), (vb,)],
                         token=name.upper(), sink="VARSIZED")
        elif input_type == "tfloat":                    # single tfloat instant (value, ts)
            tmeta = dict(cols=[("value", "FLOAT64"), ("ts", "UINT64")],
                         rows=[("5.5", "1609459200"), ("8.5", "1609545600")],
                         token=name.upper(), sink="VARSIZED")
        else:                                           # container/box text literal
            l0, l1 = CONV_BOX_LIT.get(name, LITERALS[input_type])
            tmeta = dict(cols=[("a", "VARSIZED")], rows=[(l0,), (l1,)],
                         token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- value_n: bool f(Temporal|Set*, int n, T *result) -> the n-th value via
    #      an out-param. Exercised with n=1 (the first value/element, always
    #      defined for a non-empty input) so the validity flag is true. ----
    if (name.endswith("_value_n") and len(plist) == 3
            and plist[0][1] and plist[1] == ("int", False) and plist[2][1]
            and (plist[2][0] in OUT_PARAM_RET or plist[2][0] in VALUE_N_VARSIZED)):
        stem = name[:-len("_value_n")]
        if plist[0][0] == "Temporal" and stem in VALUE_N_TEMPORAL:
            input_type = stem
            vsql, va, vb = VALUE_N_TEMPORAL[stem]
            cols = [("value", vsql), ("ts", "UINT64")]
            rows = [(va, "1609459200"), (vb, "1609545600")]
        elif plist[0][0] == "Temporal" and stem in ALWAYS_TINPUT:
            # multi-column temporal (tgeo lon/lat, tpose x/y/theta) built like the
            # always/ever wave: each value column + an appended ts.
            input_type = ALWAYS_INPUT_TYPE.get(stem, stem)
            tcols = ALWAYS_TINPUT[stem]
            cols = [(c, s) for c, s, _, _ in tcols] + [("ts", "UINT64")]
            rows = [tuple([a for _, _, a, _ in tcols] + ["1609459200"]),
                    tuple([b for _, _, _, b in tcols] + ["1609545600"])]
        elif plist[0][0] == "Set" and (stem in VALUE_N_SET or stem in OBJECT_SETS):
            input_type = stem
            l0, l1 = LITERALS[input_type]
            cols, rows = [("a", "VARSIZED")], [(l0,), (l1,)]
        else:
            return None, f"value_n operand {plist[0][0]}/{stem} unsupported"
        if plist[2][0] in VALUE_N_VARSIZED:                 # heap-pointer out-param
            oc, rkind = plist[2][0], VALUE_N_VARSIZED[plist[2][0]]
        else:                                               # scalar out-param
            oc, rkind = OUT_PARAM_RET[plist[2][0]]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[], extra_call_args=["1"], out_param=dict(cpp=oc),
                     comment_one_liner=f"{name} (out-param {plist[2][0]}, n=1) — {input_type} value_n.")
        tmeta = dict(cols=cols, rows=rows, token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    # ---- arity-2 (base | numspan) + (timestamptz | tstzspan) -> TBox constructor
    #      (int/float/numspan _timestamptz/_tstzspan _to_tbox). Must precede the
    #      generic container+scalar branch, whose subtype-from-name guess is wrong
    #      here (the time token names operand 2, not the numeric primary). ----
    if name.endswith("_to_tbox") and len(plist) == 2 and rbase == "TBox":
        (b0, p0), (b1, p1) = plist
        if not p0 and b0 in CONV_BASE_IN:               # base-scalar primary
            input_type = CONV_BASE_IN[b0]
            vcol, va, vb = CONV_BASE_COL[input_type]
            prim_cols, prim_rows = [("value", vcol)], [[va], [vb]]
        elif p0 and b0 == "Span":                       # numspan primary (float lit)
            input_type = "floatspan"
            l0, l1 = LITERALS["floatspan"]
            prim_cols, prim_rows = [("a", "VARSIZED")], [[l0], [l1]]
        else:
            return None, f"to_tbox primary {b0}{'*' if p0 else ''} unsupported"
        if not p1 and b1 == "TimestampTz":              # timestamptz scalar value
            parser, pextra, sa, sb = SCALARS_TEXT["TimestampTz"]
            extra = dict(kind="scalar_text", ctype="TimestampTz", parser=parser,
                         parser_extra=pextra, header="meos.h")
        elif p1 and b1 == "Span":                       # tstzspan literal
            extra = dict(kind="box", box_type="Span", parser="tstzspan_in", header="meos.h")
            sa = "[2021-01-01, 2021-01-03)"
            sb = "[2021-01-05, 2021-01-07)"
        else:
            return None, f"to_tbox time arg {b1}{'*' if p1 else ''} unsupported"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind="tbox_text_out",
                     extra_args=[extra],
                     comment_one_liner=f"{name} ({ret.strip()}) — {b0} + {b1} -> TBox constructor.")
        cols = prim_cols + [("arg", "VARSIZED")]
        rows = [tuple(prim_rows[0] + [sa]), tuple(prim_rows[1] + [sb])]
        tmeta = dict(cols=cols, rows=rows, token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- Interval-consuming ops: expand-by-interval (tstzspan/tbox/stbox +
    #      Interval -> same), timestamptz±interval (TimestampTz + Interval ->
    #      TimestampTz µs), and interval arithmetic (Interval ⊗ Interval|double ->
    #      Interval). Interval* is already wired (interval_in box-extra typmod -1
    #      + interval_out); the primary uses the interval/tstzspan/timestamptz_base
    #      inputs. ----
    if len(plist) == 2 and (plist[0][0] == "Interval" or plist[1][0] == "Interval"):
        (b0, p0), (b1, p1) = plist
        if b0 == "Span" and p0 and rbase == "Span":          # tstzspan_expand
            input_type, rkind = "tstzspan", "tstzspan_text"
            prim_cols, prim_rows = [("a", "VARSIZED")], [[LITERALS["tstzspan"][0]], [LITERALS["tstzspan"][1]]]
        elif b0 in BOX_INPUT and p0:                          # tbox/stbox_expand_time
            input_type = BOX_INPUT[b0]
            rkind = "tbox_text_out" if b0 == "TBox" else "stbox_text_out"
            # expand_time touches the T dimension, so the box must carry one: the
            # default X-only STBox literal yields an empty result. Use an XT box.
            l0, l1 = LITERALS[input_type]
            if b0 == "STBox":
                l0 = "STBOX XT(((1,1),(5,5)),[2020-01-01, 2020-01-05])"
                l1 = "STBOX XT(((3,3),(7,7)),[2020-01-03, 2020-01-07])"
            prim_cols, prim_rows = [("a", "VARSIZED")], [[l0], [l1]]
        elif b0 == "TimestampTz" and not p0:                  # timestamptz_shift/add/minus
            input_type, rkind = "timestamptz_base", "int64"
            vcol, va, vb = CONV_BASE_COL["timestamptz_base"]
            prim_cols, prim_rows = [("value", vcol)], [[va], [vb]]
        elif b0 == "Interval" and p0:                         # add_interval_interval / mul_interval_double
            input_type, rkind = "interval", "interval_out"
            prim_cols, prim_rows = [("a", "VARSIZED")], [[LITERALS["interval"][0]], [LITERALS["interval"][1]]]
        else:
            return None, f"interval op primary {b0}{'*' if p0 else ''} unsupported"
        if b1 == "Interval" and p1:                           # 2nd operand = interval
            extra, ecol, sa, sb = dict(IVAL_EXTRA), "VARSIZED", LITERALS["interval"][0], LITERALS["interval"][1]
        elif b1 == "double" and not p1:                       # mul_interval_double
            extra, ecol, sa, sb = dict(kind="scalar", cpp="double"), "FLOAT64", "2.0", "3.0"
        else:
            return None, f"interval op 2nd operand {b1}{'*' if p1 else ''} unsupported"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[extra],
                     comment_one_liner=f"{name} ({ret.strip()}) — Interval-consuming op.")
        cols = prim_cols + [("arg", ecol)]
        rows = [tuple(prim_rows[0] + [sa]), tuple(prim_rows[1] + [sb])]
        tmeta = dict(cols=cols, rows=rows, token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    # ---- arity-1 ttext transform/accessor: a single ttext instant ->
    #      a ttext (lower/upper/initcap, via ttext_out) or its text* value
    #      (start/end/min/max_value, via text_out). VARSIZED, no '(' so records. ----
    if (len(plist) == 1 and plist[0] == ("Temporal", True) and "ttext" in name
            and rbase in ("Temporal", "text")):
        rkind = "ttext_out" if rbase == "Temporal" else "text_value_out"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type="ttext", return_kind=rkind,
                     extra_args=[],
                     comment_one_liner=f"{name} ({ret.strip()}) — unary ttext -> {rbase}.")
        tmeta = dict(cols=[("value", "VARSIZED"), ("ts", "UINT64")],
                     rows=[("ABC", "1609459200"), ("DEF", "1609545600")],
                     token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- name-gated unary temporal -> span/spanset (temporal_time,
    #      tnumber_valuespans): a tfloat instant in, a VARSIZED span/spanset out.
    #      No '_to_' so the conv branch above misses them. ----
    if (len(plist) == 1 and plist[0] == ("Temporal", True)
            and name in TEMPORAL_UNARY_VARSIZED):
        rkind = TEMPORAL_UNARY_VARSIZED[name]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type="tfloat", return_kind=rkind,
                     extra_args=[],
                     comment_one_liner=f"{name} ({ret.strip()}) — unary temporal -> {rbase}.")
        tmeta = dict(cols=[("value", "FLOAT64"), ("ts", "UINT64")],
                     rows=[("5.5", "1609459200"), ("8.5", "1609545600")],
                     token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- `_duration`: a span/spanset (text literal) or temporal (tfloat
    #      instant) -> an Interval (ISO text via interval_out). The spanset/
    #      temporal variants take a trailing `boundspan` bool (passed true). ----
    if name.endswith("_duration") and rbase == "Interval":
        stem = name[:-len("_duration")]
        eca = ["true"] if (len(plist) == 2 and plist[-1] == ("bool", False)) else []
        if stem == "temporal":
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type="tfloat", return_kind="interval_out",
                         extra_args=[], extra_call_args=eca,
                         comment_one_liner=f"{name} (Interval *) — temporal duration.")
            tmeta = dict(cols=[("value", "FLOAT64"), ("ts", "UINT64")],
                         rows=[("5.5", "1609459200"), ("8.5", "1609545600")],
                         token=name.upper(), sink="VARSIZED")
            return entry, tmeta
        if stem in LITERALS:
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=stem, return_kind="interval_out",
                         extra_args=[], extra_call_args=eca,
                         comment_one_liner=f"{name} (Interval *) — {stem} duration.")
            l0, l1 = LITERALS[stem]
            tmeta = dict(cols=[("a", "VARSIZED")], rows=[(l0,), (l1,)],
                         token=name.upper(), sink="VARSIZED")
            return entry, tmeta
        return None, f"duration operand {stem} unsupported"
    # ---- unary accessor: one Span/Set/SpanSet/TBox/STBox/object/Temporal
    #      operand -> scalar (hash, srid, …) ----
    if len(plist) == 1:
        base, ptr = plist[0]
        tfloat_instant = False
        tinstant_sub = None
        if ptr and base in BOX_INPUT:
            input_type = BOX_INPUT[base]            # tbox_text / stbox_text primary
        elif ptr and base in ("Span", "Set", "SpanSet"):
            ckey = name.split("_", 1)[0]
            input_type = GENERIC_CONTAINER.get(ckey, ckey)
            if input_type not in LITERALS:
                return None, f"no literal for container {input_type}"
        elif ptr and base in OBJECT_TYPES:          # cbuffer/pose/npoint/nsegment
            input_type = OBJECT_TYPES[base][0]      # -> family-gated subdir
            if input_type not in LITERALS:
                return None, f"no literal for object {input_type}"
        elif ptr and base == "GSERIALIZED":         # bare geometry via geom_in
            input_type = "geom"
        elif ptr and base == "Temporal":            # single temporal instant
            # a named spatial subtype (tnpoint_route, tpose_start_value,
            # tgeo_*, tcbuffer_*) builds its OWN instant; everything else
            # (temporal_*, tfloat_*, tint_*) is a tfloat instant.
            tinstant_sub = next((t for t in ("tnpoint", "tpose", "tgeo", "tcbuffer")
                                 if t in name.split("_")), None)
            if tinstant_sub:
                input_type = ALWAYS_INPUT_TYPE.get(tinstant_sub, tinstant_sub)
            else:
                input_type, tfloat_instant = "tfloat", True
        else:
            return None, f"unary param {base}{'*' if ptr else ''} not a span/set/spanset/box/object/temporal"
        # A heap-object return (Cbuffer*/Npoint*/Pose*/GSERIALIZED*, e.g. the
        # object-set start_value/end_value) serialises through its *_value_out the
        # same way the value_n out-param family does; otherwise map a scalar sink.
        if rbase in VALUE_N_VARSIZED:
            rkind = VALUE_N_VARSIZED[rbase]
        else:
            rkind = SCALAR_RET.get(ret.strip())
            if rkind is None:
                return None, f"unary return {ret.strip()} unmapped"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[],
                     comment_one_liner=f"{name} ({ret.strip()}) — unary {input_type} accessor.")
        if tfloat_instant:
            tmeta = dict(cols=[("value", "FLOAT64"), ("ts", "UINT64")],
                         rows=[("5.5", "1609459200"), ("8.5", "1609545600")],
                         token=name.upper(), sink=sink_of(rkind))
        elif tinstant_sub:                              # multi-column spatial instant
            tcols = ALWAYS_TINPUT[tinstant_sub]
            cols = [(c, s) for c, s, _, _ in tcols] + [("ts", "UINT64")]
            rowa = tuple([a for _, _, a, _ in tcols] + ["1609459200"])
            rowb = tuple([b for _, _, _, b in tcols] + ["1609545600"])
            tmeta = dict(cols=cols, rows=[rowa, rowb], token=name.upper(), sink=sink_of(rkind))
        else:
            tmeta = dict(cols=[("a", "VARSIZED")],
                         rows=[(LITERALS[input_type][0],), (LITERALS[input_type][1],)],
                         token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    # arity-3 spatial restriction (temp, STBox, bool border_inc) is handled by the
    # spatial-restriction branch below; let it through the arity-2 gate.
    _restrict3 = (len(plist) == 3 and rbase == "Temporal" and plist[0] == ("Temporal", True)
                  and plist[2] == ("bool", False) and ("_at_" in name or "_minus_" in name)
                  and name.split("_")[0] in ("tpoint", "tgeo", "tcbuffer", "tnpoint", "tpose"))
    if len(plist) != 2 and not _restrict3:
        return None, f"arity {len(plist)} (not 2)"
    # ---- `_round`: f(operand, int maxdd) -> SAME type. The operand base picks
    #      the primary input and its same-type *_out return; maxdd is a fixed
    #      trailing call arg. float_round (a plain double) returns a scalar. ----
    if name.endswith("_round") and plist[1] == ("int", False):
        base, ptr = plist[0]
        if not ptr and base == "double":               # float_round(double, int)
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type="float_base", return_kind="double",
                         extra_args=[], extra_call_args=["2"],
                         comment_one_liner=f"{name} (double) — round to 2 decimals.")
            col_sql, va, vb = CONV_BASE_COL["float_base"]
            tmeta = dict(cols=[("value", col_sql)], rows=[("5.567",), ("8.123",)],
                         token=name.upper(), sink=sink_of("double"))
            return entry, tmeta
        if ptr and base in ROUND_RET:
            input_type, rkind = ROUND_RET[base]
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=input_type, return_kind=rkind,
                         extra_args=[], extra_call_args=["2"],
                         comment_one_liner=f"{name} ({ret.strip()}) — round {input_type} to 2 decimals.")
            if input_type == "tfloat":
                tmeta = dict(cols=[("value", "FLOAT64"), ("ts", "UINT64")],
                             rows=[("5.567", "1609459200"), ("8.123", "1609545600")],
                             token=name.upper(), sink="VARSIZED")
            else:
                l0, l1 = LITERALS[input_type]
                tmeta = dict(cols=[("a", "VARSIZED")], rows=[(l0,), (l1,)],
                             token=name.upper(), sink="VARSIZED")
            return entry, tmeta
        return None, f"round operand {base}{'*' if ptr else ''} unsupported"
    # ---- box out-param accessor: bool f(const Box*, T *result) -> scalar in
    #      *result (tbox_xmin/xmax/tmin/tmax + _inc, stbox_tmin/tmax + _inc) ----
    if (plist[0][1] and plist[0][0] in BOX_INPUT
            and plist[1][1] and plist[1][0] in OUT_PARAM_RET
            and ret.strip() == "bool"):
        oc, rkind = OUT_PARAM_RET[plist[1][0]]
        input_type = BOX_INPUT[plist[0][0]]
        lit0, lit1 = ACCESSOR_BOX_LIT.get(name, LITERALS[input_type])
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=input_type, return_kind=rkind,
                     extra_args=[], out_param=dict(cpp=oc),
                     comment_one_liner=f"{name} (out-param {plist[1][0]}) — {input_type} accessor.")
        tmeta = dict(cols=[("a", "VARSIZED")], rows=[(lit0,), (lit1,)],
                     token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
    # ---- box accessor with a fixed bool flag: f(box, bool) -> scalar
    #      (stbox_area/stbox_perimeter take a spheroid flag; planar = false) ----
    if plist[0][1] and plist[0][0] in BOX_INPUT and plist[1] == ("bool", False):
        input_type = BOX_INPUT[plist[0][0]]
        rkind = SCALAR_RET.get(ret.strip())
        if rkind is not None:
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=input_type, return_kind=rkind,
                         extra_args=[], extra_call_args=["false"],
                         comment_one_liner=f"{name} ({ret.strip()}) — {input_type} accessor (planar flag).")
            tmeta = dict(cols=[("a", "VARSIZED")],
                         rows=[(LITERALS[input_type][0],), (LITERALS[input_type][1],)],
                         token=name.upper(), sink=sink_of(rkind))
            return entry, tmeta
    # ---- temporal-instant ⊗ scalar -> Temporal* (per-event, WKT-serialized) ----
    tidx = [i for i, (b, p) in enumerate(plist) if b == "Temporal"]
    bidx = [i for i, (b, p) in enumerate(plist) if b in ("int", "double", "text", "bool")]
    if (ret.replace("*", "").strip() == "Temporal" and len(plist) == 2
            and len(tidx) == 1 and len(bidx) == 1):
        toks = name.split("_")
        insub = next((t for t in ("tint", "tfloat", "ttext", "tbool") if t in toks), None)
        if insub is None:
            return None, "no tint/tfloat/ttext/tbool subtype token"
        pref = toks[0]
        # result subtype: a temporal comparison yields a tbool; everything else
        # (arithmetic, minus, shift, tdistance, textcat) keeps the INPUT subtype —
        # MEOS tdistance sets restype = temp->temptype (tdistance_tint_int -> tint).
        serializer = "tbool_out" if pref in TCMP else insub + "_out"
        vsql, vcpp, va, vb = TEMPORAL_INPUTS[insub]
        bbase = plist[bidx[0]][0]
        ssql, scpp, sa, sb = TSCALAR_COL[bbase]
        # at_value restricts the temporal to where it EQUALS the arg; the generic
        # distinct-scalar default (chosen so minus_value stays non-empty) makes
        # at_value empty and thus unrecordable. Align the arg with the instant
        # value so the restriction is the (non-empty) instant itself.
        if name.endswith("_at_value"):
            sa, sb = va, vb
        extra = dict(kind="text") if bbase == "text" else dict(kind="scalar", cpp=scpp)
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=insub, return_kind=serializer,
                     extra_args=[extra],
                     comment_one_liner=f"{name} ({ret.strip()}) — temporal {insub} ⊗ {bbase} -> {serializer}.")
        if tidx[0] == 1:               # scalar-first: call = f(scalar, temp)
            entry["box_first"] = True
        tmeta = dict(cols=[("value", vsql), ("ts", "UINT64"), ("arg", ssql)],
                     rows=[(va, "1609459200", sa), (vb, "1609545600", sb)],
                     token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- temporal restriction (temporal_at_*/minus_*, tnumber_at_*/minus_*):
    #      a tint instant ⊗ a time/value box -> the restricted tint (tint_out) ----
    if ret.replace("*", "").strip() == "Temporal" and ("_at_" in name or "_minus_" in name):
        toks = name.split("_")
        bt = toks[-1]
        tix = [i for i, (b, p) in enumerate(plist) if b == "Temporal"]
        oix = [i for i, (b, p) in enumerate(plist) if b != "Temporal"]
        if bt in RESTRICT_BASE and len(tix) == 1 and len(oix) == 1 and tix[0] == 0:
            parser, ctype, at_lit, minus_lit = RESTRICT_BASE[bt]
            lit = minus_lit if "minus" in toks else at_lit
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type="tint", return_kind="tint_out",
                         extra_args=[dict(kind="box", box_type=ctype, parser=parser, header="meos.h")],
                         comment_one_liner=f"{name} ({ret.strip()}) — tint restricted by a {bt} box -> tint_out.")
            tmeta = dict(cols=[("value", "INT32"), ("ts", "UINT64"), ("arg", "VARSIZED")],
                         rows=[("5", "1609459200", lit), ("8", "1609545600", lit)],
                         token=name.upper(), sink="VARSIZED")
            return entry, tmeta
    # ---- spatial temporal restriction at/minus: a tgeo/tpoint/tcbuffer/tnpoint/
    #      tpose instant ⊗ {geo value, same-family object, STBox(+border bool)} ->
    #      the restricted temporal (tspatial_text). at-variants align the operand
    #      to the instant (non-empty); minus-variants use a far operand. ----
    rtoks = name.split("_")
    if (rbase == "Temporal" and len(plist) >= 2 and plist[0] == ("Temporal", True)
            and ("_at_" in name or "_minus_" in name)
            and rtoks[0] in ("tpoint", "tgeo", "tcbuffer", "tnpoint", "tpose")):
        is_minus = "minus" in rtoks
        tsub = "tgeo" if rtoks[0] in ("tpoint", "tgeo") else rtoks[0]
        in_type = ALWAYS_INPUT_TYPE.get(tsub, tsub)
        tcols = ALWAYS_TINPUT[tsub]
        secb, secp = plist[1]
        srid = "SRID=4326;" if tsub == "tgeo" else ""
        eca, extra_hdr = [], None
        if secb == "GSERIALIZED":
            if tsub == "tnpoint":
                return None, "tnpoint geo restriction needs network-SRID literal"
            extra = dict(kind="geom")
            a0, a1 = ((f"{srid}Point(40 40)", f"{srid}Point(50 50)") if is_minus
                      else (f"{srid}Point(1 1)", f"{srid}Point(2 2)"))
        elif secb in ("Cbuffer", "Npoint", "Pose"):
            lits = {"Cbuffer": ("Cbuffer(Point(1 1),1)", "Cbuffer(Point(2 2),0.5)",
                                "Cbuffer(Point(40 40),1)", "Cbuffer(Point(50 50),1)"),
                    "Npoint":  ("NPoint(1, 0.5)", "NPoint(1, 0.7)", "NPoint(9, 0.5)", "NPoint(9, 0.7)"),
                    "Pose":    ("Pose(Point(1 1), 0.5)", "Pose(Point(2 2), 1.0)",
                                "Pose(Point(40 40), 0.5)", "Pose(Point(50 50), 1.0)")}[secb]
            parser = {"Cbuffer": "cbuffer_in", "Npoint": "npoint_in", "Pose": "pose_in"}[secb]
            extra = dict(kind="box", box_type=secb, parser=parser, header=ALWAYS_BASE_HDR[secb])
            extra_hdr = ALWAYS_BASE_HDR[secb]
            a0, a1 = (lits[2], lits[3]) if is_minus else (lits[0], lits[1])
        elif secb == "STBox" and len(plist) == 3 and plist[2] == ("bool", False):
            if tsub == "tnpoint":
                return None, "tnpoint stbox restriction needs network-SRID box"
            extra = dict(kind="box", box_type="STBox", parser="stbox_in", header="meos_geo.h")
            far = f"{srid}STBOX X((40,40),(60,60))"
            near = f"{srid}STBOX X((0,0),(3,3))"
            a0, a1 = (far, far) if is_minus else (near, near)
            eca = ["true"]                              # border_inc
        else:
            return None, f"spatial restriction 2nd operand {secb} unsupported"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=in_type, return_kind="tspatial_text",
                     extra_args=[extra],
                     comment_one_liner=f"{name} ({ret.strip()}) — {tsub} restriction by {secb}.")
        if extra_hdr:
            entry["extra_headers"] = [extra_hdr]
        if eca:
            entry["extra_call_args"] = eca
        cols = [(c, s) for c, s, _, _ in tcols] + [("ts", "UINT64"), ("arg", "VARSIZED")]
        rowa = tuple([a for _, _, a, _ in tcols] + ["1609459200", a0])
        rowb = tuple([b for _, _, _, b in tcols] + ["1609545600", a1])
        tmeta = dict(cols=cols, rows=[rowa, rowb], token=name.upper(), sink="VARSIZED")
        return entry, tmeta
    # ---- nearest-approach / shortest-line: a spatial temporal (tgeo/tcbuffer/
    #      tnpoint/tpose) ⊗ {geo, object, second temporal} -> nad (double),
    #      nai (the nearest-approach instant via tspatial_as_text) or shortestline
    #      (the connecting line via geo_out). temp is always operand 1. ----
    toks = name.split("_")
    if (toks[0] in ("nai", "shortestline", "nad") and len(plist) == 2
            and plist[0] == ("Temporal", True)):
        tsub = next((t for t in ("tgeo", "tcbuffer", "tnpoint", "tpose") if t in toks), None)
        if tsub is None:
            return None, f"{toks[0]} temporal subtype unsupported"
        if rbase == "double":
            rkind, sink = "double", "FLOAT64"
        elif toks[0] == "nai":
            rkind, sink = "tspatial_text", "VARSIZED"
        elif toks[0] == "shortestline":
            rkind, sink = "geo_value_out", "VARSIZED"
        else:
            return None, f"{toks[0]} return {rbase} unmapped"
        in_type = ALWAYS_INPUT_TYPE.get(tsub, tsub)
        tcols = ALWAYS_TINPUT[tsub]
        secb, secp = plist[1]
        if secb in ALWAYS_BASE:                         # geo / object literal extra
            parser, bsql, ba, bb = ALWAYS_BASE[secb]
            if secb == "GSERIALIZED" and tsub != "tgeo":   # non-tgeo families carry no SRID
                ba, bb = "Point(1 1)", "Point(2 2)"
            if parser == "geom":
                extra = dict(kind="geom")
            elif parser == "text":
                extra = dict(kind="text")
            else:
                extra = dict(kind="box", box_type=secb, parser=parser, header=ALWAYS_BASE_HDR[secb])
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=in_type, return_kind=rkind,
                         extra_args=[extra],
                         comment_one_liner=f"{name} ({ret.strip()}) — {tsub} nearest-approach ⊗ {secb}.")
            if parser not in ("geom", "text"):
                entry["extra_headers"] = [ALWAYS_BASE_HDR[secb]]
            cols = [(c, s) for c, s, _, _ in tcols] + [("ts", "UINT64"), ("arg", bsql)]
            rowa = tuple([a for _, _, a, _ in tcols] + ["1609459200", ba])
            rowb = tuple([b for _, _, _, b in tcols] + ["1609545600", bb])
            tmeta = dict(cols=cols, rows=[rowa, rowb], token=name.upper(), sink=sink)
            return entry, tmeta
        if (secb, secp) == ("Temporal", True):          # second temporal via temporal2
            if in_type not in TWO_TEMPORAL:
                return None, f"{toks[0]} two-temporal {in_type} unsupported"
            spec = TWO_TEMPORAL[in_type]
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=spec["input_type"], return_kind=rkind,
                         extra_args=[dict(kind="temporal2", t2_fields=spec["t2_fields"],
                                          t2_build=spec["t2_build"], header=spec["header"])],
                         comment_one_liner=f"{name} ({ret.strip()}) — two {in_type} instants nearest-approach.")
            tmeta = dict(cols=spec["make_cols"], rows=spec["rows"], token=name.upper(), sink=sink)
            return entry, tmeta
        return None, f"{toks[0]} second operand {secb} unsupported"
    # ---- scalar distance / nad between an object/box/geo pair -> double:
    #      distance_cbuffer/pose_geo/stbox, nad_cbuffer_stbox, nad_stbox_geo.
    #      operand 1 = primary (object/box literal), operand 2 = geom/box extra. ----
    if toks[0] in ("distance", "nad") and len(plist) == 2 and rbase == "double":
        PRIM = {"Cbuffer": "cbuffer", "Pose": "pose", "Npoint": "npoint",
                "STBox": "stbox_text"}
        (b0, p0), (b1, p1) = plist
        if b0 not in PRIM:
            return None, f"{toks[0]} primary {b0} unsupported"
        in_type = PRIM[b0]
        if b1 == "GSERIALIZED":                         # planar object/box -> SRID 0 geo
            extra, arg0, arg1 = dict(kind="geom"), "Point(1 1)", "Point(2 2)"
        elif b1 == "STBox":
            extra = dict(kind="box", box_type="STBox", parser="stbox_in", header="meos_geo.h")
            arg0, arg1 = "STBOX X((0,0),(3,3))", "STBOX X((1,1),(4,4))"
        else:
            return None, f"{toks[0]} 2nd operand {b1} unsupported"
        l0, l1 = LITERALS[in_type]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=in_type, return_kind="double",
                     extra_args=[extra],
                     comment_one_liner=f"{name} (double) — {b0} ⊗ {b1} distance.")
        tmeta = dict(cols=[("a", "VARSIZED"), ("arg", "VARSIZED")],
                     rows=[(l0, arg0), (l1, arg1)], token=name.upper(), sink="FLOAT64")
        return entry, tmeta
    # ---- two-temporal: both operands built from instant columns -> tbool / ttext ----
    if (ret.replace("*", "").strip() == "Temporal" and len(plist) == 2
            and all(b == "Temporal" for b, p in plist)):
        toks = name.split("_")
        sub = ("tgeompoint" if "tgeo" in toks else "tcbuffer" if "tcbuffer" in toks
               else "ttext" if "ttext" in toks else "tint")
        if sub in TWO_TEMPORAL:
            pref = toks[0]
            if pref in TBOOL_PREFIX:
                rkind = "tbool_out"
            elif pref == "textcat":
                rkind = "ttext_out"
            else:
                return None, f"two-temporal {pref} not a clean tbool/ttext family"
            spec = TWO_TEMPORAL[sub]
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=spec["input_type"], return_kind=rkind,
                         extra_args=[dict(kind="temporal2", t2_fields=spec["t2_fields"],
                                          t2_build=spec["t2_build"], header=spec["header"])],
                         comment_one_liner=f"{name} ({ret.strip()}) — two {sub} instants -> {rkind}.")
            tmeta = dict(cols=spec["make_cols"], rows=spec["rows"],
                         token=name.upper(), sink="VARSIZED")
            return entry, tmeta
    # ---- multi-column temporal-instant ⊗ base value: reductions (always/ever,
    #      a*/e* spatial rels) -> int; temporal comparison / spatial relation ->
    #      tbool; tdistance -> tfloat. base = bool / text / npoint / pose / cbuffer
    #      / geo literal. (tgeo restriction returning a tgeo is deferred — geo WKT
    #      output carries '(' and is unrecordable.) ----
    rb = ret.replace("*", "").strip()
    if rb in ("bool", "int", "Temporal"):
        toks = name.split("_")
        tsub = next((t for t in ALWAYS_TINPUT if t in toks), None)
        tix = [i for i, (b, p) in enumerate(plist) if b == "Temporal"]
        oix = [i for i, (b, p) in enumerate(plist) if b != "Temporal"]
        if tsub and len(tix) == 1 and len(oix) == 1 and plist[oix[0]][0] in ALWAYS_BASE:
            pref = toks[0]
            if rb in ("bool", "int"):
                rkind = ret_kind(ret, None)
            elif pref in TBOOL_PREFIX:
                rkind = "tbool_out"
            elif pref == "tdistance":
                rkind = "tfloat_out"
            else:
                return None, f"temporal-return {pref} not a clean tbool/tfloat family"
            bbase = plist[oix[0]][0]
            parser, bsql, ba, bb = ALWAYS_BASE[bbase]
            # geometry base SRID must match the temporal: tgeo (tgeompoint) is
            # built SRID=4326, but tcbuffer/tpose/tnpoint instants carry no SRID,
            # so use a planar (SRID 0) point literal for those to avoid a
            # mixed-SRID guard.
            if bbase == "GSERIALIZED" and tsub != "tgeo":
                ba, bb = "Point(1 1)", "Point(2 2)"
            if parser == "scalar":
                extra = dict(kind="scalar", cpp="bool")
            elif parser == "text":
                extra = dict(kind="text")
            elif parser == "geom":
                extra = dict(kind="geom")
            else:
                extra = dict(kind="box", box_type=bbase, parser=parser,
                             header=ALWAYS_BASE_HDR[bbase])
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=ALWAYS_INPUT_TYPE.get(tsub, tsub),
                         return_kind=rkind, extra_args=[extra],
                         comment_one_liner=f"{name} ({ret.strip()}) — temporal {tsub} ⊗ {bbase} -> {rkind}.")
            if parser not in ("scalar", "text", "geom"):
                entry["extra_headers"] = [ALWAYS_BASE_HDR[bbase]]
            if oix[0] == 0:                 # base-first: call = f(base, temp)
                entry["box_first"] = True
            tcols = ALWAYS_TINPUT[tsub]
            cols = [(c, s) for c, s, _, _ in tcols] + [("ts", "UINT64"), ("arg", bsql)]
            rowa = tuple([a for _, _, a, _ in tcols] + ["1609459200", ba])
            rowb = tuple([b for _, _, _, b in tcols] + ["1609545600", bb])
            tmeta = dict(cols=cols, rows=[rowa, rowb], token=name.upper(), sink=sink_of(ret_kind(ret, None)))
            return entry, tmeta
    # ---- object/geo/text set ± value: f(Set, value) | f(value, Set) where the
    #      value is a geo/cbuffer/npoint/pose/text. Predicates (contains/contained/
    #      left/right/overleft/overright) -> bool; union/intersection/minus -> a new
    #      object-set (serialized via *set_out / spatialset_as_text). The value
    #      reuses the geom/text extra kinds or an object `box`-kind parse+free. ----
    toks = name.split("_")
    if (len(plist) == 2 and len(toks) == 3 and toks[0] in SETVAL_OPS
            and ("set" in (toks[1], toks[2]))):
        set_first = toks[1] == "set"
        vtok = toks[2] if set_first else toks[1]
        if vtok in SETVAL and ((set_first and plist[0] == ("Set", True))
                               or (not set_first and plist[1] == ("Set", True))):
            set_in, vextra, set_ret = SETVAL[vtok]
            if rbase == "bool":
                rkind = "int"
            elif rbase == "Set":
                rkind = set_ret
            else:
                return None, f"set±value return {rbase} unmapped"
            # value-first minus ({value} \ set) is empty when value ∈ set, so use an
            # out-of-set value there; everything else wants value ∈ set (contains
            # true, intersection / set-first minus non-empty).
            vlits = SETVAL_LIT_OUT if (toks[0] == "minus" and not set_first) else SETVAL_LIT_IN
            v0, v1 = vlits[vtok]
            entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                         build_generic=True, input_type=set_in, return_kind=rkind,
                         extra_args=[dict(vextra)],
                         comment_one_liner=f"{name} ({ret.strip()}) — object-set ± {vtok} value.")
            if not set_first:        # Set is operand 2 -> swap so call = f(value, set)
                entry["box_first"] = True
            sl0, sl1 = LITERALS[set_in]
            tmeta = dict(cols=[("a", "VARSIZED"), ("arg", "VARSIZED")],
                         rows=[(sl0, v0), (sl1, v1)], token=name.upper(), sink=sink_of(rkind))
            return entry, tmeta
    # ---- tnumber/temporal ⊗ numspan/tstzspan bbox predicate -> bool. One operand
    #      is a Temporal (tnumber/temporal, built as a tfloat instant — its value
    #      bbox vs a numspan, or its time bbox vs a tstzspan), the other a span
    #      literal parsed as a box-kind extra. box_first when the span comes first. ----
    if (len(plist) == 2 and rbase == "bool" and len(toks) == 3
            and toks[0] in ("contains", "contained", "left", "right", "overleft",
                            "overright", "overlaps", "same", "adjacent")
            and ({"numspan", "tstzspan"} & set(toks[1:]))
            and ({"tnumber", "temporal"} & set(toks[1:]))):
        span_tok = "numspan" if "numspan" in toks[1:] else "tstzspan"
        if span_tok == "numspan":
            span_parser, slit = "floatspan_in", LITERALS["floatspan"]
        else:
            span_parser, slit = "tstzspan_in", LITERALS["tstzspan"]
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type="tfloat", return_kind="int",
                     extra_args=[dict(kind="box", box_type="Span", parser=span_parser, header="meos.h")],
                     comment_one_liner=f"{name} (bool) — temporal ⊗ {span_tok} bbox predicate.")
        if toks[1] == span_tok:        # span is operand 1 -> swap so call = f(span, temp)
            entry["box_first"] = True
        tmeta = dict(cols=[("value", "FLOAT64"), ("ts", "UINT64"), ("arg", "VARSIZED")],
                     rows=[("5.5", "1609459200", slit[0]), ("8.5", "1609545600", slit[1])],
                     token=name.upper(), sink="INT32")
        return entry, tmeta
    # ---- pure-geometry binary ops: a geom primary ⊗ {geom, int/double scalar} ->
    #      bool/int predicate, double measure, or a geometry via geo_out. Excludes
    #      geography (geog_, needs a geography input), array-input ops, and
    #      out-param/temporal second operands. ----
    if (toks[0] in ("geom", "geo") and len(plist) == 2 and plist[0] == ("GSERIALIZED", True)
            and "array" not in name):
        b1, p1 = plist[1]
        if b1 == "GSERIALIZED" and p1:
            extra, ecol, a0, a1 = dict(kind="geom"), "VARSIZED", "SRID=4326;Linestring(0 2, 2 0)", "SRID=4326;Linestring(1 0, 1 3)"
        elif b1 in ("int", "int32") and not p1:
            extra, ecol = dict(kind="scalar", cpp="int32_t"), "INT32"
            iv = "1" if name.endswith("_n") else "3857" if "transform" in name else "4326"
            a0, a1 = iv, iv
        elif b1 == "double" and not p1:
            extra, ecol, a0, a1 = dict(kind="scalar", cpp="double"), "FLOAT64", "0.0", "0.0"
        else:
            return None, f"geom 2nd operand {b1}{'*' if p1 else ''} unsupported"
        if rbase in ("bool", "int"):
            rkind, sink = "int", "INT32"
        elif rbase == "double":
            rkind, sink = "double", "FLOAT64"
        elif rbase == "GSERIALIZED":
            rkind, sink = "geo_value_out", "VARSIZED"
        else:
            return None, f"geom return {rbase} unmapped"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type="geom", return_kind=rkind,
                     extra_args=[extra],
                     comment_one_liner=f"{name} ({ret.strip()}) — geometry binary op.")
        tmeta = dict(cols=[("a", "VARSIZED"), ("arg", ecol)],
                     rows=[("SRID=4326;Linestring(0 0, 2 2)", a0), ("SRID=4326;Linestring(0 0, 3 3)", a1)],
                     token=name.upper(), sink=sink)
        return entry, tmeta
    # ---- same-type object-scalar comparison / relation (cbuffer/pose/npoint/
    # nsegment cmp/eq/.../same + cbuffer spatial rels): two literals -> bool/int ----
    (b0, p0), (b1, p1) = plist
    if p0 and p1 and b0 == b1 and b0 in OBJECT_TYPES:
        key, parser, header = OBJECT_TYPES[b0]
        rkind = ret_kind(ret, None)
        if rkind is None:
            return None, f"obj-cmp return {ret} unmapped"
        entry = dict(nebula_name=camel(name), sql_token=name.upper(), meos_call=name,
                     build_generic=True, input_type=key, return_kind=rkind,
                     extra_args=[dict(kind="box", box_type=b0, parser=parser, header=header)],
                     extra_headers=[header],
                     comment_one_liner=f"{name} ({ret.strip()}) — {b0} scalar comparison/relation.")
        l0, l1 = LITERALS[key]
        tmeta = dict(cols=[("a", "VARSIZED"), ("b", "VARSIZED")],
                     rows=[(l0, l1), (l1, l0)], token=name.upper(), sink=sink_of(rkind))
        return entry, tmeta
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
        # Span/Set/SpanSet over two literals. Subtype-SPECIFIC symbols name their
        # operand types as the last two underscore tokens (distance_datespanset_
        # datespan -> datespanset, datespan); TYPE-GENERIC symbols (span_cmp,
        # contains_span_spanset) don't, so fall back to the int subtype.
        KEY_CTYPE = {**BOX_CTYPE, "datespan": "Span", "tstzspan": "Span",
                     "dateset": "Set", "tstzset": "Set",
                     "datespanset": "SpanSet", "tstzspanset": "SpanSet"}
        toks = name.split("_")
        k0, k1 = (toks[-2], toks[-1]) if len(toks) >= 3 else (None, None)
        if (k0 in KEY_CTYPE and k1 in KEY_CTYPE
                and KEY_CTYPE[k0] == c0 and KEY_CTYPE[k1] == c1):
            prim, sec = k0, k1
            rkind = ret_kind(ret, None)
        else:
            INTKEY = {"Span": "intspan", "Set": "intset", "SpanSet": "intspanset"}
            if c0 not in INTKEY or c1 not in INTKEY:
                return None, f"unhandled box-box {c0}/{c1}"
            prim, sec = INTKEY[c0], INTKEY[c1]
            rkind = ret_kind(ret, "int")
        if rkind is None:
            return None, f"box-box return {ret} unmapped"
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
    if ret == "int64":
        return "int64"
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
