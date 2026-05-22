#!/usr/bin/env python3
"""NebulaStream adapter for the streaming-parity harness.

Introspects an ACCUMULATED-PR Nebula build to emit a coverage feed
(`function<TAB>{proven|wired}`) consumed by streaming_parity.py.

  L2 WIRED  — every distinct MEOS function called by a generated/hand-written
              operator (the `meos_call` inside each *PhysicalFunction.cpp).
  L3 PROVEN — a wired function whose operator is exercised by a systest that
              PASSED. Mapping: systest SQL token (TEMPORAL_…) -> operator NAME
              (PascalCase of the token) -> its meos_call.

Pass the list of green systests (one basename per line, no extension) via
--passing; produce it by running the `systest` binary over
nes-systests/function/meos/ on the accumulated build. Functions called only by
operators with no passing systest come out as `wired` — the honest backlog.
"""
from __future__ import annotations
import argparse
import os
import re
import sys

CALL_RE = re.compile(r"\b([a-z][a-z0-9_]*)\s*\(", )
# MEOS spatial/temporal call shapes we treat as the operator's backing function.
MEOS_CALL_HINT = re.compile(r"^(e|a|t)?[a-z]+_(tgeo|tnpoint|tpose|tcbuffer|tgeompoint)_"
                            r"|^nad_|^tnumber_|^tfloat_|^tint_|^tpoint_|^temporal_|^tgeo_")


def operator_calls(root):
    """Map operator NAME -> set(meos_call). NAME taken from the file stem
    (…PhysicalFunction.cpp -> the operator name)."""
    name2calls = {}
    dirs = ["nes-physical-operators/src/Functions/Meos",
            "nes-physical-operators/src/Aggregation/Function/Meos"]
    for d in dirs:
        full = os.path.join(root, d)
        if not os.path.isdir(full):
            continue
        for f in os.listdir(full):
            if not f.endswith("PhysicalFunction.cpp"):
                continue
            name = f[:-len("PhysicalFunction.cpp")]
            txt = open(os.path.join(full, f)).read()
            # find the MEOS backing call: the call assigned to result/returned
            calls = set()
            for m in re.finditer(r"\b([a-z][a-z0-9_]+)\(", txt):
                fn = m.group(1)
                if MEOS_CALL_HINT.match(fn) and fn not in (
                        "tpose_in", "tnpoint_in", "tfloat_in", "tgeompoint_in",
                        "tpose_to_tpoint", "tnpoint_to_tgeompoint"):
                    calls.add(fn)
            if calls:
                name2calls[name] = calls
    return name2calls


def systest_token(path):
    for line in open(path):
        m = re.search(r"\b(TEMPORAL_[A-Z0-9_]+|[A-Z][A-Z0-9_]*_TGEO_GEO|TGEO_AT_STBOX)\s*\(", line)
        if m:
            return m.group(1)
    return None


def build_token2name(root):
    """Authoritative token -> LogicalFunction operator NAME, parsed from the
    parser dispatch (case AntlrSQLLexer::<TOKEN>: … <Name>LogicalFunction(…))."""
    t2n = {}
    parser = os.path.join(root, "nes-sql-parser/src/AntlrSQLQueryPlanCreator.cpp")
    if not os.path.exists(parser):
        return t2n
    cur = None
    for line in open(parser):
        mc = re.search(r"case AntlrSQLLexer::([A-Z0-9_]+):", line)
        if mc:
            cur = mc.group(1)
        m = re.search(r"\b([A-Z][A-Za-z0-9]+)LogicalFunction\(", line)
        if m and cur:
            t2n[cur] = m.group(1)
            cur = None
    return t2n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".", help="Nebula repo root (accumulated build checkout)")
    ap.add_argument("--systests", default="nes-systests/function/meos")
    ap.add_argument("--passing", help="file: one passing-systest basename per line")
    a = ap.parse_args()

    name2calls = operator_calls(a.root)
    token2name = build_token2name(a.root)
    wired = set().union(*name2calls.values()) if name2calls else set()

    passing = set()
    if a.passing and os.path.exists(a.passing):
        passing = {ln.strip() for ln in open(a.passing) if ln.strip()}

    proven = set()
    sysdir = os.path.join(a.root, a.systests)
    if os.path.isdir(sysdir):
        for f in os.listdir(sysdir):
            if not f.endswith(".test"):
                continue
            base = f[:-len(".test")]
            if passing and base not in passing:
                continue
            tok = systest_token(os.path.join(sysdir, f))
            if not tok:
                continue
            name = token2name.get(tok)              # authoritative token -> operator
            if name and name in name2calls:
                proven |= name2calls[name]

    for fn in sorted(wired):
        print(f"{fn}\t{'proven' if fn in proven else 'wired'}")
    sys.stderr.write(f"[nebula adapter] operators={len(name2calls)} wired-calls={len(wired)} "
                     f"proven-calls={len(proven)} passing-systests={len(passing) or 'all'}\n")


if __name__ == "__main__":
    main()
