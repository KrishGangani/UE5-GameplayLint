// GameplayLintDataRules - incremental runs: which unchanged assets a change can break (02 section 6.3).
// Everything here reads the asset registry; the only assets loaded are changed tables, to learn their current rows.

#include "GLDataRules.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Curves/CurveFloat.h"
#include "DataValidationChangelist.h"
#include "Engine/CompositeCurveTable.h"
#include "Engine/CompositeDataTable.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace GLDataRulesDependents
{
	using UE::AssetRegistry::EDependencyCategory;

	/**
	 * The project's tag configuration: DefaultGameplayTags.ini and any .ini under a Config/Tags folder. A path in the
	 * change list may be relative to the project (a CI change file); it is made absolute first.
	 */
	bool IsTagConfig(const FString& File)
	{
		FString Path = FPaths::IsRelative(File) ? FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), File) : File;
		FPaths::NormalizeFilename(Path);
		return Path.EndsWith(TEXT(".ini"), ESearchCase::IgnoreCase)
			&& (Path.Contains(TEXT("/Config/Tags/"), ESearchCase::IgnoreCase) || Path.EndsWith(TEXT("GameplayTags.ini"), ESearchCase::IgnoreCase));
	}

	/** A DataTable of FGameplayTagTableRow rows: the other kind of tag source (registry tag "RowStructure", DataTable.cpp:347-361). */
	bool IsTagSourceTable(const FAssetData& Data)
	{
		FString RowStructure;
		return Data.IsInstanceOf(UDataTable::StaticClass()) && Data.GetTagValue(TEXT("RowStructure"), RowStructure)
			&& RowStructure == FGameplayTagTableRow::StaticStruct()->GetPathName();
	}

	void AddPackageAssets(const IAssetRegistry& Registry, FName PackageName, TArray<FAssetData>& OutAssets)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(PackageName, Assets, /*bIncludeOnlyOnDiskAssets*/ true);
		OutAssets.Append(Assets);
	}

	/** The project's own content: /Game and the content of the project's plugins (not the engine's). */
	TArray<FAssetData> ProjectAssets(const IAssetRegistry& Registry)
	{
		TArray<FName> Roots = { TEXT("/Game") };
		for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
		{
			if (Plugin->GetLoadedFrom() == EPluginLoadedFrom::Project)
			{
				FString Root = Plugin->GetMountedAssetPath();
				Root.RemoveFromEnd(TEXT("/"));
				Roots.Add(FName(*Root));
			}
		}
		TArray<FAssetData> Assets;
		for (const FName Root : Roots)
		{
			Registry.GetAssetsByPath(Root, Assets, /*bRecursive*/ true, /*bIncludeOnlyOnDiskAssets*/ true);
		}
		return Assets;
	}

	bool IsCompositeTable(const FAssetData& Data)
	{
		return Data.IsInstanceOf(UCompositeDataTable::StaticClass()) || Data.IsInstanceOf(UCompositeCurveTable::StaticClass());
	}
}

void UGLRule_UnknownTag::GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const
{
	using namespace GLDataRulesDependents;
	const IAssetRegistry& Registry = IAssetRegistry::GetChecked();

	// Only a change to a tag source makes a tag unknown without touching the assets that hold it: a tag .ini file, or a
	// tag DataTable. A changed asset is already in the changelist; the victims are the unchanged assets that name a tag.
	const bool bTagFileChanged = Changelist.ModifiedFiles.ContainsByPredicate(IsTagConfig) || Changelist.DeletedFiles.ContainsByPredicate(IsTagConfig);
	const bool bTagTableChanged = Changelist.ModifiedPackageNames.ContainsByPredicate([&Registry](const FName Package)
	{
		TArray<FAssetData> InPackage;
		Registry.GetAssetsByPackageName(Package, InPackage, true);
		return InPackage.ContainsByPredicate(IsTagSourceTable);
	});
	if (!bTagFileChanged && !bTagTableChanged)
	{
		return;
	}

	const UGameplayTagsManager& Tags = UGameplayTagsManager::Get();
	// Tags are recorded as searchable names on FGameplayTag (GameplayTagContainer.cpp:957, 1647).
	const FAssetIdentifier TagType(FGameplayTag::StaticStruct(), NAME_None);
	TSet<FName> Packages;
	for (const FAssetData& Asset : ProjectAssets(Registry))
	{
		Packages.Add(Asset.PackageName);
	}
	for (const FName Package : Packages)
	{
		TArray<FAssetIdentifier> Names;
		Registry.GetDependencies(FAssetIdentifier(Package), Names, EDependencyCategory::SearchableName);
		const bool bAffected = Names.ContainsByPredicate([&](const FAssetIdentifier& Id)
		{
			return Id.PackageName == TagType.PackageName && Id.ObjectName == TagType.ObjectName
				&& (Mode == EGameplayLintDependents::Coarse || !Tags.RequestGameplayTag(Id.ValueName, /*ErrorIfNotFound*/ false).IsValid());
		});
		if (bAffected)
		{
			AddPackageAssets(Registry, Package, OutAssets);
		}
	}
}

