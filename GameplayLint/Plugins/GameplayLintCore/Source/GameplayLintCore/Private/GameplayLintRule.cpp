// GameplayLintCore - UGameplayLintRule.

#include "GameplayLintRule.h"

#include "DataValidationChangelist.h"
#include "Editor.h"
#include "GameplayLintReportSubsystem.h"
#include "GameplayLintSettings.h"
#include "HAL/PlatformTime.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/DataValidation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayLintRule)

#define LOCTEXT_NAMESPACE "GameplayLint"

namespace GameplayLintRule
{
	/** The report subsystem, or null outside an editor (it never is in practice: rules are editor-only). */
	UGameplayLintReportSubsystem* Report()
	{
		return GEditor ? GEditor->GetEditorSubsystem<UGameplayLintReportSubsystem>() : nullptr;
	}
}

bool UGameplayLintRule::IsEnabled() const
{
	// The engine's own switches first (bIsEnabled, and bIsConfigDisabled from ini), then GameplayLint's.
	return Super::IsEnabled() && !RuleId.IsNone() && GetDefault<UGameplayLintSettings>()->IsRuleEnabled(RuleId);
}

bool UGameplayLintRule::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return InAsset != nullptr && AppliesTo(InAssetData);
}

EDataValidationResult UGameplayLintRule::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	FGameplayLintContext Lint(InAssetData, DefaultSeverity);
	const uint64 Start = FPlatformTime::Cycles64();
	CheckAsset(InAsset, Lint);
	const double Milliseconds = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Start);

	UGameplayLintReportSubsystem* Report = GameplayLintRule::Report();
	const FText Rule = FText::FromName(RuleId);
	for (const FGameplayLintContext::FIssue& Issue : Lint.GetIssues())
	{
		// "[GL.Data.DanglingRow] Payloads[3].Loot: row 'Sword_02' is not in DT_Loot": clickable in the Message Log,
		// searchable in CI logs. An error makes the asset invalid; a warning does not.
		const FText Text = FText::Format(LOCTEXT("Finding", "[{0}] {1}: {2}"), Rule, FText::FromString(Issue.Where), Issue.Message);
		AssetMessage(InAssetData, Issue.Severity == EGameplayLintSeverity::Error ? EMessageSeverity::Error : EMessageSeverity::Warning, Text);
		if (Report)
		{
			FGameplayLintFinding Finding;
			Finding.RuleId = RuleId;
			Finding.Asset = InAssetData.GetSoftObjectPath();
			Finding.Severity = Issue.Severity;
			Finding.Where = Issue.Where;
			Finding.Message = Issue.Message.ToString();
			Report->RecordFinding(Finding);
		}
	}
	if (Report)
	{
		Report->RecordCheckTime(RuleId, Milliseconds);
	}

	// Always end with a result: AssetPasses unless an error made the asset invalid (calling it after an error ensures).
	if (GetValidationResult() != EDataValidationResult::Invalid)
	{
		AssetPasses(InAsset);
	}
	return GetValidationResult();
}

TArray<FAssetData> UGameplayLintRule::GetAssetsToValidateFromChangelist(UDataValidationChangelist* InChangelist, FDataValidationContext& InContext) const
{
	TArray<FAssetData> Assets;
	const EGameplayLintDependents Mode = GetDefault<UGameplayLintSettings>()->GetDependents();
	if (!InChangelist || Mode == EGameplayLintDependents::Off)
	{
		return Assets;
	}
	const uint64 Start = FPlatformTime::Cycles64();
	GatherDependents(*InChangelist, Mode, Assets);
	if (UGameplayLintReportSubsystem* Report = GameplayLintRule::Report())
	{
		Report->RecordDependents(RuleId, Assets.Num(), FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Start));
	}
	return Assets;
}

#undef LOCTEXT_NAMESPACE
