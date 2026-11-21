# 06 · Unreal APIs And Macros: GameplayLint

**Rule:**
- Only APIs this project **actually uses** appear here, each in the same step it enters the code.
- Each entry follows the 8-point format: what it is · why it exists · how it works · when to use it · when not to · common mistakes · a small example · how we use it.
- Verified against UE **5.8.4** source; file references are relative to `Engine/`.

| # | API | First used in |
|---|---|---|
| 1 | `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`, `FDefaultGameModuleImpl` / `FDefaultModuleImpl` | Phase 4 |
| 2 | `ModuleRules`: dependency lists and `PCHUsage` | Phase 4 |
| 3 | `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion`, `ExtraModuleNames` | Phase 4 |
| 4 | Module descriptor: `Type` (host type) and `LoadingPhase` | Phase 4 |
| 5 | Plugin descriptor: `EnabledByDefault`, `CanContainContent` and plugin dependencies | Phase 4 |
| 6 | Commandlets and `-run=` (the stock `DataValidation` commandlet) | Phase 4 (`Tools\validate.bat`) |
| 7 | Reflection specifiers used (`UCLASS`, `USTRUCT`, `UENUM`, `UPROPERTY`, `meta`) and the export rules | S1 |
| 8 | Gameplay tags: `FGameplayTag`, `FGameplayTagContainer`, `UGameplayTagsManager` | S1 |
| 9 | DataTables and CurveTables: `FDataTableRowHandle`, `UDataTable`, `FTableRowBase::IsDataValid`, `FCurveTableRowHandle`, `UCurveTable` | S1 |
| 10 | Curves: `FRuntimeFloatCurve`, `FRichCurve` | S1 |
| 11 | `FInstancedStruct` and `TSoftObjectPtr` | S1 |
| 12 | Creating and saving assets from code | S2 |
| 13 | Creating Blueprints from code: `FKismetEditorUtilities`, `USimpleConstructionScript` | S2 |
| 14 | `FRandomStream` | S2 |
| 15 | `TJsonWriter` and `FFileHelper` (with encodings) | S2 |
| 16 | `UDeveloperSettings` and config | S3 |
| 17 | `UEditorSubsystem` | S3 |
| 18 | `UCommandlet` and `UDataValidationCommandlet` extension points; `FParse` | S2 (seeder), S3 (child commandlet) |
| 19 | `UEditorValidatorBase` from the inside: the 5.8 overrides, `AssetMessage`, `AssetPasses`, `FDataValidationContext`, `UDataValidationChangelist` | S3 (commandlet), S4 (rule base) |
| 20 | `FPropertyValueIterator`, property flags, `CastField`, `FindMetaData` | S4 |
| 21 | `AActor::ForEachComponentOfActorClassDefault` | S4 |
| 22 | Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST` | S4 |
| 23 | Small utilities: `TOptional`, `TFunctionRef`, `StaticEnum`, `LOCTEXT`/`NSLOCTEXT`/`INVTEXT`, `FPlatformTime::Cycles64` | S1–S4 |
| 24 | `FAssetData` (the registry's view of an asset) and `FindObject` by path | S5 |
| 25 | Tick functions and activation: `FTickFunction`, `bAutoActivate`, `ETickingGroup` | S7 |
| 26 | Classes by path (`FSoftClassPath`), Blueprint parents without loading, `StaticEnum` display names | S7 |
| 27 | `TStrongObjectPtr` (keeping a UObject alive from plain C++) | S7 |

---

## 1. `IMPLEMENT_PRIMARY_GAME_MODULE` / `IMPLEMENT_MODULE`

1. **What it is.** Macros that generate a module's entry point (`Source/Runtime/Core/Public/Modules/ModuleManager.h:946–955` for `IMPLEMENT_MODULE`, `:984` for the game variant, `:1100–1136` for the primary variant).
2. **Why it exists.** The engine loads code as **modules**: DLLs in editor builds, static code in packaged builds. Each module needs a standard way to be found and started, so the module manager can call `StartupModule()` and `ShutdownModule()`.
3. **How it works.**
   - In a modular (editor) build it exports an `InitializeModule` function, which the module manager calls after loading the DLL; it returns an instance of the class you name.
   - In a monolithic (packaged) build it registers a static initializer instead.
   - The *primary* variant also defines project-wide globals for packaged builds, such as the project name. Its third argument is named `DEPRECATED_GameName` in 5.8 (`:1100, :1114`), with the comment that manually specifying the game name "is no longer necessary" (`:1096`).
4. **When to use it.** Exactly one per module; exactly one *primary* per project.
5. **When not to.** Never twice in a module (duplicate symbols). A plugin module never uses the primary variant.
6. **Common mistakes.** The module name argument doesn't match `Build.cs` and the descriptor ("module not found" at start-up); writing an empty custom module class when `FDefaultModuleImpl` would do; thinking the third argument still names the game.
7. **Example.**
   ```cpp
   #include "Modules/ModuleManager.h"
   IMPLEMENT_MODULE(FDefaultModuleImpl, MyPluginModule);
   ```
8. **In this project.** `GameplayLintModule.cpp` uses the primary variant with `FDefaultGameModuleImpl` (`ModuleManager.h:892`). The bench module and the three plugin modules use `IMPLEMENT_MODULE(FDefaultModuleImpl, …)` (`:884`). None needs start-up logic, and **the plugin modules never will**: rules are found by reflection (`01b` #1), so there is nothing to register.

## 2. `ModuleRules`: dependencies and `PCHUsage`

1. **What it is.** The C# class in `<Module>.Build.cs` that tells UnrealBuildTool how to compile one module.
2. **Why it exists.** Unreal is hundreds of modules. Each declares what it depends on, so UBT can work out include paths, link order and what to rebuild.
3. **How it works.**
   - `PublicDependencyModuleNames`: their include paths are given to us **and to every module that depends on us**.
   - `PrivateDependencyModuleNames`: only to us.
   - Both link the dependency.
   - `PCHUsage = UseExplicitOrSharedPCHs` uses a shared precompiled header built for a common dependency set, unless the module names its own.
4. **When to use which.** Public only if one of **your public headers** includes that module's headers. Everything else is private.
5. **When not to.** Don't add modules "just in case": every dependency costs build time and couples modules.
6. **Common mistakes.**
   - Making everything public: include paths spread and rebuilds get wider.
   - A missing dependency: `cannot open include file` or `unresolved external symbol`.
   - Depending on another plugin's module without listing that plugin in your `.uplugin` (§5).
   - A circular dependency between modules (UBT reports it).
7. **Example.**
   ```csharp
   PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DataValidation" });
   PrivateDependencyModuleNames.Add("GameplayLintCore");
   ```
8. **In this project.**
   - Core plugin: **public** `DataValidation`, because its public rule base class derives from `UEditorValidatorBase` and every pack includes that header.
   - Rule packs and the bench module: **only private** dependencies; they have no public headers (`02` §3).
   - Game module: public `Core`, `CoreUObject`, `Engine` for its public fixture headers (S1); **no** lint dependency, ever.

## 3. `TargetRules`: `Type`, `DefaultBuildSettings`, `IncludeOrderVersion`, `ExtraModuleNames`

1. **What it is.** The C# class in `<Name>.Target.cs` that describes one executable: game, editor, client, server or program.
2. **Why it exists.** The same modules build into different executables with different settings and module sets.
3. **How it works.** UBT finds the class `<Name>Target` and builds its `ExtraModuleNames`, the engine, and the enabled plugins' modules that are allowed in this target type (§4). `DefaultBuildSettings` picks a versioned set of build defaults; `IncludeOrderVersion` picks an engine version's header include order.
4. **When to use it.** One file per executable type. Here: `GameplayLint` (game) and `GameplayLintEditor` (editor).
5. **When not to.** No `Client`/`Server` targets: nothing here is networked.
6. **Common mistakes.** A class name that doesn't follow `<Name>Target`; an old `IncludeOrderVersion` after an engine upgrade; expecting `Test` to build on a launcher engine (it doesn't, UE5-UpdateBudget Phase 4).
7. **Example.** See `Source\GameplayLint.Target.cs` (`05` A2).
8. **In this project.** `V7` and `Unreal5_8`, the same values as Epic's 5.8 template. The game target lists only `GameplayLint`; the editor target also lists `GameplayLintBench`. The Phase 4 game build compiled only `GameplayLintModule.cpp` from the project (`09` F-2), which is the isolation this split is for.

## 4. Module descriptor: `Type` (host type) and `LoadingPhase`

1. **What it is.** Two fields on each module entry in a `.uproject` or `.uplugin`. `Type` says **which kinds of program may load the module**; `LoadingPhase` says **when** during start-up.
2. **Why it exists.** Editor code must never reach a shipped game, and some modules must exist before others start.
3. **How it works.** `EHostType` (`Source/Runtime/Projects/Public/ModuleDescriptor.h:82–118`):
   - `Runtime`: "Loads on all targets, except programs" (`:86–87`);
   - `Editor`: "Loads only when the editor is starting up" (`:109–110`);
   - `EditorNoCommandlet`: "… but not in commandlet mode" (`:112–113`). The existence of this separate type is why plain `Editor` modules **do** load in commandlets;
   - `UncookedOnly`, `DeveloperTool` and others for special cases (`:98–107`).

   UBT also uses the type to decide which modules to compile into a target. `LoadingPhase` values run from `PostConfigInit` through `PreDefault`, `Default`, `PostDefault` and later. A module that another loaded module depends on loads first regardless.
4. **When to use which.** `Runtime` for game code and data types; `Editor` for anything that uses editor modules (`UnrealEd`, `DataValidation`); `Default` loading unless something must exist earlier.
5. **When not to.** Not `Runtime` for code that links editor modules (the game target fails to link). Not an early phase "to be safe": early modules can't use systems that aren't up yet.
6. **Common mistakes.** Putting a validator or commandlet in a `Runtime` module; picking `EditorNoCommandlet` for a module whose commandlet must run in CI; the deprecated `Developer` type (`:101–104` explains its ambiguity).
7. **Example.** `{ "Name": "GameplayLintCore", "Type": "Editor", "LoadingPhase": "Default" }`
8. **In this project.** `GameplayLint` is `Runtime`; `GameplayLintBench` and all three plugin modules are `Editor`. DataValidation itself is `Editor` with `PreDefault` (`DataValidation.uplugin:22–23`), so it is loaded before our `Default` modules. The Phase 4 commandlet log mounts all three plugins (`09` F-3).

## 5. Plugin descriptor: `EnabledByDefault`, `CanContainContent`, plugin dependencies

1. **What it is.** Fields of a `.uplugin` file (`Source/Runtime/Projects/Public/PluginDescriptor.h:124–139` for the C++ side), plus its `"Plugins"` array of plugin references (`PluginReferenceDescriptor.h:26–35`: name, `bEnabled`, `bOptional`).
2. **Why it exists.** A plugin is a unit of features a project can switch on or off, copy between projects, and depend on.
3. **How it works.**
   - `EnabledByDefault` decides whether a project that doesn't mention the plugin gets it.
   - `CanContainContent` mounts a content folder.
   - The `"Plugins"` array enables the listed plugins along with this one. UBT **warns** when a plugin's module depends on another plugin's module that the plugin doesn't list: "Plugin '…' does not list plugin '…' as a dependency, but module '…' depends on module '…'." (`Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1564–1579`).
4. **When to use it.** List every plugin whose modules your modules use.
5. **When not to.** Don't list plugins you don't use; each listed one is force-enabled in every project that uses yours.
6. **Common mistakes.** A missing plugin reference: the build "works" with a warning, then fails in a project where that plugin is disabled. `CanContainContent: true` on a code-only plugin (an empty mount point).
7. **Example.**
   ```json
   "Plugins": [ { "Name": "GameplayLintCore", "Enabled": true } ]
   ```
8. **In this project.** Core lists `DataValidation`; each pack lists `GameplayLintCore`. All three are `EnabledByDefault: true` and `CanContainContent: false`. The 0-warning Phase 4 build is the evidence that the references are complete. The `.uproject` uses the same reference format to **turn off** an engine plugin that is on by default: `{ "Name": "AndroidFileServer", "Enabled": false }`, because its editor module writes a generated token into the project config (`05` A1).

## 6. Commandlets and `-run=`

1. **What it is.** A commandlet is a `UCommandlet` subclass whose `Main(const FString& Params)` runs instead of the normal editor loop: the engine starts, runs `Main`, and exits with its return code.
2. **Why it exists.** Batch work (cooking, resaving, validating) needs the editor's code and asset access without a window, which is what CI needs.
3. **How it works.** `UnrealEditor-Cmd.exe <project> -run=<Name>` finds the class `<Name>Commandlet`; the launcher appends "Commandlet" to the token (`Source/Runtime/Launch/Private/LaunchEngineLoop.cpp:2988–2990`). After `Main` returns, the process exit code is `Main`'s value, **turned into 1 if any error was logged** and the commandlet didn't set `UseCommandletResultAsExitCode` (`:4217–4224`). The run ends with "Execution of commandlet took: … seconds, PeakPhysMemoryMB=…" (`:4232`).
4. **When to use it.** Automated, repeatable editor work: here, validation in CI and creating the test corpus.
5. **When not to.** Anything a user does interactively; or anything that must not load editor modules.
6. **Common mistakes.**
   - Reading the exit code as "the validation result": the stock `DataValidation` commandlet returns success even when assets fail, and 1 when anything at all logged an error (`01b` #7, B-2).
   - Forgetting `-unattended`, so a dialog waits forever.
   - Running it outside the UE job queue on this machine.
7. **Example.** `UnrealEditor-Cmd.exe GameplayLint.uproject -run=DataValidation -unattended -nullrhi -stdout`
8. **In this project.** `Tools\validate.bat` runs the stock commandlet (`09` F-3: 9 validators enabled, 1 object checked, 0 errors, 0 warnings). S2 adds `-run=GameplayLintSeed` and S3 `-run=GameplayLint`, a child of the stock commandlet.

## 7. Reflection specifiers used, and the export rules

1. **What it is.** `UCLASS`, `USTRUCT`, `UENUM` and `UPROPERTY` mark types and fields for UHT, which generates the reflection data the engine uses to save, load, edit, garbage-collect and (here) walk them.
2. **Why it exists.** C++ has no runtime type information about fields. Everything a validator checks generically is found through this data.
3. **How it works (the specifiers this project uses).**
   - `MinimalAPI`: export only the class's type information (`StaticClass`, casts), not its functions.
   - `Abstract`: no instance, and the validator subsystem skips it.
   - `Blueprintable`: Blueprints may derive from it.
   - `EditInlineNew` / `DefaultToInstanced` / `Instanced`: an object property owns its own copy of the object, created in place and saved in the owner's package (`CPF_InstancedReference`).
   - `CollapseCategories`: a flat inline editor.
   - `config = Editor, defaultconfig`: the class's `config` properties load from, and save to, the project's `DefaultEditor.ini`.
   - `EditAnywhere`: editable on instances and defaults. `Category`: the details panel group.
   - `meta = (…)`: free-form key/value metadata, available in editor builds (`WITH_METADATA`). Used here: `BaseStruct`, `BlueprintSpawnableComponent`, `DisplayName`, `MetaClass`, and the project's own `LintRequired`, `LintMonotonic`.
4. **When to use it.** Anything that must be saved, edited, garbage-collected or found by reflection.
5. **When not to.** Plain bookkeeping that nothing else reads (the settings' command-line overrides, the report's statistics struct).
6. **Common mistakes.**
   - Forgetting `UPROPERTY`: the field isn't saved and a generic rule can't see it.
   - Putting `UE_API` on every member "to be safe": it exports what nobody calls.
   - Thinking a struct needs `MODULE_API` for `StaticStruct()`: in 5.8 UHT exports the struct's constructor function itself (`Intermediate/Build/Win64/UnrealEditor/Inc/GameplayTags/UHT/GameplayTagContainer.generated.h:23`, the generated header of the engine's own build).
7. **Example.** `UPROPERTY(EditAnywhere, Category = "Fixture", meta = (LintMonotonic = "Increasing")) FRuntimeFloatCurve CooldownByLevel;`
8. **In this project.** The custom metadata keys compiled with 0 warnings (S1), so UHT does not reject unknown keys (`01b` §3). Fixture classes are `MinimalAPI` because the bench module and tests use their `StaticClass`.

## 8. Gameplay tags

1. **What it is.** `FGameplayTag` (one hierarchical name), `FGameplayTagContainer` (a set of tags) and `UGameplayTagsManager` (the dictionary, loaded from config and native declarations).
2. **Why it exists.** Designers query state by name ("Status.Stunned") without hard-coded enums.
3. **How it works.**
   - A tag stores an `FName`. `IsValid()` is only `TagName != None` (`GameplayTagContainer.h:138–141`).
   - `UGameplayTagsManager::RequestGameplayTag(Name, ErrorIfNotFound)` (`GameplayTagsManager.h:375`) asks the dictionary.
   - `FromExportString(Text, PPF_SerializedAsImportText)` imports a name and keeps unknown ones (`GameplayTagsManager.cpp:1177–1178`).
   - `AddTagFast` adds to a container without the parent-tag bookkeeping.
4. **When to use it.** Anything that should be queryable by name across systems.
5. **When not to.** As a free-form string store: tags must be declared.
6. **Common mistakes.** Treating `IsValid()` as "exists"; editing tag `.ini` files by hand; expecting a load warning on save (`01b` #4); writing `+GameplayTagList=` in a `Config/Tags/*.ini` file. Such a file is read on its own, without ini symbol commands, so the `+` becomes part of the key and no tag loads (this project's first baseline, `07`).
7. **Example.** `if (!UGameplayTagsManager::Get().RequestGameplayTag(Tag.GetTagName(), false).IsValid()) { /* unknown */ }`
8. **In this project.** The seeder makes every corpus tag with `FromExportString` (none is in the dictionary while seeding) and writes `Config/Tags/GLSeed*.ini`. The tests use the same path.

## 9. DataTables and CurveTables

1. **What it is.**
   - `UDataTable`: rows of one `FTableRowBase`-derived struct, keyed by `FName`.
   - `FDataTableRowHandle`: a table pointer plus a row name.
   - `UCurveTable` and `FCurveTableRowHandle`: the same for curves.
2. **Why it exists.** Spreadsheet-style gameplay data that designers edit and import.
3. **How it works.**
   - `RowStruct` (public, `DataTable.h:89–91`) is the row type. `AddRow(Name, Row)`, `RemoveRow(Name)` (`:312–316`), `FindRow<T>(Name, Context)` (`:219`) and `GetRowMap()` edit and read rows.
   - `FTableRowBase::IsDataValid` (editor only, `:60–68`) is called per row by `UDataTable::IsDataValid`.
   - Handles record their row as a searchable name when saved (`DataTable.cpp:1115–1121`).
   - `UCurveTable::AddRichCurve(Name)` (`CurveTable.h:61`) adds a row.
4. **When to use it.** Tabular data with stable keys.
5. **When not to.** Data with deep structure per entry (a data asset fits better).
6. **Common mistakes.**
   - Renaming a row: every handle that names it breaks, and nothing re-checks them.
   - A handle with a row but no table.
   - Forgetting the `WITH_EDITOR` guard around an `IsDataValid` override, which breaks the game build.
7. **Example.** `Table->RowStruct = FGLFixtureRow::StaticStruct(); Table->AddRow(TEXT("R00"), Row);`
8. **In this project.**
   - The seeder builds three shared target tables, a curve table and the corpus tables.
   - M1 and M4 edit them.
   - `FGLFixtureRow::IsDataValid` makes the masking set.

## 10. Curves: `FRuntimeFloatCurve`, `FRichCurve`

1. **What it is.** `FRuntimeFloatCurve` holds either its own `FRichCurve EditorCurveData` or a pointer to a `UCurveFloat` (`ExternalCurve`) (`CurveFloat.h:16–20`).
2. **Why it exists.** Designer-tuned functions of one variable (damage by distance, cooldown by level).
3. **How it works.** `AddKey(Time, Value)` (`RichCurve.h:232`); `GetRichCurveConst()` returns the external curve if set, else the embedded one (`CurveFloat.cpp:28–36`); `GetConstRefOfKeys()` (`RichCurve.h:209`).
4. **When to use it.** Tunable relationships that designers own.
5. **When not to.** A fixed formula that never changes.
6. **Common mistakes.** NaN or infinite keys from a bad import; a curve whose user expects it to rise but which falls; ignoring `ExternalCurve` when checking keys.
7. **Example.** `Curve.EditorCurveData.AddKey(1.f, 2.f);`
8. **In this project.** The seeder writes NaN and infinite keys, reversals and empty curves. S6's rule reads keys through `GetRichCurveConst()`.
9. **Added in S6.**
   - `FRealCurve::DefaultValue` (`RealCurve.h:226`) is what an empty curve returns. `HasAnyData()` (`:140–143`) means "a key, or a default value other than `MAX_flt`".
   - `FRichCurveKey` carries `ArriveTangent`, `LeaveTangent` and their weights (`RichCurve.h:106–118`). Cubic interpolation reads them, so a NaN tangent poisons the values between keys.
   - `FRichCurve::GetKey(Handle)` (`:215`) returns a key by the handle `AddKey` gave.
   - `FSimpleCurve` has keys with only `Time` and `Value` (`SimpleCurve.h:11–21`, `GetConstRefOfKeys` `:85`).
   - `UCurveFloat::FloatCurve` (`CurveFloat.h:36`) is a curve asset's own data.
   - A `UCurveTable` holds either rich or simple rows: `GetCurveTableMode()`, `GetRichCurveRowMap()`, `GetSimpleCurveRowMap()` (`CurveTable.h:51–57`).
   - `FMath::IsFinite` (`GenericPlatformMath.h:583`) is false for NaN and both infinities.

## 11. `FInstancedStruct` and `TSoftObjectPtr`

1. **What it is.**
   - `FInstancedStruct` (CoreUObject `StructUtils`, `InstancedStruct.h`): a value of any struct type chosen at run time.
   - `TSoftObjectPtr<T>`: a reference by path that does not load its target.
2. **Why it exists.** Polymorphic data without UObjects, and references that don't pull whole asset graphs into memory.
3. **How it works.**
   - `FInstancedStruct::Make(Value)` (`:135`) copies a value in.
   - `FPropertyValueIterator` unwraps its payload (`UnrealType.cpp:57–65`).
   - A soft pointer stores an `FSoftObjectPath`.
4. **When to use it.** Variant data (`FInstancedStruct`); optional or large references (`TSoftObjectPtr`).
5. **When not to.** When one fixed struct type would do; when the target must always be loaded.
6. **Common mistakes.** Assuming generic tools can't see inside an instanced struct (this project's own Phase 2 mistake, corrected in S4); building a project-specific reference out of a soft pointer and a name, which no generic rule can recognise.
7. **Example.** `Asset->Extra = FInstancedStruct::Make(Payload);`
8. **In this project.** `Extra` became a regular corpus variant. `LootTable` + `LootRow` is the held-out kind.

## 12. Creating and saving assets from code

1. **What it is.**
   - `CreatePackage(Name)` (`UObjectGlobals.h:1225`) and `NewObject<T>(Package, Name, RF_Public | RF_Standalone)`.
   - `UPackage::SavePackage(Package, Asset, Filename, FSavePackageArgs)` (`Package.h:1204`).
   - `FPackageName::LongPackageNameToFilename` (`PackageName.h:167`) with `GetAssetPackageExtension()`.
   - `FAssetRegistryModule::AssetCreated` (`AssetRegistryModule.h:61`).
2. **Why it exists.** Tools, importers and commandlets make assets without the editor UI.
3. **How it works.**
   - A package is the unit saved to one `.uasset` file.
   - `RF_Public` lets other packages reference the object; `RF_Standalone` keeps it alive while unreferenced.
   - `AssetCreated` tells the running registry about the new asset.
4. **When to use it.** Reproducible generation: test corpora, bulk tools.
5. **When not to.** One-off edits a person should make and review.
6. **Common mistakes.** Forgetting `RF_Standalone` (the asset is collected before it is saved); saving to a path outside a mounted content root; relying on the registry of the *same* process to see a deleted file.
7. **Example.** See `SaveAsset` (`05` C2).
8. **In this project.** Every corpus asset is created and saved this way, inside the queue.

## 13. Creating Blueprints from code

1. **What it is.**
   - `FKismetEditorUtilities::CreateBlueprint(Parent, Outer, Name, BPTYPE_Normal, Context)` and `CompileBlueprint(Blueprint)` (`KismetEditorUtilities.h:110, 169`).
   - `USimpleConstructionScript::CreateNode(Class, Name)` / `AddNode(Node)` (`SimpleConstructionScript.h:170, 92`).
   - `USCS_Node::ComponentTemplate` (`SCS_Node.h:30`).
2. **Why it exists.** Blueprint assets need their generated class, class default object and construction script set up consistently; these helpers do what the editor does.
3. **How it works.**
   - A Blueprint compiles into a generated class (`UBlueprint::GeneratedClass`) with a class default object.
   - Components added in the Blueprint editor are SCS nodes whose templates hold the defaults.
   - The compiler sets `bCanEverTick` from a connected Event Tick (`KismetCompiler.cpp:5525–5613`).
4. **When to use it.** Generating Blueprint fixtures or bulk-editing Blueprints in tools.
5. **When not to.** In runtime code (these are editor modules).
6. **Common mistakes.**
   - Setting class-default values before compiling: the compile creates the class those values must live in.
   - Reading SCS nodes directly instead of `ForEachComponentOfActorClassDefault`, which misses inherited overrides.
7. **Example.** `UActorComponent* Template = AddComponent(Blueprint, UGLFixtureTicker::StaticClass(), TEXT("Ticker"));`
8. **In this project.** Actor and component Blueprints of the corpus, their shared parents, and M3's edit.

## 14. `FRandomStream`

1. **What it is.** A seeded pseudo-random generator (`Math/RandomStream.h`).
2. **Why it exists.** Reproducible randomness: the same seed gives the same sequence.
3. **How it works.** `FRand()` gives [0, 1); `RandRange(Min, Max)` gives an inclusive integer.
4. **When to use it.** Generated content, tests and benchmarks that must repeat.
5. **When not to.** Security or anything that must not be predictable.
6. **Common mistakes.** Mixing in `FMath::Rand` (global state); changing the order of calls, which changes every later value.
7. **Example.** `FRandomStream Random(Seed); const bool bDefect = Random.FRand() < 0.2f;`
8. **In this project.** One stream drives the whole corpus, in index order: criterion E2.

## 15. `TJsonWriter` and `FFileHelper`

1. **What it is.**
   - `TJsonWriterFactory<TCHAR, Policy>::Create(&String)` (`JsonWriter.h:778–788`): a streaming writer (`WriteObjectStart`, `WriteValue`, `WriteArrayStart`, `Close`).
   - `FFileHelper::SaveStringToFile` / `SaveStringArrayToFile` / `LoadFileToStringArray` (`FileHelper.h:196, 202`).
2. **Why it exists.** Machine-readable output for other tools.
3. **How it works.**
   - The writer appends text as you go: no object tree is built in memory.
   - The save helpers write a string with a chosen `EEncodingOptions` (`FileHelper.h:40`).
4. **When to use it.** Reports and manifests read by scripts.
5. **When not to.** Data the engine itself reloads (use config or assets).
6. **Common mistakes.** The default encoding `AutoDetect` writes **UTF-16** as soon as a string has a non-ANSI character, which a UTF-8 reader can't parse. Always pass `ForceUTF8WithoutBOM` for files that scripts read (review finding, S1–S4).
7. **Example.** `FFileHelper::SaveStringToFile(Json, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);`
8. **In this project.** The ground-truth manifest, the change list, the tag files and the commandlet's report.

## 16. `UDeveloperSettings` and config

1. **What it is.** A base class whose `config` properties appear on a Project Settings page (`Engine/DeveloperSettings.h`).
2. **Why it exists.** One editable, saved, versioned place for a system's options.
3. **How it works.**
   - `config = Editor` reads the `[/Script/<Module>.<Class>]` section of the editor config hierarchy; `defaultconfig` saves to `Default<Config>.ini`.
   - The class default object holds the values; `GetDefault<T>()` reads them.
   - `GetCategoryName()` picks the settings group.
4. **When to use it.** Project-wide tool or system options.
5. **When not to.** Per-asset data (put it on the asset) or values that change every frame.
6. **Common mistakes.**
   - Writing command-line overrides into config properties: they would be saved.
   - Expecting `+Array=` in an ini to append to constructor defaults; when the key exists in config, the config array replaces them.
7. **Example.** `GetDefault<UGameplayLintSettings>()->IsRuleEnabled(RuleId)`
8. **In this project.** `UGameplayLintSettings` (core) and, from S7, the UpdateBudget pack's settings.

## 17. `UEditorSubsystem`

1. **What it is.** An engine-managed singleton that lives as long as the editor (`Editor/EditorSubsystem/Public/EditorSubsystem.h:20`).
2. **Why it exists.** Editor-wide services without global variables.
3. **How it works.** `GEditor->GetEditorSubsystem<T>()` returns the instance. It exists in commandlets too: the Data Validation subsystem is one.
4. **When to use it.** State shared by editor tools for the whole session.
5. **When not to.** Per-world state (`UWorldSubsystem`), or anything a packaged game needs.
6. **Common mistakes.** Using it from runtime modules; keeping `UObject*` to assets that may unload.
7. **Example.** `GEditor->GetEditorSubsystem<UGameplayLintReportSubsystem>()->Begin();`
8. **In this project.** The report subsystem: rules record into it while the commandlet has a report open.

## 18. `UCommandlet` and `UDataValidationCommandlet` extension points; `FParse`

1. **What it is.**
   - `UCommandlet::Main(Params)` and its flags (`Commandlet.h:66–96`): `LogToConsole`, `UseCommandletResultAsExitCode`.
   - The stock validation commandlet's virtual steps (`DataValidationCommandlet.h:33–42`).
   - `FParse::Value(Stream, Key, Out, bShouldStopOnSeparator)` and `FParse::Param`.
2. **Why it exists.** Batch work with full editor access.
3. **How it works.**
   - The stock `Main` runs `ProcessCommandLine`, `GetAssetsToValidate`, `FilterAssetsToValidate`, `SetupValidationSettings`, `ValidateAssets`, `ProcessValidationResults`, and returns 0, or 2 if one step reported failure.
   - With `UseCommandletResultAsExitCode`, logged errors don't override that.
   - `FParse::Value` stops an unquoted value at `,`, `)` or whitespace unless told not to (`Parse.cpp:298`).
4. **When to use it.** CI entry points; customising validation without copying the engine's code.
5. **When not to.** Interactive tools.
6. **Common mistakes.** Parsing a comma-separated list with the default `FParse::Value` (review finding); returning success when an input file was unreadable (review finding).
7. **Example.** `FParse::Value(*CommandLine, TEXT("Paths="), PathList, /*bShouldStopOnSeparator*/ false);`
8. **In this project.** `UGameplayLintCommandlet` overrides five steps and `Main`; `UGameplayLintSeedCommandlet` is a plain commandlet.

## 19. `UEditorValidatorBase` from the inside

1. **What it is.** The validator base class (`DataValidation/Public/EditorValidatorBase.h:27–157`).
   - The 5.8 overrides: `CanValidateAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&)` and `ValidateLoadedAsset_Implementation(…)`.
   - Helpers: `AssetMessage`, `AssetPasses`, `AssetFails`.
   - `GetAssetsToValidateFromChangelist`.
   - `FDataValidationContext` (`Misc/DataValidation.h:44–137`).
   - `UDataValidationChangelist` (`DataValidationChangelist.h:17–50`; public lists `:42–47`) with `UEditorValidatorSubsystem::ValidateChangelist`.
2. **Why it exists.** One way for every check to plug into save, menus, pre-submit and CI.
3. **How it works.**
   - The public `ValidateLoadedAsset` resets state, runs the Blueprint, legacy and native paths, and moves messages into the context (`EditorValidatorBase.cpp:16–85`).
   - `AssetMessage` with Error makes the result Invalid.
   - `AddMessage` counts errors and warnings by severity (`DataValidation.cpp:44–59`).
4. **When to use it.** Checks on engine or cross-cutting types; `IsDataValid` for checks a class owns about itself.
5. **When not to.** Checks that need the editor's interactive state.
6. **Common mistakes.** The deprecated 5.0–5.3 signatures; ending without a result; calling `AssetPasses` after an error; a module that subclasses it without linking `DataValidation` (the link rule, `02` §3).
7. **Example.** See `UGameplayLintRule::ValidateLoadedAsset_Implementation` (`05` E1).
8. **In this project.** The rule base implements the overrides once; the commandlet builds a changelist from a file and calls `ValidateChangelist`; the contract test calls `ValidateLoadedAsset` directly.

## 20. `FPropertyValueIterator`, property flags, `CastField`, `FindMetaData`

1. **What it is.** A recursive iterator over a struct's property values (`UnrealType.h:7512–7703`), with the `FProperty` flags and casts.
2. **Why it exists.** Generic code (serialisers, tools) that must visit every value of a type without knowing the type.
3. **How it works.**
   - Constructed with a property class (`FProperty::StaticClass()` matches everything, `UnrealType.cpp:24`), a struct and its memory.
   - It descends into structs, arrays, sets, maps, optionals and instanced-struct payloads (`UnrealType.cpp:36–65`).
   - `Key()` is the property, `Value()` the memory, `SkipRecursiveProperty()` stops the descent, and `GetPropertyPathDebugString()` gives "List[2].Tag".
   - `HasAnyPropertyFlags(CPF_Transient | CPF_InstancedReference)`, `CastField<FStructProperty>`, `FindMetaData(Key)` (`Field.h:938`).
4. **When to use it.** Tools that inspect arbitrary data.
5. **When not to.** Hot runtime paths (reflection is slower than direct access).
6. **Common mistakes.** Not skipping transient (derived) data; expecting it to follow object references (it doesn't); reading metadata from an array's inner property instead of the array.
7. **Example.** See `Walk` (`05` E2).
8. **In this project.** The walker, shared by every data rule.

## 21. `AActor::ForEachComponentOfActorClassDefault`

1. **What it is.** A static helper that visits every component template of an actor class: native ones, Blueprint-added ones and inherited ones, with child overrides applied (`Actor.h:4249`; `Actor.cpp:7349–7400`).
2. **Why it exists.** A class default object's `GetComponents()` misses Blueprint-added components.
3. **How it works.** Native components from the CDO, then each Blueprint class in the hierarchy's SCS nodes through `GetActualComponentTemplate`.
4. **When to use it.** Inspecting what an actor class will spawn with, without spawning it.
5. **When not to.** On spawned actors (use their components).
6. **Common mistakes.** Walking only the most-derived Blueprint's SCS (inherited components missed).
7. **Example.** `AActor::ForEachComponentOfActorClassDefault(Class, UActorComponent::StaticClass(), [](const UActorComponent* T) { return true; });`
8. **In this project.** The walker's Blueprint branch; from S7, the UpdateBudget rules.

## 22. Automation tests: `IMPLEMENT_SIMPLE_AUTOMATION_TEST`

1. **What it is.** A macro that declares a test class with a name and flags; `RunTest` holds the body (`Misc/AutomationTest.h`).
2. **Why it exists.** Repeatable checks the engine can run headless and report.
3. **How it works.**
   - `EAutomationTestFlags` set the context (`EditorContext`) and exactly one filter (`ProductFilter`).
   - `TestEqual` / `TestTrue` / `AddInfo` record results.
   - `Automation RunTests <prefix>` runs every matching test.
4. **When to use it.** Unit-style checks of C++ logic.
5. **When not to.** Measurements (use a benchmark run).
6. **Common mistakes.** More than one filter flag; tests that depend on assets on disk; a test-only validator that the engine then runs in every real scan (here: `IsEnabled()` returns false).
7. **Example.** See `GameplayLintCoreTests.cpp` (`05` E3).
8. **In this project.** Four tests under `GameplayLint.*`, run by `Tools\run_tests.bat`.

## 23. Small utilities

| API | What it does here |
|---|---|
| `TOptional<T>` | "Not given" for the command-line overrides, without a magic value; `Get(Default)` |
| `TFunctionRef<Sig>` | A non-owning callable for the walker's `Visit` and the seeder's lambdas: no allocation; it must not outlive the call |
| `StaticEnum<E>()->GetValueByNameString` / `GetNameStringByValue` | Parse and print `EGameplayLintDependents` from its reflection data (case-insensitive lookup by default, `Class.h:2857–2869`) |
| `LOCTEXT` / `NSLOCTEXT` / `INVTEXT`, `FText::Format` | Localisable messages with a namespace (file-wide `LOCTEXT_NAMESPACE`, or explicit), and an invariant text for test data |
| `FPlatformTime::Cycles64` / `ToMilliseconds64` | Per-asset rule timing with the high-resolution counter |
| `FRandomStream`, `std::numeric_limits<float>::quiet_NaN()` / `infinity()` | Reproducible choices; the non-finite curve keys |

## 24. `FAssetData` (the registry's view of an asset) and `FindObject` by path

1. **What it is.**
   - `FAssetData` (`CoreUObject/Public/AssetRegistry/AssetData.h:163`): what the asset registry knows about an asset without loading it. That is its `PackageName` and `AssetName` (`:190, 196`), its class, and its tags.
   - `IsRedirector()` (`:452`), `IsInstanceOf(Class)` (`:472`), `GetTagValue(Tag, Out)` (`:645`), and `GetAsset()` (`:573`), which loads it.
   - `FindObject<T>(Outer, Name)` (`UObjectGlobals.h:2075`): finds an object that is already in memory.
2. **Why it exists.** Tools must decide what to look at before paying for a load. And code that may not include a class's header can still find the class by its path.
3. **How it works.**
   - The registry fills `FAssetData` from the package headers on disk.
   - `IsInstanceOf` compares the asset's class path with the class hierarchy of loaded classes (by default it does not load the asset's class).
   - `FindObject` with a null outer reads the name as a full path that starts with a package, such as `/Script/GameplayLintDataRules.GLRule_UnknownTag` (`StaticFindObject`'s comment, `UObjectGlobals.h`).
4. **When to use it.** `FAssetData` in `CanValidateAsset` / `AppliesTo`, and in dependents queries; `FindObject` for native classes, which are always loaded.
5. **When not to.** `GetAsset()` inside `AppliesTo` (a load per asset, for every rule). `FindObject` for an asset that may not be loaded yet (it returns null; use a soft path and load it).
6. **Common mistakes.** Treating a redirector as the asset it points to; expecting `FindObject` to load.
7. **Example.** `return !AssetData.IsRedirector();` · `UClass* Class = FindObject<UClass>(nullptr, TEXT("/Script/GameplayLintDataRules.GLRule_UnknownTag"));`
8. **In this project.** Every data rule's `AppliesTo`. The rule tests find the packs' rule classes by path, so the bench module needs no pack header.

## 25. Tick functions and activation: `FTickFunction`, `bAutoActivate`, `ETickingGroup`

1. **What it is.**
   - `FTickFunction` (`Engine/EngineBaseTypes.h`): one tick registration, with `bCanEverTick`, `bStartWithTickEnabled`, `TickInterval` and `TickGroup` (a `TEnumAsByte<ETickingGroup>`).
   - An actor has `PrimaryActorTick`; a component has `PrimaryComponentTick`.
   - `UActorComponent::bAutoActivate` (`ActorComponent.h:317`): activate the component when its actor initialises its components.
2. **Why it exists.** Per-frame work is scheduled per object, in frame phases (`TG_PrePhysics`, `TG_DuringPhysics`, …), and can be throttled with an interval.
3. **How it works.**
   - At registration, a component's tick is enabled if it starts enabled or is already enabled (`ActorComponent.cpp:1739–1752`).
   - `AActor::InitializeComponents` activates each `bAutoActivate` component (`Actor.cpp:6399`), and `Activate` calls `SetComponentTickEnabled(true)` (`ActorComponent.cpp:2815–2819`).
   - A Blueprint compile copies the parent's `bCanEverTick` and sets it if the graph has a tick event (`KismetCompiler.cpp:5504`).
4. **When to use it.** Reading what an object will do per frame, without spawning it.
5. **When not to.** As the truth for a spawned object at run time; read its live tick state instead.
6. **Common mistakes.** Deciding "never ticks" from `bStartWithTickEnabled` alone and forgetting auto-activation (the S7 code review); comparing a `TEnumAsByte` without `GetValue()`.
7. **Example.** `return Tick.bCanEverTick && (Tick.bStartWithTickEnabled || Component.bAutoActivate);`
8. **In this project.** `StartsTicking` and `TicksEveryFrame` in the UpdateBudget pack (`05` G2).

## 26. Classes by path, and Blueprint parents without loading

1. **What it is.**
   - `FSoftClassPath` (`UObject/SoftObjectPath.h`): a class named by path; `ResolveClass()` finds it if it is loaded, and does not load it.
   - `UBlueprint::GetBlueprintParentClassFromAssetTags(AssetData)` (`Blueprint.h:1094`; `Blueprint.cpp:2316–2333`): the native parent class from registry tags.
   - `UBlueprint::GeneratedClass` and `UClass::GetDefaultObject<T>()`.
   - `StaticEnum<E>()->GetDisplayNameTextByValue(Value)`: an enum value's display name.
2. **Why it exists.** Settings must name classes from modules that may not exist in this project, and `CanValidateAsset` must decide without loading.
3. **How it works.**
   - The path is stored as text; `ResolveClass` is a lookup of loaded objects.
   - The parent tag is written when the Blueprint is saved: `NativeParentClassPath` first, else `ParentClassPath`.
4. **When to use it.** Plugin settings that refer to optional classes; registry-only filters.
5. **When not to.** When the class must exist; load it explicitly with `TryLoadClass`.
6. **Common mistakes.** Resolving the same paths inside an inner loop (the S7 code review: resolve once per check); expecting a class from an unloaded module to resolve.
7. **Example.** `if (const UClass* Class = Path.ResolveClass()) { Out.Add(Class); }`
8. **In this project.** `FClasses` and `NativeParent` in the UpdateBudget pack (`05` G2).

## 27. `TStrongObjectPtr`

1. **What it is.** A smart pointer that keeps a UObject alive: a reference the garbage collector sees (`Core/Public/UObject/StrongObjectPtrTemplates.h:25`; include `UObject/StrongObjectPtr.h`).
2. **Why it exists.** A raw `UObject*` held by plain C++ code is invisible to the garbage collector; the object can be freed under it.
3. **How it works.** The constructor registers the object as referenced; the destructor releases it. `Get()`, `operator*`, `operator->`, `explicit operator bool` (`:95–117`).
4. **When to use it.** Short-lived C++ code (tests, tools) that must keep an object across calls that may collect garbage.
5. **When not to.** Inside UObjects: use a `UPROPERTY`. For long-lived caches, prefer weak pointers, so nothing leaks.
6. **Common mistakes.** Assuming no collection happens in between: compiling a Blueprint collects garbage (`BlueprintCompilationManager.cpp:423–427`).
7. **Example.** `const TStrongObjectPtr<UEditorValidatorBase> Rule = NewRule(*this, ClassPath);`
8. **In this project.** The rule tests (`05` F4); added after the first S7 test run crashed.
