// GameplayLintUpdateBudget - UE5-UpdateBudget's data-level mistakes as rules, and the settings that name its classes
// (02 section 4.3). The pack never links UE5-UpdateBudget's code: classes are named by soft class path.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "GameplayLintRule.h"
#include "GLUpdateBudgetRules.generated.h"

/**
 * Project Settings > Plugins > Gameplay Lint: UpdateBudget. Which classes are budget clients, where the scheduler
 * ticks, which actors must put their per-frame work through the budget, and which components may tick every frame.
 * Paths are resolved without loading: a class that is not loaded (another project's) simply matches nothing.
 */
UCLASS(config = Editor, defaultconfig, meta = (DisplayName = "Gameplay Lint: UpdateBudget"))
class UGameplayLintUpdateBudgetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UGameplayLintUpdateBudgetSettings();

	/** Component classes that run through the update budget (UE5-UpdateBudget: UBudgetedUpdateComponent). */
	UPROPERTY(EditAnywhere, config, Category = "Budget", meta = (MetaClass = "/Script/Engine.ActorComponent"))
	TArray<FSoftClassPath> BudgetClientClasses;

	/** The scheduler's tick group in UE5-UpdateBudget. Budget clients must tick in the same group. */
	UPROPERTY(EditAnywhere, config, Category = "Budget")
	TEnumAsByte<ETickingGroup> SchedulerTickGroup = TG_PrePhysics;

	/** Actor classes whose per-frame work must go through the budget. An actor with a budget client is always in scope. */
	UPROPERTY(EditAnywhere, config, Category = "Budget", meta = (MetaClass = "/Script/Engine.Actor"))
	TArray<FSoftClassPath> BudgetScopeActorClasses;

	/** Components allowed to tick every frame inside the scope (animation has its own budget; movement integrates per frame). */
	UPROPERTY(EditAnywhere, config, Category = "Budget", meta = (MetaClass = "/Script/Engine.ActorComponent"))
	TArray<FSoftClassPath> PerFrameAllowList;

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};

/** GL.UB.TickOutsideBudget: an actor in the budget's scope that still ticks every frame through another path. */
UCLASS()
class UGLRule_TickOutsideBudget : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLRule_TickOutsideBudget();

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override;
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override;
};

/** GL.UB.ClientTickGroup: a budget client that ticks in another group than the scheduler. */
UCLASS()
class UGLRule_ClientTickGroup : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLRule_ClientTickGroup();

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override;
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override;
};

/** GL.UB.ClientNeverTicks: a budget client whose tick can never start, so its native path never runs. */
UCLASS()
class UGLRule_ClientNeverTicks : public UGameplayLintRule
{
	GENERATED_BODY()

public:
	UGLRule_ClientNeverTicks();

protected:
	virtual bool AppliesTo(const FAssetData& AssetData) const override;
	virtual void CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const override;
};
