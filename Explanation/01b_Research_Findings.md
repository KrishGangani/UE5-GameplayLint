# 01b · Research Findings (Phase 2): GameplayLint

**Phase:** 2 (Research), completed 2026-11-21 · **Engine:** UE 5.8.4 (CL 58902052), read from `C:\Program Files\Epic Games\UE_5.8\Engine`

**Method:**
- Four investigations: Data Validation internals, the gameplay-data mechanisms the rules depend on, existing work (engine and community), and measurement/build facts.
- Every engine claim cites a path relative to `Engine/` and a line number in 5.8.4. Shortened prefixes: `DV/` = `Plugins/Editor/DataValidation/Source/DataValidation/`; `EVS.cpp` = `DV/Private/EditorValidatorSubsystem.cpp`.
- The claims that drive design decisions were re-opened at the cited lines before being written here; they are marked ✔.
- Claims drawn from following code flow rather than from a direct statement are marked **[inferred]**. Things to be measured are marked **[to measure]**.

**Why this phase matters.** Phase 1 framed the problem from the roadmap. Phase 2 found that the engine already does more than the roadmap assumed: it ships the incremental hook, fixers, per-validator timing and an extensible commandlet, and it already warns about unknown tags on load. Seven findings changed the design, three changed the benchmark plan, and one narrowed the gap claim. Without this phase, the project would have rebuilt engine features and claimed detection the engine already provides.

---

## 1. What Phase 2 changed

