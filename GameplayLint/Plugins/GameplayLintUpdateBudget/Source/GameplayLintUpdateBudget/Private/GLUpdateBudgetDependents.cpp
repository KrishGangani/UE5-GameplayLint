// GameplayLintUpdateBudget - incremental runs. A changed actor Blueprint can break the Blueprints that derive from it
// (they inherit its components); a changed component Blueprint can break the actors that use it as a component; a
// changed pack setting can change the verdict on every Blueprint the rules apply to.

#include "GLUpdateBudgetRules.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Blueprint/BlueprintSupport.h"
#include "Components/ActorComponent.h"
#include "DataValidationChangelist.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace GLUpdateBudgetDependents
{
	using UE::AssetRegistry::EDependencyCategory;

	/** The pack's settings are config=Editor: DefaultEditor.ini and its platform layers (<Platform>Editor.ini). */
	bool IsSettingsFile(const FString& File)
	{
		return FPaths::GetCleanFilename(File).EndsWith(TEXT("Editor.ini"), ESearchCase::IgnoreCase);
	}

	/** Adds the assets of Package that the rule applies to. */
	void AddApplicable(const IAssetRegistry& Registry, FName Package, TFunctionRef<bool(const FAssetData&)> AppliesTo, TArray<FAssetData>& OutAssets)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPackageName(Package, Assets, /*bIncludeOnlyOnDiskAssets*/ true);
		for (const FAssetData& Asset : Assets)
		{
			if (AppliesTo(Asset))
			{
				OutAssets.Add(Asset);
			}
		}
	}

	/**
	 * Precise and Coarse are the same here: the registry's class hierarchy and package references name exactly the
	 * Blueprints a change can affect, with nothing to compare inside them.
	 */
	void Gather(const UDataValidationChangelist& Changelist, TFunctionRef<bool(const FAssetData&)> AppliesTo, TArray<FAssetData>& OutAssets)
	{
		const IAssetRegistry& Registry = IAssetRegistry::GetChecked();

		// Changed or deleted settings: every Blueprint the rule applies to may get another verdict.
		if (Changelist.ModifiedFiles.ContainsByPredicate(IsSettingsFile) || Changelist.DeletedFiles.ContainsByPredicate(IsSettingsFile))
		{
			TArray<FAssetData> Blueprints;
			Registry.GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), Blueprints, /*bSearchSubClasses*/ true);
			for (const FAssetData& Blueprint : Blueprints)
			{
				if (AppliesTo(Blueprint))
				{
					OutAssets.Add(Blueprint);
				}
			}
			return;
		}

		for (const FName Package : Changelist.ModifiedPackageNames)
		{
			TArray<FAssetData> InPackage;
			Registry.GetAssetsByPackageName(Package, InPackage, true);
			for (const FAssetData& Data : InPackage)
			{
				FString ExportPath;
				if (!Data.IsInstanceOf(UBlueprint::StaticClass()) || !Data.GetTagValue(FBlueprintTags::GeneratedClassPath, ExportPath))
				{
					continue;
				}
				// Blueprints that derive from it inherit its components and defaults.
				const FTopLevelAssetPath Generated(FPackageName::ExportTextPathToObjectPath(ExportPath));
				TSet<FTopLevelAssetPath> Derived;
				Registry.GetDerivedClassNames({ Generated }, {}, Derived);   // includes Generated itself (AssetRegistry.cpp:9742)
				for (const FTopLevelAssetPath& ClassPath : Derived)
				{
					if (ClassPath != Generated)
					{
						AddApplicable(Registry, ClassPath.GetPackageName(), AppliesTo, OutAssets);
					}
				}
				// Actors that use a changed component Blueprint as a component hold its templates (S8 code review).
				const UClass* Parent = UBlueprint::GetBlueprintParentClassFromAssetTags(Data);
				if (Parent && Parent->IsChildOf(UActorComponent::StaticClass()))
				{
					TArray<FName> Referencers;
					Registry.GetReferencers(Package, Referencers, EDependencyCategory::Package);
					for (const FName Referencer : Referencers)
					{
						AddApplicable(Registry, Referencer, AppliesTo, OutAssets);
					}
				}
			}
		}
	}
}

void UGLRule_TickOutsideBudget::GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const
{
	GLUpdateBudgetDependents::Gather(Changelist, [this](const FAssetData& Data) { return AppliesTo(Data); }, OutAssets);
}

void UGLRule_ClientTickGroup::GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const
{
	GLUpdateBudgetDependents::Gather(Changelist, [this](const FAssetData& Data) { return AppliesTo(Data); }, OutAssets);
}

void UGLRule_ClientNeverTicks::GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const
{
	GLUpdateBudgetDependents::Gather(Changelist, [this](const FAssetData& Data) { return AppliesTo(Data); }, OutAssets);
}
