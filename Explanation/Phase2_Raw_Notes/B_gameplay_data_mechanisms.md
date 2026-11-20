# B · Gameplay-data mechanisms the rules depend on (UE 5.8.4)

Working notes, 2026-11-21. Paths are relative to `Engine/`.

## B1. Gameplay tags

**What happens when an asset holds a tag that is not in the dictionary**

- Loading, single tag: `FGameplayTag::PostSerialize` (`Source/Runtime/GameplayTags/Private/GameplayTagContainer.cpp:1632–1647`) calls `UGameplayTagsManager::SingleGameplayTagLoaded` → `RedirectSingleGameplayTag` (`Source/Runtime/GameplayTags/Private/GameplayTagsManager.cpp:1098–1127`). With no redirect and `WITH_EDITOR`, it requests the tag with `ErrorIfNotFound = false` and, if invalid and `ShouldWarnOnInvalidTags()`, logs `UE_ASSET_LOG(LogGameplayTags, Warning, ..., "Invalid GameplayTag %s found in property %s. Package:%s")` (`:1123`). Comment: "Warn about invalid tags at load time in editor builds, too late to fix it in cooked builds".
- Loading, container: same check inside `RedirectTagsForContainer` (`:1063–1078`, warning at `:1076`).
- Text import: `ImportSingleGameplayTag` (`:1130–1189`). With `PPF_SerializedAsImportText` an unknown name is **kept** ("leave invalid ones the same way normal serialization does to avoid data loss", `:1176–1178`) and a deferred warning is logged (`:1160–1171`). The DataTable CSV/JSON import path is one way unknown tags enter data **[inferred: which import path passes this flag was not traced]**.
- The warning is on by default: `bShouldWarnOnInvalidTags = true` in the manager constructor (`:343`) and `WarnOnInvalidTags = true` in `UGameplayTagsSettings` (`Source/Runtime/GameplayTags/Private/GameplayTagsSettings.cpp:65`), loaded at `:735`. Clearing invalid tags is deprecated ("We never clear invalid tags ...", `Classes/GameplayTagsSettings.h:115–117`).
- `FGameplayTag::IsValid()` is only `TagName != NAME_None` (`Classes/GameplayTagContainer.h:138–141`). An unknown tag name is "valid" by that test. A rule must ask the manager: `UGameplayTagsManager::RequestGameplayTag(Name, /*ErrorIfNotFound*/ false)` (`Classes/GameplayTagsManager.h:375`) and check the result.

**Consequence for the baseline:** the stock Data Validation run loads each asset with load-log capture on (A5), so an unknown tag in an asset that was **not already loaded** surfaces as a validation *warning* ("Warnings loading asset ..."). The stock path should therefore detect many unknown-tag defects already. This must be measured, not assumed. It lowers the expected marginal value of a tag rule, and the write-up must say so.

**Where tags come from**
- `ImportTagsFromConfig = true` by default (`GameplayTagsSettings.cpp:64`). The project's `Config/Tags/` folder is a search path for extra tag `.ini` files (`GameplayTagsManager.cpp:682–683`). → The seeding pipeline can define "removable" tags in their own file under `Config/Tags/` and delete that file to simulate a tag removed from config.

**Tag references are in the asset registry**
- Saving a tag marks it as a searchable name: containers `UnderlyingArchive.MarkSearchableName(FGameplayTag::StaticStruct(), Tag.TagName)` (`GameplayTagContainer.cpp:957`), single tags `Ar.MarkSearchableName(...)` if `IsValid()` (non-None) (`:1647`).
- The editor's tag tools query exactly that: `FAssetIdentifier TagId = FAssetIdentifier(FGameplayTag::StaticStruct(), TagNameToDelete); ... GetReferencers(TagId, Referencers, EDependencyCategory::SearchableName)` and refuse to delete a referenced tag: "Cannot delete tag {0}, still referenced by {1} and possibly others" (`Plugins/Editor/GameplayTagsEditor/Source/GameplayTagsEditor/Private/GameplayTagsEditorModule.cpp:682–690`). The same query powers "unused tags" (`:1222–1227`).
- So the editor UI already blocks the obvious way to break a reference. Defects come from paths the UI does not guard: hand-edited or merged `.ini` files, a tag source plugin that is not enabled, text imports. That is where the seeded "tag removed" defects come from.

## B2. DataTable and CurveTable row handles

