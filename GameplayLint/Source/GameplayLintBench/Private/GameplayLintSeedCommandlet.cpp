// P10 GameplayLint - UGameplayLintSeedCommandlet: the seeded corpus (01 section 7.1) and its mutations M1-M4.

#include "GameplayLintSeedCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GLFixtureActors.h"
#include "GLFixtureData.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Math/RandomStream.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#include <limits>

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayLintSeedCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogGameplayLintSeed, Log, All);

namespace GameplayLintSeed
{
	// Layout. Everything lives under one folder, so a scan or a delete can address the whole corpus.
	const TCHAR* const RootPath = TEXT("/Game/GLSeed");
	const TCHAR* const SharedPath = TEXT("/Game/GLSeed/Shared");
	const TCHAR* const AssetsPath = TEXT("/Game/GLSeed/Assets");

	constexpr int32 NumTargets = 3;        // DT_TargetsA, B, C
	constexpr int32 TargetRows = 32;       // T00..T31 in each shared target table
	constexpr int32 TableRows = 20;        // R00..R19 in each corpus DataTable
	constexpr int32 CurveRows = 16;        // C00..C15 in the shared CurveTable
	constexpr int32 PermanentTags = 16;    // Fixture.Tag.00..15
	constexpr int32 RemovableTags = 8;     // Fixture.Removable.00..07: M2 deletes their file
	constexpr int32 CleanParents = 4;      // Shared/BP_Parent_0..3: M3 adds a per-frame ticker to BP_Parent_0
	constexpr int32 TickingParents = 2;    // Shared/BP_TickingParent_0..1: parents that already tick every frame

	// The M1 rows: removed from tables A and B. M4 edits a value in table C and removes nothing.
	const TCHAR* const RemovedRows[] = { TEXT("T30"), TEXT("T31") };

	/** One line of the ground truth. The rules never read it; Tools\score.py compares their findings with it. */
	struct FEntry
	{
		FString Asset;      // object path of the asset the engine validates
		FString Type;       // DataAsset, DataTable, CurveTable, ActorBlueprint, ComponentBlueprint
		FString Category;   // defect, nearmiss, clean, heldout (expected miss), masked (expected miss)
		FString Rule;       // the rule that should report it (defect, heldout, masked), else empty
		FString Kind;       // which defect or near-miss
		FString Variant;    // where it hides: Top, Nested, Array, Map, Instanced, Row, BlueprintDefault, ...
		FString Where;      // the property path, to make reports readable
	};

	/** The defect kinds of the data rules. The first four can sit at any payload position; the rest are top level. */
	enum class EDataKind : uint8
	{
		TagSingle, RowMissing, RowNullTable, CurveNonFinite,                     // payload kinds
		TagContainer, RowRequired, CurveRowMissing, CurveReversal, CurveRequired // top-level-only kinds
	};
	constexpr int32 PayloadKinds = 4;
	constexpr int32 DataKinds = 9;
	constexpr int32 NumVariants = 5;       // Top, Nested, Array, Map, Instanced (data assets)

	const TCHAR* KindName(EDataKind Kind)
	{
		switch (Kind)
		{
		case EDataKind::TagSingle:       return TEXT("TagSingle");
		case EDataKind::RowMissing:      return TEXT("RowMissing");
		case EDataKind::RowNullTable:    return TEXT("RowNullTable");
		case EDataKind::CurveNonFinite:  return TEXT("CurveNonFinite");
		case EDataKind::TagContainer:    return TEXT("TagContainer");
		case EDataKind::RowRequired:     return TEXT("RowRequired");
		case EDataKind::CurveRowMissing: return TEXT("CurveRowMissing");
		case EDataKind::CurveReversal:   return TEXT("CurveReversal");
		case EDataKind::CurveRequired:   return TEXT("CurveRequired");
		}
		return TEXT("?");
	}

	const TCHAR* RuleFor(EDataKind Kind)
	{
		switch (Kind)
		{
		case EDataKind::TagSingle:
		case EDataKind::TagContainer:   return TEXT("GL.Data.UnknownTag");
		case EDataKind::RowMissing:
		case EDataKind::RowNullTable:
		case EDataKind::RowRequired:
		case EDataKind::CurveRowMissing: return TEXT("GL.Data.DanglingRow");
		default:                        return TEXT("GL.Data.Curve");
		}
	}

