# A · Data Validation plugin internals (UE 5.8.4)

Working notes, 2026-11-21. Engine: `C:\Program Files\Epic Games\UE_5.8\Engine` (`Build/Build.version`: 5.8.4, CL 58902052, branch `++UE5+Release-5.8`). Paths below are relative to `Engine/`. Plugin root: `Plugins/Editor/DataValidation/Source/DataValidation/` (shortened to `DV/`).

## A1. The plugin itself

- `Plugins/Editor/DataValidation/DataValidation.uplugin:13` `"EnabledByDefault" : true`; `:22–23` one module, `"Type": "Editor"`, `"LoadingPhase" : "PreDefault"`. No Beta/Experimental flag.
- `DV/DataValidation.Build.cs:14–23` public deps: Core, CoreUObject, Engine, TargetPlatform, EditorSubsystem, DeveloperSettings. Private deps include UnrealEd, AssetRegistry, SourceControl, UncontrolledChangelists, Blutility (`:25–52`).
- The launcher engine ships the import library needed to link against it: `Plugins/Editor/DataValidation/Intermediate/Build/Win64/x64/UnrealEditor/Development/DataValidation/UnrealEditor-DataValidation.lib` and `Binaries/Win64/UnrealEditor-DataValidation.dll` exist on this machine. So a project plugin can derive from its classes with a launcher build.

## A2. `UEditorValidatorBase` (the class every validator derives from)

File: `DV/Public/EditorValidatorBase.h`.

- `:27` `UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (ShowWorldContextPin), Config=Editor)`, `:28` derives from `UObject`.
- Header comment `:19–25`: "It should be used when checking engine-level classes, as UObject::IsDataValid requires overriding the base class. You can create project-specific version of the validator base, with custom logging and enabled logic." And: "C++ and Blueprint validators will be gathered on editor start, while python validators need to register themselves".
- `:35` public entry point `EDataValidationResult ValidateLoadedAsset(const FAssetData&, UObject*, FDataValidationContext&)`, non-virtual: it dispatches to BP and native paths.
- `:38–41` `virtual TArray<FAssetData> GetAssetsToValidateFromChangelist(UDataValidationChangelist*, FDataValidationContext&) const` with the comment "Override this to return additional assets that need validation because of other changes in a changelist. e.g. referencers of a specific type may be broken by edits to a dependency." **This is the engine hook for cross-asset incremental validation.** Default returns `{}`.
- `:44` `virtual void PostAssetValidation(TArray<TSharedRef<FTokenizedMessage>>&)`: "do any state cleanup or add additional validation messages after all per asset validation is complete." A once-per-run hook.
- `:51–54` `virtual bool CanValidateAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&) const` (default false).
- `:57–60` `virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&)` (default NotValidated).
- `:63–74` Blueprint events `K2_CanValidate`, `K2_CanValidateAsset`, `K2_ValidateLoadedAsset`.
- `:79` `AssetFails`, `:83` `AssetPasses` ("Failure to call this will report the validator as not having checked the asset."), `:87` `AssetWarning`, `:92–93` `AssetMessage(...)` returning a `FTokenizedMessage` (tokens: asset links, text, fixes).
- `:95–98` `virtual bool IsEnabled() const { return bIsEnabled && !bIsConfigDisabled; }`.
- `:137–142` `bIsEnabled` (EditAnywhere) and `UPROPERTY(Config) bool bIsConfigDisabled` ("Allows disabling validators from config without checking in assets/modifying code.").
- `:120–134` the 5.0–5.3 signatures are `UE_DEPRECATED("5.4", ...)`. Old tutorials use them; 5.8 code must override the `FAssetData`+context versions.

File: `DV/Private/EditorValidatorBase.cpp`.

- `:27–35` BP path runs only if `K2_CanValidate(...) && K2_CanValidateAsset(Asset)`; `:92–95` `K2_CanValidateAsset_Implementation` returns false, so a C++ validator never accidentally runs the BP path.
- `:37–50` legacy path fires `ensureMsgf(false, "... running deprecated validation path ...")`.
- `:52–62` native path: `if (CanValidateAsset_Implementation(...)) { NewResult = ValidateLoadedAsset_Implementation(...); ... ensureMsgf(NewResult != NotValidated, "Validator %s did not return a validation result from native validation path ...") }`. **Rule:** once `CanValidateAsset` says yes, the validator must end with Valid or Invalid (call `AssetPasses` or report an error), or an ensure fires.
- `:64–76` per-asset timing is logged only at `VeryVerbose`.
- `:102–120` `AssetFails` formats "{CustomMessage}. ({ValidatorName})" unless `bOnlyPrintCustomMessage`.
- `:152` `ensureMsgf(ValidationResult != Invalid, "AssetPasses called after errors were reported")`.
- `:156–173` `AssetMessage` with severity Error sets the result to Invalid; it adds an `FAssetDataToken` (clickable asset link) when the asset data is valid.
- `:81` `FEditorToolDelegates::OnEditorToolStarted.Broadcast(GetClass()->GetName())` whenever a validator actually validated (telemetry).

