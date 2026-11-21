# 05 · Line By Line Code Explanation: GameplayLint

**Rule:** every file that exists in `GameplayLint\` and `Tools\` is explained here, line by line, in the same step it is written (Hard rule 13). Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, five `Build.cs`, five module `.cpp`, three `.uplugin`, three config `.ini`, `Tools\*.bat`, `Tools\core_mask.py` |
| B | S1 (fixture data types) | `GLFixtureData.h/.cpp`, `GLFixtureActors.h/.cpp`, game `Build.cs` |
| C | S2 (seeded corpus) | `GameplayLintSeedCommandlet.h/.cpp`, bench `Build.cs`, `Tools\seed.py` |
| D | S3 (harness) | `GameplayLintTypes.h`, `GameplayLintSettings.h/.cpp`, `GameplayLintReportSubsystem.h/.cpp`, `GameplayLintCommandlet.h/.cpp`, core `Build.cs`, `Tools\run_scan.py`, `Tools\score.py` |
| E | S4 (rule base, walker, tests) | `GameplayLintRule.h/.cpp`, `GameplayLintWalker.h/.cpp`, `Tests\GLTestRule.h`, `Tests\GameplayLintCoreTests.cpp`, `Tools\run_tests.bat` |
| F | S5–S6 (data rules) | `GLDataRules.h/.cpp`, the pack's `Build.cs` and `.uplugin`, `Tests\GameplayLintRuleTests.cpp`, `Tests\GLTestHelpers.h` |
| G | S7 (UpdateBudget pack) | `GLUpdateBudgetRules.h/.cpp`, the pack's `Build.cs` and `.uplugin`, `Config\DefaultEditor.ini`, the UpdateBudget test |

---

## Part A · Foundation (Phase 4)

### A1. `GameplayLint.uproject`: the project descriptor

```json
{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "P10 GameplayLint: meaning-level checks for gameplay data (UE 5.8.4 portfolio project).",
	"Modules": [
		{ "Name": "GameplayLint",      "Type": "Runtime", "LoadingPhase": "Default" },
		{ "Name": "GameplayLintBench", "Type": "Editor",  "LoadingPhase": "Default" }
	],
	"Plugins": [
		{ "Name": "GameplayLintCore",         "Enabled": true },
		{ "Name": "GameplayLintDataRules",    "Enabled": true },
		{ "Name": "GameplayLintUpdateBudget", "Enabled": true },
		{ "Name": "AndroidFileServer",        "Enabled": false }
	]
}
```

| Line | What it does, and why |
|---|---|
| `"FileVersion": 3` | The descriptor schema the engine expects in 5.8; Epic's own plugins use 3 too (for example `DataValidation.uplugin`). A wrong value makes the editor refuse or upgrade the file |
| `"EngineAssociation": "5.8"` | Which installed engine opens the project. A launcher build is identified by its version string. Blank would make the launcher ask every time |
| `"Description"` | Shown in the project browser. Cosmetic |
| `"Modules"` | The C++ modules the **project** owns. UnrealBuildTool (UBT) compiles exactly these, plus enabled plugins' modules |
| `"GameplayLint"`, `"Type": "Runtime"` | The primary game module. `Runtime` means it is built into **both** the game and the editor targets. In S1 it receives the fixture data types; they are game data, so a packaged game must contain them |
| `"GameplayLintBench"`, `"Type": "Editor"` | The seeding commandlet's module. `Editor` modules are built only for editor targets and load when the editor starts, **including in commandlet mode**: the engine has a separate `EditorNoCommandlet` type for modules that must not (`Source/Runtime/Projects/Public/ModuleDescriptor.h:110–113`). That matters: the seeder *is* a commandlet. *Alternative:* `Runtime`, which would drag `UnrealEd` into the packaged game and fail to link |
| `"LoadingPhase": "Default"` | Loads at the normal point, before the first world exists, so the module's classes are registered before anything needs them (06 §4) |
| `"Plugins"` | Explicitly enables the project's three plugins. Each is `EnabledByDefault`, so it would load anyway, but listing them makes the dependency visible in one place. A project that doesn't want the UpdateBudget rules sets that entry's `"Enabled"` to `false`; nothing else changes |
| `{ "Name": "AndroidFileServer", "Enabled": false }` | **Turns off an engine plugin that is on by default** (`Plugins/Runtime/AndroidFileServer/AndroidFileServer.uplugin:15`). Its editor module, `AndroidFileServerEditor`, generates a random `SecurityToken` on the first editor start and writes it into the project's `DefaultEngine.ini` (`Plugins/Runtime/AndroidFileServer/Source/AndroidFileServerEditor/Private/AndroidFileServerRuntimeSettings.cpp:27–38`). The first Phase 4 run did exactly that (`09` F-6). A generated token must not reach a public repository, and this project never deploys to Android, so the plugin is disabled and the block deleted. Portfolio rule since 2026-11-21 |
| *Not listed:* `DataValidation` | It is enabled by default (`DataValidation.uplugin:13`) and `GameplayLintCore.uplugin` already lists it as a dependency (A6), which is where the dependency belongs |

### A2. `Source\GameplayLint.Target.cs` and `GameplayLintEditor.Target.cs`: build targets

```csharp
using UnrealBuildTool;

