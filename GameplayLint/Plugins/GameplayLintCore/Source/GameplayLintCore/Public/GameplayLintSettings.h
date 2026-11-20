// GameplayLintCore - project settings: which rules run, and how incremental runs find dependents.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayLintTypes.h"
#include "GameplayLintSettings.generated.h"

#define UE_API GAMEPLAYLINTCORE_API

/**
 * Project Settings > Plugins > Gameplay Lint. One place that decides which rules run.
 *
 * Command-line overrides (for benchmark configurations, 02 section 9), read once at start-up:
 *   -GameplayLintRules=All | None | <prefix>[,<prefix>...]     e.g. -GameplayLintRules=GL.Data
 *   -GameplayLintDependents=Off | Coarse | Precise
 * They replace the saved settings for that process only and are never written back.
 */
UCLASS(MinimalAPI, config = Editor, defaultconfig, meta = (DisplayName = "Gameplay Lint"))
class UGameplayLintSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Master switch for every GameplayLint rule. */
	UPROPERTY(EditAnywhere, config, Category = "Rules")
	bool bEnabled = true;

	/** Rule IDs, or ID prefixes such as "GL.UB", that never run in this project. */
	UPROPERTY(EditAnywhere, config, Category = "Rules")
	TArray<FString> DisabledRules;

	/** How incremental runs find the assets a change can break. Precise is the design; the others exist to measure it. */
	UPROPERTY(EditAnywhere, config, Category = "Incremental")
	EGameplayLintDependents Dependents = EGameplayLintDependents::Precise;

	/** True if the rule with this ID should run, after the command-line override. */
	UE_API bool IsRuleEnabled(FName RuleId) const;

	/** The dependent mode, after the command-line override. */
	UE_API EGameplayLintDependents GetDependents() const;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

protected:
	UE_API virtual void PostInitProperties() override;

private:
	/** Set from -GameplayLintRules=: "All", "None" or prefixes. Unset means "use the saved settings". */
	TOptional<TArray<FString>> CommandLineRules;

	/** Set from -GameplayLintDependents=. */
	TOptional<EGameplayLintDependents> CommandLineDependents;
};

#undef UE_API
