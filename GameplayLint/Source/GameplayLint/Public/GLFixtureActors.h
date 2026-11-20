// P10 GameplayLint - fixture actors and components for the UpdateBudget rules and the Blueprint-default variant.
// UGLFixtureBudgetClient stands in for UE5-UpdateBudget's UBudgetedUpdateComponent: same tick contract, no link to
// that project. The project config tells the UpdateBudget rule pack to treat it as a budget client.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "GLFixtureData.h"
#include "GLFixtureActors.generated.h"

/**
 * The budget scope's base class: an agent whose per-frame work must go through the update budget.
 * It never ticks itself. Payload lets a Blueprint subclass hold gameplay data in its class default object.
 */
UCLASS(MinimalAPI, Blueprintable)
class AGLFixtureAgent : public AActor
{
	GENERATED_BODY()

public:
	AGLFixtureAgent();

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGLFixturePayload Payload;
};

/** An agent whose native class ticks every frame, so every Blueprint made from it inherits a per-frame actor tick. */
UCLASS(MinimalAPI, Blueprintable)
class AGLFixtureTickingAgent : public AGLFixtureAgent
{
	GENERATED_BODY()

public:
	AGLFixtureTickingAgent();
};

/**
 * Stand-in for UE5-UpdateBudget's budget client. The same tick defaults: it can tick (the native baseline path), and
 * it ticks in TG_PrePhysics, the scheduler's group. A designer can still change both on a template, which is what the
 * UpdateBudget rules catch.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (GameplayLint), meta = (BlueprintSpawnableComponent))
class UGLFixtureBudgetClient : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLFixtureBudgetClient();
};

/** A plain component that ticks every frame in the default group: per-frame work outside any budget. */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (GameplayLint), meta = (BlueprintSpawnableComponent))
class UGLFixtureTicker : public UActorComponent
{
	GENERATED_BODY()

public:
	UGLFixtureTicker();
};
