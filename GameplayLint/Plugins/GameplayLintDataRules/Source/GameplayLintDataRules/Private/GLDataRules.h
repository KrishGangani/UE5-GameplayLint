// GameplayLintDataRules - the generic relation rules: tags, row handles, curves (02 section 4.2).
// The engine finds these classes by reflection; nothing includes this header except its own .cpp.

#pragma once

#include "GameplayLintRule.h"
#include "GLDataRules.generated.h"

/** GL.Data.UnknownTag: a gameplay tag stored in data that the tag dictionary does not contain. */
UCLASS()
class UGLRule_UnknownTag : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLRule_UnknownTag();

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override;
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override;
	virtual void GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const override;
};

/**
 * GL.Data.DanglingRow: a DataTable or CurveTable row handle that points at a row that does not exist, names a row
 * without a table, or is empty although its property says LintRequired.
 */
UCLASS()
class UGLRule_DanglingRow : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLRule_DanglingRow();

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override;
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override;
	virtual void GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const override;
};

/**
 * GL.Data.Curve: non-finite curve data (keys, tangents, default value) in a curve property, a curve asset or a curve
 * table row (always); a reversal where the property says LintMonotonic="Increasing" or "Decreasing"; no data at
 * all where the property says LintRequired. An external curve's keys are reported on the curve asset, once.
 */
UCLASS()
class UGLRule_Curve : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLRule_Curve();

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override;
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override;
	virtual void GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const override;
};