## A3. How validators are discovered (`UEditorValidatorSubsystem`)

Files: `DV/Public/EditorValidatorSubsystem.h`, `DV/Private/EditorValidatorSubsystem.cpp`.

- `.h:257–258` `UCLASS(MinimalAPI, Config = Editor) class UEditorValidatorSubsystem : public UEditorSubsystem`. `.h:253–255`: "this subsystem may be subclassed ... If a subclass exist in your project module, it will supercede the engine validation subsystem." `.cpp:302–318` implements that: the base class refuses to be created if a child class wants to be.
- `.cpp:320–335` `Initialize`: native registration is deferred to `FCoreDelegates::GetOnPostEngineInit()`; Blueprint registration waits for `AssetRegistry.OnFilesLoaded()` if the registry is still scanning.
- `.cpp:373–401` `RegisterNativeValidators`: `GetDerivedClasses(UEditorValidatorBase::StaticClass(), ValidatorClasses)`; keep classes that are **not `CLASS_Abstract`**, not already registered, and live in a **script package** (`FPackageName::IsScriptPackage`). For each, `NewObject<UEditorValidatorBase>(GetTransientPackage(), ValidatorClass)`. So: **every concrete native subclass in any loaded module becomes a validator, with no registration code.** An abstract intermediate base class (ours) is skipped automatically.
- `.cpp:398` and `:1519–1536`, `:1597–1641`: modules loaded later (`OnModulesChanged`) are picked up on the next `UpdateValidators()`.
- `.cpp:403–437` Blueprint validators: `AssetRegistry.GetDerivedClassNames({UEditorValidatorBase})`, non-script classes, only if `bAllowBlueprintValidators` (`.h:568–569`, default true in the constructor `.cpp:299`). Loaded lazily in `UpdateValidators` (`:1644–1673`).
- `.cpp:554–569` `ForEachEnabledValidator` calls `UpdateValidators()` then filters on `IsEnabled()`.
- `.cpp:480–487` `AddValidator(UEditorValidatorBase*)` is the manual path (Python validators).
- Validators are stored in `UPROPERTY(Transient) TMap<FTopLevelAssetPath, TObjectPtr<UEditorValidatorBase>> Validators` (`.h:546–547`): one instance per class, kept alive by GC through the subsystem.

## A4. What runs for one asset (`ValidateObjectInternal`)

`.cpp:645–697`:

```text
Result = InObject->IsDataValid(Context)          // class-level validation first (:657)
if Result == Invalid: return Result               // (:660-663) ← custom validators are SKIPPED
for each enabled validator:                        // (:665)
    timed with FScopedDurationTimer if stats wanted (:671-685)
    Result = Combine(Result, Validator->ValidateLoadedAsset(...))
```

- Consequence 1: **an `IsDataValid` error on an asset hides every validator's findings for that asset.** A seeded defect can be masked by an unrelated class-level error. The test corpus must account for that (one defect per asset, plus a deliberate masking case).
- Consequence 2: per-validator timing exists: `FValidatorStatistics` (`.h:74–89`) has `AssetsValidated`, `AssetsAddedForValidation` and `TotalDurationSeconds`, keyed by validator class in `FValidateAssetsResults::ValidatorStatistics` (`.h:135–136`). The subsystem logs the counts (`.cpp:1102–1108`) but not the durations. A child commandlet can read them from the results struct.

## A5. Batch validation (`ValidateAssetsInternal`)

`.cpp:738–1117`:

- `:753–758` logs "Enabled validators:" and each class path. Useful as evidence of which validators ran in a configuration.
- `:767–821` external objects (World Partition actors) are validated through their owning map.
- `:842–857` cooked packages and excluded directories are skipped.
- `:864` `FValidationAssetRange`: assets in batches of `EditorValidator.AssetValidationBatchSize` (default 50, `:49–53`); optional async preloading `EditorValidator.EnablePreloadingAssets` (default **false**, `:55–59`) and only when load-log capture is off (`:85`).
- `:903–938` **assets that are not loaded are loaded** (`Data.GetAsset()`), with `FScopedLogMessageGatherer` capturing warnings/errors logged during the load when `bCaptureAssetLoadLogs` (default true, `.h:189–191`). Load warnings become a validation warning, load errors make the asset Invalid (`:921–937`).
- `:950–981` logs written during validation are captured as well (`bCaptureLogsDuringValidation`, default true).
- `:998–1047` counters `NumChecked/NumValid/NumInvalid/NumWarnings/NumUnableToValidate`.
- `:1049–1064` per-asset details (errors, warnings, tokenized messages) only if `bCollectPerAssetDetails` (default false).
- `:1091–1095` `PostAssetValidation` is called once on every enabled validator after the loop.
- **Asset loading dominates:** every asset is loaded before any validator sees it, even a validator that only needs asset registry data. A rule cannot make the stock path faster than its loads.

