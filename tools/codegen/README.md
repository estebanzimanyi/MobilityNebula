# MobilityNebula MEOS per-event operator codegen

This directory contains the IDL-driven code generator for the canonical
615-op per-event MEOS surface in MobilityNebula.

## What it is

The generator reads JSON descriptor files that describe each MEOS operator
(name, SQL token, MEOS C function, argument types, return type) and emits
the four C++ files that NES requires per operator:

- `nes-logical-operators/include/Functions/Meos/<Name>LogicalFunction.hpp`
- `nes-logical-operators/src/Functions/Meos/<Name>LogicalFunction.cpp`
- `nes-physical-operators/include/Functions/Meos/<Name>PhysicalFunction.hpp`
- `nes-physical-operators/src/Functions/Meos/<Name>PhysicalFunction.cpp`

In addition to the per-operator files, the generator injects the
CMakeLists.txt target lists, the AntlrSQL.g4 token rules, and the
AntlrSQLQueryPlanCreator.cpp dispatch table so the full NES build stays
consistent with the committed surface.

## Pipeline

```
MobilityDB master
    └─► MEOS-API run.py <headers>          # libclang extracts the C API
        └─► meos-idl-master.json           # IDL: all public MEOS functions
            └─► build_descriptor.py        # author a JSON descriptor family
                └─► *-descriptor.json      # per-family operator list
                    └─► gen_all.py         # orchestrates all 9 families
                        └─► 4 C++ files per op + CMake/grammar/QPC glue
```

`meos-idl-master.json` committed here is a snapshot of the IDL generated
from the current MobilityDB master.  Regenerate it via MEOS-API when the
upstream C surface changes.

## Descriptor families (9 total, 615 operators)

| Descriptor file                         | Family                                      |
|-----------------------------------------|---------------------------------------------|
| `tfloat-transforms-descriptor.json`     | tfloat trig / abs / shift-scale             |
| `numeric-descriptor.json`               | tint/tfloat/tbigint arithmetic, comparisons |
| `spatial-predicates-descriptor.json`    | tgeo/tcbuffer/tnpoint/tpose spatial preds   |
| `spatial-phasec-full-descriptor.json`   | full phase-C tgeo/tcbuffer/trgeometry preds |
| `phasec-rest-descriptor.json`           | remaining phase-C spatial ops               |
| `trgeo-descriptor.json`                 | trgeometry spatial predicates               |
| `phasec-composed-descriptor.json`       | composed / multi-step spatial ops           |
| `missing-ops-descriptor.json`           | 69 canonical ops not covered above          |
| `canonical-new-ops-descriptor.json`     | 85 gap-fill ops closing the TARGET_SURFACE  |

Intentionally excluded: bbox/topological operators (stbox/stbox,
stbox/tspatial, tbox/tnumber families) and non-canonical per-type numeric
two-temporal comparisons are out of scope for the 615-op canonical surface.

## How to regenerate after a MobilityDB API change

```sh
python tools/codegen/gen_all.py --output-root <repo-root>
```

`<repo-root>` must be the MobilityNebula repository root (the directory
that contains `nes-logical-operators/`).  The script writes all 2 460 C++
files and updates CMakeLists.txt, AntlrSQL.g4, and
AntlrSQLQueryPlanCreator.cpp in place.  Commit the result.

## Files in this directory

| File                          | Purpose                                           |
|-------------------------------|---------------------------------------------------|
| `codegen_nebula.py`           | Core emitter: templates for all 4 C++ file kinds  |
| `gen_all.py`                  | Orchestrator: runs all 9 descriptor families      |
| `codegen_aggregations.py`     | Supplementary generator for aggregation operators |
| `build_descriptor.py`         | Helper: build a new descriptor from IDL entries   |
| `codegen_input.example.json`  | Annotated example descriptor for new families     |
| `meos-idl-master.json`        | Committed IDL snapshot from MobilityDB master     |
| `build_local.sh`              | Local convenience wrapper for gen_all.py          |
| `*-descriptor.json`           | The 9 operator family descriptors                 |

## Drift guard

`.github/workflows/codegen-drift-guard.yml` (added in the follow-up PR)
runs on every pull request.  It executes `gen_all.py --output-root .` and
then asserts `git diff --quiet`.  Any divergence between the committed
operator files and the generator output fails the check and prints the
diff stat.  To fix a drift failure, run the regeneration command above and
commit the updated files.
