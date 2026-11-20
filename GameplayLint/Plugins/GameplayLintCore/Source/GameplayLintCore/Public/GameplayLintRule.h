// GameplayLintCore - the rule contract. Every GameplayLint rule is a Data Validation validator deriving from this.

#pragma once

#include "EditorValidatorBase.h"
#include "GameplayLintTypes.h"
#include "GameplayLintRule.generated.h"

#define UE_API GAMEPLAYLINTCORE_API

class UDataValidationChangelist;

/** The findings of one rule on one asset, collected while the rule runs. The rule base class turns them into results. */
class FGameplayLintContext
{
public:
	struct FIssue
	{
		EGameplayLintSeverity Severity;
		FString Where;
		FText Message;
	};

	FGameplayLintContext(const FAssetData& InAssetData, EGameplayLintSeverity InDefaultSeverity)
		: AssetData(InAssetData), DefaultSeverity(InDefaultSeverity)
	{
	}

	/** Reports a problem at Where (a readable property path) with the rule's default severity. */
	void Report(const FString& Where, const FText& Message) { Report(DefaultSeverity, Where, Message); }

	void Report(EGameplayLintSeverity Severity, const FString& Where, const FText& Message)
	{
		Issues.Add({ Severity, Where, Message });
	}

	const FAssetData& GetAssetData() const { return AssetData; }
	const TArray<FIssue>& GetIssues() const { return Issues; }

private:
	const FAssetData& AssetData;
	EGameplayLintSeverity DefaultSeverity;
	TArray<FIssue> Issues;
};

/**
 * Base class of every GameplayLint rule (02 section 4.1).
 *
 * The engine creates one instance of every concrete subclass, in any loaded module, and calls it for every asset it
 * validates, on save, from menus, before submit and in CI (01b #1). This class is Abstract, so the engine skips it.
 * It implements the engine's validator overrides once:
 *   - IsEnabled: the engine's switches and GameplayLint's settings / command line;
 *   - CanValidateAsset: the rule's AppliesTo, from registry data only;
 *   - ValidateLoadedAsset: times CheckAsset, turns its findings into messages and report entries, then always ends
 *     with a result (an error, or AssetPasses), which the engine requires (EditorValidatorBase.cpp:52-62);
 *   - GetAssetsToValidateFromChangelist: the rule's GatherDependents, in the configured mode.
 *
 * A rule sets RuleId, Lesson and DefaultSeverity in its constructor and overrides AppliesTo, CheckAsset and optionally
 * GatherDependents. CheckAsset must never modify the asset: validation runs on save and before submit.
 */
UCLASS(MinimalAPI, Abstract)
class UGameplayLintRule : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	FName GetRuleId() const { return RuleId; }
	const FText& GetLesson() const { return Lesson; }

	UE_API virtual bool IsEnabled() const override;
	UE_API virtual TArray<FAssetData> GetAssetsToValidateFromChangelist(UDataValidationChangelist* InChangelist, FDataValidationContext& InContext) const override;

protected:
	/** Does this rule check this kind of asset? Registry data only: the asset may not be loaded yet. */
	virtual bool AppliesTo(const FAssetData& AssetData) const { return false; }

	/** Checks the loaded asset and reports through Lint. Must not modify the asset. */
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const {}

	/** Incremental runs: adds the assets that this change can break although they did not change themselves. */
	virtual void GatherDependents(const UDataValidationChangelist& Changelist, EGameplayLintDependents Mode, TArray<FAssetData>& OutAssets) const {}

	UE_API virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	UE_API virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;

	/** Stable ID, for example GL.Data.DanglingRow: in every message, the report, the settings and CI. */
	FName RuleId;

	/** Where the rule comes from, for example "UE5-UpdateBudget 07: Leaving the default TG_DuringPhysics". */
	FText Lesson;

	EGameplayLintSeverity DefaultSeverity = EGameplayLintSeverity::Error;
};

#undef UE_API
