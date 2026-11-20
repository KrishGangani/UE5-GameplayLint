# 04 · C++ Fundamentals: GameplayLint

**Rule:** only the C++ (and C#) ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step: sections 1–4 came with Phase 4, sections 5–12 with steps S1–S4 (fixtures, seeder, core, walker).

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
- `.ini` files are Unreal's **config** format: `[Section]` headers, `Key=Value` lines, `+Key=` appends to an array, `-Key=` removes from it, `;` starts a comment. **Only in the layered files** (`Default*.ini` and the engine's `Base*.ini`): a file the engine reads on its own, such as `Config/Tags/*.ini`, is parsed without these commands, so `+Key` becomes part of the key's name. There a repeated plain `Key=` line appends (`07`, S1–S4).
- `.bat` files are **Windows batch scripts** run by `cmd.exe` (`%~dp0` is the script's folder; `%~1` is the first argument without quotes; `call` runs another batch file and returns).
- `core_mask.py` is **Python**: `ctypes` calls Windows API functions from Python (05 A10).

## 5. Classes, inheritance and `virtual` (S1–S4)

```cpp
struct FShape { virtual float Area() const { return 0.f; } virtual ~FShape() = default; };
struct FSquare : FShape { float Side = 1.f; float Area() const override { return Side * Side; } };
const FShape& Any = FSquare{};   // Any.Area() calls FSquare::Area: the call is chosen at run time
```

- `class B : public A` makes `B` an `A` with more. A `virtual` function can be replaced in a derived class, and the call through a base reference or pointer reaches the derived version.
- `override` says "this replaces a base function". If the signature doesn't match any base function, the compiler fails instead of silently adding a new function. That matters here: the 5.8 validator signatures differ from the 5.0–5.3 ones in older tutorials (`07`).
- `Super::` is Unreal's alias for the direct base class (`Super::PostInitProperties()` runs the base version first).
- **In our code:**
  - `UGameplayLintRule` overrides four `UEditorValidatorBase` functions and declares three empty `virtual` ones (`AppliesTo`, `CheckAsset`, `GatherDependents`) for rule packs to replace (`05` E1).
  - `UGameplayLintCommandlet` overrides five steps of the stock commandlet and `Main`.
  - `FGLFixtureRow::IsDataValid` overrides the row base's empty check.

## 6. `const`, references and pointers

```cpp
void Print(const FString& Text);          // by const reference: no copy, and Print can't change it
int32 Count() const;                      // a const member function: it doesn't change the object
const void* Data = nullptr;               // a pointer to memory we only read; nullptr = "points nowhere"
```

- `T&` is another name for an existing object. `const T&` passes big things without copying and promises not to change them. Almost every parameter in our code is one (`const FAssetData&`, `const FString&`).
- A `const` member function may be called on a `const` object. Rules check assets in `const` functions (`CheckAsset(...) const`): the compiler then stops a rule from changing its own state while it checks.
- `T*` holds an address. `nullptr` means none; check before use. `TObjectPtr<T>` is Unreal's pointer type for `UPROPERTY` object members (it behaves like `T*` in code).
- **In our code:** the walker hands out `FValue { const void* Data; const FProperty* Property; }`, which is read-only memory plus the reflection data that says what it is.

## 7. Casts

```cpp
const FGameplayTag* Tag = static_cast<const FGameplayTag*>(Value.Data);   // "trust me": no check
const FStructProperty* Struct = CastField<FStructProperty>(Property);      // checked: nullptr if it isn't one
```

- `static_cast<T>` converts without a run-time check. It is correct only when something else guarantees the type. Here the walker only visits values of the struct type that was asked for.
- Unreal's `Cast<T>` (objects) and `CastField<T>` (properties) check the real type and return `nullptr` when it doesn't match. Use them whenever the type isn't already guaranteed.
- **In our code:** each rule casts the walker's `Data` to the struct it asked for; the walker uses `CastField` to find struct and object properties.

## 8. Namespaces, `constexpr` and `enum class`

```cpp
namespace GameplayLintSeed { constexpr int32 TableRows = 20; }   // a compile-time constant, scoped to a name
enum class EDataKind : uint8 { TagSingle, RowHandle };           // scoped enum stored in one byte
const EDataKind Kind = EDataKind::RowHandle;                     // must be written with its enum name
```

- A `namespace` groups names so that `GameplayLintSeed::SaveAsset` can't clash with another module's `SaveAsset`. Helpers that only one `.cpp` uses live in a namespace inside that file, so no header shows them.
- `constexpr` values are known while compiling: no hidden global variable, and they can size arrays.
- `enum class` values don't convert to integers by accident and don't leak their names into the surrounding scope. `: uint8` sets the storage size.
- **In our code:** the seeder's constants and `EDataKind`, the commandlet's `EFailOn`, and the reflected `EGameplayLintSeverity` and `EGameplayLintDependents` (those also carry `UENUM`, so `StaticEnum` can parse their names, `06` §23).

## 9. Lambdas, captures and `TFunctionRef`

```cpp
int32 Found = 0;
auto Count = [&Found](const FValue& Value) { ++Found; };          // [&Found]: use Found by reference
Rows.RemoveAll([this](const FAssetData& Asset) { return !Keep(Asset); });  // [this]: call members
```

- A lambda is an unnamed function object written in place. The `[...]` part lists what it *captures* from the surrounding code: `[&]` everything by reference, `[this]` the object's members, `[X]` a copy of `X`.
- `TFunctionRef<void(const FValue&)>` accepts any callable with that signature **without copying it or allocating memory**. It only points at the caller's lambda, so it must not be stored and used after the call returns. That is why the walker takes one: it calls `Visit` immediately and keeps nothing.
- **In our code:** every rule passes a lambda to `GameplayLint::ForEachValue`; the commandlet's path filter is a `RemoveAll` lambda; the seeder's tag-file writer is a local lambda.

## 10. Range-based `for`, `auto` and moving

```cpp
for (const FAssetData& Asset : Assets) { ... }   // every element, by const reference
auto Write = [this](...) { ... };                // auto: the compiler writes the (unnameable) lambda type
Rules = MoveTemp(Parts);                         // hand over Parts' memory instead of copying it
```

- `for (X : Container)` visits each element. Write `const T&`, or `for (T Copy : …)` copies every element.
- `auto` lets the compiler name the type. Our code uses it only where the type can't be written (lambdas), so readers still see real types elsewhere.
- `MoveTemp(X)` (Unreal's `std::move`) lets the destination take over `X`'s buffer; `X` is left empty. It is used where a local array is no longer needed after the assignment.

## 11. Conditional compilation: `#if WITH_EDITOR`

```cpp
#if WITH_EDITOR
virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
```

- `#if` is decided by the preprocessor (§2) before compiling: the lines inside simply don't exist in builds where the condition is 0.
- `WITH_EDITOR` is 1 in editor builds and 0 in a packaged game. The engine declares `FTableRowBase::IsDataValid` only inside the same guard. The override must be guarded too, or the game build fails ("does not override any base function").
- **In our code:** `FGLFixtureRow::IsDataValid` in the runtime fixture module. The lint modules don't need guards: they are `Editor` modules, never built into the game (`06` §4).

## 12. `std::initializer_list` and forward declarations

```cpp
FRuntimeFloatCurve MakeCurve(std::initializer_list<FCurveKey> Keys);   // call as MakeCurve({{0, 1}, {1, 2}})
struct FValidateAssetsSettings;                                          // "this type exists": enough for a reference
virtual void SetupValidationSettings(FValidateAssetsSettings& Settings) override;
```

- `std::initializer_list<T>` lets a function take a brace list of values. The seeder writes each curve's keys in place this way.
- A **forward declaration** tells the compiler a type exists without its definition. Using it by reference or pointer then needs no `#include`. The commandlet header forward-declares the two validation structs, so files that include it don't pull in the whole validation subsystem header. Only the `.cpp`, which uses their fields, includes it.