	/** Saves one asset to its package file. Returns false (and logs) on failure. */
	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogGameplayLintSeed, Error, TEXT("Failed to save %s"), *Package->GetName());
			return false;
		}
		return true;
	}

	/** The field a payload kind lives in. RowField is "Row" in payloads and "Link" in DataTable rows. */
	const TCHAR* FieldFor(EDataKind Kind, const TCHAR* RowField)
	{
		return Kind == EDataKind::TagSingle ? TEXT("Tag") : (Kind == EDataKind::CurveNonFinite ? TEXT("Curve") : RowField);
	}

	/**
	 * A tag by name, whether or not the dictionary knows it. The import path with PPF_SerializedAsImportText keeps an
	 * unknown name "to avoid data loss" (GameplayTagsManager.cpp:1177-1178), exactly as a text import would. At seed
	 * time no corpus tag is in the dictionary yet (this run writes their .ini files), so every tag is made this way.
	 */
	FGameplayTag MakeTag(const FString& Name)
	{
		FGameplayTag Tag;
		Tag.FromExportString(Name, PPF_SerializedAsImportText);
		return Tag;
	}

	/** One curve key, as written in the corpus code. */
	struct FCurveKey
	{
		float Time;
		float Value;
	};

	FRuntimeFloatCurve MakeCurve(std::initializer_list<FCurveKey> Keys)
	{
		FRuntimeFloatCurve Curve;
		for (const FCurveKey& Key : Keys)
		{
			Curve.EditorCurveData.AddKey(Key.Time, Key.Value);
		}
		return Curve;
	}

	UBlueprint* NewBlueprint(UClass* Parent, const FString& Path, const FString& Name)
	{
		UPackage* Package = CreatePackage(*(Path / Name));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, FName(*Name), BPTYPE_Normal, TEXT("GameplayLintSeed"));
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		return Blueprint;
	}

	/** Adds a component node to a Blueprint and returns its template: the object whose properties become the component's defaults. */
	UActorComponent* AddComponent(UBlueprint* Blueprint, UClass* Class, const TCHAR* Name)
	{
		USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(Class, Name);
		Blueprint->SimpleConstructionScript->AddNode(Node);
		return Node->ComponentTemplate;
	}

	/** Builds the corpus. One instance per run; every random choice comes from one seeded stream, in index order. */
	class FSeeder
	{
	public:
		FSeeder(int32 InSeed, int32 InAssets) : Seed(InSeed), Assets(InAssets), Random(InSeed) {}

		bool Run(const FString& ManifestFile);

	private:
		// Valid values. Removable tags are used often enough that M2 has victims.
		FGameplayTag ValidTag()
		{
			return Random.FRand() < 0.1f
				? MakeTag(FString::Printf(TEXT("Fixture.Removable.%02d"), Random.RandRange(0, RemovableTags - 1)))
				: MakeTag(FString::Printf(TEXT("Fixture.Tag.%02d"), Random.RandRange(0, PermanentTags - 1)));
		}
		FName TargetRow() { return FName(*FString::Printf(TEXT("T%02d"), Random.RandRange(0, TargetRows - 1))); }
		FDataTableRowHandle ValidRow()
		{
			FDataTableRowHandle Handle;
			Handle.DataTable = Targets[Random.RandRange(0, NumTargets - 1)];
			Handle.RowName = TargetRow();
			return Handle;
		}
		FRuntimeFloatCurve ValidCurve() { return MakeCurve({ { 0.f, 1.f }, { 1.f, 1.f + Random.FRand() } }); }

		void FillPayload(FGameplayTag& Tag, FDataTableRowHandle& Row, FRuntimeFloatCurve& Curve)
		{
			Tag = ValidTag();
			Row = ValidRow();
			Curve = ValidCurve();
		}
		void ApplyPayloadDefect(EDataKind Kind, int32 Index, FGameplayTag& Tag, FDataTableRowHandle& Row, FRuntimeFloatCurve& Curve);

		FEntry& AddEntry(const UObject* Asset, const TCHAR* Type, const TCHAR* Category)
		{
			FEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Asset = Asset->GetPathName();   // the object path the engine validates and reports
			Entry.Type = Type;
			Entry.Category = Category;
			return Entry;
		}
		static void SetDefect(FEntry& Entry, const TCHAR* Rule, const FString& Kind, const TCHAR* Variant, const FString& Where)
		{
			Entry.Rule = Rule;
			Entry.Kind = Kind;
			Entry.Variant = Variant;
			Entry.Where = Where;
		}

		/** Defect 20 %, near-miss 15 %, clean 65 % (01 section 7.1). */
		const TCHAR* PickCategory()
		{
			const float Roll = Random.FRand();
			return Roll < 0.20f ? TEXT("defect") : (Roll < 0.35f ? TEXT("nearmiss") : TEXT("clean"));
		}

		bool CreateShared();
		bool CreateDataAsset(int32 Index);
		bool CreateDataTable(int32 Index);
		bool CreateActorBlueprint(int32 Index);
		bool CreateComponentBlueprint(int32 Index);
		bool WriteTagFiles() const;
		bool WriteManifest(const FString& File) const;

		int32 Seed;
		int32 Assets;
		FRandomStream Random;
		TArray<FEntry> Entries;
		UDataTable* Targets[NumTargets] = {};
		UCurveTable* CurveTable = nullptr;
		UBlueprint* Parents[CleanParents] = {};
		UBlueprint* TickingParentBlueprints[TickingParents] = {};
	};

	void FSeeder::ApplyPayloadDefect(EDataKind Kind, int32 Index, FGameplayTag& Tag, FDataTableRowHandle& Row, FRuntimeFloatCurve& Curve)
	{
		switch (Kind)
		{
		case EDataKind::TagSingle:
			Tag = MakeTag(FString::Printf(TEXT("Fixture.Missing.%04d"), Index));
			break;
		case EDataKind::RowMissing:
			Row.DataTable = Targets[Random.RandRange(0, NumTargets - 1)];
			Row.RowName = TEXT("T99");
			break;
		case EDataKind::RowNullTable:
			Row.DataTable = nullptr;
			Row.RowName = TargetRow();
			break;
		case EDataKind::CurveNonFinite:
			Curve = MakeCurve({ { 0.f, 1.f },
				{ 1.f, Random.FRand() < 0.5f ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity() } });
			break;
		default:
			checkNoEntry();   // top-level kinds are applied by CreateDataAsset
		}
	}

	bool FSeeder::CreateShared()
	{
		// Three target tables: every row valid. Corpus handles point into them; M1 and M4 edit them.
		const TCHAR* TableNames[NumTargets] = { TEXT("DT_TargetsA"), TEXT("DT_TargetsB"), TEXT("DT_TargetsC") };
		for (int32 T = 0; T < NumTargets; ++T)
		{
			UPackage* Package = CreatePackage(*(FString(SharedPath) / TableNames[T]));
			UDataTable* Table = NewObject<UDataTable>(Package, TableNames[T], RF_Public | RF_Standalone);
			Table->RowStruct = FGLFixtureRow::StaticStruct();
			for (int32 Row = 0; Row < TargetRows; ++Row)
			{
				FGLFixtureRow Data;
				Data.Value = Row;
				Data.Tag = MakeTag(FString::Printf(TEXT("Fixture.Tag.%02d"), Row % PermanentTags));
				Data.Curve = MakeCurve({ { 0.f, 1.f }, { 1.f, 1.f } });
				Table->AddRow(FName(*FString::Printf(TEXT("T%02d"), Row)), Data);
			}
			FAssetRegistryModule::AssetCreated(Table);
			Targets[T] = Table;
			AddEntry(Table, TEXT("DataTable"), TEXT("clean")).Variant = TEXT("Shared");
			if (!SaveAsset(Table))
			{
				return false;
			}
		}

		UPackage* CurvePackage = CreatePackage(*(FString(SharedPath) / TEXT("CT_Curves")));
		CurveTable = NewObject<UCurveTable>(CurvePackage, TEXT("CT_Curves"), RF_Public | RF_Standalone);
		for (int32 Row = 0; Row < CurveRows; ++Row)
		{
			FRichCurve& Curve = CurveTable->AddRichCurve(FName(*FString::Printf(TEXT("C%02d"), Row)));
			Curve.AddKey(0.f, 0.f);
			Curve.AddKey(1.f, float(Row));
		}
		FAssetRegistryModule::AssetCreated(CurveTable);
		AddEntry(CurveTable, TEXT("CurveTable"), TEXT("clean")).Variant = TEXT("Shared");
		if (!SaveAsset(CurveTable))
		{
			return false;
		}

		// Clean parents: in the budget scope through their client. Half the clean actor Blueprints derive from them.
		for (int32 P = 0; P < CleanParents; ++P)
		{
			UBlueprint* Parent = NewBlueprint(AGLFixtureAgent::StaticClass(), SharedPath, FString::Printf(TEXT("BP_Parent_%d"), P));
			if (!Parent)
			{
				return false;
			}
			AddComponent(Parent, UGLFixtureBudgetClient::StaticClass(), TEXT("Client"));
			FKismetEditorUtilities::CompileBlueprint(Parent);
			Parents[P] = Parent;
			AddEntry(Parent, TEXT("ActorBlueprint"), TEXT("clean")).Variant = TEXT("Shared");
			if (!SaveAsset(Parent))
			{
				return false;
			}
		}

		// Parents that already tick every frame beside their client: defective themselves, and so are their children.
		for (int32 P = 0; P < TickingParents; ++P)
		{
			UBlueprint* Parent = NewBlueprint(AGLFixtureAgent::StaticClass(), SharedPath, FString::Printf(TEXT("BP_TickingParent_%d"), P));
			if (!Parent)
			{
				return false;
			}
			AddComponent(Parent, UGLFixtureBudgetClient::StaticClass(), TEXT("Client"));
			AddComponent(Parent, UGLFixtureTicker::StaticClass(), TEXT("Ticker"));
			FKismetEditorUtilities::CompileBlueprint(Parent);
			TickingParentBlueprints[P] = Parent;
			SetDefect(AddEntry(Parent, TEXT("ActorBlueprint"), TEXT("defect")), TEXT("GL.UB.TickOutsideBudget"), TEXT("Component"), TEXT("Shared"), TEXT("Ticker"));
			if (!SaveAsset(Parent))
			{
				return false;
			}
		}
		return true;
	}

	bool FSeeder::CreateDataAsset(int32 Index)
	{
		const FString Name = FString::Printf(TEXT("DA_%04d"), Index);
		UPackage* Package = CreatePackage(*(FString(AssetsPath) / Name));
		UGLFixtureDataAsset* Asset = NewObject<UGLFixtureDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

		// A clean asset first: every reference valid, every annotated contract met.
		Asset->Tag = ValidTag();
		Asset->Tags.AddTagFast(ValidTag());
		Asset->Tags.AddTagFast(ValidTag());
		Asset->Row = ValidRow();
		Asset->RequiredRow = ValidRow();
		Asset->CurveRow.CurveTable = CurveTable;
		Asset->CurveRow.RowName = FName(*FString::Printf(TEXT("C%02d"), Random.RandRange(0, CurveRows - 1)));
		Asset->Curve = ValidCurve();
		Asset->CooldownByLevel = MakeCurve({ { 1.f, 1.f }, { 2.f, 1.5f }, { 3.f, 2.f } });
		Asset->RequiredCurve = ValidCurve();
		FillPayload(Asset->Nested.Tag, Asset->Nested.Row, Asset->Nested.Curve);
		for (int32 Element = 0; Element < 3; ++Element)
		{
			FGLFixturePayload& Payload = Asset->List.AddDefaulted_GetRef();
			FillPayload(Payload.Tag, Payload.Row, Payload.Curve);
		}
		for (int32 Element = 0; Element < 2; ++Element)
		{
			FGLFixturePayload& Payload = Asset->Map.Add(FName(*FString::Printf(TEXT("K%d"), Element)));
			FillPayload(Payload.Tag, Payload.Row, Payload.Curve);
		}
		Asset->Instanced = NewObject<UGLFixtureInstanced>(Asset, TEXT("Instanced"));
		FillPayload(Asset->Instanced->Payload.Tag, Asset->Instanced->Payload.Row, Asset->Instanced->Payload.Curve);
		FGLFixturePayload Extra;
		FillPayload(Extra.Tag, Extra.Row, Extra.Curve);
		Asset->LootTable = Targets[Random.RandRange(0, NumTargets - 1)];
		Asset->LootRow = TargetRow();

		// 1 % carry a defect inside the instanced struct (a regular variant), 1 % are held out: a missing row in the
		// project's own two-field reference, which no generic rule can recognise (an expected miss).
		const float Special = Random.FRand();
		const bool bInstancedStruct = Special < 0.01f;
		const bool bHeldOut = !bInstancedStruct && Special < 0.02f;
		const TCHAR* Category = bInstancedStruct ? TEXT("defect") : (bHeldOut ? TEXT("heldout") : PickCategory());
		FEntry& Entry = AddEntry(Asset, TEXT("DataAsset"), Category);
		if (bInstancedStruct)
		{
			Extra.Tag = MakeTag(FString::Printf(TEXT("Fixture.Missing.%04d"), Index));
			SetDefect(Entry, TEXT("GL.Data.UnknownTag"), TEXT("TagSingle"), TEXT("InstancedStruct"), TEXT("Extra.Tag"));
		}
		else if (bHeldOut)
		{
			Asset->LootRow = TEXT("T99");
			SetDefect(Entry, TEXT("GL.Data.DanglingRow"), TEXT("CustomReference"), TEXT("ProjectType"), TEXT("LootTable + LootRow"));
		}
		else if (FCString::Strcmp(Category, TEXT("defect")) == 0)
		{
			// Payload kinds at five positions plus the five top-level-only kinds: 25 cells, chosen uniformly.
			const TCHAR* Variants[NumVariants] = { TEXT("Top"), TEXT("Nested"), TEXT("Array"), TEXT("Map"), TEXT("Instanced") };
			const int32 Cell = Random.RandRange(0, PayloadKinds * NumVariants + (DataKinds - PayloadKinds) - 1);
			if (Cell < PayloadKinds * NumVariants)
			{
				const EDataKind Kind = EDataKind(Cell % PayloadKinds);
				const int32 Variant = Cell / PayloadKinds;
				FString Where;
				FGameplayTag* Tag = nullptr; FDataTableRowHandle* Row = nullptr; FRuntimeFloatCurve* Curve = nullptr;
				switch (Variant)
				{
				case 0: Tag = &Asset->Tag; Row = &Asset->Row; Curve = &Asset->Curve; Where = TEXT(""); break;
				case 1: Tag = &Asset->Nested.Tag; Row = &Asset->Nested.Row; Curve = &Asset->Nested.Curve; Where = TEXT("Nested."); break;
				case 2:
				{
					const int32 Element = Random.RandRange(0, Asset->List.Num() - 1);
					FGLFixturePayload& Payload = Asset->List[Element];
					Tag = &Payload.Tag; Row = &Payload.Row; Curve = &Payload.Curve; Where = FString::Printf(TEXT("List[%d]."), Element);
					break;
				}
				case 3:
				{
					FGLFixturePayload& Payload = Asset->Map.FindChecked(TEXT("K1"));
					Tag = &Payload.Tag; Row = &Payload.Row; Curve = &Payload.Curve; Where = TEXT("Map[K1].");
					break;
				}
				default:
					Tag = &Asset->Instanced->Payload.Tag; Row = &Asset->Instanced->Payload.Row; Curve = &Asset->Instanced->Payload.Curve;
					Where = TEXT("Instanced.Payload.");
				}
				ApplyPayloadDefect(Kind, Index, *Tag, *Row, *Curve);
				SetDefect(Entry, RuleFor(Kind), KindName(Kind), Variants[Variant], Where + FieldFor(Kind, TEXT("Row")));
			}
			else
			{
				const EDataKind Kind = EDataKind(PayloadKinds + (Cell - PayloadKinds * NumVariants));
				FString Where;
				switch (Kind)
				{
				case EDataKind::TagContainer:
					Asset->Tags.AddTagFast(MakeTag(FString::Printf(TEXT("Fixture.Missing.%04d"), Index)));
					Where = TEXT("Tags");
					break;
				case EDataKind::RowRequired:
					Asset->RequiredRow = FDataTableRowHandle();
					Where = TEXT("RequiredRow");
					break;
				case EDataKind::CurveRowMissing:
					Asset->CurveRow.RowName = TEXT("C99");
					Where = TEXT("CurveRow");
					break;
				case EDataKind::CurveReversal:
					Asset->CooldownByLevel = MakeCurve({ { 1.f, 1.f }, { 2.f, 2.f }, { 3.f, 1.5f } });
					Where = TEXT("CooldownByLevel");
					break;
				default:   // CurveRequired
					Asset->RequiredCurve = FRuntimeFloatCurve();
					Where = TEXT("RequiredCurve");
				}
				SetDefect(Entry, RuleFor(Kind), KindName(Kind), TEXT("Top"), Where);
			}
		}
		else if (FCString::Strcmp(Category, TEXT("nearmiss")) == 0)
		{
			// Values a careless rule would flag. None of them is a defect.
			switch (Random.RandRange(0, 4))
			{
			case 0: Asset->Tag = MakeTag(TEXT("Fixture.Tag")); Entry.Kind = TEXT("ParentTag"); break;   // an implicit parent tag exists
			case 1: Asset->Tags = FGameplayTagContainer(); Entry.Kind = TEXT("EmptyContainer"); break;
			case 2: Asset->Row = FDataTableRowHandle(); Entry.Kind = TEXT("EmptyRow"); break;          // optional and unset
			case 3: Asset->CooldownByLevel = MakeCurve({ { 1.f, 2.f }, { 2.f, 2.f }, { 3.f, 2.f } }); Entry.Kind = TEXT("FlatMonotonic"); break;
			default: Asset->Curve = FRuntimeFloatCurve(); Entry.Kind = TEXT("EmptyOptionalCurve");      // not LintRequired
			}
		}
		Asset->Extra = FInstancedStruct::Make(Extra);

		FAssetRegistryModule::AssetCreated(Asset);
		return SaveAsset(Asset);
	}

	bool FSeeder::CreateDataTable(int32 Index)
	{
		const FString Name = FString::Printf(TEXT("DT_%04d"), Index);
		UPackage* Package = CreatePackage(*(FString(AssetsPath) / Name));
		UDataTable* Table = NewObject<UDataTable>(Package, FName(*Name), RF_Public | RF_Standalone);
		Table->RowStruct = FGLFixtureRow::StaticStruct();

		// 5 % of tables are masked: a class-level error in row R00 hides a rule defect in row R05 (01b #2).
		const bool bMasked = Random.FRand() < 0.05f;
		const TCHAR* Category = bMasked ? TEXT("masked") : PickCategory();
		FEntry& Entry = AddEntry(Table, TEXT("DataTable"), Category);
		const bool bDefect = bMasked || FCString::Strcmp(Category, TEXT("defect")) == 0;
		const bool bNearMiss = FCString::Strcmp(Category, TEXT("nearmiss")) == 0;
		const int32 DefectRow = Random.RandRange(1, TableRows - 1);
		const EDataKind Kind = bMasked ? EDataKind::TagSingle : EDataKind(Random.RandRange(0, PayloadKinds - 1));

		for (int32 RowIndex = 0; RowIndex < TableRows; ++RowIndex)
		{
			FGLFixtureRow Row;
			Row.Value = (bMasked && RowIndex == 0) ? -1 : RowIndex;
			FillPayload(Row.Tag, Row.Link, Row.Curve);
			if (bNearMiss)
			{
				Row.Link = FDataTableRowHandle();                       // optional links left empty
				Row.Curve = MakeCurve({ { 0.f, 1.f }, { 1.f, 1.f } });   // flat
			}
			if (bDefect && RowIndex == DefectRow)
			{
				ApplyPayloadDefect(Kind, Index, Row.Tag, Row.Link, Row.Curve);
			}
			Table->AddRow(FName(*FString::Printf(TEXT("R%02d"), RowIndex)), Row);
		}
		if (bDefect)
		{
			SetDefect(Entry, RuleFor(Kind), KindName(Kind), TEXT("Row"), FString::Printf(TEXT("R%02d.%s"), DefectRow, FieldFor(Kind, TEXT("Link"))));
		}
		else if (bNearMiss)
		{
			Entry.Kind = TEXT("EmptyLinksFlatCurves");
		}

		FAssetRegistryModule::AssetCreated(Table);
		return SaveAsset(Table);
	}

	bool FSeeder::CreateActorBlueprint(int32 Index)
	{
		const TCHAR* Category = PickCategory();
		const bool bDefect = FCString::Strcmp(Category, TEXT("defect")) == 0;
		const bool bNearMiss = FCString::Strcmp(Category, TEXT("nearmiss")) == 0;

		// Decide the shape first, because it decides the parent class.
		// Defects: 5 UpdateBudget kinds + 4 data kinds in the class default object, chosen uniformly.
		const int32 DefectCell = bDefect ? Random.RandRange(0, 8) : -1;
		const int32 NearMiss = bNearMiss ? Random.RandRange(0, 3) : -1;
		UClass* Parent = AGLFixtureAgent::StaticClass();
		if (DefectCell == 1)
		{
			Parent = AGLFixtureTickingAgent::StaticClass();
		}
		else if (DefectCell == 2)
		{
			Parent = TickingParentBlueprints[Random.RandRange(0, TickingParents - 1)]->GeneratedClass;
		}
		else if (NearMiss == 2)
		{
			Parent = AActor::StaticClass();   // outside the budget scope: no client, not an agent
		}
		else if (!bDefect && !bNearMiss && Random.FRand() < 0.5f)
		{
			Parent = Parents[Random.RandRange(0, CleanParents - 1)]->GeneratedClass;
		}

		UBlueprint* Blueprint = NewBlueprint(Parent, AssetsPath, FString::Printf(TEXT("BPA_%04d"), Index));
		if (!Blueprint)
		{
			return false;
		}
		FEntry& Entry = AddEntry(Blueprint, TEXT("ActorBlueprint"), Category);

		// Every agent that isn't a child of a parent with a client gets its own client.
		const bool bInheritsClient = Parent != AGLFixtureAgent::StaticClass() && Parent != AGLFixtureTickingAgent::StaticClass() && Parent != AActor::StaticClass();
		UActorComponent* Client = (bInheritsClient || NearMiss == 2) ? nullptr : AddComponent(Blueprint, UGLFixtureBudgetClient::StaticClass(), TEXT("Client"));

		switch (DefectCell)
		{
		case 0:
			AddComponent(Blueprint, UGLFixtureTicker::StaticClass(), TEXT("Ticker"));
			SetDefect(Entry, TEXT("GL.UB.TickOutsideBudget"), TEXT("Component"), TEXT("Template"), TEXT("Ticker"));
			break;
		case 1:
			SetDefect(Entry, TEXT("GL.UB.TickOutsideBudget"), TEXT("ActorTick"), TEXT("NativeParent"), TEXT("PrimaryActorTick"));
			break;
		case 2:
			SetDefect(Entry, TEXT("GL.UB.TickOutsideBudget"), TEXT("Component"), TEXT("Inherited"), TEXT("Ticker (from parent)"));
			break;
		case 3:
			Client->PrimaryComponentTick.TickGroup = TG_DuringPhysics;
			SetDefect(Entry, TEXT("GL.UB.ClientTickGroup"), TEXT("TickGroup"), TEXT("Template"), TEXT("Client.PrimaryComponentTick.TickGroup"));
			break;
		case 4:
			Client->PrimaryComponentTick.bStartWithTickEnabled = false;
			SetDefect(Entry, TEXT("GL.UB.ClientNeverTicks"), TEXT("StartDisabled"), TEXT("Template"), TEXT("Client.PrimaryComponentTick.bStartWithTickEnabled"));
			break;
		default:
			break;
		}
		switch (NearMiss)
		{
		case 0:
			AddComponent(Blueprint, UGLFixtureTicker::StaticClass(), TEXT("Ticker"))->PrimaryComponentTick.TickInterval = 0.2f;
			Entry.Kind = TEXT("IntervalTicker");
			break;
		case 1:
			AddComponent(Blueprint, UGLFixtureTicker::StaticClass(), TEXT("Ticker"))->PrimaryComponentTick.bStartWithTickEnabled = false;
			Entry.Kind = TEXT("DisabledTicker");
			break;
		case 2:
			AddComponent(Blueprint, UGLFixtureTicker::StaticClass(), TEXT("Ticker"));
			Entry.Kind = TEXT("OutOfScopeTicker");
			break;
		case 3:
			AddComponent(Blueprint, USkeletalMeshComponent::StaticClass(), TEXT("Mesh"));
			AddComponent(Blueprint, UProjectileMovementComponent::StaticClass(), TEXT("Movement"));
			Entry.Kind = TEXT("AllowListed");
			break;
		default:
			break;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		// The class default object holds gameplay data too. Set it after compiling, on the class the compile produced.
		if (AGLFixtureAgent* Defaults = Cast<AGLFixtureAgent>(Blueprint->GeneratedClass->GetDefaultObject()))
		{
			FillPayload(Defaults->Payload.Tag, Defaults->Payload.Row, Defaults->Payload.Curve);
			if (DefectCell >= 5)
			{
				const EDataKind Kind = EDataKind(DefectCell - 5);
				ApplyPayloadDefect(Kind, Index, Defaults->Payload.Tag, Defaults->Payload.Row, Defaults->Payload.Curve);
				SetDefect(Entry, RuleFor(Kind), KindName(Kind), TEXT("BlueprintDefault"), FString(TEXT("Payload.")) + FieldFor(Kind, TEXT("Row")));
			}
		}
		return SaveAsset(Blueprint);
	}

	bool FSeeder::CreateComponentBlueprint(int32 Index)
	{
		const TCHAR* Category = PickCategory();
		const bool bDefect = FCString::Strcmp(Category, TEXT("defect")) == 0;
		const bool bNearMiss = FCString::Strcmp(Category, TEXT("nearmiss")) == 0;

		// A near-miss is a Blueprint of the plain ticker: it ticks every frame, but it isn't a client and isn't in an actor.
		UClass* Parent = bNearMiss ? UGLFixtureTicker::StaticClass() : UGLFixtureBudgetClient::StaticClass();
		UBlueprint* Blueprint = NewBlueprint(Parent, AssetsPath, FString::Printf(TEXT("BPC_%04d"), Index));
		if (!Blueprint)
		{
			return false;
		}
		FEntry& Entry = AddEntry(Blueprint, TEXT("ComponentBlueprint"), Category);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		UActorComponent* Defaults = Cast<UActorComponent>(Blueprint->GeneratedClass->GetDefaultObject());
		if (bDefect && Defaults)
		{
			if (Random.FRand() < 0.5f)
			{
				Defaults->PrimaryComponentTick.TickGroup = TG_PostPhysics;
				SetDefect(Entry, TEXT("GL.UB.ClientTickGroup"), TEXT("TickGroup"), TEXT("Subclass"), TEXT("PrimaryComponentTick.TickGroup"));
			}
			else
			{
				Defaults->PrimaryComponentTick.bStartWithTickEnabled = false;
				SetDefect(Entry, TEXT("GL.UB.ClientNeverTicks"), TEXT("StartDisabled"), TEXT("Subclass"), TEXT("PrimaryComponentTick.bStartWithTickEnabled"));
			}
		}
		else if (bNearMiss)
		{
			Entry.Kind = TEXT("TickerSubclass");
		}
		return SaveAsset(Blueprint);
	}

	bool FSeeder::WriteTagFiles() const
	{
		// Config/Tags/*.ini is a tag search path (GameplayTagsManager.cpp:682); each file loads as a UGameplayTagsList (:582).
		// Such a loose file is read WITHOUT ini symbol commands (ConfigCacheIni.cpp:2216-2219, 2076-2096): a '+' would stay
		// part of the key, so the list would be empty. A repeated plain key appends (the default command, :2051).
		auto Write = [this](const TCHAR* File, const TCHAR* Prefix, int32 Count)
		{
			FString Text = FString::Printf(TEXT("; Generated by -run=GameplayLintSeed (seed %d). Recreated with the corpus; never committed.\r\n[/Script/GameplayTags.GameplayTagsList]\r\n"), Seed);
			for (int32 Tag = 0; Tag < Count; ++Tag)
			{
				Text += FString::Printf(TEXT("GameplayTagList=(Tag=\"%s.%02d\",DevComment=\"\")\r\n"), Prefix, Tag);
			}
			return FFileHelper::SaveStringToFile(Text, *(FPaths::ProjectConfigDir() / TEXT("Tags") / File), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		};
		return Write(TEXT("GLSeedPermanent.ini"), TEXT("Fixture.Tag"), PermanentTags)
			&& Write(TEXT("GLSeedRemovable.ini"), TEXT("Fixture.Removable"), RemovableTags);
	}

	bool FSeeder::WriteManifest(const FString& File) const
	{
		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("version"), 1);
		Writer->WriteValue(TEXT("seed"), Seed);
		Writer->WriteValue(TEXT("assets"), Assets);
		Writer->WriteArrayStart(TEXT("entries"));
		for (const FEntry& Entry : Entries)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("asset"), Entry.Asset);
			Writer->WriteValue(TEXT("type"), Entry.Type);
			Writer->WriteValue(TEXT("category"), Entry.Category);
			Writer->WriteValue(TEXT("rule"), Entry.Rule);
			Writer->WriteValue(TEXT("kind"), Entry.Kind);
			Writer->WriteValue(TEXT("variant"), Entry.Variant);
			Writer->WriteValue(TEXT("where"), Entry.Where);
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
		return FFileHelper::SaveStringToFile(Json, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	bool FSeeder::Run(const FString& ManifestFile)
	{
		// Mix per 100 assets: 40 data assets, 10 DataTables, 35 actor Blueprints, 15 component Blueprints (01 section 7.1).
		const int32 NumDataAssets = Assets * 40 / 100;
		const int32 NumTables = Assets * 10 / 100;
		const int32 NumActorBlueprints = Assets * 35 / 100;
		const int32 NumComponentBlueprints = Assets - NumDataAssets - NumTables - NumActorBlueprints;

		if (!CreateShared())
		{
			return false;
		}
		int32 Index = 0;
		for (int32 I = 0; I < NumDataAssets; ++I, ++Index) { if (!CreateDataAsset(Index)) return false; }
		for (int32 I = 0; I < NumTables; ++I, ++Index) { if (!CreateDataTable(Index)) return false; }
		for (int32 I = 0; I < NumActorBlueprints; ++I, ++Index) { if (!CreateActorBlueprint(Index)) return false; }
		for (int32 I = 0; I < NumComponentBlueprints; ++I, ++Index) { if (!CreateComponentBlueprint(Index)) return false; }

		UE_LOG(LogGameplayLintSeed, Display, TEXT("Seeded %d assets (+%d shared) with seed %d: %d data assets, %d tables, %d actor and %d component Blueprints."),
			Assets, Entries.Num() - Assets, Seed, NumDataAssets, NumTables, NumActorBlueprints, NumComponentBlueprints);
		return WriteTagFiles() && WriteManifest(ManifestFile);
	}

	/** M1-M4, applied together (01 section 7.1). Writes the pseudo-changelist for -run=GameplayLint -Changes=. */
	bool Mutate(const FString& ChangesFile)
	{
		TArray<FString> Changes;

		// Refuse a corpus that is already mutated: a second run would half-apply and no longer match the ground truth.
		const UDataTable* Probe = LoadObject<UDataTable>(nullptr, *(FString(SharedPath) / TEXT("DT_TargetsA.DT_TargetsA")));
		if (!Probe || !Probe->GetRowMap().Contains(RemovedRows[0]))
		{
			UE_LOG(LogGameplayLintSeed, Error, TEXT("The corpus is missing or already mutated. Reseed it first (Tools\\seed.py seed)."));
			return false;
		}

		// M1: two rows removed from each of tables A and B. Their referencers don't change, and are now broken.
		for (const TCHAR* TableName : { TEXT("DT_TargetsA"), TEXT("DT_TargetsB") })
		{
			const FString Path = FString(SharedPath) / TableName;
			UDataTable* Table = LoadObject<UDataTable>(nullptr, *(Path + TEXT(".") + TableName));
			if (!Table)
			{
				UE_LOG(LogGameplayLintSeed, Error, TEXT("M1: %s not found"), *Path);
				return false;
			}
			for (const TCHAR* Row : RemovedRows)
			{
				Table->RemoveRow(Row);
			}
			if (!SaveAsset(Table))
			{
				return false;
			}
			Changes.Add(TEXT("M ") + Path);
		}

		// M2: the removable tag file is deleted. A non-asset change: the stock path has no referencers to add for it.
		const FString TagFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("Tags/GLSeedRemovable.ini"));
		if (!IFileManager::Get().Delete(*TagFile))
		{
			UE_LOG(LogGameplayLintSeed, Error, TEXT("M2: could not delete %s"), *TagFile);
			return false;
		}
		Changes.Add(TEXT("X ") + TagFile);

		// M3: a per-frame ticker added to a clean parent. Its in-scope children inherit it.
		const FString ParentPath = FString(SharedPath) / TEXT("BP_Parent_0");
		UBlueprint* Parent = LoadObject<UBlueprint>(nullptr, *(ParentPath + TEXT(".BP_Parent_0")));
		if (!Parent)
		{
			UE_LOG(LogGameplayLintSeed, Error, TEXT("M3: %s not found"), *ParentPath);
			return false;
		}
		AddComponent(Parent, UGLFixtureTicker::StaticClass(), TEXT("Ticker"));
		FKismetEditorUtilities::CompileBlueprint(Parent);
		if (!SaveAsset(Parent))
		{
			return false;
		}
		Changes.Add(TEXT("M ") + ParentPath);

		// M4 (control): a value edited in table C, no row removed. No new finding may appear.
		const FString ControlPath = FString(SharedPath) / TEXT("DT_TargetsC");
		UDataTable* Control = LoadObject<UDataTable>(nullptr, *(ControlPath + TEXT(".DT_TargetsC")));
		FGLFixtureRow* Row = Control ? Control->FindRow<FGLFixtureRow>(TEXT("T05"), TEXT("GameplayLintSeed M4")) : nullptr;
		if (!Row)
		{
			UE_LOG(LogGameplayLintSeed, Error, TEXT("M4: %s row T05 not found"), *ControlPath);
			return false;
		}
		Row->Value += 100;
		if (!SaveAsset(Control))
		{
			return false;
		}
		Changes.Add(TEXT("M ") + ControlPath);

		UE_LOG(LogGameplayLintSeed, Display, TEXT("Applied M1-M4: %d changes."), Changes.Num());
		return FFileHelper::SaveStringArrayToFile(Changes, *ChangesFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

UGameplayLintSeedCommandlet::UGameplayLintSeedCommandlet()
{
	LogToConsole = true;
}

int32 UGameplayLintSeedCommandlet::Main(const FString& Params)
{
	using namespace GameplayLintSeed;

	// The registry must know what exists before we check that the corpus folder is empty.
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	AssetRegistry.SearchAllAssets(true);

	FString ChangesFile;
	if (FParse::Param(*Params, TEXT("Mutate")))
	{
		if (!FParse::Value(*Params, TEXT("Changes="), ChangesFile))
		{
			UE_LOG(LogGameplayLintSeed, Error, TEXT("-Mutate needs -Changes=<file>"));
			return 1;
		}
		return Mutate(ChangesFile) ? 0 : 1;
	}

	int32 Seed = 1;
	int32 Assets = 1000;
	FString Manifest;
	FParse::Value(*Params, TEXT("Seed="), Seed);
	FParse::Value(*Params, TEXT("Assets="), Assets);
	if (!FParse::Value(*Params, TEXT("Manifest="), Manifest) || Assets < 1)
	{
		UE_LOG(LogGameplayLintSeed, Error, TEXT("Usage: -run=GameplayLintSeed -Seed=<n> -Assets=<N> -Manifest=<file>"));
		return 1;
	}

	TArray<FAssetData> Existing;
	AssetRegistry.GetAssetsByPath(RootPath, Existing, true);
	if (Existing.Num() > 0)
	{
		UE_LOG(LogGameplayLintSeed, Error, TEXT("%s already holds %d assets. Delete the corpus first (Tools\\seed.py does)."), RootPath, Existing.Num());
		return 1;
	}

	FSeeder Seeder(Seed, Assets);
	return Seeder.Run(Manifest) ? 0 : 1;
}
