# 09 · Testing And Benchmarks: GameplayLint

**Rule:** every check is recorded with its date, its exact command and its result, **including failures**. The full functional and benchmark sections are filled in during Phases 7–8; results from earlier phases are recorded as they happen. Every Unreal job below ran through the machine-wide UE job queue (`tools\ue_queue.py`); the job ID identifies its log.

---

## Phase 4 · Foundation checks (2026-11-21)

Queue job `20261121_010916_af4023` ran F-1 to F-5a as one job (`call build.bat … && call build.bat … && call validate.bat && call package.bat && call smoke_game.bat`), exit 0, 160.5 s in total. F-5b is the retry after the input fix; F-6 repeats the whole foundation after two portfolio-wide rules changed the project file and the smoke script. The F-5a command line below is the one that ran then (it still had `-ExitAfterSeconds=0`).

| # | Check | Command (from `P10_GameplayLint\`) | Result |
|---|---|---|---|
| F-1 | Editor target builds | `Tools\build.bat` (`GameplayLintEditor Win64 Development`) | ✅ Succeeded in 37.2 s. 27 actions; all five modules compiled and linked (`GameplayLint`, `GameplayLintBench`, `GameplayLintCore`, `GameplayLintDataRules`, `GameplayLintUpdateBudget`). **0 errors, 0 warnings**, so **no plugin-dependency warning** (`UEBuildTarget.cs:1564–1579`): the `.uplugin` references are complete. Toolchain: MSVC 14.44.35229, Windows SDK 10.0.22621.0 |
| F-2 | Game target builds and contains **no lint code** | `Tools\build.bat GameplayLint Development` | ✅ Succeeded in 37.3 s; 23 actions, **0 errors, 0 warnings**. The only project source compiled is `GameplayLintModule.cpp` (plus the shared PCH and the resource file): the editor-only modules are excluded from the runtime build, as designed |
| F-3 | The engine's **stock** Data Validation commandlet runs on the project | `Tools\validate.bat` (`-run=DataValidation -unattended -nullrhi -nosplash -traceautostart=0 -stdout -FullStdOutLogOutput`) | ✅ Exit 0, "Success - 0 error(s), 0 warning(s)". Plugins mounted: `GameplayLintCore`, `GameplayLintUpdateBudget`, `GameplayLintDataRules`. **9 validators enabled by default** (below). It validated **1 object**, `/Script/CoreUObject.Default__VerseClass`, which "contains valid data". The commandlet itself took 0.60 s, PeakPhysMemoryMB = 2031 |
| F-4 | Package a **Development** build | `Tools\package.bat` | ✅ `BUILD SUCCESSFUL`, `AutomationTool exiting with ExitCode=0`. The build step found the F-2 binary up to date. Cook: **516 packages cooked**, 7 skipped by platform, 523 total; cook "Success - 0 error(s), 0 warning(s)". Output `Build\Windows\GameplayLint.exe` (git-ignored) |
| F-5a | Packaged build starts, loads the map and quits | `Tools\smoke_game.bat packaged` (`-nullrhi -nosound -unattended -nosplash -traceautostart=0 -ExitAfterSeconds=0 -ExecCmds=quit`) | ⚠ Exit 0; `Template_Default` loaded in 0.020 s; clean exit. **1 warning:** `LogEnhancedInput: Warning: UEnhancedInputLocalPlayerSubsystem for Local Player '…' does not have a valid PlayerInput object. Failed to load user settings.` Cause and fix: no `DefaultInput.ini`, so the legacy player input class was used (`05` A9). Fixed by adding the two Enhanced Input class lines |
| F-5b | Re-package and smoke run after the fix | queue job `20261121_012414_c0b2cc`: `call package.bat Development && call smoke_game.bat packaged` | ✅ Exit 0 in 67.8 s. Cook: 516 cooked, 7 skipped by platform, 523 total, "Success - 0 error(s), 0 warning(s)"; `BUILD SUCCESSFUL`. Smoke: map loaded in 0.036 s, clean exit, **0 warnings** (the Enhanced Input warning is gone) |
| F-6 | Re-check after two portfolio rules (2026-11-21): Android File Server disabled in the `.uproject` and its engine-written block (with a random `SecurityToken`) deleted from `DefaultEngine.ini`; `-ExitAfterSeconds=0` removed from `smoke_game.bat` (5.8.4 doesn't parse it) | queue job `20261121_015047_145491`: the same five steps as F-1 to F-5 | ✅ Exit 0 in 88.1 s. **Editor:** 1 action (the target receipt rewritten for the new plugin set), 0/0. **Game:** `GameplayLint.exe` relinked without the Android File Server runtime module, 0/0. **Stock commandlet:** "Success - 0 error(s), 0 warning(s)", the same 1 object, 0.18 s. **Package:** 516 cooked, cook 0/0, `BUILD SUCCESSFUL`. **Smoke:** map loaded in 0.020 s, clean exit, **0 warnings**. The plugin is mounted nowhere in the log, and the editor runs did **not** write the token block again (`DefaultEngine.ini` has no `AndroidFileServer` line afterwards). One transient retry during staging: "Failed reading oplog from Zen at [::1]:8558 (attempt 1/3) … actively refused it … Re-checking ZenServer readiness and retrying"; UAT then started Zen for the stage step and finished normally |
| T-1 | CPU pinning works and is inherited (not a UE job: plain Python) | `python -I Tools\core_mask.py`; then `pin_self(performance_core_mask())` in a Python process, `applied_mask()` before and after, and `applied_mask()` read in a child process started afterwards | ✅ Mask `FFFF`. Before: `0xFFFFFF (all logical processors)`; after `pin_self` (returned `True`): `0xFFFF (performance cores only; hybrid CPU)`; **the child read back `0xFFFF`**, so every editor the runner starts inherits the pin |

**The default validator set (F-3), which configuration F0 inherits:**

```text
/Script/DataValidation.DirtyFilesChangelistValidator
/Script/DataValidation.EditorValidator_ActionUtility
/Script/DataValidation.EditorValidator_Localization
/Script/DataValidation.EditorValidator_Material
/Script/DataValidation.PackageFileValidator
/Script/DataValidation.WorldPartitionChangelistValidator
/Script/InputBlueprintNodes.EnhancedInputUserWidgetValidator
/Script/MutableValidation.AssetValidator_CustomizableObjects
/Script/MutableValidation.AssetValidator_ReferencedCustomizableObjects
```

Four of the 13 shipped validator classes are absent because their plugins are not enabled in a new project: Asset Referencing Restrictions (two classes, `EnabledByDefault: false`), MaterialValidation and GameFeatures.

**What F-3 taught us:**
- On a project with no content, the stock run "validates" one object: the class default object of `VerseClass`, a `/Script` object the registry lists. The stock filter removes `/Engine` and engine plugins by file path, and keeps any package it can't map to a file ("We don't recognize this packagepath, so keep it", `DataValidationCommandlet.cpp:196–200`) **[inferred: that this is why it was kept]**. The scoring script must ignore objects outside `/Game/GLSeed/`, or every run would count one extra "valid" asset.
- The stock run's Message Log lines appear in the log under category `AssetCheck` ("… Validating asset", "… contains valid data."), as `01b` B-2 expected. The scoring of F0 will still use per-asset details, not log lines.

**What F-5a taught us:** a hand-made project misses small things a template gives you. The packaged smoke run is the cheapest place to find them, which is why it is part of the foundation.

**What F-6 taught us:**
- The engine writes into project config files on its own (here, a generated token). Every engine-written block must be read before it is committed.
- A copied command-line switch can be dead. `-ExitAfterSeconds` did nothing in any run; `-ExecCmds=quit` was what ended them.
- The Zen retry is infrastructure, not project code: the local Zen server is shared by every project on this machine and had stopped between cook and stage **[inferred: why it had stopped was not traced]**. It matters later because the corpus scans also use the local DDC. A scan that hits a Zen retry is flagged in its manifest, so a slow run can be traced to it.

---

## Phase 5 · Steps S1–S4: fixtures, corpus, harness, rule base (2026-11-21)

Three queue jobs. The first failed to link; the second ran everything, but its baseline was invalid; the third is the record.

| # | Check | Command / job | Result |
|---|---|---|---|
| P5-1 | First S1–S4 build | job `20261121_030734_92a7d8`: build editor (then game, E2, stock scan) | ❌ Exit 6 after 17.4 s. Compiled, then **4 unresolved externals** in `UnrealEditor-GameplayLintBench.dll`: `UEditorValidatorBase::K2_ValidateLoadedAsset_Implementation`, `K2_CanValidateAsset_Implementation`, `K2_CanValidate_Implementation` (from the generated code of `UGLTestRule`) and `ValidateLoadedAsset` (from the contract test). Cause: the bench module subclasses a validator but listed only `GameplayLintCore`, whose public `DataValidation` dependency gives include paths, not the import library. Fix: `DataValidation` in the bench's dependencies (the link rule, `02` §3, `07`) |
| P5-2 | Builds, E2, corpus, baseline, tests | job `20261121_032421_87e808` (the full S1–S4 chain, `03`) | Builds ✅ editor 14.3 s, game 14.8 s, **0 errors, 0 warnings**. E2 ✅ (below). Seeding 1,000 ✅. Tests ✅ 4/4. **Baseline ❌ invalid:** the stock run reported "Failure - 7 error(s), 7661 warning(s)", and the warnings named *permanent* tags as invalid, even in the shared tables. Cause: the seeder wrote `+GameplayTagList=` lines, and a loose `Config/Tags/*.ini` file is read without ini symbol commands (`ConfigCacheIni.cpp:2216–2219`), so the engine loaded none of the corpus's tags. The batch `2026-11-21_035120_baseline` is kept, marked INVALID in its summary |
| P5-3 | Re-run after the tag-file fix | job `20261121_035657_9d0bde` (same chain) | ✅ Exit 0 in 197.6 s. Builds: editor 14.2 s (the bench module rebuilt), game 1.3 s (up to date), **0 errors, 0 warnings**. E2 ✅. Seeding 1,000 ✅ (manifest sha256 `11fdc0bea9a7fe60…`, as in P5-2: the fix changes the tag files, not the manifest). Baseline ✅: batch `2026-11-21_041526_baseline`, 9 runs, all ok, **0 permanent-tag warnings**. Tests ✅ 4/4 (`GameplayLint.Rule.Contract`, `.Walker.DataTableRows`, `.Walker.EveryVariant`, `.Walker.Metadata`) |

**E2, corpus determinism (P5-2 and P5-3):** `seed.py determinism --assets 500` seeds with seed 2, then twice with seed 1, and compares the manifest hashes.

| Seeding | P5-2 manifest sha256 | P5-3 manifest sha256 |
|---|---|---|
| seed 2, 500 assets | `e6bdcc7a60c61a3a…` | `e6bdcc7a60c61a3a…` |
| seed 1, 500 assets | `38a3e124c0b4a5d0…` | `38a3e124c0b4a5d0…` |
| seed 1 again | `38a3e124c0b4a5d0…` (identical) | `38a3e124c0b4a5d0…` (identical) |

The tag-file fix doesn't touch the manifest, so the hashes may match across jobs. What E2 requires is within a job: the same seed gives the same hash, and a different seed gives a different one.

### S3 baseline: the stock engine on the corpus

Batch `2026-11-21_041526_baseline`: N = 1,000, seed 1. Command: `python -I Tools
un_scan.py --configs S,F0 --all-assets --label baseline` (pinned to `0xFFFF`; one priming run, then 4 interleaved runs of each).

| Config | Runs | Process wall (s) | Commandlet (s) | Validation phase (s) | Checked | Valid | Valid with warnings | Invalid |
|---|---|---|---|---|---|---|---|---|
| S (stock `-run=DataValidation`) | 4 | 9.33 [9.20–10.05] | 1.21 [1.16–1.25] | n/a | 1,011 | 976 | 28 | 7 |
| F0 (our commandlet, no rules) | 4 | 9.29 [9.10–9.34] | 1.16 [1.14–1.17] | 0.97 [0.95–0.98] | 1,011 | 976 | 28 | 7 |

- **What was checked:** 1,011 = the 1,000 corpus assets + 10 shared assets (3 target tables, the curve table, 6 parent Blueprints) + the `VerseClass` default object (F-3).
- **S = F0 ✅ (`02` §9):** the per-asset result lines are identical in all 9 runs, priming included. With no rules, the child commandlet changes neither what the engine validates nor what it concludes, and it adds no measurable time.
- **A corrected summary column:** the first version of the summary showed F0 "Warnings 35" against S "28". The F0 number was the engine's own total, which also counts the 7 invalid assets that have warnings. Every row now counts the same result lines (`run_scan.py`, S1–S4).
- **The run time is mostly editor start-up** (`08` §3): about 9.3 s per process, of which the validation phase is under 1 s. The `03` estimate of 0.5–2.5 minutes per run was far too high. Phase 8 fits the owner's budget with room to spare; N = 5,000 is not measured yet.

**Stock detection** (`score.py`, F0; S has identical result lines):

| Defect class (the rule meant to find it) | Seeded | Flagged by the stock run (asset level) | How |
|---|---|---|---|
| Unknown tag (`GL.Data.UnknownTag`) | 28 | **28 (100 %)** | The tag system's load warning, "Invalid GameplayTag Fixture.Missing.0684 found in property /Script/GameplayLint.GLFixturePayload:Tag". A **warning**: the asset still "contains valid data" |
| Dangling row handle (`GL.Data.DanglingRow`) | 64 | 0 | — |
| Bad curve (`GL.Data.Curve`) | 33 | 0 | — |
| Per-frame tick outside the budget (`GL.UB.TickOutsideBudget`) | 28 | 0 | — |
| Client in the wrong tick group (`GL.UB.ClientTickGroup`) | 21 | 0 | — |
| Client that never ticks (`GL.UB.ClientNeverTicks`) | 26 | 0 | — |
| **All seeded defects** | **200** | **28 (14 %)** | |

- **Controls:** 0 of 672 clean assets and 0 of 130 near-misses flagged.
- **Held out:** 0 of 1 found.
- **Masked:** 7 of 7 invalid, through the fixture row's own `IsDataValid` ("Value must not be negative"), as designed. Their second defect is an unknown tag.
- **Rule-level detection:** 0 / 200, as expected with no rules.

**What the baseline answers (before any rule code):**
- **H2 ✅ supported.** Row handles and curves inside project structs: 0 of 97 flagged. Tags are seen only because the tag system itself warns on load.
- **H3, fresh-load half ✅.** A fresh-load commandlet run sees every unknown tag, in every structural variant: instanced objects and structs, map values, Blueprint defaults, DataTable rows. **The on-save half** (validating already-loaded assets) needs the script entry point and is **not measured yet**; it moves to Phase 7.
- **H4 ✅ (none expected).** 0 of 75 `GL.UB.*` defects.
- **H5, partly.**
  - The 28 tag findings are warnings, so on their own they don't fail the run. The run's exit code was 1 only because the 7 masked assets have errors (`01b` #7).
  - The messages name the *struct property* (`GLFixturePayload:Tag`), not the field (`Map[K1].Tag`), and have no stable ID: mapping them to defect classes needs text matching.
  - The exit code of a corpus with warnings only was not run; `01b` #7 implies it is 0 **[inferred]**.
- **The stock side of SC3:** per-class stock recall is in the table above. At most 172 of the 200 defects (86 %) can count as "found by F1 and not flagged by F0": every class except unknown tags.

### Audit trail for S1–S4

**`code-review` (medium effort) on the S1–S4 diff:** 7 findings, all fixed before the S1–S4 job was queued (the job built the fixed code).

| # | Finding | Fix |
|---|---|---|
| R1 | `FParse::Value` stops at `,`: `-GameplayLintRules=GL.Data,GL.UB` kept only `GL.Data`, and `-Paths=` only its first folder | `bShouldStopOnSeparator = false` for both (`Parse.cpp:298`) |
| R2 | An unreadable `-Changes=` file validated an empty change list and exited 0 | `bChangesUnreadable`: `ProcessValidationResults` returns false, so the run fails |
| R3 | The held-out `FInstancedStruct` entries are in fact walked (`UnrealType.cpp:57–65`), so the "expected misses" would be detected and the held-out count would be wrong | Instanced-struct payloads became a regular variant (1 % of data assets); new held-out kind: `LootTable` + `LootRow` (`01` §7.1, `01b` #11, `02` §2) |
| R4 | `FFileHelper`'s default `AutoDetect` encoding writes UTF-16 when a string holds a non-ANSI character; the Python readers expect UTF-8 | `ForceUTF8WithoutBOM` for the manifest, the change list, the tag files and the report |
| R5 | `-Paths` was silently ignored when `-Changes` was given | Warn and reset `Paths`; `run_scan.py` no longer passes `-Paths` to the I configurations |
| R6 | An unknown `-GameplayLintDependents=` value fell back to the configured mode without a word | Warning naming the value; the report records `dependents_mode` |
| R7 | Running `-Mutate` twice applied M1–M4 again (M4 adds 100 twice; M1 removes nothing), so the change list no longer matched the ground truth | `Mutate` refuses a corpus whose `DT_TargetsA` lacks row `T30` |

**`simplify`:** the skill asks for four parallel review agents. The brief forbids sub-agents, so I reviewed the same four angles (reuse, simplification, efficiency, altitude) myself on the same diff.

| Angle | Finding | Decision |
|---|---|---|
| Reuse | `-GameplayLintDependents=` parsing and the report's mode name were hand-written string switches over the enum | **Applied:** `StaticEnum<EGameplayLintDependents>()->GetValueByNameString` / `GetNameStringByValue` (the enum is a `UENUM`) |
| Simplification | The walker had two near-identical property-iterator loops: one in `VisitObject` (an asset and its sub-objects), one in `ForEachValueIn` (a row or a template) | **Applied:** one `Walk` function that both call; it collects instanced sub-objects only when the caller asks |
| Simplification | The seeder computed the field name of a defect cell with the same nested ternary in three places | **Applied:** `FieldFor(Kind, RowField)` |
| Simplification | `ObjectPath(Asset)` was a one-line wrapper of `GetPathName()` | **Applied:** inlined |
| Reuse | `MakeTag` exists in the seeder and in the tests | **Skipped:** different modules (a commandlet and a test file), 3 lines each; a shared public helper would add an export only for them |
| Reuse | `sha256()` exists in `seed.py` and `run_scan.py` | **Skipped:** the scripts run with `python -I`, which leaves the script folder off `sys.path`, so a shared module would need path code longer than the 3-line function |
| Simplification | `ResultName` is a switch over `EDataValidationResult` | **Skipped:** the report format depends on these three exact strings; a switch states them, a reflection lookup would hide them |
| Efficiency | `FText::FromName(RuleId)` runs for each asset a rule checks | **Skipped:** one small allocation per checked asset, against an asset load measured in milliseconds; the per-rule timing (`check_times_ms`) will show it if it matters |
| Altitude | No finding: the fixes sit where the causes are (parsing in the settings and the commandlet, the corpus design in the seeder) | — |

**Changes after the review passes** (self-reviewed for correctness, reuse, simplification, efficiency and altitude, because they came after `code-review` and `simplify` ran):
- The seeder's tag line lost its `+`. One character, with the engine citation in the comment.
- `run_scan.py`: a permanent-tag check that marks a run not ok. This is the altitude fix: it guards the measurement itself, so a broken dictionary can't pass as a baseline in any configuration.
- `run_scan.py`: every summary row now counts the same per-asset result lines. Simplification: one counting path instead of two definitions.

**Other findings in S1–S4:**
- **The tag files loaded no tags** (P5-2): found by reading the first baseline's warnings, not by a review. Neither the code review nor I had questioned the `+` prefix, because it is right in every `Default*.ini`. Root cause in the engine: loose files skip ini symbol commands (`ConfigCacheIni.cpp:2216–2219`, `2076–2096`). The fix was one character in the seeder. Prevention: `run_scan.py` marks a run not ok if any permanent corpus tag is reported invalid, so a broken dictionary can no longer produce a "baseline".
- **Link failure** (queue job `20261121_030734_92a7d8`, below): the root cause is in the build rules, not the code. Rule packs inherit the fix as the documented link rule (`02` §3, `07`).
- **Citation check** (a script opens every engine `File:line` that `06` §7–23 cites and looks for the named symbol within ±3 lines): 48 citations, 1 wrong. `DataValidationChangelist.h:62–95` / `:86–92`, carried from Phase 2, don't exist in the 5.8.4 header (52 lines). Corrected to `:17–50` / `:42–47` in `01b`, the Phase 2 notes and `06`. Eight other misses were the check's own fault (a range that starts before the symbol, or a flag passed under another name), confirmed by reading each line.
- **`TEXT("Tools\seed.py")`** in the seeder's messages: `\s` is not an escape (MSVC C4129, which would break the 0-warning rule). Caught by reading before the build; now `\\`.
- **Doc tooling:** a shell here-document turned `\r` in a `05` table into a carriage return. Fixed by script, and every changed file was scanned for control characters (clean).

---

## Phase 5 · Step S5: `GL.Data.UnknownTag` and `GL.Data.DanglingRow` (2026-11-21)

| # | Check | Command / job | Result |
|---|---|---|---|
| P5-4 | Build, tests, first F1 run | job `20261121_042708_01780c`: `build.bat` editor + game, `run_tests.bat`, `run_scan.py --configs F1 --runs 1 --no-prime --label s5` | ✅ Exit 0. Builds: editor 12.2 s, game 2.0 s, **0 errors, 0 warnings** (no plugin-dependency warning: the pack lists the `DataValidation` plugin). **Tests ✅ 6/6**: the four S4 tests plus `GameplayLint.Rules.UnknownTag` (its declared-tag case ran: no "dictionary is empty" note in the log) and `GameplayLint.Rules.DanglingRow`. F1 run: exit 3 (assets failed, `-FailOn=Error`), 9.4 s |

**F1 correctness at N = 1,000** (batch `2026-11-21_050802_s5`, one run: a correctness check; timing belongs to Phase 8):

| Rule | Seeded defects | Found at rule level | Cells (kind × variant) | False positives |
|---|---|---|---|---|
| `GL.Data.UnknownTag` | 28 | **28 (100 %)** | 10 cells, all 100 %, incl. instanced structs, map values, Blueprint defaults, DataTable rows | 0 |
| `GL.Data.DanglingRow` | 64 | **64 (100 %)** | 17 cells, all 100 %: missing DataTable and CurveTable rows, rows without a table, empty required handles | 0 |
| (rules from S6–S7) | 108 | 0 (not built yet) | | |

- **Controls:** 0 of 672 clean assets and 0 of 130 near-misses carry any finding (SC2 target ≤ 0.5 %).
- **Wrong-rule findings:** 0.
- **Held out:** 0 of 1, an expected miss.
- **Masked:** the 7 assets whose own `IsDataValid` fails were not checked by any rule. Rules ran on 1,003 of the 1,010 assets, as the engine's order predicts (`EditorValidatorSubsystem.cpp:657–663`).
- **Per-asset rule time** (`check_times_ms`, 1,003 assets): DanglingRow median 0.034 ms, p99 0.060 ms; UnknownTag median 0.025 ms, p99 0.055 ms. That is well inside SC5 (median ≤ 0.5 ms, p99 ≤ 5 ms) at this size; the full SC5 answer is Phase 8's.
- **The stock run's view of the same corpus:** the 28 unknown tags are now **errors with a rule ID and a field path** (`[GL.Data.UnknownTag] Map[K1].Tag: …`) instead of asset-level load warnings. The 64 dangling handles were invisible to the stock run.

**Audit trail for S5:**
- **`code-review` (medium):** 6 findings.
  - **R1 (efficiency):** the rules apply to every asset that is not a redirector, so each walks every property of meshes and textures too. **Deferred with data:** it goes to `11` as a candidate (a per-struct cache in the walker), waiting for SC4/SC5 numbers on large non-data assets.
  - **R2:** the UnknownTag test had no declared-tag case, so a rule that flagged every tag would pass. **Fixed.**
  - **R3:** fixed object names in the transient package. **Fixed:** the tables are unnamed.
  - **R4:** the tests would write findings into the editor's report. **False positive:** the report records only while a commandlet has it open, and `Begin()` resets it (`GameplayLintReportSubsystem.cpp:7–34`).
  - **R5 (pre-existing):** maps' placed actors are not walked. **Documented** as an expected miss (`02` §2).
  - **R6:** the docs for S5 were not yet written at review time. **Done** in this step.
- **`simplify`** (the four angles, by myself; the brief forbids sub-agents):
  - **Reuse, applied:** `Flags` and `MakeTag` now live in one shared test header, `GLTestHelpers.h`, instead of one copy per test file.
  - **Simplification:** the two handle types already share one decision function (`CheckHandle`); nothing further.
  - **Efficiency:** see R1.
  - **Altitude:** none. The rules use the core walker and add no special cases to it.

---

## Phase 5 · Step S6: `GL.Data.Curve` (2026-11-21)

| # | Check | Command / job | Result |
|---|---|---|---|
| P5-5 | Build, tests, F1 run | job `20261121_051420_2cf657`: `build.bat` editor + game, `run_tests.bat`, `run_scan.py --configs F1 --runs 1 --no-prime --label s6` | ✅ Exit 0. Builds: editor 11.4 s, game 15.1 s (the fixture module changed: a new `FalloffByDistance` curve), **0 errors, 0 warnings**. **Tests ✅ 7/7**: the new `GameplayLint.Rules.Curve` with 15 cases, and `Walker.Metadata`, updated for the second `LintMonotonic` curve. F1 run: exit 3, 9.5 s |

**F1 correctness at N = 1,000** (batch `2026-11-21_061551_s6`, one run):

| Rule | Seeded defects | Found at rule level | Cells | False positives |
|---|---|---|---|---|
| `GL.Data.Curve` | 33 | **33 (100 %)** | 8 cells, all 100 %: non-finite keys in every variant (array, map, nested, instanced, Blueprint default, DataTable row), empty required curves, a reversal | 0 |
| `GL.Data.UnknownTag`, `GL.Data.DanglingRow` | 92 | 92 (100 %), unchanged | | 0 |
| **The data rules together** | **125** | **125 (100 %)** | | **0 of 802 controls** |

- **Near-misses:** 0 of 130 flagged, including the flat curve and the curve that rises without falling.
- **Wrong-rule findings:** 0.
- **Rule time per asset:** Curve median 0.012 ms, p99 0.030 ms (1,003 assets).
- The `GL.UB.*` defects (75) wait for S7.

**Audit trail for S6:**
- **`code-review` (medium):** 9 findings, all fixed before the build.
  - **Missing coverage:** curve assets (`UCurveFloat`) and curve-table rows were never checked; NaN tangents passed; the default value was ignored in both directions (a constant curve was flagged as empty, a NaN default passed); a required curve pointing at an empty external curve passed.
  - **Over-reporting:** an external curve's keys were reported on every asset that uses it, without naming it; an infinite key in a must-rise curve gave two findings.
  - **Guessing:** an unknown `LintMonotonic` value was read as "Increasing".
  - **Tests and wording:** the "Decreasing", external and NaN-time branches had no test; one word of a message was not localisable.
  - **The fixes, in the rule (`05` F2):** one templated check for rich and simple keys; the curve asset owns its keys; `HasAnyData` decides "empty"; an unknown annotation is reported; two complete messages. A fixture curve with `LintMonotonic="Decreasing"` was added for the test.
- **`simplify`** (by myself): one generic lambda instead of two copies of the curve-table row loop. Nothing to reuse elsewhere; no efficiency or altitude finding.
- **Caught before the build:** the new fixture curve would have failed the S4 `Walker.Metadata` test, which counted exactly one annotated curve. Fixed in the same step (`07`).

---

## Phase 5 · Step S7: the UpdateBudget pack (2026-11-21)

| # | Check | Command / job | Result |
|---|---|---|---|
| P5-6 | Build, tests, F1 run | job `20261121_062431_018fb5`: `build.bat` editor + game, `run_tests.bat`, `run_scan.py --configs F1 --runs 1 --no-prime --label s7` | ❌ Exit 3. Builds ✅ 0/0. Tests: the four data and walker tests passed, then `GameplayLint.Rules.UpdateBudget` **crashed the editor**: `EXCEPTION_ACCESS_VIOLATION reading address 0xffffffffffffffff` inside `UEditorValidatorBase::ValidateLoadedAsset`, called from the test's `Validate`. Cause: the test held its rule objects in raw pointers; compiling a test Blueprint runs a garbage collection (`BlueprintCompilationManager.cpp:423–427`), which freed them. Fix: `TStrongObjectPtr` for the rules and for the objects reused across cases (`06` §27, `07`) |
| P5-7 | The same, after the fix | job `20261121_071612_b36ca2` | ✅ Exit 0. Builds: editor 14.9 s, game 1.0 s (up to date), **0 errors, 0 warnings**. **Tests ✅ 8/8**, including `GameplayLint.Rules.UpdateBudget` (16 cases). F1 run: exit 3, 9.6 s |
| E1 | The pack plugs in without core changes | `git diff --stat -- GameplayLint/Plugins/GameplayLintCore` before the S7 commit | ✅ **0 lines changed** under `Plugins/GameplayLintCore/`. The second pack was added with settings, three rules, config and tests, and the framework did not change |

**F1 correctness at N = 1,000** (batch `2026-11-21_083618_s7`, one run, all six rules):

| Rule | Seeded defects | Found at rule level | Cells |
|---|---|---|---|
| `GL.UB.TickOutsideBudget` | 28 | **28 (100 %)** | an actor whose native parent ticks (9); a per-frame component that is a template (10), inherited (7) or on a shared parent (2) |
| `GL.UB.ClientTickGroup` | 21 | **21 (100 %)** | template (6), subclass defaults (15) |
| `GL.UB.ClientNeverTicks` | 26 | **26 (100 %)** | template (10), subclass defaults (16) |
| The data rules | 125 | 125 (100 %), unchanged | |
| **All rules** | **200** | **200 (100 %)** | |

- **False positives:** 0 of 802 controls carry any finding. The UB near-misses (interval tickers, start-disabled tickers, allow-listed skeletal mesh and projectile movement) pass.
- **Wrong-rule findings:** 0.
- **Held out:** 0 of 1.
- **Masked:** the 7 assets were not checked, as designed.
- **Rule time per asset:** UB rules median 0.0014–0.0029 ms (356 and 506 assets: only actor and component Blueprints); the data rules unchanged.

**Audit trail for S7:**
- **`code-review` (medium):** 5 findings.
  - **Important, fixed:** `bAutoActivate` was ignored. An auto-activating component's tick is enabled by `Activate` (`ActorComponent.cpp:2815–2819`, `Actor.cpp:6399`), so ClientNeverTicks would have failed a working client, and TickOutsideBudget would have missed a per-frame ticker. The fix is one shared `StartsTicking`.
  - **Fixed:** the tests gained the missing paths (subclass defaults, opted-in scope, allow list, start-disabled and auto-activating tickers).
  - **Fixed:** a never-ticking client in the wrong group was reported by two rules; the group check now skips it.
  - **Fixed:** settings paths were resolved in the innermost loop; they are now resolved once per asset (`FClasses`).
  - **Skipped:** isolating the test from the project's config. The settings class is private to the pack, and the test deliberately runs against the checked-in `DefaultEditor.ini`.
- **`simplify`** (by myself): one finding, skipped. The test's `MakeBlueprint` / `AddComponent` resemble the seeder's helpers, but merging them would tie the corpus generator to test code, for four lines.
- **The crash (P5-6)** was not found by review: neither the review nor I asked what can collect garbage between creating an object and using it.

---

## Step S8 and Phases 7–8 (to be written)

- S8: F1 correctness runs per rule pack, the incremental scenarios M1–M4 (SC6).
- Functional, edge-case and stability tests (Phase 7), including the on-save half of H3.
- The full benchmark protocol and the SC1–SC7, E1–E2 results (Phase 8).
