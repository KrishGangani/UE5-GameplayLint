# 12 · Future Scalability: GameplayLint

What v1 does not do, and how it would grow. The growth paths decided in Phase 3 are in [02_System_Architecture.md](02_System_Architecture.md) §11.

## Measurement that was not run
- **The Phase 8 matrix:** F0–F3 interleaved at N = 500 / 1,000 / 5,000, and I0–I3 after M1–M4 at N = 5,000. The scripts exist (`run_scan.py`, `score.py --incremental`), and so does the cost estimate (`03`: about 9 s per scan process at N = 1,000).
- **SC4 risk:** one indicative F1 run was about 1.33 × F0 in the validation phase. The first optimisation to try is a per-struct cache in the walker that skips structs whose property tree cannot contain the asked-for type (`11`).

## Dropped steps (owner's direction, 2026-11-21)
- **S9, a fixer for `GL.UB.ClientTickGroup`:** set the template's tick group to the scheduler's, using the engine's fixer helpers. It applies only from the Message Log UI.
- **S10, cook reachability:** UE5-UpdateBudget's "an asset referenced only from C++ isn't cooked". The design:
  1. collect soft and hard object paths in the class defaults of project modules;
  2. subtract what the cook reaches: maps, primary assets, `DirectoriesToAlwaysCook`;
  3. report the rest.
  Its ground truth is a packaged build's cooked file list.

## Coverage gaps (expected misses in v1)
- Data placed in maps: a level's actors are not walked.
- A project's own reference types: a table plus a row name in two plain fields.
- Blueprint graph pin literals, and values in custom serialisation.
- The on-save half of H3: validating already-loaded assets.

## Growth
- **One rule pack per portfolio project**, from its documented mistakes. A pack needs no core change (E1).
- **Suppressions with reasons and expiry**, CI trend reports, and a real-content false-positive rate measured on the other portfolio projects.
