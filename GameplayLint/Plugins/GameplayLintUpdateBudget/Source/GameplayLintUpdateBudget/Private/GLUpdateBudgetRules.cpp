// GameplayLintUpdateBudget - the settings and the three GL.UB rules.

#include "GLUpdateBudgetRules.h"

#include "Components/ActorComponent.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GLUpdateBudgetRules)

#define LOCTEXT_NAMESPACE "GameplayLintUpdateBudget"

namespace GLUpdateBudget
{
	/**
	 * The settings' class lists, resolved once per checked asset rather than once per template and rule. Paths are
	 * resolved without loading: a class that is not loaded (another project's) matches nothing.
	 */
	struct FClasses
	{
		TArray<const UClass*> Clients;
		TArray<const UClass*> Scope;
		TArray<const UClass*> AllowList;
		ETickingGroup SchedulerGroup = TG_PrePhysics;

		FClasses()
		{
			const UGameplayLintUpdateBudgetSettings& Settings = *GetDefault<UGameplayLintUpdateBudgetSettings>();
			Resolve(Settings.BudgetClientClasses, Clients);
			Resolve(Settings.BudgetScopeActorClasses, Scope);
			Resolve(Settings.PerFrameAllowList, AllowList);
			SchedulerGroup = Settings.SchedulerTickGroup.GetValue();
		}

		bool IsClient(const UClass* Class) const { return IsChildOfAny(Class, Clients); }
		bool IsInScope(const UClass* Class) const { return IsChildOfAny(Class, Scope); }
		bool IsAllowListed(const UClass* Class) const { return IsChildOfAny(Class, AllowList); }

	private:
		static void Resolve(const TArray<FSoftClassPath>& Paths, TArray<const UClass*>& Out)
		{
			for (const FSoftClassPath& Path : Paths)
			{
				if (const UClass* Class = Path.ResolveClass())
				{
					Out.Add(Class);
				}
			}
		}

		static bool IsChildOfAny(const UClass* Class, const TArray<const UClass*>& Bases)
		{
			return Class && Bases.ContainsByPredicate([Class](const UClass* Base) { return Class->IsChildOf(Base); });
		}
	};

	/**
	 * Is the component's tick enabled once it spawns? The tick manager enables a tick that starts enabled
	 * (ActorComponent.cpp:1739-1752). Auto-activation enables it too: InitializeComponents activates every bAutoActivate
	 * component (Actor.cpp:6399) and Activate enables the tick (ActorComponent.cpp:2815-2819).
	 */
	bool StartsTicking(const UActorComponent& Component)
	{
		const FTickFunction& Tick = Component.PrimaryComponentTick;
		return Tick.bCanEverTick && (Tick.bStartWithTickEnabled || Component.bAutoActivate);
	}

	bool TicksEveryFrame(const UActorComponent& Component)
	{
		return StartsTicking(Component) && Component.PrimaryComponentTick.TickInterval <= 0.f;
	}

	/** The actor's own tick function: enabled at spawn if it can tick and starts enabled; every frame without an interval. */
	bool TicksEveryFrame(const AActor& Actor)
	{
		const FTickFunction& Tick = Actor.PrimaryActorTick;
		return Tick.bCanEverTick && Tick.bStartWithTickEnabled && Tick.TickInterval <= 0.f;
	}

	/** The Blueprint's native parent class, read from registry tags without loading (Blueprint.cpp:2316-2333). */
	const UClass* NativeParent(const FAssetData& AssetData)
	{
		return AssetData.IsInstanceOf(UBlueprint::StaticClass()) ? UBlueprint::GetBlueprintParentClassFromAssetTags(AssetData) : nullptr;
	}