- `FDataTableRowHandle` (`Source/Runtime/Engine/Classes/Engine/DataTable.h:424–484`): `UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<const UDataTable> DataTable` (`:436–437`), `FName RowName` (`:440–441`), `IsNull()`, `GetRow<T>()`, which **only logs at run time** when the table is missing (`:457`).
- On save, a valid handle records a searchable name on the table: `if (Ar.IsSaving() && !IsNull() && DataTable) Ar.MarkSearchableName(DataTable, RowName);` (`Source/Runtime/Engine/Private/DataTable.cpp:1115–1121`). `FCurveTableRowHandle` does the same (`Source/Runtime/Engine/Private/CurveTable.cpp:1158–1164`). Both declare `WithPostSerialize = true` (`DataTable.h:491`, `CurveTable.h:336`).
- `FAssetIdentifier(UObject* SourceObject, FName InValueName)` → package of the object, object name, value name (`Source/Runtime/CoreUObject/Private/AssetRegistry/AssetData.cpp:1386–1395`). So "who references row R of table T" is `GetReferencers(FAssetIdentifier(T, R), ..., SearchableName)`, and "which rows does package P reference" is `GetDependencies(FAssetIdentifier(P), ..., SearchableName)`, **without loading P**.
- A handle with `DataTable == null` but a `RowName` is **not** recorded (the `&& DataTable` condition). Such a handle can only be found by loading the asset.
- `UDataTable::GetAssetRegistryTags` adds only `RowStructure` (and import source data) (`DataTable.cpp:347–361`). **Row names are not in the registry**, so checking "does row R exist" needs the table loaded once. Tables are few compared to their referencers, so a per-run cache of row names per table is cheap. **[inferred cost; measure]**
- `UDataTable::IsDataValid` (`DataTable.cpp:418–462`) now calls `FTableRowBase::IsDataValid` for each row and appends "[Row Name: X]" to row issues. The default `FTableRowBase::IsDataValid` returns `NotValidated` (`DataTable.h:67`). So a row struct can validate itself, but nothing checks a *handle that points into* a table. (A community plugin written against 5.3 says tables had no validation at all then, C5. That is no longer true in 5.8.)
- GAS already validates one specific case: `FScalableFloat::IsDataValid` reports "RowName {1} does not exist in CurveTable {2}" (`Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Private/ScalableFloat.cpp:22–38`). It runs only where a GAS class calls it (`GameplayEffect.cpp`, `GameplayAbility.cpp` and the effect components, grep list). It is credited, and a generic rule must not duplicate it inside `FScalableFloat`.

## B3. Curves

- `FRuntimeFloatCurve`, `UCurveFloat` (`FloatCurve` is an `FRichCurve`), `FRichCurve::AddKey`. Keys are kept sorted by time by the curve API **[inferred from usage; not re-read]**.
- No engine `IsDataValid` checks curve key values for NaN/Inf or monotonicity (grep of `IsDataValid` in `Source/Runtime/Engine/Private/Curves/` found none).
- "Must be monotonic" is a meaning that only the gameplay programmer knows (a cooldown curve must not decrease with level; a damage fall-off must not increase with distance). It needs an annotation. UPROPERTY metadata is free-form: UHT's metadata code (`Source/Programs/Shared/EpicGames.UHT/Types/UhtMetaData.cs`) only remaps a few old keys (`:550`) and rejects a key seen twice with different values (`:569`). I found no check against unknown keys → a project key such as `meta=(LintMonotonic)` should compile **[inferred; verify when first used]**. Metadata exists only in editor builds (`WITH_METADATA`), which is fine for an editor tool.

## B4. Walking an asset's data

- `FPropertyValueIterator` (`Source/Runtime/CoreUObject/Public/UObject/UnrealType.h:7512–7703`): constructed with a property class (e.g. `FStructProperty::StaticClass()`), a `UStruct*` and a value pointer, `EPropertyValueIteratorFlags::FullRecursion` (`:7505–7509`) recurses into structs, arrays, maps, sets and optionals (`EPropertyValueFlags` `:7619–7632`). `SkipRecursiveProperty()` (`:7592`), `GetPropertyChain` (`:7602`), `GetPropertyPathDebugString()` gives "ActionsArray[3].ActionInfo.ActionPower" style paths (`:7604–7617`).
- It does **not** follow object references, so instanced sub-objects must be visited explicitly.
- Where gameplay data lives, per asset type:
  - plain data asset: the asset object itself plus instanced sub-objects outered to it;
  - DataTable: each row in `GetRowMap()` with `RowStruct`;
  - Blueprint: the generated class's CDO, plus component templates.
- Component templates of an actor Blueprint, including inherited ones and child-Blueprint overrides: `AActor::ForEachComponentOfActorClassDefault(ActorClass, ComponentClass, Func)` (`Source/Runtime/Engine/Classes/GameFramework/Actor.h:4249`). It visits native CDO components, then every BP class in the hierarchy's SCS nodes through `Node->GetActualComponentTemplate(ActorBlueprintGeneratedClass)`, which applies inherited-component overrides (`Source/Runtime/Engine/Private/Actor.cpp:7349–7400`).
- `UBlueprint::IsDataValid` validates the CDO, SCS, inheritable component handler, component templates and timelines (`Source/Runtime/Engine/Private/Blueprint.cpp:2233–2270`). This is the class-level path that runs before validators (A4).

