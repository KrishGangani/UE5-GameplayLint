// P10 GameplayLint - a rule that exists only for the rule-contract tests.

#pragma once

#include "GameplayLintRule.h"
#include "GLTestRule.generated.h"

/**
 * Reports an error on objects whose name contains "Bad" and a warning on names containing "Warn".
 * The engine instantiates every concrete validator class (01b #1), so this one says it is never enabled: real scans
 * never run it, and the tests call ValidateLoadedAsset on it directly.
 */
UCLASS()
class UGLTestRule : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLTestRule()
	{
		RuleId = TEXT("GL.Test.Rule");
		DefaultSeverity = EGameplayLintSeverity::Error;
	}

	virtual bool IsEnabled() const override { return false; }

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override { return true; }

	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override
	{
		if (Asset->GetName().Contains(TEXT("Bad")))
		{
			Lint.Report(TEXT("Where"), INVTEXT("bad data"));
		}
		if (Asset->GetName().Contains(TEXT("Warn")))
		{
			Lint.Report(EGameplayLintSeverity::Warning, TEXT("Where"), INVTEXT("suspicious data"));
		}
	}
};