public class GameplayLintTarget : TargetRules
{
	public GameplayLintTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("GameplayLint");
	}
}
```

| Line | What it does, and why |
|---|---|
| `using UnrealBuildTool;` | Target files are C#, compiled by UBT itself (04 §3). This brings `TargetRules`, `TargetType` and the rest into scope |
| `public class GameplayLintTarget : TargetRules` | UBT finds targets by class name, `<Name>Target`. This one defines the target `GameplayLint`, which is why `build.bat GameplayLint` works |
| `(TargetInfo Target) : base(Target)` | UBT passes the platform, configuration and architecture; the base constructor stores them |
| `Type = TargetType.Game` | A standalone game executable with no editor code. This is the target that gets packaged. The editor file uses `TargetType.Editor` (DLLs loaded by `UnrealEditor.exe`) |
| `DefaultBuildSettings = BuildSettingsVersion.V7` | The newest default compiler and build settings for this engine, the value Epic's 5.8 template uses. Older values keep older defaults for compatibility |
| `IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8` | Use 5.8's engine header include order. Pinning it means an engine upgrade can't silently change what our code includes |
| `ExtraModuleNames.Add("GameplayLint")` | The project modules this target builds. **Only the runtime module**: the game target must contain no lint code, and the Phase 4 build log confirms it compiled only `GameplayLintModule.cpp` from the project (`09` F-2) |
| Editor target: `ExtraModuleNames.AddRange(new string[] { "GameplayLint", "GameplayLintBench" })` | The editor builds both project modules. Plugin modules come in through the plugin system, not this list |

**Why two target files?** The editor and a packaged game are different executables with different module sets, so UBT needs one target rule each.

### A3. `Source\GameplayLint\GameplayLint.Build.cs`: the game module's build rules

```csharp
public class GameplayLint : ModuleRules
{
	public GameplayLint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
```

| Line | What it does, and why |
|---|---|
| `class GameplayLint : ModuleRules` | The class name **must equal the module name**; UBT maps each `*.Build.cs` to its module this way |
| `ReadOnlyTargetRules Target` | A module may read target settings (platform, configuration) but not change them, so modules can't fight over them |
| `PCHUsage = UseExplicitOrSharedPCHs` | Use a **shared precompiled header** built by the engine for a common dependency set (the build log shows `SharedPCH.Engine.Project…` for the game and `SharedPCH.UnrealEd.Project…` for the editor). It makes each `.cpp` compile faster. *Rejected:* `NoPCHs` (slow) or a private PCH (unneeded for a small module) |
| `PublicDependencyModuleNames …` | `Core` (containers, strings, logging), `CoreUObject` (UObject, reflection, GC), `Engine` (actors, components, DataTables). **Public** because S1's public headers declare types that derive from engine classes (`UPrimaryDataAsset`, `AActor`, `UActorComponent`), so any module including them needs these include paths too. Rule of thumb: private unless a public header needs it (06 §2) |
| *Not yet:* `GameplayTags` | Added in S1, when the first header declares an `FGameplayTag` property. Dependencies are added when first needed, not "just in case" |
| *Never:* any `GameplayLint*` plugin module | The game must not know the rules exist (`02` §3). If a later change adds one, the packaged build (A9) would start compiling lint code, and the isolation check fails |

### A4. `Source\GameplayLint\Private\GameplayLintModule.cpp`

```cpp
#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, GameplayLint, "GameplayLint");
```

| Line | What it does, and why |
|---|---|
| `#include "Modules/ModuleManager.h"` | Declares `IMPLEMENT_PRIMARY_GAME_MODULE` and `FDefaultGameModuleImpl` (`Source/Runtime/Core/Public/Modules/ModuleManager.h:892, 1100–1136`). Only the header that is used (04 §1) |
| `IMPLEMENT_PRIMARY_GAME_MODULE(…)` | Generates the module's entry point. **Primary** means "the project's main game module"; a project needs exactly one. In packaged (monolithic) builds it also defines the project-name global the game starts from (06 §1) |
| `FDefaultGameModuleImpl` | An engine-provided module class with empty `StartupModule`/`ShutdownModule`. We have no startup logic, so writing our own class would be a class with no reason |
| `GameplayLint` | The module name; it must match the `.uproject` and `Build.cs` |
| `"GameplayLint"` | The old "game name" argument. In 5.8 it is ignored (the macro parameter is named `DEPRECATED_GameName`, `ModuleManager.h:1096–1114`, "manually specifying a name is no longer necessary"); the macro still takes three arguments, so we pass the project name, as Epic's template does |

**Why there is no `GameplayLint.h`:** the template's empty module header is included by nothing. An empty header adds nothing.

### A5. `Source\GameplayLintBench\GameplayLintBench.Build.cs` and `Private\GameplayLintBenchModule.cpp`

```csharp
PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
```
```cpp
#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintBench);
```

| Line | What it does, and why |
|---|---|
| `PrivateDependencyModuleNames` | **Private**, unlike A3: nothing will ever depend on the bench module, and its only class (the seeding commandlet, S2) can be declared in a `Private` header. UHT accepts reflected classes in `Private` folders. Private dependencies keep include paths from spreading |
| `"Core", "CoreUObject", "Engine"` | The minimum for any module with reflected classes. `UnrealEd`, `Kismet`, `GameplayTags` and `Json` are added in S2, when the seeder first uses them |
| `IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintBench)` | The non-primary entry point: an ordinary module. `FDefaultModuleImpl` (`ModuleManager.h:884`) is the empty default implementation |

### A6. `Plugins\GameplayLintCore\GameplayLintCore.uplugin`: the framework plugin

| Field | Value | Why |
|---|---|---|
| `FileVersion`, `Version`, `VersionName` | 3, 1, "1.0" | Same schema as Epic's `DataValidation.uplugin` |
| `FriendlyName`, `Description`, `Category` | "Gameplay Lint Core", …, "Editor" | Shown in the editor's Plugins window. Same category as Data Validation. The description says it contains no rules, which is a design rule (`02` §3) |
| `CreatedBy` and the URL fields | empty | Kept as empty strings, as Epic's schema has them; no personal details in a public repository |
| `EnabledByDefault` | `true` | A project that copies the plugin folder gets it working without editing its `.uproject` |
| `CanContainContent` | `false` | Code only; avoids an empty content mount point |
| `IsBetaVersion`, `IsExperimentalVersion` | `false`, `false` | No warning badge. Honest, because the plugin is not experimental code, though it is not "production-ready" either (Hard rule 11) |
| `Installed` | `false` | Part of the project, not installed into the engine |
| `Modules[0]` | `GameplayLintCore`, `"Type": "Editor"`, `Default` | Editor only: validators are an editor concept (`UEditorValidatorBase` lives in the Editor-type DataValidation module, `DataValidation.uplugin:22`). `Editor` modules load in commandlets too (A1) |
| `"Plugins": [ { "Name": "DataValidation", "Enabled": true } ]` | **Declares the plugin dependency.** The core module depends on the `DataValidation` module (A7); UBT warns if a plugin's module depends on another plugin's module and the plugin doesn't list it (`Source/Programs/UnrealBuildTool/Configuration/UEBuildTarget.cs:1564–1579`). The Phase 4 build has 0 warnings, so the wiring is right. It also guarantees DataValidation is enabled wherever the core is, even in a project that turned it off |

### A7. `GameplayLintCore.Build.cs` and `Private\GameplayLintCoreModule.cpp`

```csharp
PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
// DataValidation is public: the rule base class (a public header) derives from its UEditorValidatorBase.
PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DataValidation" });
```
```cpp
#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintCore);
```

| Line | What it does, and why |
|---|---|
| `"DataValidation"` (public) | The first core file (S3/S4) is a **public** header declaring `UGameplayLintRule : public UEditorValidatorBase`. Every rule pack includes it, so packs need DataValidation's include path too: that is what *public* means (06 §2). It is added in Phase 4, one step early, so the foundation build checks the plugin wiring of A6 and proves that a project plugin can depend on DataValidation with the **launcher** engine. Its import library ships with the engine (`01b` §2) |
| `"Core", "CoreUObject", "Engine"` (public) | The core's public headers will expose UObject-derived types (settings, subsystem, commandlet) |
| *Not yet:* `AssetRegistry`, `DeveloperSettings`, `Json` | Private dependencies added in S3–S8 when first used (`02` §3) |
| `IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintCore)` | An ordinary module with no startup logic. **The comment in the file says why there is nothing to register:** the engine discovers validators by reflection (`01b` #1). A framework that needed a registration call per rule would have code here |

### A8. The two rule-pack plugins: `GameplayLintDataRules` and `GameplayLintUpdateBudget`

The two packs are identical in Phase 4 except for names and descriptions, so they are explained once. `<Pack>` stands for `DataRules` or `UpdateBudget`.

`.uplugin` fields differing from A6:

| Field | Value | Why |
|---|---|---|
| `FriendlyName` | "Gameplay Lint: Data Rules" / "Gameplay Lint: UpdateBudget Rules" | Groups the packs under one name in the Plugins window |
| `Description` | the rules the pack holds; the UpdateBudget pack names its source, the UE5-UpdateBudget project | A team browsing plugins sees what each pack checks and where the lesson came from |
| `"Plugins": [ { "Name": "GameplayLintCore", "Enabled": true } ]` | A pack depends on the core plugin and on nothing else. Enabling a pack enables the core. UBT's plugin-dependency check (A6) applies here too |

`GameplayLint<Pack>.Build.cs`:

```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameplayLintCore" });
```

| Line | What it does, and why |
|---|---|
| `PrivateDependencyModuleNames` | **All private.** A pack has no public headers: its rule classes live in `Private\`, the engine finds them by reflection, and no other module includes them (`02` §3). *Alternative:* public dependencies, which would only matter if another module included the pack's headers; none ever should |
| `"GameplayLintCore"` | The rule base class and the walker. The only project code a pack may use. **Not** `GameplayLint` (the game module) or another pack: packs are independent of each other and of any one game |
| *Later:* `"GameplayTags"` (DataRules) | Added in S5 with the first tag rule |

`Private\GameplayLint<Pack>Module.cpp`: `IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLint<Pack>);` with the comment that the engine discovers the pack's rules by reflection. Same reasoning as A7.

**The plug-in claim, at the file level:** adding a later project's pack means adding one more folder shaped like this one. Nothing in `Plugins\GameplayLintCore\` changes, which is criterion E1 (`01` §7.4).

### A9. Config files

**`Config\DefaultEngine.ini`**

| Section / key | Why |
|---|---|
| `GameMapsSettings: GameDefaultMap / EditorStartupMap = /Engine/Maps/Templates/Template_Default` | A shipped engine map. The project has no hand-authored map, so it stays fully buildable from text files; the packaged smoke run loads this map |
| `RendererSettings: r.AllowStaticLighting=False, r.Shadow.Virtual.Enable=1` | Copied from Epic's 5.8 template: standard modern defaults, so the packaged build matches a fresh 5.8 project. They have no effect on validation |
| `WindowsTargetSettings: DX12, -SM5 +SM6` | Also the 5.8 template's: Direct3D 12 with Shader Model 6. The cook compiles shaders for this format |
| *Not copied from UE5-UpdateBudget:* `bSmoothFrameRate`, `bUseFixedFrameRate`, `r.VSync` | Those protect frame-time measurements. This project measures commandlet runs, not frames, so restating them would be noise |
| *Deleted:* `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]` | **Written by the engine, not by us, and removed.** On the first editor start, the Android File Server plugin generated a random `SecurityToken` (a new GUID) and saved its settings section into this file (`AndroidFileServerRuntimeSettings.cpp:27–38`), rewriting it with CRLF line endings. The block configures Epic's Android device file server and does nothing on Windows. A generated token has no place in a public repository, so the plugin is disabled in the `.uproject` (A1) and the block deleted; with the plugin off, its editor module never loads and can't write it again (`09` F-6) |

**`Config\DefaultGame.ini`**: `ProjectName` and `Description` under `GeneralProjectSettings`, shown in the window title and project settings.

**`Config\DefaultInput.ini`**

```ini
[/Script/Engine.InputSettings]
DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput
DefaultInputComponentClass=/Script/EnhancedInput.EnhancedInputComponent
```

| Line | Why |
|---|---|
| The whole file | **Added in Phase 4 after the first packaged smoke run logged a warning** (`09` F-5a): "UEnhancedInputLocalPlayerSubsystem … does not have a valid PlayerInput object. Failed to load user settings." The Enhanced Input plugin is enabled by default (`Plugins/EnhancedInput/EnhancedInput.uplugin:13`). Its local player subsystem looks for an Enhanced player input object and warns when it isn't there (`Plugins/EnhancedInput/Source/EnhancedInput/Private/EnhancedInputSubsystems.cpp:115–119`). The engine's default player input class is the legacy one, and no `Engine/Config/Base*.ini` sets the Enhanced classes (grep). Epic's templates set these two lines; UE5-UpdateBudget copied the template's whole input file, which is why it never saw the warning |
| `DefaultPlayerInputClass` | The class created per local player to process input. The Enhanced one is what the subsystem expects |
| `DefaultInputComponentClass` | The input component class actors get; the Enhanced one, so the two classes match |
| *Not copied:* the template's 40 axis-config lines | This project reads no input. Two lines fix the warning; the rest would be unexplained noise |

### A10. `Tools\`: build, package, smoke, validate, CPU mask

All `.bat` files start with `@echo off` (don't print each command), `setlocal` (variables stay local to the script), set `UE_ROOT=C:\Program Files\Epic Games\UE_5.8` and `PROJECT=%~dp0..\GameplayLint\GameplayLint.uproject` (`%~dp0` is the script's own folder, so the scripts work from any working directory), and end with `exit /b %ERRORLEVEL%` (pass the tool's exit code to the caller: the UE job queue records it). They are saved with CRLF line endings, which `.gitattributes` enforces, because `cmd.exe` can misparse LF-only batch files.

| Script | Key line | Why |
|---|---|---|
| `build.bat [Target] [Config]` | `call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -WaitMutex -NoHotReloadFromIDE` | A reproducible command-line build. Defaults: `GameplayLintEditor`, `Development`. `-WaitMutex` waits for another UBT instead of failing; `-NoHotReloadFromIDE` avoids hot-reload side effects. *Test* is not offered: the launcher engine can't build it (UE5-UpdateBudget, Phase 4) |
| `package.bat [Config]` | `call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project=… -noP4 -utf8output -unattended -platform=Win64 -clientconfig=%CONFIG% -build -cook -stage -pak -archive -archivedirectory=…` | Builds, cooks, stages, paks and archives a standalone game into `Build\` (git-ignored). Here its purpose is the **isolation check**: a runtime build that must compile and cook without any lint module |
| `smoke_game.bat [packaged\|uncooked]` | packaged: `Build\Windows\GameplayLint.exe` + common args; uncooked: `UnrealEditor-Cmd.exe <project> -game` + common args | Starts the game, loads the default map and quits. Common args: `-nullrhi` (no GPU needed), `-nosound`, `-unattended` (no dialogs), `-nosplash`, `-traceautostart=0` (an open Insights doesn't trace it), `-ExecCmds=quit` (runs the `quit` console command once the engine is up), `-log -abslog=…` (log file). *Removed:* `-ExitAfterSeconds=0`, copied from UE5-UpdateBudget's script: UE 5.8.4 doesn't parse it (no occurrence anywhere in `Engine/Source` or `Engine/Plugins`), so it did nothing; `quit` alone ends the run (`09` F-6). The default is the packaged build, because that is the Phase 4 check the brief asks for |
| `validate.bat [args]` | `UnrealEditor-Cmd.exe <project> -run=DataValidation -unattended -nullrhi -nosplash -traceautostart=0 -stdout -FullStdOutLogOutput -abslog=… %*` | Runs the **engine's stock** Data Validation commandlet: configuration "stock" in S3's equivalence check. `-stdout -FullStdOutLogOutput` print every log line to stdout, so the queue's idle watchdog sees progress (`01b` §4). `%*` passes extra stock arguments (`-AssetType=`, `-includeengine`) |

**`Tools\core_mask.py`** (copied from UE5-UpdateBudget's `run_bench.py`; used from S3 by `run_scan.py`):

| Part | What it does, and why |
|---|---|
| `performance_core_mask()` | Asks Windows for every physical core's `EfficiencyClass` with `GetLogicalProcessorInformationEx(RelationProcessorCore)` through `ctypes`, then ORs together the affinity masks of the cores with the highest class. Returns `None` on a non-hybrid CPU. On this i7-14650HX it returns `0xFFFF` (logical CPUs 0–15, the 8 P-cores with hyper-threading) |
| `struct.unpack_from("<II", raw, offset)` | Reads the record header: relationship type and record size, little-endian 32-bit integers. `raw[offset + 9]` is the `EfficiencyClass` byte; `"<QH"` at `offset + 32` is the group mask (64-bit mask, 16-bit group) |
| `KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)` and the `restype`/`argtypes` lines | Loads the Windows kernel library once, at import, and declares the handle and mask types (`c_void_p`, `c_size_t`) so `ctypes` passes 64-bit values correctly. `None` off Windows, so every function can say "not available" instead of crashing |
| `pin_self(mask)` | `SetProcessAffinityMask(GetCurrentProcess(), mask)`: this Python process and every thread it ever has run only on those CPUs. **A child process inherits its parent's affinity**, so every editor the runner starts afterwards is pinned from its first instruction, engine start-up included. Checked on 2026-11-21: after `pin_self`, a child Python process read back `0xFFFF` (`09` T-1) |
| `applied_mask()` | Reads the mask back with `GetProcessAffinityMask` and says whether it is "all logical processors" or a restriction. The environment record stores what Windows actually applied, not what was asked for |
| *Replaced:* pinning each child after `Popen` (`pin_process`) | The first version, also copied from UE5-UpdateBudget, pinned each editor after it started. It worked, but left a short start-up window unpinned and needed more code; the portfolio protocol switched to pinning the runner itself |
| Why pin at all | UE5-UpdateBudget measured runs at two stable levels 1.6× apart when Windows scheduled the game thread on an E-core. A scan's timing must not depend on where Windows puts it |
| Why not `cmd /c start /affinity` | It returns exit code 0 whatever the program returned (found by another portfolio project), so failures would go unnoticed. The runner (already pinned) starts the editor with `subprocess.Popen`, waits with a timeout and kills the tree in a `finally` block |
| `if __name__ == "__main__"` | Run as a script, it prints the mask (`FFFF`) so it can be recorded and checked by hand |

---

## Part B · Step S1: the fixture data types (game module)

**State after S1 (2026-11-21):** the game module holds the gameplay data the corpus is made of. The types stand in for a real game's data: the rules never include them and find their values by reflection, like any project's data. Compiled with 0 errors and 0 warnings in both targets (`09`, S1–S4).

### B1. `Source\GameplayLint\Public\GLFixtureData.h`

| Line(s) | What, why, and what breaks if it changes |
|---|---|
| Includes: `Curves/CurveFloat.h`, `Engine/CurveTable.h`, `Engine/DataAsset.h`, `Engine/DataTable.h`, `GameplayTagContainer.h`, `Misc/DataValidation.h`, `StructUtils/InstancedStruct.h` | One include per type used by value: `FRuntimeFloatCurve`, `FCurveTableRowHandle`, `UPrimaryDataAsset`, `FDataTableRowHandle`/`FTableRowBase`, `FGameplayTag`/`FGameplayTagContainer`, `FDataValidationContext` (for the row's `IsDataValid`), `FInstancedStruct` (in CoreUObject since 5.5). A by-value member needs the full type; a forward declaration would not compile |
| `#include "GLFixtureData.generated.h"` | UHT's generated code for the reflected types below. Must be the last include (UHT checks) |
| *No* `#define UE_API` | Other modules (the bench module, the tests) use `StaticStruct()`/`StaticClass()` and the data members, never member functions. In 5.8 UHT exports a struct's constructor function itself (the generated `Z_Construct_UScriptStruct_…` carries the module's API macro, e.g. `GameplayTagContainer.generated.h:23` for FGameplayTag), and `UCLASS(MinimalAPI)` exports a class's `StaticClass`. Nothing else needs exporting, so the alias would be dead code |
| `USTRUCT(BlueprintType) struct FGLFixturePayload` | A reflected struct: the walker finds its fields through reflection. `BlueprintType` lets designers use it in Blueprints, which real gameplay data allows. **Fields:** `FGameplayTag Tag`, `FDataTableRowHandle Row`, `FRuntimeFloatCurve Curve`: one of each reference kind the data rules check, so one struct can be placed at every nesting level |
| `UPROPERTY(EditAnywhere, Category = "Fixture")` | Reflected (so saved, walked and editable) and visible in the details panel. `Category` groups it. Without `UPROPERTY` a field is invisible to reflection: neither saved nor checked |
| `struct FGLFixtureRow : public FTableRowBase` | A DataTable row type (tables require `FTableRowBase`). `int32 Value` (the M4 control mutation edits only this), `Tag`, `Link` (a row handle *inside* a row: tables point at other tables), `Curve` |
| `#if WITH_EDITOR virtual EDataValidationResult IsDataValid(FDataValidationContext&) const override` | The **class-level check a row owner writes**: a negative `Value` is an error. `UDataTable::IsDataValid` calls it for each row (`DataTable.cpp:418–462`). The engine runs class-level checks before any validator and skips validators when it fails (`EditorValidatorSubsystem.cpp:657–663`). The corpus uses this to build its **masking set**: tables with a class-level error *and* a rule defect. `WITH_EDITOR` because the base declaration is editor-only (`DataTable.h:60–68`); without the guard the game target fails to compile. `NSLOCTEXT` gives the message a localisation key without a file-wide namespace |
| `UCLASS(MinimalAPI, EditInlineNew, DefaultToInstanced, CollapseCategories) class UGLFixtureInstanced : public UObject` | An **instanced sub-object**: data that lives in its own UObject inside the owning asset's package. `EditInlineNew` lets the details panel create one in place; `DefaultToInstanced` makes every property of this type an instanced reference (the owner gets its own copy, saved in its package); `CollapseCategories` keeps the inline UI flat. The walker follows exactly these references (`CPF_InstancedReference`) and no others |
| `UCLASS(MinimalAPI) class UGLFixtureDataAsset : public UPrimaryDataAsset` | The corpus's data asset, with one field per **structural variant** of `01` §7.1: top level (`Tag`, `Row`, `Curve`, …), `Tags` (container), `Nested`, `List` (`TArray`), `Map` (`TMap<FName, …>`), `Instanced`, `Extra` (`FInstancedStruct`), and the two-field `LootTable`/`LootRow`. `UPrimaryDataAsset` is the usual base for gameplay data assets (Asset Manager support) |
| `meta = (LintRequired)` on `RequiredRow` and `RequiredCurve` | **Free-form UPROPERTY metadata** a programmer writes to say "this must be set". UHT accepted the keys (0 warnings, S1), which settles the `[inferred]` of `01b` §3. Metadata exists only in editor builds (`WITH_METADATA`), which is where the rules run. *Alternative:* a settings list of property paths, which would separate the meaning from the declaration |
| `meta = (LintMonotonic = "Increasing")` on `CooldownByLevel` | The curve's contract: it must never fall as the level rises. The value names the direction |
| `meta = (LintMonotonic = "Decreasing")` on `FalloffByDistance` (added in S6) | The opposite contract, so the curve rule's "must not rise" branch has a fixture to test. The corpus leaves it empty, so it adds no ground truth |
| `UPROPERTY(EditAnywhere, Instanced, …) TObjectPtr<UGLFixtureInstanced> Instanced` | `Instanced` (with the class's `DefaultToInstanced`) marks the reference `CPF_InstancedReference`. `TObjectPtr` is UE5's UObject pointer wrapper (GC-visible as a UPROPERTY) |
| `meta = (BaseStruct = "/Script/GameplayLint.GLFixturePayload") FInstancedStruct Extra` | An instanced struct that may only hold an `FGLFixturePayload` (the editor's type picker uses `BaseStruct`). **5.8's property iterator walks into it** (`UnrealType.cpp:57–65`), which corrected the corpus design in S4 (`01` §7.1) |
| `TSoftObjectPtr<UDataTable> LootTable; FName LootRow;` | A **project-specific reference type**: a table and a row name in two plain fields. Nothing tells a generic rule they belong together, so a missing row here is the **held-out kind** (an expected miss). `TSoftObjectPtr` is a by-path reference that does not load the table |

### B2. `Source\GameplayLint\Public\GLFixtureActors.h` and `Private\GLFixtureActors.cpp`

| Class / line | What, why |
|---|---|
| `UCLASS(MinimalAPI, Blueprintable) class AGLFixtureAgent : public AActor` | The budget scope's base class (the project config names it, S7). `Blueprintable` so the corpus can derive Blueprints. **Constructor:** `PrimaryActorTick.bCanEverTick = false` (already the actor default, `Actor.cpp:278`, restated because "the agent never ticks itself" is the point). `FGLFixturePayload Payload` lets a Blueprint subclass hold gameplay data in its class default object: the "Blueprint default" variant |
| `AGLFixtureTickingAgent : AGLFixtureAgent` | A native parent that ticks: `bCanEverTick = true`, `bStartWithTickEnabled = true`. Every Blueprint made from it inherits a per-frame actor tick (the Kismet compiler starts from the parent's flag, `KismetCompiler.cpp:5525–5527`): the "inherited actor tick" defect |
| `UCLASS(MinimalAPI, Blueprintable, ClassGroup = (GameplayLint), meta = (BlueprintSpawnableComponent)) UGLFixtureBudgetClient : UActorComponent` | **Stand-in for UE5-UpdateBudget's `UBudgetedUpdateComponent`**, with the same tick contract: `bCanEverTick = true` (components default to false) and `TickGroup = TG_PrePhysics` (components default to `TG_DuringPhysics`, `ActorComponent.cpp:548–550`). `BlueprintSpawnableComponent` makes it addable in a Blueprint's component list; `ClassGroup` is the "Add Component" category. *Why not link the real one:* projects never reference each other's folders (`02` §3) |
| `UGLFixtureTicker : UActorComponent` | A plain component that ticks every frame in the default group: the per-frame work the UpdateBudget rule looks for. Only `bCanEverTick` needs setting; start-enabled and interval 0 are defaults |
| `GLFixtureActors.cpp`: `#include UE_INLINE_GENERATED_CPP_BY_NAME(GLFixtureActors)` | Compiles UHT's generated code into this file instead of a separate one (fewer translation units) |
| `GLFixtureData.cpp` | No code of its own: it exists to host `UE_INLINE_GENERATED_CPP_BY_NAME(GLFixtureData)` for the header's types |

### B3. `GameplayLint.Build.cs` (changed in S1)

`"GameplayTags"` joined the **public** dependencies: `GLFixtureData.h`, a public header, declares `FGameplayTag` members, so any module including it needs the GameplayTags include path (`06` §2).

---

## Part C · Step S2: the seeded corpus (`GameplayLintBench`) and `Tools\seed.py`

**State after S2:** a commandlet builds the corpus of `01` §7.1 from a seed and writes its ground truth; the same commandlet applies M1–M4. Everything runs through the UE job queue via `Tools\seed.py`.

### C1. `Private\GameplayLintSeedCommandlet.h`

| Line | Why |
|---|---|
| `UCLASS() class UGameplayLintSeedCommandlet : public UCommandlet` | A commandlet: `-run=GameplayLintSeed` finds `GameplayLintSeedCommandlet` (`06` §6). In `Private`, because nothing includes it; UHT accepts reflected classes there |
| `virtual int32 Main(const FString& Params) override` | The entry point the engine calls instead of the editor loop; its return value becomes the exit code |

### C2. `Private\GameplayLintSeedCommandlet.cpp`

| Part | What it does, and why |
|---|---|
| `namespace GameplayLintSeed { … }` | Everything the corpus needs, without polluting the global namespace (the module is a unity build: other files share the translation unit) |
| Layout constants (`RootPath`, `SharedPath`, `AssetsPath`, `NumTargets = 3`, `TargetRows = 32`, `TableRows = 20`, `CurveRows = 16`, `PermanentTags = 16`, `RemovableTags = 8`, `CleanParents = 4`, `TickingParents = 2`, `RemovedRows`) | Named sizes with a comment each, instead of numbers in the code. One root folder so a scan, a `-Paths` filter or a delete can address the whole corpus |
| `struct FEntry` | One ground-truth line: asset object path, type, category (`defect`, `nearmiss`, `clean`, `heldout`, `masked`), expected rule, kind, variant and the property path |
| `enum class EDataKind` + `KindName` + `RuleFor` + `FieldFor` | The nine data defect kinds; the first four (`TagSingle`, `RowMissing`, `RowNullTable`, `CurveNonFinite`) can sit at any payload position, the other five only at top level. `RuleFor` maps a kind to the rule that must report it; `FieldFor` names the field it lives in (shared by three callers since the simplify pass) |
| `SaveAsset(UObject*)` | `FPackageName::LongPackageNameToFilename(…, GetAssetPackageExtension())` turns `/Game/GLSeed/Assets/DA_0001` into the `.uasset` path; `FSavePackageArgs{ TopLevelFlags = RF_Public \| RF_Standalone, SaveFlags = SAVE_NoError }`; `UPackage::SavePackage` (`Package.h:1204`). `RF_Public` makes the asset referenceable from other packages, `RF_Standalone` keeps it from being garbage-collected while unreferenced |
| `MakeTag(Name)` | `FGameplayTag::FromExportString(Name, PPF_SerializedAsImportText)`: with that flag an unknown name is **kept** "to avoid data loss" (`GameplayTagsManager.cpp:1177–1178`), as a text import keeps it. At seed time no corpus tag is in the dictionary yet (this run writes their `.ini` files), so every tag is made this way; in the validation run the permanent and removable tags exist and `Fixture.Missing.*` do not |
| `FCurveKey` + `MakeCurve({…})` | A two-float aggregate and an `std::initializer_list`, so a curve reads as `MakeCurve({ { 0.f, 1.f }, { 1.f, 2.f } })`. *Renamed from `FKey`* during review: Input Core already has a global `FKey` |
| `NewBlueprint` | `CreatePackage` + `FKismetEditorUtilities::CreateBlueprint(Parent, Package, Name, BPTYPE_Normal, CallingContext)` (`KismetEditorUtilities.h:110`) + `FAssetRegistryModule::AssetCreated` (tells this process's registry the asset exists) |
| `AddComponent` | `SimpleConstructionScript->CreateNode(Class, Name)` + `AddNode` (`SimpleConstructionScript.h:92, 170`) and returns `Node->ComponentTemplate`: the object whose properties become the component's defaults. That is where a designer would change "Tick Group" or "Start with Tick Enabled" |
| `class FSeeder` | One instance per run. **Determinism:** every random choice comes from one `FRandomStream(Seed)` and the assets are created in index order, so the same seed gives the same corpus and the same manifest (E2). `FRandomStream` is Unreal's seeded generator; `FMath::Rand` would not be repeatable |
| `ValidTag`, `TargetRow`, `ValidRow`, `ValidCurve`, `FillPayload` | Valid values. 10 % of tags are "removable" ones, so M2 has victims; rows point at `T00`–`T31` of one of three shared tables, so M1 has victims |
| `ApplyPayloadDefect` | Turns one payload value into a defect: `Fixture.Missing.NNNN`; a row `T99`; a null table with a row name; a NaN or infinite key (`std::numeric_limits<float>::quiet_NaN()` / `infinity()`). `checkNoEntry()` for the top-level kinds, which only `CreateDataAsset` applies |
| `PickCategory` | 20 % defect, 15 % near-miss, 65 % clean (`01` §7.1) |
| `CreateShared` | Three target tables (`T00`–`T31`, every row valid), the curve table (`C00`–`C15`), four clean parent Blueprints (agent + client) and two ticking parents (agent + client + ticker: defective themselves). They are created first because the corpus references them |
| `CreateDataAsset` | A clean asset first (every reference valid, every annotated contract met), then 1 % instanced-struct defects, 1 % held-out (`LootRow = "T99"`), and otherwise a category: a defect cell chosen uniformly among 25 (four payload kinds × five positions + five top-level kinds), or one of five near-misses (a parent tag that exists, an empty container, an unset optional row, a flat monotonic curve, an empty optional curve) |
| `CreateDataTable` | 20 rows. 5 % are **masked**: row `R00` gets `Value = -1` (the class-level error) and another row gets an unknown tag. Defective tables have one payload defect in one row; near-misses leave links empty and curves flat |
| `CreateActorBlueprint` | Chooses the shape first, because it decides the parent: nine defect cells (five UpdateBudget kinds, four data kinds in the class default object) or four near-misses (a ticker with a 0.2 s interval, a ticker that starts disabled, an out-of-scope actor that ticks, allow-listed skeletal mesh and movement components). Half the clean ones derive from a clean shared parent (M3's victims). The class default object's payload is set **after** `CompileBlueprint`, on the class the compile produced |
| `CreateComponentBlueprint` | A Blueprint subclass of the client (clean, or a defect: tick group `TG_PostPhysics`, or start disabled), or a near-miss: a Blueprint of the plain ticker, which ticks every frame but is not a client and not in an actor |
| `WriteTagFiles` | `Config/Tags/GLSeedPermanent.ini` and `GLSeedRemovable.ini`: section `[/Script/GameplayTags.GameplayTagsList]` with one plain `GameplayTagList=(Tag="…",DevComment="")` line per tag, the format the manager loads from that folder (`GameplayTagsManager.cpp:582, 682`). **No `+` prefix:** a loose file is read without ini symbol commands, so `+GameplayTagList` would be a different key and the list would load empty; a repeated plain key appends (`Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp:2051, 2076–2096, 2216–2219`). The first S1–S4 baseline ran with `+` lines and every corpus tag was unknown to the engine (`09` S1–S4, `07`). Git-ignored: regenerated with the corpus |
| `WriteManifest` | The ground truth as JSON through `TJsonWriter` with a pretty-print policy. Saved as **UTF-8 without BOM** (the default would write UTF-16 as soon as a string held a non-ANSI character; review finding) |
| `Run` | The mix per 100 assets (40 / 10 / 35 / 15), shared assets first, then each type in index order |
| `Mutate` | **Refuses an already-mutated corpus** (row `T30` must still exist in `DT_TargetsA`; review finding). Then M1 (`RemoveRow` on two tables), M2 (deletes `GLSeedRemovable.ini`), M3 (adds a ticker to `BP_Parent_0` and recompiles), M4 (`Value += 100` on `DT_TargetsC`, row `T05`). Writes the pseudo-changelist (`M <package>`, `X <file>`) |
| `Main` | `SearchAllAssets(true)` first (the registry must know what exists), then `-Mutate -Changes=` or `-Seed= -Assets= -Manifest=` (`FParse::Param`/`FParse::Value`). Refuses to seed into a non-empty `/Game/GLSeed` |
| Constructor: `LogToConsole = true` | Log lines reach the console, so the queue's watchdog sees progress |

### C3. `GameplayLintBench.Build.cs` (changed in S2–S4)

Private dependencies, each with its reason in the file: `AssetRegistry`, `DataValidation` (**added after the first S1–S4 build failed to link**: the tests' rule derives from `UEditorValidatorBase`, and the core's public dependency gives the include path but not the import library; `02` §3), `GameplayTags`, `Json`, `UnrealEd` (`FKismetEditorUtilities`), `GameplayLint` (the fixtures), `GameplayLintCore` (the framework under test).

### C4. `Tools\seed.py`

| Part | Why |
|---|---|
| `seed` | Deletes `Content/GLSeed`, the generated tag files and the old manifest (the commandlet refuses a non-empty folder), runs the commandlet, prints the manifest's SHA-256 |
| `determinism` | **E2:** seed 2 once, seed 1 twice; the two seed-1 manifests must be byte-identical and differ from seed 2. Seed 2 runs first so the corpus left behind is the seed-1 corpus. Exit 3 if the check fails |
| `mutate` | Runs `-Mutate` and prints `changes.txt` |
| `run_editor` | Starts `UnrealEditor-Cmd.exe` itself with `subprocess.Popen` (never `cmd /c start`), output to our stdout (the queue's watchdog sees it), a timeout, and a kill of the process tree in `finally` |

---

## Part D · Step S3: the harness (core types, settings, report, commandlet) and `Tools\run_scan.py`, `score.py`

### D1. `Public\GameplayLintTypes.h`

| Type | Why |
|---|---|
| `UENUM() enum class EGameplayLintSeverity : uint8 { Warning, Error }` | A finding's level. `UENUM` so it can be a UPROPERTY and be named by reflection |
| `UENUM() enum class EGameplayLintDependents : uint8 { Off, Coarse, Precise }` | How incremental runs gather dependents (`02` §6.2). Its reflection data is also used to parse and print it (simplify pass) |
| `USTRUCT() struct FGameplayLintFinding` | Rule ID (`FName`, cheap to compare and hash), asset (`FSoftObjectPath`: a path, so a report survives unloaded assets), severity, `Where`, `Message`. `UPROPERTY()` without specifiers: reflected for the report, not editable |

### D2. `Public\GameplayLintSettings.h`, `Private\GameplayLintSettings.cpp`

| Part | Why |
|---|---|
| `UCLASS(MinimalAPI, config = Editor, defaultconfig, meta = (DisplayName = "Gameplay Lint")) : UDeveloperSettings` | A Project Settings page. `config = Editor` reads `[/Script/GameplayLintCore.GameplayLintSettings]` from the editor config; `defaultconfig` saves to the project's `DefaultEditor.ini`, which is committed. `UDeveloperSettings` registers the page automatically |
| `bEnabled`, `DisabledRules`, `Dependents` (`UPROPERTY(EditAnywhere, config)`) | The saved choices: master switch, rule IDs or prefixes that never run, dependent mode |
| `TOptional<TArray<FString>> CommandLineRules`, `TOptional<EGameplayLintDependents> CommandLineDependents` | The command-line overrides for benchmark configurations. **Not** UPROPERTYs, so they are never saved. `TOptional` says "not given" without a magic value |
| `PostInitProperties` | `Super` first (loads the saved config), then only on the class default object (the only instance ever asked, `GetDefault<>`): `FParse::Value(FCommandLine::Get(), TEXT("GameplayLintRules="), Rules, false)`. **`bShouldStopOnSeparator = false`** is needed because the value is a comma-separated list and FParse stops at a comma otherwise (`Parse.cpp:298`; review finding: `-GameplayLintRules=GL.Data,GL.UB` would have enabled only GL.Data). The dependent mode is parsed through `StaticEnum<EGameplayLintDependents>()->GetValueByNameString` (case-insensitive by default, `Class.h:2857–2869`); an unknown value logs a warning and keeps the saved setting instead of silently running another configuration (review finding) |
| `IsRuleEnabled(RuleId)` | With an override: `None` → off, `All` → on, else on if a prefix matches. Without: `bEnabled` and not disabled. `MatchesPrefix` matches `GL.UB` against `GL.UB.ClientTickGroup` but not `GL.UBX` |
| `GetCategoryName()` → `Plugins` | Groups the page under Plugins in Project Settings |

### D3. `Public\GameplayLintReportSubsystem.h`, `Private\…cpp`

| Part | Why |
|---|---|
| `UCLASS(MinimalAPI) : UEditorSubsystem` | The engine, not us, creates the rules, so they need a well-known place to report to. An editor subsystem lives as long as the editor session and is found with `GEditor->GetEditorSubsystem<>()` (`02` §4.1) |
| `Begin` / `End` / `IsOpen` | Only an open report records anything. Normal editing (validate on save) produces Message Log entries and keeps nothing |
| `RecordFinding`, `RecordCheckTime` (per rule, one duration per asset), `RecordDependents` (calls, assets added, ms) | What the commandlet's report needs. Times are kept raw; `score.py` computes medians and p99 |
| `struct FDependentStats` (plain C++) | Not reflected: nothing outside C++ reads it |

### D4. `Public\GameplayLintCommandlet.h`, `Private\GameplayLintCommandlet.cpp`

| Part | Why |
|---|---|
| `UCLASS(MinimalAPI) class UGameplayLintCommandlet : public UDataValidationCommandlet` | `-run=GameplayLint`. A child of the stock commandlet through its 5.8 virtual steps (`DataValidationCommandlet.h:33–42`): the gathering, the validation path and the engine validators stay the stock ones |
| Constructor `(const FObjectInitializer&) : Super(…)`; `UseCommandletResultAsExitCode = true` | The base class only has an `FObjectInitializer` constructor (`CustomConstructor`). Setting the flag makes the exit code this commandlet's verdict: otherwise any logged error turns 0 into 1 (`LaunchEngineLoop.cpp:4217–4224`), and validation errors are logged on purpose |
| `Main` | Parses `-Paths=` (with `bShouldStopOnSeparator = false`, review finding), `-Changes=`, `-Report=` (default `Saved/GameplayLint/Report.json`), `-FailOn=`. `-Paths` with `-Changes` is ignored with a warning: the changelist path gathers its own assets (review finding). Then `Super::Main`; 0 becomes 3 when the fail threshold was reached |
| `FilterAssetsToValidate` | The stock engine-content filter, then keep only packages under a `-Paths` folder (`"/Game/GLSeed/"` with the trailing slash, so `/Game/GLSeedOld` does not match) |
| `SetupValidationSettings` | Stock settings plus `bCollectPerAssetDetails = true` (every asset's errors and warnings, how the stock configuration is scored) and `bSilent = true` (no progress dialog) |
| `ValidateAssets` | Opens the report, times the phase, and runs either the stock full validation or a **pseudo-changelist** through the engine's own `ValidateChangelist` (`ReadChanges`: `M`/`D` packages, `F`/`X` files). An unreadable change list sets `bChangesUnreadable` |
| `ProcessValidationResults` | Exit 2 for an unreadable change list (review finding: it used to look like a clean pass); the fail threshold from `NumInvalid` / `NumWarnings`; writes the report |
| `WriteReport` | Streaming JSON (`TJsonWriter`, condensed): command line, change list, dependent mode, phase times (`MainStart - GStartTime`, main to validation, validation), counts, every validator's `FValidatorStatistics`, every asset's errors and warnings, the findings, per-rule per-asset times, dependent statistics. **UTF-8 without BOM** (review finding) |

### D5. `GameplayLintCore.Build.cs` (changed in S3)

Public: `DeveloperSettings` and `EditorSubsystem` join `DataValidation`, because public headers derive from their classes. Private: `Json`, `SourceControl` (`DataValidationChangelist.h` includes its changelist headers), `UnrealEd` (`GEditor`).

### D6. `Tools\run_scan.py`

| Part | Why |
|---|---|
| `pin_self(mask)` first, then `SetThreadExecutionState(ES_CONTINUOUS \| ES_SYSTEM_REQUIRED)` | Every editor it starts inherits the P-core mask (`05` A10, `09` T-1); the laptop doesn't sleep during a batch |
| `CONFIGS` | The eight configurations of `01` §7.2 plus `S` (stock): rule set and dependent mode per name. `command_for` builds the command line; incremental configurations get `-Changes` and no `-Paths` |
| `COMMON_ARGS` | `-unattended -nullrhi -nosplash -traceautostart=0 -BUILDMACHINE -stdout -FullStdOutLogOutput` for every configuration, so they differ only in what they validate. `-BUILDMACHINE` is needed by the pseudo-changelist (`02` §4.1) and is passed to all for equal conditions |
| Priming run, then rounds with the order rotated; `--runs` default = smallest multiple of the number of configurations ≥ 4 | The protocol (`01` §8) |
| `run_one` | `Popen`, timeout, kill in `finally`; accepts exit 0/1 for the stock commandlet and 0/3 for GameplayLint; reads the report; refuses to overwrite raw data |
| `parse_log` | The stock commandlet writes no report, so its per-asset result lines (`EditorValidatorSubsystem.cpp:1011–1033`) and "Execution of commandlet took" are counted from the log. Flags a Zen retry (`09` F-6) |
| `write_summary` | Median [min–max] of validation-phase, process and commandlet times per configuration, and the counts |

### D7. `Tools\score.py`

The definitions of `01` §7.3 in code, documented at the top of the file: rule-level detection over category `defect`; asset-level flagging (any error or warning, from the per-asset details); held-out and masked reported apart; false positives = control assets with a finding; wrong-rule findings listed; only `/Game/GLSeed/` counts; repeated runs must give identical finding sets; linear-interpolation percentiles. `--selftest` checks the numbers on a hand-made ground truth and report (`09` S3).

---

## Part E · Step S4: the rule base class, the walker and the tests

### E1. `Public\GameplayLintRule.h`, `Private\GameplayLintRule.cpp`

| Part | Why |
|---|---|
| `class FGameplayLintContext` (plain C++) | Collects one rule's findings on one asset: `Report(Where, Message)` with the rule's default severity, or with an explicit one. Holds a **reference** to the asset data: it never outlives the call |
| `UCLASS(MinimalAPI, Abstract) class UGameplayLintRule : public UEditorValidatorBase` | The engine instantiates every concrete validator subclass and skips abstract ones (`EditorValidatorSubsystem.cpp:381–395`), so this base never runs itself and needs no registration |
| `RuleId`, `Lesson`, `DefaultSeverity` (protected, plain members) | Set by each rule's constructor. Not UPROPERTYs: nothing edits or saves them |
| `AppliesTo`, `CheckAsset`, `GatherDependents` (virtual, empty defaults) | The whole author's API (`02` §4.1). Empty defaults instead of pure virtual: a UObject class must be constructible (its class default object) |
| `IsEnabled` | `Super::IsEnabled()` (the engine's `bIsEnabled` and `bIsConfigDisabled`) and our settings |
| `CanValidateAsset_Implementation` | The 5.8 signature (`EditorValidatorBase.h:51`); calls `AppliesTo` |
| `ValidateLoadedAsset_Implementation` | Times `CheckAsset` with `FPlatformTime::Cycles64`; for each issue an `AssetMessage` with an asset token and the text `[RuleId] Where: Message` (an error makes the asset invalid) and a report entry; then **always ends with a result**: `AssetPasses` unless invalid (the engine ensures otherwise, `EditorValidatorBase.cpp:52–62, 152`) |
| `GetAssetsToValidateFromChangelist` | The engine's incremental hook (`EditorValidatorBase.h:37–41`): calls `GatherDependents` in the configured mode, times it, reports it. `Off` returns nothing |
| `GEditor ? GEditor->GetEditorSubsystem<…>() : nullptr` | Rules are editor-only, but a null check costs nothing and keeps the base safe in any host |

### E2. `Public\GameplayLintWalker.h`, `Private\GameplayLintWalker.cpp`

| Part | Why |
|---|---|
| `struct FValue { Data, Property, Path }` | The value's memory, the property that declares it (for an element of an array, set or map: the **container** property, where UHT puts the metadata), and a readable path |
| `ForEachValue(Asset, Struct, Visit)` | A DataTable: every row (`Row[R05].`); a Blueprint: the class default object (`Default.`) and every component template through `AActor::ForEachComponentOfActorClassDefault` (`Component[Name].`, inherited ones and child overrides included, `Actor.cpp:7349–7400`); anything else: the object |
| `Walk` (one loop, since the simplify pass) | `FPropertyValueIterator` with `FProperty::StaticClass()` (match everything, so containers are seen before their elements); **skip transient properties** with `SkipRecursiveProperty` (e.g. `FGameplayTagContainer::ParentTags`, `GameplayTagContainer.h:623–624`, which is derived data and would double-count tags); a matching struct is visited and not searched further; an instanced reference owned by the object is collected for the next walk |
| `VisitObject` | Walks an object, then its instanced sub-objects; a `Visited` set stops cycles and the double visit of a native component that is both a CDO sub-object and a component template |
| `ForEachValueIn` | The same walk for a raw memory block (DataTable rows, tests) |
| `FindMetaData` | `FProperty::FindMetaData` under `WITH_METADATA` (null in builds without metadata) |

### E3. `GameplayLintBench\Private\Tests\GLTestRule.h` and `GameplayLintCoreTests.cpp`

| Part | Why |
|---|---|
| `UGLTestRule` | A concrete rule that reports an error on names containing "Bad" and a warning on "Warn". **`IsEnabled()` returns false**: the engine instantiates every concrete validator (`01b` #1), and this one must never run in real scans; the tests call `ValidateLoadedAsset` directly |
| `EAutomationTestFlags::EditorContext \| ProductFilter` | Runs in the editor; one filter flag, as the test macro requires. Since S5 it comes from `GLTestHelpers.h` together with `MakeTag` (`05` F4) |
| `GameplayLint.Walker.EveryVariant` | One tag per structural variant (top, container, nested, array, map, instanced object, instanced struct) on an in-memory data asset; the walker must find each exactly once, with element and sub-object paths |
| `GameplayLint.Walker.DataTableRows` | Two rows, one tag each; paths start with `Row[R01].` |
| `GameplayLint.Walker.Metadata` | Exactly two `LintMonotonic` curves (CooldownByLevel "Increasing"; FalloffByDistance "Decreasing", since S6) and one `LintRequired` curve |
| `GameplayLint.Rule.Contract` | Good / Bad / Warn objects: result, error and warning counts, the rule ID in every message, and the rule never enabled |
| `Tools\run_tests.bat` | Adapted from UE5-UpdateBudget: `-ExecCmds="Automation RunTests <filter>;Quit"`, headless, `-ReportExportPath`, exit code = result |

---

## Part F · Data rules (S5–S6)

The first rule code. Everything here is in the `GameplayLintDataRules` pack; the core did not change (`02` §3).

### F1. `Private\GLDataRules.h`

| Part | Why |
|---|---|
| In `Private\`, not `Public\` | Nothing may include a pack (the dependency rule, `02` §3). UHT still reflects private headers, so the engine finds the classes |
| `UCLASS() class UGLRule_UnknownTag : public UGameplayLintRule`, and the same for `UGLRule_DanglingRow` | Concrete, so the validator subsystem instantiates each one at start-up (`EditorValidatorSubsystem.cpp:381–395`); no registration code anywhere. Not `MinimalAPI`: no other module uses the class |
| Constructor; `AppliesTo` and `CheckAsset` overrides | The rule author's whole API (`05` E1). The constructor sets `RuleId`, `Lesson` and `DefaultSeverity` |

### F2. `Private\GLDataRules.cpp`

| Part | Why |
|---|---|
| `#include UE_INLINE_GENERATED_CPP_BY_NAME(GLDataRules)` | The generated reflection code for this header, compiled in this file |
| `LOCTEXT_NAMESPACE "GameplayLintDataRules"` | Every message is localisable text (`06` §23) |
| `AppliesTo`: `!AssetData.IsRedirector()` | Any asset can hold a tag or a row handle, so the rules apply to every class; a redirector holds no data of its own. `AppliesTo` sees only registry data, so it must not load anything |
| **UnknownTag** `CheckAsset`: `ForEachValue(Asset, FGameplayTag::StaticStruct(), …)` | The walker visits every tag value: top-level, inside containers (`FGameplayTagContainer` stores an array of `FGameplayTag`), nested, array and map elements, instanced objects and structs, DataTable rows, Blueprint defaults and templates |
| `RequestGameplayTag(Name, /*ErrorIfNotFound*/ false).IsValid()` | The dictionary check. `FGameplayTag::IsValid()` would only test for `None` (`GameplayTagContainer.h:138–141`) |
| One finding per unknown value, at `Value.Path` | The path names the field ("List[0].Tag"), so the message says where to look |
| **DanglingRow** `GLDataRules::CheckHandle` | One decision table for both handle types: table set and row empty (only an error if the property says `LintRequired`); table set and row missing; row set but no table; both empty and required |
| `FindMetaData(Value, "LintRequired")` | The annotation lives on the declaring property; for an array element, the walker hands over the array property (`05` E2) |
| `Handle.DataTable->GetRowMap().Contains(Handle.RowName)` | A direct map lookup. `FindRow` would log a warning for every missing row and needs the row type |
| The same for `FCurveTableRowHandle` | Same defect, other table type; `UCurveTable::GetRowMap()` holds the curves by row name |
| No asset loads | A handle's table is a hard reference, so it was loaded with the asset |
| **Curve** `IsFiniteKey` (two overloads) | Rich keys: time, value, both tangents and both tangent weights, because cubic interpolation reads the tangents. Simple keys: time and value |
| `CheckCurveData<KeyType>(Keys, DefaultValue, Monotonic, bOwnsKeys, Where, Lint)` | One template for both key types (`04` §13). Non-finite data first: a key, or the default value, which an empty curve returns. It is reported only if `bOwnsKeys`, and the direction is then skipped, so one bad key gives one finding. Then the direction, non-strict, on key values only. Two complete messages instead of a word inserted with `FromString`, so both are localisable |
| `CheckAsset`: `UCurveFloat` and `UCurveTable` first | Curve data that is not in a reflected `FRuntimeFloatCurve`: a curve asset's `FloatCurve` and a table's rows (rich or simple, `CurveTable.h:51–55`; one generic lambda for both) |
| `CheckAsset`: `ForEachValue(…, FRuntimeFloatCurve::StaticStruct(), …)` | `GetRichCurveConst()` returns the external curve if one is set (`CurveFloat.cpp:28–36`). Then its keys belong to that asset: `bOwnsKeys` is false, and `Where` names the external asset |
| `LintMonotonic`: "Increasing", "Decreasing", else a finding | An unknown value is reported, never guessed as one of the two (S6 code review) |
| `LintRequired`: `!Data.HasAnyData()` | "Has data" as the engine defines it: a key, or a default value other than `MAX_flt` (`RealCurve.h:140–143`). An external curve with no keys does not pass |

### F3. `GameplayLintDataRules.Build.cs` and `GameplayLintDataRules.uplugin` (changed in S5)

| Part | Why |
|---|---|
| `DataValidation` in the private dependencies | The link rule: a module that defines a rule links the validator base itself (`02` §3; the S1–S4 link failure, `09`) |
| `GameplayTags` | `UGameplayTagsManager` and `FGameplayTag` |
| `"DataValidation"` under `"Plugins"` in the `.uplugin` | `DataValidation` is a plugin module, so the plugin must list the plugin (`06` §5); otherwise UBT warns (`UEBuildTarget.cs:1564–1579`) |

### F4. `GameplayLintBench\Private\Tests\GameplayLintRuleTests.cpp` and `GLTestHelpers.h`

| Part | Why |
|---|---|
| `GLTestHelpers.h`: `Flags`, `MakeTag` | Shared by both test files of the module (the simplify pass in S5 found the second copy). `inline` so that one header can define them in several `.cpp` files |
| `NewRule(Test, ClassPath)`: `FindObject<UClass>(nullptr, TEXT("/Script/GameplayLintDataRules.GLRule_UnknownTag"))`, returned as a `TStrongObjectPtr` (since S7) | The packs have no public headers, so the tests find a rule class by its path, as the engine does. The rule object is rooted, and so are the tables and curve assets reused across cases: compiling a Blueprint collects garbage (`BlueprintCompilationManager.cpp:423–427`), and the first S7 run crashed because it freed an unrooted rule (`07`). A null outer means "the name starts with a package" (`UObjectGlobals.h`, `StaticFindObject`) |
| `Validate(Rule, Asset, OutResult)` | The engine's own entry point, `UEditorValidatorBase::ValidateLoadedAsset`, with a fresh `FDataValidationContext`; returns every message text |
| `Expect(Test, Rule, RuleId, Case, Setup, Expected)` | One case: a new in-memory data asset, `Setup` changes it, then the exact number of findings, the result (Invalid exactly when a finding is expected), a substring per expected finding, and the rule ID in every message |
| `GameplayLint.Rules.UnknownTag` | No tags: valid. **A declared tag** (the first one `RequestAllGameplayTags` returns), single and in a container: valid. Without this case, a rule that flagged every tag would pass (code review, S5); if the dictionary is empty, the case is skipped and the test says so. An unknown top-level tag. An unknown tag in a container and in an array element (the path names `List[0]`) |
| `GameplayLint.Rules.Curve` (S6) | Near-misses (a rising cooldown with a flat segment, a falling fall-off); NaN value, NaN time, NaN tangent, infinite key in a nested payload; an infinite key in a must-rise curve gives **one** finding; a fall in a must-rise curve and a rise in a must-fall curve; an empty required curve; a required curve with only a default value (valid); a required curve pointing at an empty curve asset; a curve pointing at a broken curve asset (nothing on the owner), and that asset itself (a finding); a curve table with an infinite row |
| `GameplayLint.Rules.DanglingRow` | An in-memory DataTable (`R00`) and CurveTable (`C00`), **unnamed**, so a second run in the same editor session gets fresh objects. Valid handles; a missing DataTable row; a missing CurveTable row; a row without a table in a nested payload; an empty required handle; a required handle with a table but no row |

---

## Part G · The UpdateBudget pack (S7)

A second rule pack, built against the core's public API only. **E1:** the S7 commit changes 0 lines under `Plugins/GameplayLintCore/` (`09` P5-6).

### G1. `Private\GLUpdateBudgetRules.h`

| Part | Why |
|---|---|
| `UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Gameplay Lint: UpdateBudget")) UGameplayLintUpdateBudgetSettings : UDeveloperSettings` | The pack's own Project Settings page (`06` §16). The core knows nothing about it |
| `TArray<FSoftClassPath> BudgetClientClasses`, `BudgetScopeActorClasses`, `PerFrameAllowList` (`MetaClass` filters the class picker) | The pack never links UE5-UpdateBudget's code: classes are named by path and resolved only if loaded (`06` §26) |
| `TEnumAsByte<ETickingGroup> SchedulerTickGroup = TG_PrePhysics` | Where UE5-UpdateBudget's scheduler ticks; clients must tick in the same group |
| Three `UCLASS() UGLRule_*` classes | Same shape as the data rules (`05` F1) |

### G2. `Private\GLUpdateBudgetRules.cpp`

| Part | Why |
|---|---|
| `GLUpdateBudget::FClasses` | Resolves the three class lists **once per checked asset** (S7 code review: before, every template of every asset resolved every path in every rule) and answers `IsClient`, `IsInScope`, `IsAllowListed` with `IsChildOf` |
| `StartsTicking(Component)` | `bCanEverTick && (bStartWithTickEnabled \|\| bAutoActivate)`. The tick manager enables a tick that starts enabled (`ActorComponent.cpp:1739–1752`); `InitializeComponents` activates every auto-activating component (`Actor.cpp:6399`), and `Activate` enables its tick (`ActorComponent.cpp:2815–2819`) |
| `TicksEveryFrame(Component)`, `TicksEveryFrame(Actor)` | Starts ticking and has no `TickInterval`. The actor's own tick has no auto-activation |
| `NativeParent(AssetData)` | `UBlueprint::GetBlueprintParentClassFromAssetTags` (`Blueprint.cpp:2316–2333`): the native parent from registry tags, so `AppliesTo` loads nothing |
| `Templates(ActorClass)` | `AActor::ForEachComponentOfActorClassDefault`: native, added and inherited templates, with child overrides (`06` §21) |
| `ComponentPath` | `Component[Name]`, without the `_GEN_VARIABLE` suffix of Blueprint templates |
| `ForEachClient` | The client templates of an actor Blueprint, or the defaults of a Blueprint subclass of a client (`Default`) |
| **TickOutsideBudget** | In scope: a configured actor class, or an actor that already has a client (it opted in). Then the actor's own tick and every template that is neither a client nor allow-listed |
| **ClientTickGroup** | Only for clients that start ticking; the group is compared with `TickGroup.GetValue()` (a `TEnumAsByte`) and named with `StaticEnum<ETickingGroup>()->GetDisplayNameTextByValue` |
| **ClientNeverTicks** | `!StartsTicking(Client)`; two complete messages, one for `bCanEverTick` false, one for "starts disabled and does not auto-activate" |

### G3. `Build.cs`, `.uplugin` and `Config\DefaultEditor.ini`

| Part | Why |
|---|---|
| `DataValidation`, `DeveloperSettings` in the private dependencies; the `DataValidation` plugin in the `.uplugin` | The link rule (`02` §3), and the settings base class |
| `DefaultEditor.ini`: `+BudgetClientClasses=/Script/GameplayLint.GLFixtureBudgetClient`, `+BudgetScopeActorClasses=/Script/GameplayLint.GLFixtureAgent` | Points the pack at the fixture classes. A config array with entries replaces the constructor's list (`Obj.cpp`, `ProcessArrayProperty`: `EmptyAndAddValues`), so the client list is only the fixture client; the allow list keeps its defaults. Here `+` is right: `DefaultEditor.ini` is a layered file (`04` §4) |

### G4. `GameplayLint.Rules.UpdateBudget` (in `GameplayLintRuleTests.cpp`)

| Part | Why |
|---|---|
| `MakeBlueprint(Parent, Name, AddComponents, SetDefaults)` | An in-memory Blueprint with a unique name: templates are set before the compile, class defaults after it |
| TickOutsideBudget cases | Near-misses: an interval ticker, a start-disabled ticker, allow-listed skeletal mesh and projectile movement. Findings: a per-frame ticker, a start-disabled ticker that auto-activates, a ticking native parent. Scope: a plain actor is out of scope, unless it opted in with a client |
| ClientTickGroup cases | A client in the right group; a client template in `TG_DuringPhysics`; a client subclass whose defaults are in `TG_PostPhysics`; a never-ticking client in another group, which is reported only by ClientNeverTicks |
| ClientNeverTicks cases | A start-disabled template; start-disabled subclass defaults; a start-disabled client that auto-activates (no finding); a ticking client |
| Depends on the project's config | The settings class is private to the pack. The test runs against the checked-in `DefaultEditor.ini`, and changing that file is meant to fail the test |
