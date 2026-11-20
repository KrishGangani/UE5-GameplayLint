# 04 · C++ Fundamentals: GameplayLint

**Rule:** only the C++ (and C#) ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step. After Phase 4 the project has almost no C++ yet, so the list is short on purpose.

---

## 1. Translation units and `#include`

```cpp
// A .cpp file plus everything it #includes is one "translation unit": the compiler sees it as one long file.
#include "Modules/ModuleManager.h"   // pastes the declarations of that header in here
```
- The compiler compiles each `.cpp` separately; the linker joins the results.
- A header is included for the **declarations** a file uses. Including more than needed slows every build that touches the file ("include what you use").
- **In our code:** each module `.cpp` (`GameplayLintModule.cpp`, `GameplayLintCoreModule.cpp`, …) includes exactly one header, `Modules/ModuleManager.h`, because the only thing it uses is the module macro declared there.

## 2. Macros: code that writes code

```cpp
#define SQUARE(X) ((X) * (X))     // the preprocessor replaces SQUARE(3) with ((3) * (3)) before compiling
```
- A macro is text substitution done before the compiler runs. Unreal uses macros where C++ alone can't generate the boilerplate (exported entry points, reflection glue).
- **In our code:** `IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintCore)` expands to the function the engine calls to create the module (06 §1). We never write that function by hand.

## 3. C# in the build files (`*.Build.cs`, `*.Target.cs`)

```csharp
public class MyModule : ModuleRules                  // a C# class deriving from an engine-provided base class
{
	public MyModule(ReadOnlyTargetRules Target) : base(Target)   // constructor; ": base(Target)" calls the parent's constructor first
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "Engine" });   // a list of strings, appended to
	}
}
```
- These files are **C#, not C++**. UnrealBuildTool compiles and runs them to learn how to build each module and target. They never end up in the game.
- `new string[] { … }` creates an array; `AddRange` appends all of it to a list. `Add` appends one item.
- **In our code:** every module has one `*.Build.cs`; the project has two `*.Target.cs` (game and editor). The class name must match the file name, which UBT relies on (05 Part A).

## 4. Data files that are not code

- `.uproject` and `.uplugin` are **JSON**: objects in `{ }`, arrays in `[ ]`, `"key": value` pairs. The engine and UBT read them to learn which modules and plugins exist.
- `.ini` files are Unreal's **config** format: `[Section]` headers, `Key=Value` lines, `+Key=` appends to an array, `-Key=` removes from it, `;` starts a comment.
- `.bat` files are **Windows batch scripts** run by `cmd.exe` (`%~dp0` is the script's folder; `%~1` is the first argument without quotes; `call` runs another batch file and returns).
- `core_mask.py` is **Python**: `ctypes` calls Windows API functions from Python (05 A10).
