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