void UGLRule_DanglingRow::GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const
{
	using namespace GLDataRulesDependents;
	const IAssetRegistry& Registry = IAssetRegistry::GetChecked();

	// Deleted tables need nothing from us: the engine adds referencers of deleted packages (EditorValidatorSubsystem.cpp:1492-1516).
	// A composite table that includes a changed table changes with it, although its own package did not: it is
	// followed as if it had changed (a work list, each package once).
	TArray<FName> Work = Changelist.ModifiedPackageNames;
	TSet<FName> Seen;
	Seen.Append(Work);
	while (!Work.IsEmpty())
	{
		const FName Package = Work.Pop();
		TArray<FAssetData> InPackage;
		Registry.GetAssetsByPackageName(Package, InPackage, true);
		for (const FAssetData& TableData : InPackage)
		{
			if (!TableData.IsInstanceOf(UDataTable::StaticClass()) && !TableData.IsInstanceOf(UCurveTable::StaticClass()))
			{
				continue;
			}

			// The table's rows now. Row names are not registry tags (DataTable.cpp:347-361), so the table is loaded once.
			TSet<FName> Rows;
			if (Mode == EGameplayLintDependents::Precise)
			{
				UObject* Table = TableData.GetAsset();
				if (const UDataTable* DataTable = Cast<UDataTable>(Table))
				{
					DataTable->GetRowMap().GetKeys(Rows);
				}
				else if (const UCurveTable* CurveTable = Cast<UCurveTable>(Table))
				{
					CurveTable->GetRowMap().GetKeys(Rows);
				}
			}

			TArray<FName> Referencers;
			Registry.GetReferencers(Package, Referencers, EDependencyCategory::Package);
			for (const FName Referencer : Referencers)
			{
				TArray<FAssetData> ReferencerAssets;
				Registry.GetAssetsByPackageName(Referencer, ReferencerAssets, true);
				if (ReferencerAssets.ContainsByPredicate(IsCompositeTable))
				{
					bool bAlreadySeen = false;
					Seen.Add(Referencer, &bAlreadySeen);
					if (!bAlreadySeen)
					{
						Work.Add(Referencer);
					}
				}

				bool bAffected = Mode == EGameplayLintDependents::Coarse;
				if (!bAffected)
				{
					// Precise: does this referencer name a row of this table that is gone? (DataTable.cpp:1115-1121)
					TArray<FAssetIdentifier> Names;
					Registry.GetDependencies(FAssetIdentifier(Referencer), Names, EDependencyCategory::SearchableName);
					bAffected = Names.ContainsByPredicate([&](const FAssetIdentifier& Id)
					{
						return Id.PackageName == Package && Id.ObjectName == TableData.AssetName && !Rows.Contains(Id.ValueName);
					});
				}
				if (bAffected)
				{
					OutAssets.Append(ReferencerAssets);
				}
			}
		}
	}
}

void UGLRule_Curve::GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const
{
	using namespace GLDataRulesDependents;
	// A changed curve asset can break the owners that point at it: their LintRequired and LintMonotonic contracts are
	// checked against its keys (05 F2). Coarse and Precise are the same: the owners hold nothing to compare.
	const IAssetRegistry& Registry = IAssetRegistry::GetChecked();
	for (const FName Package : Changelist.ModifiedPackageNames)
	{
		TArray<FAssetData> InPackage;
		Registry.GetAssetsByPackageName(Package, InPackage, true);
		if (!InPackage.ContainsByPredicate([](const FAssetData& Data) { return Data.IsInstanceOf(UCurveFloat::StaticClass()); }))
		{
			continue;
		}
		TArray<FName> Referencers;
		Registry.GetReferencers(Package, Referencers, EDependencyCategory::Package);
		for (const FName Referencer : Referencers)
		{
			AddPackageAssets(Registry, Referencer, OutAssets);
		}
	}
}
