// P10 GameplayLint - fixture actors and components.

#include "GLFixtureActors.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GLFixtureActors)

AGLFixtureAgent::AGLFixtureAgent()
{
	// The agent's own tick is off: per-frame work belongs in a budget client.
	PrimaryActorTick.bCanEverTick = false;
}

AGLFixtureTickingAgent::AGLFixtureTickingAgent()
{
	// A native parent that ticks: the "inherited per-frame actor tick" defect of the corpus.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

UGLFixtureBudgetClient::UGLFixtureBudgetClient()
{
	// The same contract as UBudgetedUpdateComponent: tickable (components default to false) and in the scheduler's
	// group (components default to TG_DuringPhysics, ActorComponent.cpp:548-550).
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

UGLFixtureTicker::UGLFixtureTicker()
{
	// Ticks every frame in the default group. Start-enabled and interval 0 are already the defaults.
	PrimaryComponentTick.bCanEverTick = true;
}
