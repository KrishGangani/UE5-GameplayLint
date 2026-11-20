# D · Measurement and build facts

Working notes, 2026-11-21.

## D1. What is measured, and where it runs

- The system under test is **editor tooling**. Its runs are `UnrealEditor-Cmd.exe <project> -run=<commandlet>` processes, launched through the machine-wide UE job queue (portfolio rule). There is no frame-time workload, so the packaged/fixed-step protocol of the portfolio brief does not apply to the headline numbers. The packaged game is built only to prove that no lint code leaks into a runtime build (Phase 4).
- Two kinds of numbers:
  1. **Correctness** (detection rate, false positives) is deterministic: the same corpus and the same rules give the same findings. One run is enough; repeated timing runs double as a determinism check (identical finding sets).
  2. **Time** (full and incremental scans) is noisy: disk cache, DDC, CPU scheduling. It follows the portfolio's interleaving rules (D4).

## D2. Where the time goes inside a validation run [SRC]

- Every asset is loaded before validators see it (A5). Expect asset loading to dominate the wall time; rule time is measured separately.
- `FValidatorStatistics::TotalDurationSeconds` per validator class (`Plugins/Editor/DataValidation/Source/DataValidation/Public/EditorValidatorSubsystem.h:74–89`), accumulated with `FScopedDurationTimer` around each `ValidateLoadedAsset` (`Private/EditorValidatorSubsystem.cpp:671–685`). Available in `FValidateAssetsResults` after `ValidateAssetsWithSettings`. The stock commandlet discards them (it logs only counts). A child commandlet (`ValidateAssets` / `ProcessValidationResults` are virtual) can write them to a report.
- Engine-side knobs that change scan time and must stay fixed across configurations: `EditorValidator.AssetValidationBatchSize` (50), `EditorValidator.EnablePreloadingAssets` (false), `EditorValidator.MaxAssetBatchPreloads` (20) (`EditorValidatorSubsystem.cpp:49–65`); `bCaptureAssetLoadLogs` (true) disables preloading (`:85`).
- The commandlet's own end-of-run line: "Execution of commandlet took: %.2f seconds, PeakPhysMemoryMB=..." (`Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:4232–4251`). Useful as a cross-check on the process wall time and memory.
- The process exit code is 1 if any error was logged, unless the commandlet sets `UseCommandletResultAsExitCode` (`LaunchEngineLoop.cpp:4217–4224`).

## D3. CPU pinning (portfolio protocol update, 2026-11-21)

- The CPU is hybrid: i7-14650HX, 8 P-cores (logical 0–15) + 8 E-cores (16–23). UE5-UpdateBudget measured two stable run levels about 1.6× apart when Windows put the game thread on an E-core.
- Every **timed** run: start `UnrealEditor-Cmd.exe` directly with `subprocess.Popen`, pin it at once with `pin_process(pid, mask)` (`SetProcessAffinityMask`: all present and future threads; children inherit), wait with a timeout, kill the tree in a `finally` block, record the applied mask per run. `Tools/core_mask.py` holds both functions, copied from UE5-UpdateBudget's `run_bench.py`; it returns `FFFF` on this machine (run 2026-11-21). Builds are not pinned.
- First plan, corrected the same day: launching through `cmd /c start "" /wait /affinity <MASK> …`. Another portfolio project found that this form **always returns exit code 0**, whatever the program returned, so a failed scan would look like a good one. Only engine start-up (before the pin) can run on an E-core, and start-up is outside the measured validation phase.
- Editor commandlets use worker threads for async loading and the asset registry scan. Pinning keeps them on P-cores too, which also means the timed runs use 16 logical CPUs, not 24. That is recorded in the environment, and it is the same for every configuration.

## D4. Timing protocol, adapted

- One discarded priming run per batch (warms the OS file cache and the DDC).
- Interleaved configurations with the order rotated each round; **runs per combination = number of configurations**, so each configuration sits in each position once (portfolio rule). The comparisons are therefore designed with 4 configurations for full scans (4 runs each) and 3 for incremental scans (3 runs each); see 01 §6.2.
- Statistics: for scan times there is one number per run (not a per-frame distribution), so the report is the median across runs with [min–max]. Per-asset rule time (from per-validator totals / assets validated) is reported the same way.
- Environment record with every batch: as UE5-UpdateBudget's, plus the affinity mask, the asset count N, the corpus seed and hash, and the DDC location.

## D5. Build facts relied on

- Engine 5.8.4, CL 58902052 (`Engine/Build/Build.version`).
- The launcher engine cannot build the Test configuration (UE5-UpdateBudget, Phase 4). Not needed here: the headline runs are editor commandlets.
- MSVC 14.44.35229, Windows SDK 10.0.22621.0; `BuildSettingsVersion.V7`, `EngineIncludeOrderVersion.Unreal5_8` (portfolio facts).
- UBT warns when a plugin's module depends on another plugin's module and the plugin does not list that plugin: "Plugin '{PluginName}' does not list plugin '{DependencyPluginName}' as a dependency, but module '{ModuleName}' depends on module '{DependencyModuleName}'." (`Engine/Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1564–1579`). The project-level variant is at `:1687`. So every rule pack's `.uplugin` must list `GameplayLintCore`, and the core's must list `DataValidation`, or the 0-warning build target fails. Phase 4 checks this.
- The DataValidation import library ships with the launcher engine (A1). Linking a project plugin against `DATAVALIDATION_API` symbols needs no source build.
- Editor-only modules (`"Type": "Editor"`) are not built into the game target, which is why the packaged build can prove isolation.

## D6. Command-line switches used (verified)

| Switch | Meaning | Evidence |
|---|---|---|
| `-run=<Name>` | Run the commandlet class `<Name>Commandlet` | `LaunchEngineLoop.cpp:2988–2990` |
| `-unattended` | No dialogs; also makes an unattended changelist validation with a scanning registry fatal (A7) | `EditorValidatorSubsystem.cpp:1352` |
| `-stdout`, `-FullStdOutLogOutput` | Write the full log to stdout, so the queue's idle watchdog sees progress | `Source/Runtime/Core/Private/Misc/OutputDeviceStdOut.cpp:190–195`; `LaunchEngineLoop.cpp:1892` |
| `-BUILDMACHINE` | Sets `GIsBuildMachine` (skips the changelist object's source-control check) | `LaunchEngineLoop.cpp:6496–6498`; `DataValidationChangelist.cpp:97–102` |
| `-traceautostart=0` | An open Unreal Insights does not trace the run | UE5-UpdateBudget 01b §4 |
| `-nullrhi`, `-nosplash`, `-abslog=` | No GPU, no splash, log file path | standard; used by UE5-UpdateBudget's scripts |