## A6. When validation runs

| Trigger | Code | Notes |
|---|---|---|
| **Save** | `DataValidationModule.cpp:103` hooks `UPackage::PackageSavedWithContextEvent`; `:411–416` → `ValidateSavedPackage` → next tick `ValidateAllSavedPackages` (`EditorValidatorSubsystem.cpp:1182–1236`) → `ValidateOnSave` (`:1141–1180`) | Skipped if `UDataValidationSettings::bValidateOnSave` is false (`DataValidationSettings.h:24–26`, default true), for procedural saves and autosaves (`.cpp:462–478`). Usecase `Save`, **`bLoadAssetsForValidation = false`** (`:1167`): only the saved, already-loaded assets are checked, never their referencers |
| **Menus** | `DataValidationModule.cpp:144–148` Tools → "Validate Data..." (runs `UDataValidationCommandlet::ValidateData` in-editor, `:292–303`); `:217–232` Content Browser "Validate Assets" and "Validate Assets and Dependencies"; `:251–252` "Validate Assets in Folder"; `:200–201` actors | "Dependencies" means the assets the selection **references**, not the assets that reference it |
| **Pre-submit** | `.cpp:338–339` registers with source control; `:1238–1276` `ValidateChangelistPreSubmit` | Uses `bLoadAssetsWhenValidatingChangelists` (default true) |
| **Commandlet** | `DataValidationCommandlet.cpp` | §A8 |
| **Script** | `ValidateAssetsWithSettings`, `IsObjectValid`, `IsAssetValid` are `BlueprintCallable` (`.h:289–365`) | Usecase `Script` |

`EDataValidationUsecase` values: None, Manual, Commandlet, Save, PreSubmit, Script (`Source/Runtime/CoreUObject/Public/Misc/DataValidation.h:19–39`). Validators can behave differently per usecase.

## A7. Changelist (incremental) validation

`.cpp:1335–1450` `ValidateChangelistsInternal`, `:1452–1517` `GatherAssetsToValidateFromChangelist`:

1. `:1350–1355` if the asset registry is still scanning: `Fatal` when unattended ("Unable to perform unattended content validation while asset registry scan is in progress").
2. `:1400–1408` the changelist object itself is validated (`UDataValidationChangelist::IsDataValid`).
3. `:1461–1466` **assets in modified packages**.
4. `:1469–1490` **plus whatever each enabled validator returns from `GetAssetsToValidateFromChangelist`** (counted as `AssetsAddedForValidation`).
5. `:1492–1516` **plus referencers of deleted packages** (`bValidateReferencersOfDeletedAssets`, default true, `.h:205–207`).
6. `:1420–1433` `ShouldValidateAsset` filter, then the normal batch validation.

**Finding:** the stock incremental path re-validates referencers only when a package is **deleted**. If a DataTable loses a row, or a tag disappears from a tags `.ini` (a non-asset file, `ModifiedFiles`), the assets that reference that row or tag are **not** re-validated, unless some validator adds them through `GetAssetsToValidateFromChangelist`. No shipped validator does this for tags or rows (none of the 13 classes in C1 overrides it for that purpose; checked by grep for the override name in the engine).

`UDataValidationChangelist` (`DV/Public/DataValidationChangelist.h:17–50`): public fields `ModifiedPackageNames`, `DeletedPackageNames`, `ModifiedFiles`, `DeletedFiles` (`:42–47`), `Description` (`:49`). *(Line numbers corrected in Phase 5: Phase 2 cited `:62–95` and `:86–92`, which do not exist in the 5.8.4 header.)* `Initialize` overloads take source-control states. Its `IsDataValid` (`DataValidationChangelist.cpp:95–200`) queries the source control provider for every dependency's state, **but returns Valid immediately when `GIsBuildMachine`** (`:97–102`). `GIsBuildMachine` is set by `-BUILDMACHINE` (`Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:6496–6498`). → A pseudo-changelist built from a file list (git diff) can be run through the real engine path on a machine without Perforce if the run is flagged as a build machine. **[inferred; verify in Phase 5]**

## A8. The commandlet

`DV/Public/DataValidationCommandlet.h`, `DV/Private/DataValidationCommandlet.cpp`:

