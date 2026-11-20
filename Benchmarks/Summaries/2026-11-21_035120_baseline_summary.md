# Scan batch 2026-11-21_035120_baseline

> **INVALID batch (kept as a record, not a baseline).** The seeder wrote the corpus's tag files with `+GameplayTagList=` lines.
> A loose `Config/Tags/*.ini` file is read without ini symbol commands, so the engine loaded no corpus tag, and every tag
> in every asset was unknown (one stock run's log has 23,250 lines that call a permanent corpus tag invalid). Fixed in the seeder and re-run;
> see `Explanation/09_Testing_And_Benchmarks.md`, S1-S4.

Corpus: 1000 assets, seed 1, ground truth sha256 `11fdc0bea9a7fe60`. CPU affinity: 0xFFFF (performance cores only; hybrid CPU).

Medians across measured runs, [min-max]. The priming run is excluded.

| Config | Runs | Validation phase (s) | Process wall (s) | Commandlet (s) | Checked | Invalid | Warnings | GL findings |
|---|---|---|---|---|---|---|---|---|
| S | 4 | n/a | 9.55 [9.53-10.07] | 1.42 [1.28-1.51] | 1011 | 7 | 832 | n/a |
| F0 | 4 | 1.15 [1.14-1.16] | 9.65 [9.52-9.82] | 1.38 [1.38-1.42] | 1011 | 7 | 842 | 0 |

Runs with an unexpected exit code: none.
Detection and false positives: `python -I Tools/score.py --batch 2026-11-21_035120_baseline`.
