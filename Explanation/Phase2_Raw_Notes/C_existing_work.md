# C · Existing work: engine features, community tools, talks

Working notes, 2026-11-21. Tags: **[SRC]** read in the 5.8.4 source on this machine · **[DOC]** Epic page opened · **[WEB]** third-party page opened · **(snippet)** search result only. No plugin was installed (survey only, as instructed).

## C1. Validators that ship with UE 5.8.4 [SRC]

`grep "public UEditorValidatorBase"` over `Engine/Source` and `Engine/Plugins` (headers) returns 13 classes:

| Class | Where | What it checks (from its name and header) |
|---|---|---|
| `UEditorValidator_ActionUtility` | DataValidation | Editor action utility Blueprints |
| `UEditorValidator_Localization` | DataValidation | Localisation data |
| `UEditorValidator_Material` | DataValidation | Material compilation per platform (settings in `UDataValidationSettings`) |
| `UDirtyFilesChangelistValidator` | DataValidation | Unsaved files in a changelist |
| `UPackageFileValidator` | DataValidation (private) | Package files |
| `UWorldPartitionChangelistValidator` | DataValidation (private) | World Partition streaming generation errors |
| `UAssetValidator_AssetReferenceRestrictions` | AssetReferenceRestrictions | Folder/plugin reference domains (C2) |
| `UEditorValidator_PluginAssetReferences` | AssetReferenceRestrictions | Plugin-to-plugin asset references |
| `UEditorValidator_MaterialPermutation` | MaterialValidation | Material permutations |
| `UEnhancedInputUserWidgetValidator` | EnhancedInput | Input in user widgets |
| `UAssetValidator_CustomizableObjects`, `_ReferencedCustomizableObjects` | Mutable | Mutable objects |
| `UIllegalPluginDependenciesValidator` | GameFeatures | Game feature plugin dependencies |

Class-level `IsDataValid` overrides relevant to gameplay data [SRC]: `UDataTable` (row dispatch only, B2), `UBlueprint` (CDO, SCS, component templates, B4), GAS (`UGameplayAbility`, `UGameplayEffect` and its components, `FScalableFloat` curve-table rows, B2), `USmartObjectDefinition` (`Plugins/Runtime/SmartObjects/Source/SmartObjectsModule/Private/SmartObjectDefinition.cpp:74–88`, runs its own `Validate`), `UStateTree` ("is not compiled", link errors; `Plugins/Runtime/StateTree/Source/StateTreeModule/Private/StateTree.cpp:437–462`).

**None of them** checks, generically across user-defined types: that a gameplay tag stored in data still exists; that a `FDataTableRowHandle` points at an existing row; that a curve has finite keys or the monotonicity its use requires; or a project-specific contract such as "this actor's per-frame work goes through the update budget". The closest are class-specific: GAS's curve-row check, Smart Object and StateTree self-validation, and the tag editor's refusal to delete a referenced tag (B1). Those are credited in the write-up.

## C2. Asset Referencing Restrictions [SRC]

- `Plugins/Editor/AssetReferenceRestrictions/AssetReferenceRestrictions.uplugin:13` `"EnabledByDefault": false`; depends on DataValidation.
- Settings `UAssetReferencingPolicySettings` (`Public/AssetReferencingPolicySettings.h:179`, `config=Game, defaultconfig`): domains defined by content roots or specific assets (`:150–155`), `CanReferenceTheseDomains` (`:163–164`), engine/project plugin rules (`:190–202`).
- The validator (`Private/AssetValidator_AssetReferenceRestrictions.cpp:28–39, 41–63, 65–100`) works on `FAssetData` and the registry's dependency list (`Subsystem->ValidateAssetReferences(InAssetData, ...)`), adds an error per illegal reference with an asset token, and attaches a **fixer** from `GEditor->MakeAssetReferenceFixer()` (`:84–96`).
- **What it does:** structural rules about *which folders or plugins may reference which*. **What it doesn't:** anything about the *meaning* of a reference (does the row exist, is the tag defined, is the value sensible). It is also the engine's own example of a registry-driven validator with a fixer, which is the pattern GameplayLint follows.

## C3. Epic documentation [DOC]

- "Data Validation in Unreal Engine", page header "Unreal Engine 5.8 Documentation": https://dev.epicgames.com/documentation/en-us/unreal-engine/data-validation-in-unreal-engine?application_version=5.8
  - Validators via `IsDataValid` ("works best for custom classes for your project") or `UEditorValidatorBase` in C++, Blueprint or Python; "ValidateLoadedAsset must call AssetPasses or AssetFails for each of its execution paths"; C++ and Blueprint validators are discovered at editor start, Python ones must call `AddValidator`.
  - Triggers: on save (default), Content Browser menus, Tools → Validate Data, and CI through `UnrealEditor-Cmd.exe <PROJECT_NAME>.uproject -run=DataValidation`. **No other argument is documented**; changelist validation and fixers are not covered on that page.
