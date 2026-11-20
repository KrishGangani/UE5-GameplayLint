# 07 · Common Mistakes: GameplayLint

**Rule:** for every module, the mistakes that are easy to make here, **how to spot them, how to prevent them and how to debug them**. Mistakes that **actually happened in this project** are marked ⚠ caught; they are the real interview stories. Rule-code mistakes are added step by step from S3 on. The Data Validation pitfalls below come from Phase 2's source reading, and the design already guards against them.

Paths are relative to `Engine/`; `EVS.cpp` is `Plugins/Editor/DataValidation/Source/DataValidation/Private/EditorValidatorSubsystem.cpp`, and the other DataValidation files are in the same plugin.

---

## Foundation and pipeline (Phase 4)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| ⚠ caught: **no `DefaultInput.ini`** in a hand-made project | The packaged smoke run logs "UEnhancedInputLocalPlayerSubsystem … does not have a valid PlayerInput object" (`09` F-5a) | Set the Enhanced Input player input and component classes, as Epic's templates do (`05` A9) | Grep the engine for the message (`EnhancedInputSubsystems.cpp:118`) and read the condition above it |
| A plugin module depends on another plugin's module that the `.uplugin` doesn't list | UBT warning "Plugin '…' does not list plugin '…' as a dependency …" (`UEBuildTarget.cs:1564–1579`); fails later in a project with that plugin off | List it under `"Plugins"` (`06` §5) | Read the warning: it names both modules |
| A lint module made `Runtime` | The game target tries to link `UnrealEd`/`DataValidation` and fails; or it builds and ships editor code | Every lint and bench module is `"Type": "Editor"` (`06` §4) | The game build log's compile list must show only `GameplayLintModule.cpp` from the project (`09` F-2) |
| The game module depends on a lint module | The isolation check fails: lint code compiles into the game | Dependency rule: packs → core → engine; the game knows nothing of the rules (`02` §3) | `grep GameplayLint Source/GameplayLint/GameplayLint.Build.cs` must find only the module's own name |
| ⚠ caught (portfolio audit): **a generated token committed with the config.** The default-on Android File Server plugin writes a random `SecurityToken` into `DefaultEngine.ini` on the first editor start | `git diff` shows an `[/Script/AndroidFileServerEditor.…]` block nobody wrote | Disable the plugin in the `.uproject` and delete the block (`05` A1, A9) | Diff the config before and after one editor start (`09` F-6) |
| ⚠ caught (portfolio audit): **a command-line switch the engine doesn't parse** (`-ExitAfterSeconds`, copied from another project's script) | Nothing visible: the switch is silently ignored | Before relying on a switch, grep the engine source for it; here it occurs nowhere in 5.8.4 | `grep -r ExitAfterSeconds Engine/Source Engine/Plugins` |
| ⚠ caught: **`sed -i` in Git Bash strips the CR from CRLF batch files** while editing them | `file Tools\*.bat` stops saying "with CRLF line terminators" | Re-add CRLF after any scripted edit; `.gitattributes` normalises on commit | `cat -A` shows `$` instead of `^M$` at line ends |
| Batch files saved with LF line endings | Odd `cmd.exe` parse errors, usually near labels or blocks | Save as CRLF; `.gitattributes` has `*.bat eol=crlf` | `file Tools\*.bat` shows "with CRLF line terminators" |
| Launching timed runs with `cmd /c start "" /wait /affinity …` | Never visible: it returns 0 even when the program failed (found by another portfolio project). Corrected here before any timed run | `pin_self` once in the runner, then `subprocess.Popen`, timeout, kill in `finally` (`05` A10) | Run a command that exits 3 through each method and compare the exit codes |
| Starting any UE job outside the queue | Another project's benchmark gets noisy; two compiles fight | Every build, commandlet and timed run goes through `tools\ue_queue.py` | `ue_queue.py status` shows who holds the machine |

## Data Validation: pitfalls found in Phase 2 (designed against)

| Mistake | How to spot it | How to prevent it | How to debug it |
|---|---|---|---|
| Overriding the **5.0–5.3 validator signatures** found in older tutorials | `ensure` "running deprecated validation path" (`EditorValidatorBase.cpp:37–50`); deprecation warnings at compile time | Override `CanValidateAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&)` and the matching `ValidateLoadedAsset_Implementation` (`01b` §2) | Compile with warnings as errors; the 0-warning rule catches it |
| Returning `NotValidated` after `CanValidateAsset` said yes | `ensure` "did not return a validation result from native validation path" (`EditorValidatorBase.cpp:58`) | The rule base class always ends with pass or fail; rules only report (`02` §4.1) | Break on the ensure; check which branch returned early |
| Calling `AssetPasses` after reporting an error | `ensure` "AssetPasses called after errors were reported" (`:152`) | Same: the base class decides once, after the rule ran | Same |
| Expecting a rule to run on an asset whose `IsDataValid` failed | The rule's finding is missing, but the asset is "Invalid" for another reason | Know the order: class check first, validators only if it didn't fail (`EVS.cpp:657–663`). The corpus measures masking | Validate the asset alone; read every message, not only yours |
| Testing a tag rule only on save | Unknown tags seem undetected on save but detected in CI, or the reverse | On-save validation doesn't load assets, so load warnings don't appear there (`EVS.cpp:1167`) | Compare a fresh-load commandlet run with an on-save run on the same asset (H3) |
| Using `FGameplayTag::IsValid()` as "the tag exists" | Unknown tags pass the check | It only tests for `None` (`GameplayTagContainer.h:138–141`); ask the manager: `RequestGameplayTag(Name, false)` | Log both results for a deliberately unknown tag |
| Trusting the stock commandlet's exit code in CI | A corpus with failing assets returns 0; an unrelated logged error returns 1 | Gate on the per-asset results (the child commandlet's report and `-FailOn`), not the process code (`01b` #7, B-2) | Run once on a known-bad asset and print the exit code |
| Expecting "Validate Assets and Dependencies" to check the **referencers** | A changed table's victims are never checked | It validates what the selection references, not what references it (`DataValidationModule.cpp:230–232`) | Read the menu tooltip; test with a removed row |
| Expecting changelist validation to re-check victims of a **modified** asset | Removing a row passes pre-submit; the break appears later | The engine adds referencers only of **deleted** packages (`EVS.cpp:1492–1516`); rules add the rest through `GetAssetsToValidateFromChangelist` (`02` §6.2) | Compare an incremental run with a full scan after the same change (SC6) |
| Expecting the registry to know about a handle with a **null table** | The precise dependent query misses it | Such handles aren't recorded as searchable names (`DataTable.cpp:1117`); the per-asset check still finds them | Inspect `GetDependencies(…, SearchableName)` for the asset |
| An `ExcludedDirectories` entry that is too short | Unrelated folders silently stop being validated | It is a **substring** match (`EVS.cpp:1197–1208`): use full, unique paths such as `/Game/Developers/` | Log `IsPathExcludedFromValidation` for a few paths |
| A validator that **modifies** the asset it checks | Packages become dirty on save or pre-submit; changes nobody asked for | Checks are read-only; only a user-clicked fixer may change data (`02` §8) | Look for dirty packages after a validation run |
| Unattended changelist validation while the registry is still scanning | The process ends with a Fatal error (`EVS.cpp:1350–1355`) | Wait for the registry first (the stock commandlet's `WaitForAssetRegistry`) | The log line names the cause |
