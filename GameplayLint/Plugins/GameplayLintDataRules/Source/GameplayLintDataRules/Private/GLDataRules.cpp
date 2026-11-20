// GameplayLintDataRules - UGLRule_UnknownTag, UGLRule_DanglingRow.

#include "GLDataRules.h"

#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "GameplayLintWalker.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GLDataRules)

#define LOCTEXT_NAMESPACE "GameplayLintDataRules"

// ---------------------------------------------------------------------------------------------------------------------
// GL.Data.UnknownTag

UGLRule_UnknownTag::UGLRule_UnknownTag()
{
	RuleId = TEXT("GL.Data.UnknownTag");
	Lesson = LOCTEXT("UnknownTagLesson", "A tag removed from config, or typed into an import, still sits in data; FGameplayTag::IsValid() only tests for None, so queries silently fail.");
	DefaultSeverity = EGameplayLintSeverity::Error;
}

bool UGLRule_UnknownTag::AppliesTo(const FAssetData& AssetData) const
{
	return !AssetData.IsRedirector();
}

void UGLRule_UnknownTag::CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const
{
	const UGameplayTagsManager& Tags = UGameplayTagsManager::Get();
	GameplayLint::ForEachValue(Asset, FGameplayTag::StaticStruct(), [&](const GameplayLint::FValue& Value)
	{
		// FGameplayTag::IsValid() is only "not None" (GameplayTagContainer.h:138-141): ask the dictionary instead.
		const FName Name = static_cast<const FGameplayTag*>(Value.Data)->GetTagName();
		if (!Name.IsNone() && !Tags.RequestGameplayTag(Name, /*ErrorIfNotFound*/ false).IsValid())
		{
			Lint.Report(Value.Path, FText::Format(LOCTEXT("UnknownTag", "gameplay tag '{0}' is not in the tag dictionary"), FText::FromName(Name)));
		}
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// GL.Data.DanglingRow

namespace GLDataRules
{
	/** Checks one DataTable or CurveTable handle: its table (or null), its row name, and whether that row exists. */
	void CheckHandle(const GameplayLint::FValue& Value, const UObject* Table, FName RowName, bool bRowExists, FGameplayLintContext& Lint)
	{
		const bool bRequired = GameplayLint::FindMetaData(Value, TEXT("LintRequired")) != nullptr;
		if (Table)
		{
			if (RowName.IsNone())
			{
				if (bRequired)
				{
					Lint.Report(Value.Path, FText::Format(LOCTEXT("RequiredNoRow", "required row handle names no row of {0}"), FText::FromString(Table->GetName())));
				}
			}
			else if (!bRowExists)
			{
				Lint.Report(Value.Path, FText::Format(LOCTEXT("MissingRow", "row '{0}' is not in {1}"), FText::FromName(RowName), FText::FromString(Table->GetName())));
			}
		}
		else if (!RowName.IsNone())
		{
			Lint.Report(Value.Path, FText::Format(LOCTEXT("NoTable", "row '{0}' is set but the handle has no table"), FText::FromName(RowName)));
		}
		else if (bRequired)
		{
			Lint.Report(Value.Path, LOCTEXT("RequiredEmpty", "required row handle is empty"));
		}
	}
}

UGLRule_DanglingRow::UGLRule_DanglingRow()
{
	RuleId = TEXT("GL.Data.DanglingRow");
	Lesson = LOCTEXT("DanglingRowLesson", "A row renamed or removed from a DataTable breaks every handle that names it, in assets that did not change; GetRow only warns at run time.");
	DefaultSeverity = EGameplayLintSeverity::Error;
}

bool UGLRule_DanglingRow::AppliesTo(const FAssetData& AssetData) const
{
	return !AssetData.IsRedirector();
}

void UGLRule_DanglingRow::CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const
{
	// The handle's table is a hard reference, so it was loaded with the asset: a row lookup needs no extra load.
	GameplayLint::ForEachValue(Asset, FDataTableRowHandle::StaticStruct(), [&](const GameplayLint::FValue& Value)
	{
		const FDataTableRowHandle& Handle = *static_cast<const FDataTableRowHandle*>(Value.Data);
		const bool bExists = Handle.DataTable && Handle.DataTable->GetRowMap().Contains(Handle.RowName);
		GLDataRules::CheckHandle(Value, Handle.DataTable, Handle.RowName, bExists, Lint);
	});
	GameplayLint::ForEachValue(Asset, FCurveTableRowHandle::StaticStruct(), [&](const GameplayLint::FValue& Value)
	{
		const FCurveTableRowHandle& Handle = *static_cast<const FCurveTableRowHandle*>(Value.Data);
		const bool bExists = Handle.CurveTable && Handle.CurveTable->GetRowMap().Contains(Handle.RowName);
		GLDataRules::CheckHandle(Value, Handle.CurveTable, Handle.RowName, bExists, Lint);
	});
}

#undef LOCTEXT_NAMESPACE
