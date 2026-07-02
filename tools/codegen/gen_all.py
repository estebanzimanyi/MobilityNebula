#!/usr/bin/env python3
"""gen_all.py — regenerate the full per-event MobilityNebula surface from all descriptors in one run."""
import sys
import json
import argparse
from pathlib import Path

# Add nebula-generator dir to path so we can import codegen_nebula
sys.path.insert(0, str(Path(__file__).parent))
from codegen_nebula import emit_operator, inject_cmake_entries, inject_g4, inject_parser_cpp

DESCRIPTORS = [
    # ordered: smallest first, composed last
    ("tfloat-transforms", "tfloat-transforms-descriptor.json"),
    ("numeric", "numeric-descriptor.json"),
    ("spatial-predicates", "spatial-predicates-descriptor.json"),
    ("spatial-phasec-full", "spatial-phasec-full-descriptor.json"),
    ("phasec-rest", "phasec-rest-descriptor.json"),
    ("trgeo", "trgeo-descriptor.json"),
    ("phasec-composed", "phasec-composed-descriptor.json"),
    ("missing-ops", "missing-ops-descriptor.json"),
]


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate the full per-event MobilityNebula surface from all descriptors"
    )
    parser.add_argument("--output-root", required=True, help="MobilityNebula repo root")
    args = parser.parse_args()

    output_root = Path(args.output_root).resolve()
    if not (output_root / "nes-logical-operators").exists():
        sys.exit(f"ERROR: {output_root} does not look like a MobilityNebula root (missing nes-logical-operators/)")

    script_dir = Path(__file__).parent
    grand_total = 0
    all_operators = []

    for family_name, desc_file in DESCRIPTORS:
        desc_path = script_dir / desc_file
        if not desc_path.exists():
            sys.exit(f"ERROR: descriptor not found: {desc_path}")
        with open(desc_path) as f:
            config = json.load(f)
        operators = config["operators"]
        sys.stderr.write(f"  {family_name}: {len(operators)} operators\n")
        for op in operators:
            emit_operator(op, output_root)
        all_operators.extend(operators)
        grand_total += len(operators)

    sys.stderr.write(f"\nCMakeLists.txt injection:\n")
    inject_cmake_entries(all_operators, output_root)

    sys.stderr.write(f"\nParser glue injection:\n")
    inject_g4(all_operators, output_root / "nes-sql-parser/AntlrSQL.g4")
    inject_parser_cpp(all_operators, output_root / "nes-sql-parser/src/AntlrSQLQueryPlanCreator.cpp")

    sys.stderr.write(f"\nDone. {grand_total} operators regenerated.\n")


if __name__ == "__main__":
    main()
