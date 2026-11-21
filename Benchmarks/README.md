# P10 · Benchmarks

Every number in this project's write-up must trace back to a file in this folder. The method is in [../Explanation/01_Problem_Analysis.md §7–8](../Explanation/01_Problem_Analysis.md). The results, batch by batch, are in [../Explanation/09_Testing_And_Benchmarks.md](../Explanation/09_Testing_And_Benchmarks.md).

## Measurement rules

1. **Baseline first.** The stock engine run (stock commandlet and F0) is measured on the seeded corpus before any rule exists (step S3).
2. **Editor commandlet runs** (`UnrealEditor-Cmd.exe`, Development editor, `-unattended -nullrhi -nosplash -traceautostart=0 -stdout`). This is editor tooling, so there is no packaged or fixed-step run for headline numbers; the packaged game only proves that no lint code reaches a runtime build.
3. **Pinned to the performance cores.** The runner pins **itself** first with `pin_self` (mask from `Tools\core_mask.py`: `FFFF` = logical CPUs 0–15), before it records the environment or starts anything; every editor it starts inherits the mask from its first instruction. It starts `UnrealEditor-Cmd.exe` with `subprocess.Popen`, waits with a timeout and kills the tree in a `finally` block. The mask read back with `applied_mask()` is recorded in the environment.
4. **One discarded priming run per batch**, then **interleaved runs** with the order rotated every round. **Runs per combination = the smallest multiple of the number of configurations that is at least 4** (as in UE5-UpdateBudget), so each configuration sits in each position equally often and no median rests on one or two runs.
5. **Statistics:** times are one number per run (validation phase and process wall time), reported as the median across runs with the [min–max] range; per-asset rule times as median and p99 (linear interpolation). An overhead or improvement is claimed only when the ranges support it.
6. **Correctness numbers** (detection, false positives) come from the same runs and must be identical across runs of one configuration (criterion E2).
7. **The environment is recorded with every batch** (below), automatically, by `Tools\run_scan.py` (from S3).
8. **Every Unreal job goes through the UE job queue**, including the seeding commandlet that builds the corpus.

## Folders

| Folder | What goes in it | Rule |
|---|---|---|
| `Raw\` | Per-run JSON reports, logs, and one `_manifest.json` per batch (exact command lines, environment, corpus seed and hash, per-run results) | **Never edited.** Only the manifests are in git. Cited raw batches are attached as `<batch>_raw.zip` to this repository's **Releases** page |
| `Summaries\` | Scores (recall, false positives per rule, kind and variant), time tables, charts | Each summary names the batch it was computed from |

## File naming

`<batch>_<Config>[_<Mutation>]_N<assets>_r<run>.<ext>`, with `<batch>` = `YYYY-MM-DD_HHMMSS[_label]`. The runner refuses to overwrite.

- `<Config>`: `S` stock `-run=DataValidation` · `F0` full scan, rules off · `F1` all rules · `F2` data rules only · `F3` UpdateBudget rules only · `I0` incremental, rules off · `I1` rules on, dependents off · `I2` coarse dependents · `I3` precise dependents (`01` §7.2).
- `<Mutation>` (incremental runs only): `M1` rows removed · `M2` tag file removed · `M3` ticking component added to a parent Blueprint · `M4` control edit.
- Example: `2026-11-25_101500_F1_N4000_r2.json` (+ `.log`), `2026-11-25_101500_manifest.json` in `Raw\`, `2026-11-25_101500_summary.md` in `Summaries\`.
- `_r0_prime` files are the discarded priming run, kept for transparency and excluded from every summary.
- The corpus itself (`GameplayLint\Content\GLSeed\`) is regenerated from its seed and never committed; its ground-truth manifest is stored with the batch.

## Environment record (written into every manifest and summary)

```text
Engine:            UE 5.8.4 (CL 58902052), launcher build
Build:             Development editor, UnrealEditor-Cmd.exe in commandlet mode
Compiler:          MSVC 14.44.35229 (VS 2022 17.14), Windows SDK 10.0.22621.0
CPU / GPU / RAM:   i7-14650HX (8P + 8E) / RTX 5070 Laptop / 32 GB
CPU affinity:      0xFFFF (performance cores only), applied per run and recorded
Storage:           project and corpus on D: (Kingston NVMe), engine and local DDC on C: (WD NVMe)
OS:                Windows 11 build 26200
Power:             AC, power plan "Turbo"
Corpus:            N assets, seed, ground-truth manifest hash
Command line:      exact, per run, in the manifest
Runs:              per combination: smallest multiple of the config count that is >= 4 (interleaved, rotated) + 1 priming run per batch
```

## Run log

| Batch | Configs | N | Runs | Purpose | Summary |
|---|---|---|---|---|---|
| `2026-11-21_035120_baseline` | S, F0 | 1,000 | 4 + 4, + priming | S3 baseline, first attempt | **Invalid:** the corpus's tag files loaded no tags (`09` P5-2); the summary is kept and marked |
| `2026-11-21_083618_s7` | F1 | 1,000 | 1 | S7 correctness: all six rules | [summary](Summaries/2026-11-21_083618_s7_summary.md) · [score](Summaries/2026-11-21_083618_s7_score.md) |
| `2026-11-21_061551_s6` | F1 | 1,000 | 1 | S6 correctness: the curve rule | [summary](Summaries/2026-11-21_061551_s6_summary.md) · [score](Summaries/2026-11-21_061551_s6_score.md) |
| `2026-11-21_050802_s5` | F1 | 1,000 | 1 | S5 correctness: the first two rules | [summary](Summaries/2026-11-21_050802_s5_summary.md) · [score](Summaries/2026-11-21_050802_s5_score.md) |
| `2026-11-21_041526_baseline` | S, F0 | 1,000 | 4 + 4, + priming | S3 baseline: stock vs F0 equivalence, stock detection per defect class | [summary](Summaries/2026-11-21_041526_baseline_summary.md) · [score](Summaries/2026-11-21_041526_baseline_score.md) |
