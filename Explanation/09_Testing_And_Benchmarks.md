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

## Phases 5–8 (to be written)

- S2: corpus determinism (E2). S3: the stock baseline (stock commandlet vs F0 equivalence; F0 detection per class; H2, H3, H5).
- Functional, edge-case and stability tests (Phase 7).
- The full benchmark protocol and the SC1–SC7, E1–E2 results (Phase 8).
