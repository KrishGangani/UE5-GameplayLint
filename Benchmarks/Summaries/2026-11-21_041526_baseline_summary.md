# Scan batch 2026-11-21_041526_baseline

Corpus: 1000 assets, seed 1, ground truth sha256 `11fdc0bea9a7fe60`. CPU affinity: 0xFFFF (performance cores only; hybrid CPU).

Medians across measured runs, [min-max]. The priming run is excluded.

| Config | Runs | Validation phase (s) | Process wall (s) | Commandlet (s) | Checked | Invalid | Valid with warnings | GL findings |
|---|---|---|---|---|---|---|---|---|
| S | 4 | n/a | 9.33 [9.20-10.05] | 1.21 [1.16-1.25] | 1011 | 7 | 28 | n/a |
| F0 | 4 | 0.97 [0.95-0.98] | 9.29 [9.10-9.34] | 1.16 [1.14-1.17] | 1011 | 7 | 28 | 0 |

Runs not ok (unexpected exit code, or a permanent corpus tag reported invalid): none.
Detection and false positives: `python -I Tools/score.py --batch 2026-11-21_041526_baseline`.
