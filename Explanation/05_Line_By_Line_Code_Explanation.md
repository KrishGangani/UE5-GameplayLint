# 05 · Line By Line Code Explanation: GameplayLint

**Rule:** every file that exists in `GameplayLint\` and `Tools\` is explained here, line by line, in the same step it is written (Hard rule 13). Sections follow the build order in [03_Implementation_Roadmap.md](03_Implementation_Roadmap.md).

| Part | Step | Files |
|---|---|---|
| A | Phase 4, F1–F3 (foundation) | `.uproject`, two `Target.cs`, five `Build.cs`, five module `.cpp`, three `.uplugin`, three config `.ini`, `Tools\*.bat`, `Tools\core_mask.py` |

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
