# 08 · Debugging And Profiling: GameplayLint

**Status:** started in S3 (the harness and the baseline); completed in Phases 7–8. Commands are for UE 5.8.4, run from `P10_GameplayLint\` through the queue.

**Will also contain (Phases 7–8):** `LogContentValidation` verbosity (per-asset timing at `VeryVerbose`), Unreal Insights traces of a commandlet run, and where incremental time goes.

---

## 1. What each scan leaves behind

| Output | Where | Use it for |
|---|---|---|
| The child commandlet's report | `Benchmarks\Raw\<run>.json` | Per asset: result, errors, warnings. Per finding: rule, asset, severity, path, message. Per validator: totals. Phase times (`times_s`), per-rule check times (`check_times_ms`), dependent counts |
| The full editor log of the run | `Benchmarks\Raw\<run>.log` (git-ignored) | Everything the engine printed: load warnings, `AssetCheck` result lines, Zen messages |
| The batch manifest | `Benchmarks\Raw\<batch>_manifest.json` (committed) | Exact command lines, exit codes, wall times, CPU mask, parsed result lines, `permanent_tag_warnings`, `zen_retry` |
| Summary and score | `Benchmarks\Summaries\<batch>_summary.md`, `_score.md`, `_score.json` | Medians and ranges; detection and false positives against the ground truth |

## 2. Debugging story: the baseline in which every tag was unknown (S1–S4)

**Symptom.** The first stock run at N = 1,000 reported "Failure - 7 error(s), 7661 warning(s)". The design expected about 28 warnings: one per seeded unknown tag.

**Steps:**
1. **Group the warnings, don't read them one by one.** Strip timestamps and numbers, then `sort | uniq -c`. Almost all were `LogGameplayTags: Warning: … Invalid GameplayTag Fixture.Tag.NN found in property …`.
2. **Check a case where the answer is known.** `Fixture.Tag.*` are the *permanent* tags. Even `Shared\DT_TargetsA`, which holds only permanent tags, had 32 such warnings. So the data was fine, and the dictionary was empty.
3. **Follow the engine's path from the folder to the dictionary:**
   - `Config/Tags` is searched by default (`GameplayTagsManager.cpp:682`).
   - Each file is loaded with `LoadConfig(UGameplayTagsList::StaticClass(), File)` (`:582`).
   - That reaches `FConfigCacheIni::Find`, which reads an unknown file with `FConfigFile::Read` (`ConfigCacheIni.cpp:4403`).
   - `Read` calls `FillFileFromDisk(…, bHandleSymbolCommands = false)` (`:2216–2219`). With symbol commands off, a leading `+` is not stripped (`:2076–2096`), so the key was `+GameplayTagList`, and `LoadConfig` looked for `GameplayTagList`.
4. **Fix and guard.**
   - The seeder writes plain lines; a repeated key appends, because `ArrayAdd` is the default command (`:2051`).
   - `run_scan.py` now marks a run not ok when any permanent tag is reported invalid.
   - The re-run's stock scan logged 70 warnings and flagged exactly the 28 seeded unknown-tag assets.

**Lesson.** `+Key=` is right in `Default*.ini` (layered, symbol commands on) and wrong in a loose file. Debug from a known-good case: a value you *know* is declared.

## 3. Where a scan's time goes (S3 baseline, N = 1,000)

| Part | F0 median | Source |
|---|---|---|
| Whole process (editor start, scan, exit) | 9.3 s | `wall_seconds` in the manifest |
| Process start to the commandlet's `Main` | 7.2 s (run 1) | report `times_s.process_start_to_main` |
| `Main` to validation start (registry wait, settings) | 0.16 s (run 1) | report `times_s.main_to_validate` |
| The validation phase (load and validate 1,011 assets) | 0.97 s | report `times_s.validate` |

About three quarters of a scan is editor start-up, which no rule can change. That is why SC4 compares **validation-phase** times, not process times (`01` §7.4).

**Run-to-run noise:** stock wall 9.20–10.05 s over 4 runs; F0 9.10–9.34 s. One stock run logged one extra warning: `LogZenServiceInstance: … Failed attach as sponsor process`. That is the shared Zen server, not the corpus (found by diffing the grouped warnings of two runs).