- Class `UDataValidationCommandlet : UCommandlet` (`.h:9–10`). The launcher appends "Commandlet" to the `-run=` token (`LaunchEngineLoop.cpp:2988–2990`), so the command is `UnrealEditor-Cmd.exe <project> -run=DataValidation`. Epic's 5.8 page documents exactly that invocation and no other argument (§C2).
- Arguments, all of them (`.cpp:146–151`): `-AssetType=<class>` (filter by class, recursive), `-IncludeOnlyOnDiskAssets`, `-includeengine` (otherwise `/Engine` and engine plugins are removed, `:185–207`). **No path filter, no changelist mode, no report file.**
- `.cpp:28` `check(GEditor)`: only in an editor executable.
- `.cpp:91, 233–237` waits for the asset registry with `SearchAllAssets(true)`.
- `.cpp:210–215` settings: `bSkipExcludedDirectories`, `bShowIfNoFailures`, usecase `Commandlet`.
- `.cpp:227–231` `ProcessValidationResults` returns **true always**: "Only return false if the commandlet couldn't run the validation and not if there was error in found by the validation". `Main` returns 2 only if validation could not run (`:34–38`). The process exit code is still **1 whenever any error was logged** (`LaunchEngineLoop.cpp:4217–4224`, unless the commandlet sets `UseCommandletResultAsExitCode`), and validation errors are logged (A10). So the exit code says "something logged an error", not "these assets failed".
- 5.8 extension points (`.h:33–42`), all virtual: `ProcessCommandLine(Tokens, Switches, Params)`, `GetAssetsToValidate`, `FilterAssetsToValidate`, `ShouldLoadDefaultEditorModules`, `SetupValidationSettings`, `ValidateAssets`, `ProcessValidationResults`. The subsystem can be told to use a child class for the editor menu (`DataValidationCommandletClassName`, `.h:530–531`; `DataValidationCommandlet.cpp:51–71`).

## A9. Fixers (they exist in 5.8)

- Core interface `UE::DataValidation::IFixer` (`Source/Runtime/CoreUObject/Public/Misc/DataValidation/Fixer.h:20–44`): `GetApplicability(FixIndex)`, `ApplyFix(FixIndex)`, `CreateToken(Label)`, composable with `WrappedIn<T>()`.
- `FFixResult`, `EFixApplicability`, `FFixToken` (`Source/Runtime/Core/Public/Logging/TokenizedMessage.h:769–840`).
- Helpers in `DV/Public/DataValidationFixers.h`: `MakeFix` (lambda fixers, `:51–63`), `FSingleUseFixer` (`:69–78`), `FObjectSetDependentFixer` (`:81–90`), `FAutoSavingFixer` (`:93–101`), `FValidatingFixer` (re-validates touched assets, `:104–112`), `FMutuallyExclusiveFixSet` (`:118–160`).
- Users in the engine: Asset Referencing Restrictions (`AssetValidator_AssetReferenceRestrictions.cpp:94`), `MassTraitRepository.cpp`, `WidgetPreviewToolkit.cpp`.
- **Fixes are applied only from the Message Log UI**: the only `.cpp` that handles `EMessageToken::Fix` is `Source/Developer/MessageLog/Private/UserInterface/SMessageLogMessageListRow.cpp` (grep). The commandlet has no "apply fixes" switch. A fixer is a one-click repair offered to a human.

## A10. How results reach the log (for parsing baseline runs)

- `DV/Private/EditorValidatorHelpers.cpp:19–84` `AddAssetValidationMessages` adds each issue to an `FMessageLog`.
- The log name is `"AssetCheck"` (`Source/Runtime/CoreUObject/Private/Misc/DataValidation.cpp:13`).
- `FMessageLog::Flush` mirrors to the output log unless suppressed (`Source/Runtime/Core/Private/Logging/MessageLog.cpp:271, 323`). The editor's listing model prints each message with `FMsg::Logf` at the severity's verbosity (`Source/Developer/MessageLog/Private/Model/MessageLogListingModel.cpp:57–68`; mapping `MessageLog.cpp:333–336`: Error → Error, Warning → Warning).
- So a stock run's log has lines in category `AssetCheck` at Error/Warning verbosity per finding. Whether each such line increments `GWarn`'s error count (and therefore sets exit code 1) is **[inferred]**, to be checked on the first run with a seeded error.

## A11. Settings and exclusions

- `UDataValidationSettings` (`DV/Public/DataValidationSettings.h:15–43`, `config = Editor`): `bValidateOnSave` (true), `bLoadAssetsWhenValidatingChangelists` (true), material validation switches.
- `UEditorValidatorSubsystem::ExcludedDirectories` (`.h:523–524`, config). `IsPathExcludedFromValidation` (`.cpp:1197–1208`) uses `Path.Contains(ExcludedPath.Path)`: a **substring** test, so excluding `/Game/Test` also excludes `/Game/Tests` and `/Game/Art/TestRig`. A common-mistakes entry.
- `ShouldValidateAsset` (`.cpp:342–371`) skips cooked, temp and Verse packages.
