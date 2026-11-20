# Phase 2 raw notes

These are the working notes behind [01b_Research_Findings.md](../01b_Research_Findings.md). They are kept so every statement in the findings can be traced back to the exact engine line or web page it came from.

| File | Scope |
|---|---|
| [A_data_validation_internals.md](A_data_validation_internals.md) | `UEditorValidatorBase`, how validators are discovered and run (save, menus, pre-submit, commandlet, script), changelist validation, the commandlet's arguments and exit codes, fixers, logging |
| [B_gameplay_data_mechanisms.md](B_gameplay_data_mechanisms.md) | Gameplay tags (load warnings, config sources, searchable names), DataTable/CurveTable row handles, curves, walking an asset's data, tick settings as data, the asset registry calls, creating assets from code |
| [C_existing_work.md](C_existing_work.md) | The 13 validators shipped with 5.8.4, Asset Referencing Restrictions, Epic's docs, Lyra, Linter v2, CommonValidators and other community tools, talks and postings |
| [D_measurement_and_build.md](D_measurement_and_build.md) | What is measured and where, per-validator timing, CPU pinning, the timing protocol, build facts, command-line switches |

**The findings file is authoritative.** These notes are the audit trail.