- API reference index (snippet): https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/DataValidation

## C4. Lyra (Epic sample, not installed on this machine)

- Lyra's editor module has `UEditorValidator` (abstract, derives from `UEditorValidatorBase`) and `UEditorValidator_Blueprints`, `_Load`, `_MaterialFunctions`, `_SourceControl` (snippet; walkthrough https://www.jaydengames.com/posts/ue5-black-magic-editor-validation/ [WEB]). The walkthrough shows a "Check Content" button calling `ValidateCheckedOutContent` and a helper that maps changed C++ headers to affected assets ("Source code header changes for classes may cause issues in assets based on those classes"). It shows **no gameplay-meaning rules** (tags, rows, ability data).
- A project-specific abstract validator base class is exactly what Epic's own header comment suggests (A2). GameplayLint's `UGameplayLintRule` is that idea, not a new one.

## C5. Community tools [WEB unless marked]

| Tool | What it does | What it doesn't do | Source |
|---|---|---|---|
| **Linter v2** (Gamemakin, Michael Allar) | Rule sets as data assets; rules chosen **by asset class** ("most specific" class wins, no cascade); naming conventions, Marketplace guidelines (power-of-two textures, path length, forbidden characters); **commandlet** `-run=Linter -RuleSet=<name> [paths] -json[=file] -html[=file] -TreatWarningsAsErrors`, exit 0 / 1 (lint failed) / 2 (errors found) | Style and structure, not cross-asset gameplay meaning. Docs are written for UE4; the Fab page states no engine version (opened, no version shown) | https://ue4-style-guide.readthedocs.io/en/latest/howitworks.html · https://www.fab.com/listings/10850cb2-be1d-43d2-971d-922c73d218f3 · https://github.com/Allar/ue5-style-guide |
| **CommonValidators** (Flassari; fork nurbed) | Blocking Load, Empty Tick ("only active for Unreal Engine versions older than 5.6"), Heavy Reference ("not working reliable as of now"), Pure Node, Material Texture Sampler Source; run on save | No commandlet or CI mentioned; Blueprint graph hygiene, not data meaning | https://github.com/Flassari/CommonValidators · https://github.com/nurbed/CommonValidators |
| **UnrealDataTableValidation** (Wellwick) | Calls row-struct validation from an editor validator, because "as of UE5.3" tables had no validation | 5.8 now dispatches to `FTableRowBase::IsDataValid` itself (B2). It does not check handles that point into tables | https://github.com/Wellwick/UnrealDataTableValidation |
| **OUUTags** (Jonas Reich) | Tag audits ("find unused tags and most commonly used tags") and "Enforce rules for tag creation" | Not described as checking assets for references to missing tags | https://github.com/JonasReich/OUUTags |
| YAGER DependencyAnalyser | Dependency-size budgets as automation tests | Memory budgets, not gameplay meaning (portfolio research/03 §8) | research/03 |
| Naming plugins on Fab (Asset Naming Assistant and others) | Naming | Naming only (snippet, research/03 §9.2) | research/03 |

The interesting design contrasts:
- **Linter** has its own framework (rule assets, its own commandlet and report, its own exit codes). GameplayLint deliberately does **not** build a parallel framework: it plugs into Data Validation, so on-save, menus, pre-submit and CI all run the same rules.
- **Linter's JSON report and exit codes 0/1/2** are what the stock Data Validation commandlet lacks (A8). GameplayLint's child commandlet adds a report and an exit-code policy in the same spirit, through the 5.8 extension points.

## C6. Talks, postings and the gap in practice (from the portfolio research)

- [T8-A] Routhier (Ubisoft Montréal), "Assassin's Creed Origins: Monitoring and Validation of World Design Data", GDC 2018: in a huge world it is "practically impossible" to validate world data in time by traditional means, made worse by **dependencies between AI, art and design data**. Their answer was "daily automated tests and reports" (research/02 Topic 8; abstract only).
- [T8-D] Embark, Unreal Fest 2022: build stability through alerts, tests and **Data Validation** (snippet).
- Respawn: "automated content validation" (EA2) and a Tools Developer role that must "measure the impact" (EA4) (research/01 §2).
- Research/03 §9.3, written before this phase, rated the framework gap **weak** and semantic cross-asset rules with measured outcomes **medium**. Phase 2 confirms both and sharpens the medium part (01b §6).

## C7. What I looked for and did not find

- A public UE validator that re-validates **referencers of a changed DataTable or tag configuration** in incremental (changelist) validation. The engine has the hook (`GetAssetsToValidateFromChangelist`) and uses it nowhere for this; no community tool found does. A web search cannot prove absence, and studios surely have internal tools. This is a statement about what was reviewed.
- Any published **detection rate** (recall on seeded defects) or **false-positive rate** for a UE validation tool. None found.
- Published scan times for the stock commandlet on N assets. None found. The source gives its per-validator timing struct (A4), so this project measures its own.