	/** The class a Blueprint's compile produced: where its defaults and component templates live. */
	UClass* GeneratedClass(UObject* Asset)
	{
		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	/** Every component template of an actor class: native, Blueprint-added and inherited, with child overrides. */
	TArray<const UActorComponent*> Templates(UClass* ActorClass)
	{
		TArray<const UActorComponent*> Components;
		AActor::ForEachComponentOfActorClassDefault(ActorClass, UActorComponent::StaticClass(), [&Components](const UActorComponent* Template)
		{
			Components.Add(Template);
			return true;
		});
		return Components;
	}

	FString ComponentPath(const UActorComponent* Template)
	{
		FString Name = Template->GetName();
		Name.RemoveFromEnd(TEXT("_GEN_VARIABLE"));   // how Blueprint component templates are named
		return FString::Printf(TEXT("Component[%s]"), *Name);
	}

	FText GroupName(ETickingGroup Group)
	{
		return StaticEnum<ETickingGroup>()->GetDisplayNameTextByValue(int64(Group));
	}

	/**
	 * Calls Check on every budget client the asset defines: the client templates of an actor Blueprint, or the class
	 * default object of a Blueprint subclass of a client.
	 */
	void ForEachClient(UObject* Asset, const FClasses& Classes, TFunctionRef<void(const UActorComponent&, const FString&)> Check)
	{
		UClass* Class = GeneratedClass(Asset);
		if (!Class)
		{
			return;
		}
		if (Class->IsChildOf(AActor::StaticClass()))
		{
			for (const UActorComponent* Template : Templates(Class))
			{
				if (Classes.IsClient(Template->GetClass()))
				{
					Check(*Template, ComponentPath(Template));
				}
			}
		}
		else if (Class->IsChildOf(UActorComponent::StaticClass()) && Classes.IsClient(Class))
		{
			Check(*Class->GetDefaultObject<UActorComponent>(), TEXT("Default"));
		}
	}

	bool IsActorOrComponentBlueprint(const FAssetData& AssetData)
	{
		const UClass* Parent = NativeParent(AssetData);
		return Parent && (Parent->IsChildOf(AActor::StaticClass()) || Parent->IsChildOf(UActorComponent::StaticClass()));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Settings

UGameplayLintUpdateBudgetSettings::UGameplayLintUpdateBudgetSettings()
{
	// UE5-UpdateBudget's own client. A project names its client classes in its config (this project: the fixture class).
	BudgetClientClasses.Add(FSoftClassPath(TEXT("/Script/UpdateBudgetCore.BudgetedUpdateComponent")));
	// Animation is budgeted by Epic's Animation Budget Allocator; movement integrates every frame (UE5-UpdateBudget 01 section 8).
	PerFrameAllowList.Add(FSoftClassPath(TEXT("/Script/Engine.SkeletalMeshComponent")));
	PerFrameAllowList.Add(FSoftClassPath(TEXT("/Script/Engine.MovementComponent")));
}

// ---------------------------------------------------------------------------------------------------------------------
// GL.UB.TickOutsideBudget

UGLRule_TickOutsideBudget::UGLRule_TickOutsideBudget()
{
	RuleId = TEXT("GL.UB.TickOutsideBudget");
	Lesson = LOCTEXT("TickOutsideBudgetLesson", "UE5-UpdateBudget 02 section 11 and 01 section 1: per-frame ticks cost the same whether they matter or not; work outside the budget is work the budget cannot bound.");
	DefaultSeverity = EGameplayLintSeverity::Warning;
}

bool UGLRule_TickOutsideBudget::AppliesTo(const FAssetData& AssetData) const
{
	const UClass* Parent = GLUpdateBudget::NativeParent(AssetData);
	return Parent && Parent->IsChildOf(AActor::StaticClass());
}

void UGLRule_TickOutsideBudget::CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const
{
	UClass* Class = GLUpdateBudget::GeneratedClass(Asset);
	if (!Class || !Class->IsChildOf(AActor::StaticClass()))
	{
		return;
	}
	const GLUpdateBudget::FClasses Classes;
	const TArray<const UActorComponent*> Components = GLUpdateBudget::Templates(Class);

	// In scope: a configured actor class, or any actor that already has a budget client (it opted in).
	const bool bHasClient = Components.ContainsByPredicate([&Classes](const UActorComponent* Template) { return Classes.IsClient(Template->GetClass()); });
	if (!bHasClient && !Classes.IsInScope(Class))
	{
		return;
	}

	if (GLUpdateBudget::TicksEveryFrame(*Class->GetDefaultObject<AActor>()))
	{
		Lint.Report(TEXT("Default.PrimaryActorTick"), LOCTEXT("ActorTicks", "the actor itself ticks every frame, outside the update budget"));
	}
	for (const UActorComponent* Template : Components)
	{
		const UClass* ComponentClass = Template->GetClass();
		if (!Classes.IsClient(ComponentClass) && !Classes.IsAllowListed(ComponentClass) && GLUpdateBudget::TicksEveryFrame(*Template))
		{
			Lint.Report(GLUpdateBudget::ComponentPath(Template), FText::Format(
				LOCTEXT("ComponentTicks", "{0} ticks every frame outside the update budget; make it a budget client or give it a tick interval"),
				FText::FromString(ComponentClass->GetName())));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// GL.UB.ClientTickGroup

UGLRule_ClientTickGroup::UGLRule_ClientTickGroup()
{
	RuleId = TEXT("GL.UB.ClientTickGroup");
	Lesson = LOCTEXT("ClientTickGroupLesson", "UE5-UpdateBudget 07: \"Leaving the default TG_DuringPhysics\": clients and the scheduler run at different points in the frame.");
	DefaultSeverity = EGameplayLintSeverity::Warning;
}

bool UGLRule_ClientTickGroup::AppliesTo(const FAssetData& AssetData) const
{
	return GLUpdateBudget::IsActorOrComponentBlueprint(AssetData);
}

void UGLRule_ClientTickGroup::CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const
{
	const GLUpdateBudget::FClasses Classes;
	GLUpdateBudget::ForEachClient(Asset, Classes, [&](const UActorComponent& Client, const FString& Where)
	{
		// A client that never ticks is GL.UB.ClientNeverTicks's finding; its group has no effect, so one defect gives one finding.
		const ETickingGroup Group = Client.PrimaryComponentTick.TickGroup.GetValue();
		if (GLUpdateBudget::StartsTicking(Client) && Group != Classes.SchedulerGroup)
		{
			Lint.Report(Where + TEXT(".PrimaryComponentTick.TickGroup"), FText::Format(
				LOCTEXT("WrongGroup", "budget client ticks in {0}; the update scheduler ticks in {1}"),
				GLUpdateBudget::GroupName(Group), GLUpdateBudget::GroupName(Classes.SchedulerGroup)));
		}
	});
}

// ---------------------------------------------------------------------------------------------------------------------
// GL.UB.ClientNeverTicks

UGLRule_ClientNeverTicks::UGLRule_ClientNeverTicks()
{
	RuleId = TEXT("GL.UB.ClientNeverTicks");
	Lesson = LOCTEXT("ClientNeverTicksLesson", "UE5-UpdateBudget 07: \"Forgetting bCanEverTick = true\": the native path never updates.");
	DefaultSeverity = EGameplayLintSeverity::Error;
}

bool UGLRule_ClientNeverTicks::AppliesTo(const FAssetData& AssetData) const
{
	return GLUpdateBudget::IsActorOrComponentBlueprint(AssetData);
}

void UGLRule_ClientNeverTicks::CheckAsset(UObject* Asset, FGameplayLintContext& Lint) const
{
	const GLUpdateBudget::FClasses Classes;
	GLUpdateBudget::ForEachClient(Asset, Classes, [&](const UActorComponent& Client, const FString& Where)
	{
		if (!GLUpdateBudget::StartsTicking(Client))
		{
			Lint.Report(Where + TEXT(".PrimaryComponentTick"), !Client.PrimaryComponentTick.bCanEverTick
				? LOCTEXT("CannotTick", "budget client can never tick (bCanEverTick is false), so its native update path never runs")
				: LOCTEXT("StartsDisabled", "budget client never starts ticking (Start with Tick Enabled and Auto Activate are both off), so its native update path never runs"));
		}
	});
}

#undef LOCTEXT_NAMESPACE