## B5. Tick configuration as data (for the UpdateBudget rules)

`FTickFunction` (`Source/Runtime/Engine/Classes/Engine/EngineBaseTypes.h`):
- `:195–196` `UPROPERTY(EditDefaultsOnly, Category="Tick", AdvancedDisplay) TEnumAsByte<ETickingGroup> TickGroup`.
- `:212–213` `UPROPERTY() uint8 bCanEverTick:1`. **Not editable in the editor**, only serialised.
- `:216–217` `UPROPERTY(EditDefaultsOnly, Category="Tick") uint8 bStartWithTickEnabled:1`. A designer can untick "Start with Tick Enabled" on a component template.
- `:257–258` `UPROPERTY(EditDefaultsOnly, Category="Tick", meta=(DisplayName="Tick Interval (secs)")) float TickInterval`.

Defaults (from UE5-UpdateBudget's verified findings, re-read): components `TickGroup = TG_DuringPhysics`, `bCanEverTick = false` (`Source/Runtime/Engine/Private/Components/ActorComponent.cpp:548–550`); actors `bCanEverTick = false` (`Actor.cpp:278`).

How a Blueprint gets `bCanEverTick`: the Kismet compiler resets the CDO's flag to the parent's, then sets it to true if the Blueprint overrides `ReceiveTick` with a connected Event Tick and ticking is allowed (`Source/Editor/KismetCompiler/Private/KismetCompiler.cpp:5525–5613`). A disconnected Event Tick does not enable ticking (`:5560–5572`). A native parent can forbid ticking with `meta=(ChildCannotTick)`; the compiler then **warns**: "@@ is not allowed as the C++ parent class {0} has disallowed Blueprint subclasses from ticking. Please consider using a Timer instead of Tick." (`:5574–5584`). → The engine has a compile-time, per-class "don't tick" switch. It has no notion of "ticks, but outside a budget".

When a component starts ticking: `SetupActorComponentTickFunction` registers the tick only if `bCanEverTick` and the component is not a template, enabled if `bStartWithTickEnabled` (`ActorComponent.cpp:1739–1752`). `Activate` enables the tick (`:2815–2824`). Whether `bAutoActivate = false` delays the first tick is **[unverified]**: a near-miss control in the test corpus will settle it for the rule.

## B6. Asset registry calls the rules use

- `IAssetRegistry::GetDependencies(const FAssetIdentifier&, TArray<FAssetIdentifier>&, EDependencyCategory, const FDependencyQuery&)` (`Source/Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h:517–519`); `GetReferencers` (`:580–582`). The package-name overloads (`:536`, `:598`).
- `EDependencyCategory::SearchableName = 0x04`: "Targets of SearchableName dependencies are FNames Keys inside of an Asset ... Used to search for Assets with a given Key,Value for the custom Key." (`Source/Runtime/CoreUObject/Public/Misc/AssetRegistryInterface.h:41–46`).
- `FAssetIdentifier` fields `PackageName`, `PrimaryAssetType`, `ObjectName`, `ValueName` (`Source/Runtime/CoreUObject/Public/AssetRegistry/AssetIdentifier.h:11–69`).
- The registry reads searchable names from package headers: `FPackageReader::ReadSearchableNames` (`Source/Runtime/AssetRegistry/Private/PackageReader.cpp:572–626`), turned into dependency nodes (`Source/Runtime/AssetRegistry/Private/AssetRegistry.cpp:7779–7780`). `bFilterSearchableNames=true` in `Config/BaseEngine.ini:2953` applies to the *serialised* (cooked) registry options (`AssetRegistry.cpp:1205–1217`), not to the editor's live registry, where the tag tools rely on these entries **[inferred from usage]**.

## B7. Creating test assets from code (for the seeding commandlet)

- `CreatePackage(const TCHAR*)` (`Source/Runtime/CoreUObject/Public/UObject/UObjectGlobals.h:1225`); `UPackage::Save(...)` / `SavePackage(...)` (`Source/Runtime/CoreUObject/Public/UObject/Package.h:1183, 1204`).
- `FKismetEditorUtilities::CreateBlueprint(ParentClass, Outer, Name, BlueprintType, CallingContext)` (`Source/Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h:110`); `CompileBlueprint(...)` (`:169`).
- `USimpleConstructionScript::CreateNode(UClass*, FName)` (`Source/Runtime/Engine/Classes/Engine/SimpleConstructionScript.h:170`), `AddNode(USCS_Node*)` (`:92`), `GetAllNodes()` (`:78–80`).
- These need the `UnrealEd` and `Kismet` editor modules, which is why the seeder lives in an editor module.