| # | Finding | Consequence for GameplayLint |
|---|---|---|
| 1 | **Every concrete native subclass of `UEditorValidatorBase` in a loaded module is instantiated as a validator automatically**; abstract classes are skipped (`EVS.cpp:381–395`) ✔ | One class per rule, deriving from an **abstract** `UGameplayLintRule`. A rule pack needs no registration code: a new module with new classes is enough. This is what makes "packs plug in without changing the core" true by construction |
| 2 | **Class-level `IsDataValid` runs first; if it returns Invalid, no validator runs for that asset** (`EVS.cpp:657–663`) ✔ | A GameplayLint finding can be **masked** by an unrelated class-level error. The corpus has one defect per asset, plus a deliberate masking set reported separately. Documented in `07` |
| 3 | **Stock incremental validation re-checks referencers only of *deleted* packages**; validators can add more through `GetAssetsToValidateFromChangelist`, and no shipped validator does this for tags or rows (`EVS.cpp:1461–1516`; `DV/Public/EditorValidatorBase.h:37–41`) ✔ | The core feature becomes **dependent gathering through the engine's own hook**, not a separate incremental system. It is tested by mutation scenarios (H1) |
| 4 | **Unknown gameplay tags already produce a load warning in editor builds, and the stock run captures load logs** (`Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp:1076, 1123`; `EVS.cpp:906–937`). On-save validation does not load assets (`EVS.cpp:1167`) ✔ | **The gap for tags is weaker than the roadmap said.** The tag rule is kept, but its claimed value is attribution (rule ID, property path), on-save coverage and incremental coverage, not "detects what nothing detects". SC3 is written so a stock detection of tags counts against the gap claim |
| 5 | **Tag references and DataTable/CurveTable row-handle references are recorded in the asset registry as searchable-name dependencies when saved** (`GameplayTagContainer.cpp:957, 1647`; `Source/Runtime/Engine/Private/DataTable.cpp:1115–1121`; `CurveTable.cpp:1158–1164`). The tag editor queries them (`GameplayTagsEditorModule.cpp:682–690`) ✔ | "Which unchanged assets reference the row I just removed?" can be answered **without loading anything**. That is the precise dependent query (configuration I3). Limit: a handle whose table is null is not recorded (`DataTable.cpp:1117`), so it is found only by the per-asset check |
| 6 | **Every asset is loaded before any validator sees it** (`EVS.cpp:903–938`) ✔ | No rule can make a stock scan faster than its loads. GameplayLint's per-asset cost is measured separately from load time (SC4, SC5). A registry-only "no load" mode is deferred to Phase 8, as an optimisation decided by data |
| 7 | **The 5.8 commandlet's steps are virtual** (`DV/Public/DataValidationCommandlet.h:33–42`), but it has **no path filter, no changelist input and no report**, and `ProcessValidationResults` always succeeds (`DV/Private/DataValidationCommandlet.cpp:146–151, 227–231`) ✔ | A **child commandlet** (`-run=GameplayLint`) adds a path filter, a pseudo-changelist from a file list, a JSON report and an exit-code policy. It reuses the engine's validation path for everything else |
| 8 | **Fixers exist in 5.8** (`Source/Runtime/CoreUObject/Public/Misc/DataValidation/Fixer.h:20–44`; `DV/Public/DataValidationFixers.h`), are used by Asset Referencing Restrictions (`AssetValidator_AssetReferenceRestrictions.cpp:84–96`), and are **applied only from the Message Log UI** (the only handler of `EMessageToken::Fix` is `Source/Developer/MessageLog/Private/UserInterface/SMessageLogMessageListRow.cpp`) | Fixes are offered only where a repair is unambiguous (moving a budget client to the scheduler's tick group). A dangling row or an unknown tag needs a human decision, so no fix is guessed. Fixers are the last build step and optional |
| 9 | **Validators must end with a result once `CanValidateAsset` says yes**, or an `ensure` fires (`DV/Private/EditorValidatorBase.cpp:52–62`); `AssetPasses` after an error also fires one (`:152`) ✔ | The rule base class owns this contract: rule authors report findings, and the base class decides Pass or Fail. Rule code cannot get it wrong |
| 10 | **`bCanEverTick` is not editable on a template, but `bStartWithTickEnabled`, `TickInterval` and `TickGroup` are** (`Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h:195–196, 212–217, 257–258`) ✔. The Kismet compiler sets the actor CDO's `bCanEverTick` from a connected Event Tick (`Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:5525–5613`) | The UpdateBudget rules read **compiled** class defaults and component templates (`AActor::ForEachComponentOfActorClassDefault`, `Actor.cpp:7349–7400`), not graphs. The corpus seeds "actor ticks every frame" through a ticking native parent, because creating Event Tick graph nodes from code isn't needed to exercise the rule |
| 11 | **`FPropertyValueIterator` walks structs, arrays, maps, sets and optionals and gives readable paths, but does not follow object references** (`Source/Runtime/CoreUObject/Public/UObject/UnrealType.h:7505–7703`). *Corrected in Phase 5 (S4):* it **does** unwrap `FInstancedStruct` payloads (`Source/Runtime/CoreUObject/Private/UObject/UnrealType.cpp:57–65`); the first reading missed it | One shared **data walker** in the core visits an asset, its instanced sub-objects, DataTable rows and Blueprint defaults and templates, and hands each rule the values of one struct type with a path. Instanced-struct payloads come for free, so they are a regular corpus variant; the held-out kind is a project's own two-field reference instead (`01` §7.1) |

**Benchmark-plan changes:**

| # | Finding | Consequence |
|---|---|---|
| B-1 | **Per-validator timing exists** (`FValidatorStatistics::TotalDurationSeconds`, `DV/Public/EditorValidatorSubsystem.h:74–89`, filled at `EVS.cpp:671–685`), but the stock commandlet only logs counts | The child commandlet writes per-validator totals into its report. The rule base class adds a per-asset timer for medians and p99 (SC5) |
| B-2 | **The process exit code is 1 whenever any error was logged** (`Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:4217–4224`); validation messages are mirrored to the log under category `AssetCheck` at their severity (`Source/Developer/MessageLog/Private/Model/MessageLogListingModel.cpp:57–68`; log name at `Source/Runtime/CoreUObject/Private/Misc/DataValidation.cpp:13`) | The stock configuration F0 is scored from **per-asset details**, never from its exit code. H5 is tested by looking at that exit code on a warning-only corpus |
| B-3 | **This is editor tooling; there is no frame-time workload.** Timed runs are editor commandlets, pinned to the P-cores (portfolio protocol update) | The portfolio's packaged fixed-step protocol is replaced, for headline numbers, by pinned commandlet runs with the same interleaving and rotation rules (`01` §8). The packaged game is built only to prove isolation |

---

## 2. Data Validation (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| Plugin enabled by default, one Editor module, `PreDefault`; no Beta flag | `Plugins/Editor/DataValidation/DataValidation.uplugin:13, 22–23` ✔ |
| The import library ships with the launcher engine (linking needs no source build) | `Plugins/Editor/DataValidation/Intermediate/Build/Win64/x64/UnrealEditor/Development/DataValidation/UnrealEditor-DataValidation.lib` exists ✔ |
| `UEditorValidatorBase`: `UCLASS(MinimalAPI, Abstract, Blueprintable, ..., Config=Editor)` | `DV/Public/EditorValidatorBase.h:27–28` ✔ |
| 5.8 overrides: `CanValidateAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&)`, `ValidateLoadedAsset_Implementation(same)`; the 5.0–5.3 signatures are deprecated since 5.4 | `EditorValidatorBase.h:51–60, 120–134` ✔ |
| `GetAssetsToValidateFromChangelist` ("referencers of a specific type may be broken by edits to a dependency"), `PostAssetValidation` | `EditorValidatorBase.h:37–44` ✔ |
| `IsEnabled() = bIsEnabled && !bIsConfigDisabled`; `bIsConfigDisabled` is a config property | `EditorValidatorBase.h:95–98, 140–142` ✔ |
| Native validators registered on `OnPostEngineInit`, Blueprint validators after the registry scan | `EVS.cpp:320–335` ✔ |
| Validators live in a `UPROPERTY(Transient)` map, one instance per class, in the transient package | `DV/Public/EditorValidatorSubsystem.h:546–547`; `EVS.cpp:391` ✔ |
| A project subclass of the subsystem replaces it | `EditorValidatorSubsystem.h:253–255`; `EVS.cpp:302–318` |
| Batch loop: loads unloaded assets with load-log capture; captures logs during validation; `PostAssetValidation` once per run | `EVS.cpp:903–981, 1091–1095` ✔ |
| Batching and preloading cvars (`EditorValidator.AssetValidationBatchSize` 50, `EnablePreloadingAssets` false) | `EVS.cpp:49–65` |
| On-save: skipped for procedural saves and autosaves; usecase `Save`; does not load assets | `EVS.cpp:462–478, 1141–1180` ✔ |
| Menus: Validate Data, Validate Assets, Validate Assets and Dependencies (the assets the selection *references*), Validate Assets in Folder | `DV/Private/DataValidationModule.cpp:144–148, 217–252` |
| Changelist path: Fatal if the registry is still scanning and the run is unattended | `EVS.cpp:1350–1355` ✔ |
| `UDataValidationChangelist` has public `ModifiedPackageNames`, `DeletedPackageNames`, `ModifiedFiles`, `DeletedFiles`; its own check returns Valid on a build machine (`-BUILDMACHINE`) | `DV/Public/DataValidationChangelist.h:42–47` (corrected in Phase 5 from `:86–92`); `DV/Private/DataValidationChangelist.cpp:97–102`; `LaunchEngineLoop.cpp:6496–6498` [inferred usable for a pseudo-changelist; verified in Phase 5] |
| Commandlet: `-run=DataValidation`; arguments `-AssetType=`, `-IncludeOnlyOnDiskAssets`, `-includeengine`; `/Engine` filtered out by default | `LaunchEngineLoop.cpp:2988–2990`; `DataValidationCommandlet.cpp:146–151, 185–207` ✔ |
| The editor can be told to use a child commandlet class (`DataValidationCommandletClassName`) | `EditorValidatorSubsystem.h:530–531`; `DataValidationCommandlet.cpp:51–71` |
| `ExcludedDirectories` uses a **substring** match | `EVS.cpp:1197–1208` ✔ |
| Shipped validators: 13 classes; 9 are enabled in this project by default (Phase 4 log: DataValidation ×6, EnhancedInput, Mutable ×2) | grep of the engine headers; `09` F-3 |

**How validation runs, in one picture:**

```text
trigger:  save ───────────────┐  (already-loaded assets only, usecase Save)
          menu / script ──────┤
          pre-submit ─────────┤  (changelist: modified + validators' extra + referencers of deleted)
          commandlet ─────────┘  (all assets, or a class filter)
                │
                ▼
   for each asset:  load if needed (capture load warnings) → IsDataValid → (if not Invalid) every enabled validator
                                                                              └─ GameplayLint rules are validators here
   after the loop:  PostAssetValidation on every validator
```

## 3. Gameplay data mechanisms (evidence)

| Fact | Evidence (5.8.4) |
|---|---|
| `FGameplayTag::IsValid()` is `TagName != NAME_None`; an unknown name passes it | `Source/Runtime/GameplayTags/Classes/GameplayTagContainer.h:138–141` ✔ |
| A dictionary check is `RequestGameplayTag(Name, false)` | `Source/Runtime/GameplayTags/Classes/GameplayTagsManager.h:375` ✔ |
| Load warning for unknown tags, on by default; invalid tags are never cleared (5.5+) | `GameplayTagsManager.cpp:343, 1063–1078, 1098–1127`; `GameplayTagsSettings.cpp:65`; `Classes/GameplayTagsSettings.h:115–117` ✔ |
| Text imports with `PPF_SerializedAsImportText` keep unknown names "to avoid data loss" | `GameplayTagsManager.cpp:1130–1189` (`:1177–1178`) |
| Extra tag files are read from `Config/Tags/` | `GameplayTagsManager.cpp:682–683`; `GameplayTagsSettings.cpp:64` ✔ |
| Tags saved as searchable names (containers always, single tags when not None) | `GameplayTagContainer.cpp:957, 1647` ✔ |
| Row handles saved as searchable names on their table, only when the table is set | `DataTable.cpp:1115–1121`; `CurveTable.cpp:1158–1164` ✔ |
| `FAssetIdentifier(Object, ValueName)` → package, object, value | `Source/Runtime/CoreUObject/Private/AssetRegistry/AssetData.cpp:1386–1395` ✔ |
| `EDependencyCategory::SearchableName = 0x04` | `Source/Runtime/CoreUObject/Public/Misc/AssetRegistryInterface.h:41–46` ✔ |
| `GetDependencies` / `GetReferencers` by identifier and category | `Source/Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h:517–519, 580–582` ✔ |
| DataTable row names are **not** in registry tags (only `RowStructure`) | `DataTable.cpp:347–361` ✔ |
| `UDataTable::IsDataValid` dispatches to each row's `FTableRowBase::IsDataValid` (default NotValidated) | `DataTable.cpp:418–462`; `Classes/Engine/DataTable.h:67` ✔ |
| `FDataTableRowHandle::GetRow` warns only at run time | `DataTable.h:457` |
| GAS validates `FScalableFloat` curve-table rows itself | `Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/ScalableFloat.cpp:22–38` ✔ |
| `UBlueprint::IsDataValid` covers CDO, SCS, inheritable components, templates, timelines | `Source/Runtime/Engine/Private/Blueprint.cpp:2233–2270` ✔ |
| `AActor::ForEachComponentOfActorClassDefault` visits native and inherited BP component templates, with child overrides | `Classes/GameFramework/Actor.h:4249`; `Private/Actor.cpp:7349–7400` ✔ |
| Component tick registered only if `bCanEverTick`, enabled if `bStartWithTickEnabled` | `Private/Components/ActorComponent.cpp:1739–1752` ✔ |
| Native parents can forbid Blueprint ticking with `meta=(ChildCannotTick)` (compile warning) | `KismetCompiler.cpp:5574–5584` |
| UHT has no check against unknown metadata keys (only remapping and duplicate keys) | `Source/Programs/Shared/EpicGames.UHT/Types/UhtMetaData.cs:550, 569` [inferred from absence; verified when `LintMonotonic` first compiles] |
| Asset creation from code: `CreatePackage`, `UPackage::Save`, `FKismetEditorUtilities::CreateBlueprint` / `CompileBlueprint`, `USimpleConstructionScript::CreateNode` / `AddNode` | `UObjectGlobals.h:1225`; `Package.h:1183`; `KismetEditorUtilities.h:110, 169`; `SimpleConstructionScript.h:92, 170` |

## 4. Measurement and tooling (evidence)

| Fact | Evidence |
|---|---|
| Per-validator `AssetsValidated`, `AssetsAddedForValidation`, `TotalDurationSeconds` | `DV/Public/EditorValidatorSubsystem.h:74–89`; `EVS.cpp:671–685, 1486–1487` ✔ |
| Per-asset details only with `bCollectPerAssetDetails` | `EditorValidatorSubsystem.h:166–168`; `EVS.cpp:1049–1064` ✔ |
| End-of-run line "Execution of commandlet took: … PeakPhysMemoryMB=…" | `LaunchEngineLoop.cpp:4232–4251` ✔ |
| Exit code 1 if errors logged unless `UseCommandletResultAsExitCode` | `LaunchEngineLoop.cpp:4217–4224` ✔ |
| `-stdout -FullStdOutLogOutput` put every log line on stdout (the queue's idle watchdog needs output) | `Source/Runtime/Core/Private/Misc/OutputDeviceStdOut.cpp:190–197` ✔ |
| UBT warns when a plugin's module uses another plugin's module without listing that plugin | `Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1564–1579` ✔ |
| P-core mask on this machine | `Tools\core_mask.py` → `FFFF` (run 2026-11-21) ✔ |

## 5. Existing work, updated

| Solution | Scope | Cross-asset meaning? | Incremental victims? | Machine-readable report? | Published detection/FP numbers? | Evidence |
|---|---|---|---|---|---|---|
| **Data Validation** (Epic) | Framework: triggers, discovery, changelist path, fixers, commandlet | Only through rules someone writes | Hook exists; only deleted-package referencers by default | No (log and Message Log) | No | 5.8.4 source ✔; 5.8 docs page |
| Class `IsDataValid` (GAS, Smart Objects, StateTree, DataTable rows, Blueprint) | Inside those classes | Yes, for their own references | No | No | No | source ✔ |
| Gameplay Tags load warning, delete guard | Tags | Yes (on fresh load; UI delete) | No | No | No | source ✔ |
| Asset Referencing Restrictions (Epic, off by default) | Folder/plugin reference domains | Structure only | No | No | No | source ✔ |
| Lyra validators (Epic sample) | Project base validator, Blueprint/load/source-control checks | Not shown | Maps changed headers to assets | No | No | walkthrough (opened) |
| Linter v2 (Gamemakin) | Own framework; naming, Marketplace rules | No | No | **Yes** (JSON/HTML, exit 0/1/2) | No | docs (opened) |
| CommonValidators | Blueprint hygiene, heavy refs | No | No | No | No | README (opened) |
| UnrealDataTableValidation | Row-struct validation (pre-5.8 gap, now in the engine) | No | No | No | No | README (opened) |
| OUUTags | Tag audits, tag-creation rules | Not described | No | No | No | README (opened) |

**The gap, stated conservatively (after Phase 2):**
- The framework is complete; GameplayLint builds rule packs on it.
- Among the public tools reviewed, none applies generic relation rules (tags, row handles, curve contracts) across user-defined data. None re-validates the unchanged victims of a change to a referenced row or tag file, although the engine provides the hook and the registry data. None encodes a project's documented mistakes as data rules. None publishes a detection rate, a false-positive rate or scan costs.
- The tag part of the gap is weaker than the roadmap stated (finding #4), and the write-up says so.
- Studio-internal tools aren't public, and a search can't prove absence.

## 6. Hypothesis status after Phase 2

| # | Hypothesis | Status |
|---|---|---|
| H1 | Victims of a change aren't re-validated | **Supported by code** (finding #3) **[to measure]** with M1–M3 |
| H2 | Generic data has no owner who checks it | **Supported by code** (§3: checks are class-specific) **[to measure]** with F0 per class and variant |
| H3 | The tag warning depends on how the asset was loaded | **Supported by code** (finding #4) **[to measure]** |
| H4 | Project contracts are invisible to generic validation | Unchanged **[to measure]** (expected: F0 finds none of the `GL.UB.*` defects) |
| H5 | The stock CI signal is too coarse to gate on | **Supported by code** (finding #7, B-2) **[to measure]** |
| H6 | Loading dominates validation time | **Supported by code** (finding #6) **[to measure]** |

## 7. Carried into Phase 3

- Rules are validators: an abstract `UGameplayLintRule : UEditorValidatorBase` owns the pass/fail contract, the rule ID, the per-asset timer, the report and the changelist hook. Concrete rules are discovered by the engine.
- One data walker in the core; rules ask it for values of one struct type.
- Dependent gathering through `GetAssetsToValidateFromChangelist`, with a precise mode (searchable names, derived classes) and a coarse mode (all package referencers) for comparison.
- A child commandlet with a path filter, a pseudo-changelist input, a JSON report (findings, per-asset details, per-validator totals, phase timings) and an exit-code policy.
- Fixers only where a repair is unambiguous; optional, last.
- The seeded corpus and its mutations are created by an editor commandlet from a seed, through the queue; scoring is a Python script.

**Raw research notes**, with every quote, are in [Phase2_Raw_Notes/](Phase2_Raw_Notes/README.md) as an audit trail. This file is authoritative: everything the design depends on is restated above with its evidence.
