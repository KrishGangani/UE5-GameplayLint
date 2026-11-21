// GameplayLintDataRules - UGLRule_UnknownTag, UGLRule_DanglingRow, UGLRule_Curve.

#include "GLDataRules.h"

#include "Curves/CurveFloat.h"
#include "Curves/RichCurve.h"
#include "Curves/SimpleCurve.h"
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

// ---------------------------------------------------------------------------------------------------------------------
// GL.Data.Curve

namespace GLDataRules
{
	/** Every number interpolation reads: a NaN or infinite tangent spreads into the values between keys too. */
	bool IsFiniteKey(const FRichCurveKey& Key)
	{
		return FMath::IsFinite(Key.Time) && FMath::IsFinite(Key.Value)
			&& FMath::IsFinite(Key.ArriveTangent) && FMath::IsFinite(Key.LeaveTangent)
			&& FMath::IsFinite(Key.ArriveTangentWeight) && FMath::IsFinite(Key.LeaveTangentWeight);
	}

	bool IsFiniteKey(const FSimpleCurveKey& Key)
	{
		return FMath::IsFinite(Key.Time) && FMath::IsFinite(Key.Value);
	}

	enum class EMonotonic : uint8 { None, Increasing, Decreasing };

	/**
	 * The checks on one curve's data. Non-finite data (keys, tangents, the default value that an empty curve returns)
	 * is reported only if bOwnsKeys: a curve asset reports its own keys once, not once per asset that uses it. The
	 * direction is checked only on finite keys (a NaN step compares false either way; an infinite one is already reported).
	 */
	template <typename KeyType>
	void CheckCurveData(const TArray<KeyType>& Keys, float DefaultValue, EMonotonic Monotonic, bool bOwnsKeys, const FString& Where, FGameplayLintContext& Lint)
	{
		const int32 NonFinite = Keys.IndexOfByPredicate([](const KeyType& Key) { return !IsFiniteKey(Key); });
		if (NonFinite != INDEX_NONE || !FMath::IsFinite(DefaultValue))
		{
			if (bOwnsKeys)
			{
				Lint.Report(Where, NonFinite != INDEX_NONE
					? FText::Format(LOCTEXT("NonFinite", "key {0} is not a finite number (time, value or tangent)"), NonFinite)
					: LOCTEXT("NonFiniteDefault", "the curve's default value is not a finite number"));
			}
			return;
		}
		// Non-strict: a flat segment keeps the contract. Only key values are compared, not the interpolation between them.
		for (int32 Index = 1; Monotonic != EMonotonic::None && Index < Keys.Num(); ++Index)
		{
			const float Step = Keys[Index].Value - Keys[Index - 1].Value;
			if (Monotonic == EMonotonic::Increasing ? Step < 0.f : Step > 0.f)
			{
				Lint.Report(Where, FText::Format(Monotonic == EMonotonic::Increasing
					? LOCTEXT("Falls", "the curve must not fall (LintMonotonic=\"Increasing\"), but key {0} is lower than key {1}")
					: LOCTEXT("Rises", "the curve must not rise (LintMonotonic=\"Decreasing\"), but key {0} is higher than key {1}"),
					Index, Index - 1));
				return;
			}
		}
	}
}

UGLRule_Curve::UGLRule_Curve()
{
	RuleId = TEXT("GL.Data.Curve");
	Lesson = LOCTEXT("CurveLesson", "A NaN key propagates into every value read from the curve; a curve whose meaning requires it to rise (a cooldown by level) must not fall.");
	DefaultSeverity = EGameplayLintSeverity::Error;
}

bool UGLRule_Curve::AppliesTo(const FAssetData& AssetData) const
{
	return !AssetData.IsRedirector();
}

void UGLRule_Curve::CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const
{
	using namespace GLDataRules;

	// Curve data that this asset owns outside any FRuntimeFloatCurve: a curve asset's curve and a curve table's rows.
	if (const UCurveFloat* CurveAsset = Cast<UCurveFloat>(Asset))
	{
		CheckCurveData(CurveAsset->FloatCurve.GetConstRefOfKeys(), CurveAsset->FloatCurve.DefaultValue, EMonotonic::None, true, TEXT("FloatCurve"), Lint);
	}
	else if (UCurveTable* Table = Cast<UCurveTable>(Asset))
	{
		// A table holds either rich or simple rows (CurveTable.h:51-55); both key types have a Time and a Value.
		auto CheckRows = [&Lint](const auto& Rows)
		{
			for (const auto& Row : Rows)
			{
				CheckCurveData(Row.Value->GetConstRefOfKeys(), Row.Value->DefaultValue, EMonotonic::None, true, FString::Printf(TEXT("Row[%s]"), *Row.Key.ToString()), Lint);
			}
		};
		if (Table->GetCurveTableMode() == ECurveTableMode::SimpleCurves)
		{
			CheckRows(Table->GetSimpleCurveRowMap());
		}
		else
		{
			CheckRows(Table->GetRichCurveRowMap());
		}
	}

	GameplayLint::ForEachValue(Asset, FRuntimeFloatCurve::StaticStruct(), [&](const GameplayLint::FValue& Value)
	{
		const FRuntimeFloatCurve& Curve = *static_cast<const FRuntimeFloatCurve*>(Value.Data);
		const FRichCurve& Data = *Curve.GetRichCurveConst();   // the external curve's, if set (CurveFloat.cpp:28-36)
		// Keys in an external curve asset belong to that asset: name it, and leave its non-finite data to its own check.
		const FString Where = Curve.ExternalCurve
			? FString::Printf(TEXT("%s (external curve %s)"), *Value.Path, *Curve.ExternalCurve->GetName())
			: Value.Path;

		EMonotonic Monotonic = EMonotonic::None;
		if (const FString* Direction = GameplayLint::FindMetaData(Value, TEXT("LintMonotonic")))
		{
			if (Direction->Equals(TEXT("Increasing"), ESearchCase::IgnoreCase))
			{
				Monotonic = EMonotonic::Increasing;
			}
			else if (Direction->Equals(TEXT("Decreasing"), ESearchCase::IgnoreCase))
			{
				Monotonic = EMonotonic::Decreasing;
			}
			else
			{
				Lint.Report(Where, FText::Format(LOCTEXT("UnknownDirection", "LintMonotonic=\"{0}\" is neither \"Increasing\" nor \"Decreasing\": the direction is not checked"),
					FText::FromString(*Direction)));
			}
		}
		CheckCurveData(Data.GetConstRefOfKeys(), Data.DefaultValue, Monotonic, Curve.ExternalCurve == nullptr, Where, Lint);

		// "Has data" as the engine means it: a key, or a default value (RealCurve.h:140-143), here or in the external curve.
		if (!Data.HasAnyData() && GameplayLint::FindMetaData(Value, TEXT("LintRequired")))
		{
			Lint.Report(Where, LOCTEXT("RequiredCurve", "required curve has no keys and no default value"));
		}
	});
}

#undef LOCTEXT_NAMESPACE
