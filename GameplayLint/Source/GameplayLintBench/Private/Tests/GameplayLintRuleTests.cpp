// P10 GameplayLint - automation tests for the rule packs, on in-memory objects (03, S5 onwards).
// Run: Tools\run_tests.bat (editor, headless), filter "GameplayLint".
//
// The packs have no public headers (nothing may depend on a pack, 02 section 3), so the tests find each rule class
// by its path, as the engine's validator discovery does, and drive it through the engine's own entry point.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "EditorValidatorBase.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTagsManager.h"
#include "GLFixtureActors.h"
#include "GLFixtureData.h"
#include "GLTestHelpers.h"
#include "Misc/DataValidation.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#include <limits>

namespace GameplayLintRuleTests
{
	using GameplayLintTestHelpers::Flags;
	using GameplayLintTestHelpers::MakeTag;

	/**
	 * A new instance of the rule class at ClassPath, or null (and a test error) if the pack isn't loaded. Held by a strong
	 * pointer: nothing else references it, and compiling a Blueprint collects garbage (BlueprintCompilationManager.cpp:423-427).
	 */
	TStrongObjectPtr<UEditorValidatorBase> NewRule(FAutomationTestBase& Test, const TCHAR* ClassPath)
	{
		UClass* Class = FindObject<UClass>(nullptr, ClassPath);
		Test.TestNotNull(FString::Printf(TEXT("rule class %s is loaded"), ClassPath), Class);
		return TStrongObjectPtr<UEditorValidatorBase>(Class ? NewObject<UEditorValidatorBase>(GetTransientPackage(), Class) : nullptr);
	}

	/** Runs Rule on Asset; returns every message text and sets OutResult. */
	TArray<FString> Validate(UEditorValidatorBase& Rule, UObject* Asset, EDataValidationResult& OutResult)
	{
		FDataValidationContext Context(false, EDataValidationUsecase::Script, {});
		OutResult = Rule.ValidateLoadedAsset(FAssetData(Asset), Asset, Context);
		TArray<FString> Messages;
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			Messages.Add(Issue.TokenizedMessage.IsValid() ? Issue.TokenizedMessage->ToText().ToString() : Issue.Message.ToString());
		}
		return Messages;
	}

	/**
	 * One case: Asset must give exactly the messages that contain Expected (in any order). The result must be Invalid
	 * exactly when an error is expected; a rule whose severity is Warning never fails the asset.
	 */
	void ExpectOn(FAutomationTestBase& Test, UEditorValidatorBase& Rule, const TCHAR* RuleId, const TCHAR* Case,
		UObject* Asset, TArray<const TCHAR*> Expected, bool bErrors = true)
	{
		EDataValidationResult Result = EDataValidationResult::NotValidated;
		const TArray<FString> Messages = Validate(Rule, Asset, Result);

		Test.TestEqual(FString::Printf(TEXT("%s: number of findings"), Case), Messages.Num(), Expected.Num());
		Test.TestEqual(FString::Printf(TEXT("%s: result"), Case), int32(Result),
			int32(bErrors && !Expected.IsEmpty() ? EDataValidationResult::Invalid : EDataValidationResult::Valid));
		for (const TCHAR* Text : Expected)
		{
			Test.TestTrue(FString::Printf(TEXT("%s: a finding contains '%s'"), Case, Text),
				Messages.ContainsByPredicate([Text](const FString& Message) { return Message.Contains(Text); }));
		}
		for (const FString& Message : Messages)
		{
			Test.TestTrue(FString::Printf(TEXT("%s: carries the rule ID: %s"), Case, *Message), Message.Contains(RuleId));
		}
	}

	/** ExpectOn for a new in-memory fixture data asset that Setup fills. */
	void Expect(FAutomationTestBase& Test, UEditorValidatorBase& Rule, const TCHAR* RuleId, const TCHAR* Case,
		TFunctionRef<void(UGLFixtureDataAsset&)> Setup, TArray<const TCHAR*> Expected)
	{
		UGLFixtureDataAsset* Asset = NewObject<UGLFixtureDataAsset>(GetTransientPackage());
		Setup(*Asset);
		ExpectOn(Test, Rule, RuleId, Case, Asset, MoveTemp(Expected));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintUnknownTagTest, "GameplayLint.Rules.UnknownTag", GameplayLintRuleTests::Flags)
bool FGameplayLintUnknownTagTest::RunTest(const FString& Parameters)
{
	using namespace GameplayLintRuleTests;
	const TStrongObjectPtr<UEditorValidatorBase> Rule = NewRule(*this, TEXT("/Script/GameplayLintDataRules.GLRule_UnknownTag"));
	if (!Rule)
	{
		return false;
	}
	const TCHAR* Id = TEXT("[GL.Data.UnknownTag]");

	Expect(*this, *Rule, Id, TEXT("no tags"), [](UGLFixtureDataAsset&) {}, {});

	// A tag the dictionary does contain must pass. Any declared tag will do: the tests do not depend on which tags a
	// project declares, so the case is skipped (and says so) when the dictionary is empty.
	FGameplayTagContainer Declared;
	UGameplayTagsManager::Get().RequestAllGameplayTags(Declared, /*OnlyIncludeDictionaryTags*/ true);
	if (Declared.Num() > 0)
	{
		const FGameplayTag Known = Declared.First();
		Expect(*this, *Rule, Id, TEXT("declared tag, single and in a container"),
			[Known](UGLFixtureDataAsset& Asset) { Asset.Tag = Known; Asset.Tags.AddTag(Known); }, {});
	}
	else
	{
		AddInfo(TEXT("the tag dictionary is empty: the declared-tag case is skipped"));
	}
	Expect(*this, *Rule, Id, TEXT("unknown single tag"),
		[](UGLFixtureDataAsset& Asset) { Asset.Tag = MakeTag(TEXT("Test.Lint.Unknown.Single")); },
		{ TEXT("'Test.Lint.Unknown.Single'") });
	Expect(*this, *Rule, Id, TEXT("unknown tag in a container and in an array element"),
		[](UGLFixtureDataAsset& Asset)
		{
			Asset.Tags.AddTagFast(MakeTag(TEXT("Test.Lint.Unknown.Container")));
			Asset.List.AddDefaulted_GetRef().Tag = MakeTag(TEXT("Test.Lint.Unknown.Array"));
		},
		{ TEXT("'Test.Lint.Unknown.Container'"), TEXT("List[0]") });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintDanglingRowTest, "GameplayLint.Rules.DanglingRow", GameplayLintRuleTests::Flags)
bool FGameplayLintDanglingRowTest::RunTest(const FString& Parameters)
{
	using namespace GameplayLintRuleTests;
	const TStrongObjectPtr<UEditorValidatorBase> Rule = NewRule(*this, TEXT("/Script/GameplayLintDataRules.GLRule_DanglingRow"));
	if (!Rule)
	{
		return false;
	}
	const TCHAR* Id = TEXT("[GL.Data.DanglingRow]");

	// Unnamed: a second run in the same editor session gets fresh objects.
	const TStrongObjectPtr<UDataTable> TableRoot(NewObject<UDataTable>(GetTransientPackage()));   // reused by every case
	UDataTable* Table = TableRoot.Get();
	Table->RowStruct = FGLFixtureRow::StaticStruct();
	Table->AddRow(TEXT("R00"), FGLFixtureRow());
	const TStrongObjectPtr<UCurveTable> CurvesRoot(NewObject<UCurveTable>(GetTransientPackage()));
	UCurveTable* Curves = CurvesRoot.Get();
	Curves->AddRichCurve(TEXT("C00")).AddKey(0.f, 1.f);

	// Every case starts valid: the required handle names a real row.
	auto Valid = [Table](UGLFixtureDataAsset& Asset)
	{
		Asset.RequiredRow.DataTable = Table;
		Asset.RequiredRow.RowName = TEXT("R00");
	};

	Expect(*this, *Rule, Id, TEXT("existing rows; the optional nested handles stay empty"),
		[&](UGLFixtureDataAsset& Asset)
		{
			Valid(Asset);
			Asset.Row = Asset.RequiredRow;
			Asset.CurveRow.CurveTable = Curves;
			Asset.CurveRow.RowName = TEXT("C00");
		},
		{});
	Expect(*this, *Rule, Id, TEXT("missing DataTable row"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.Row.DataTable = Table; Asset.Row.RowName = TEXT("R99"); },
		{ TEXT("'R99'") });
	Expect(*this, *Rule, Id, TEXT("missing CurveTable row"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.CurveRow.CurveTable = Curves; Asset.CurveRow.RowName = TEXT("C99"); },
		{ TEXT("'C99'") });
	Expect(*this, *Rule, Id, TEXT("row without a table, in a nested payload"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.Nested.Row.RowName = TEXT("R00"); },
		{ TEXT("no table") });
	Expect(*this, *Rule, Id, TEXT("empty required handle"),
		[](UGLFixtureDataAsset&) {},
		{ TEXT("required row handle is empty") });
	Expect(*this, *Rule, Id, TEXT("required handle with a table but no row"),
		[&](UGLFixtureDataAsset& Asset) { Asset.RequiredRow.DataTable = Table; },
		{ TEXT("names no row") });
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintCurveTest, "GameplayLint.Rules.Curve", GameplayLintRuleTests::Flags)
bool FGameplayLintCurveTest::RunTest(const FString& Parameters)
{
	using namespace GameplayLintRuleTests;
	const TStrongObjectPtr<UEditorValidatorBase> Rule = NewRule(*this, TEXT("/Script/GameplayLintDataRules.GLRule_Curve"));
	if (!Rule)
	{
		return false;
	}
	const TCHAR* Id = TEXT("[GL.Data.Curve]");
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float Infinity = std::numeric_limits<float>::infinity();

	// Every data-asset case starts valid: the required curve has a key.
	auto Valid = [](UGLFixtureDataAsset& Asset) { Asset.RequiredCurve.EditorCurveData.AddKey(0.f, 1.f); };

	Expect(*this, *Rule, Id, TEXT("rising cooldown with a flat segment, falling fall-off (the near-misses)"),
		[&](UGLFixtureDataAsset& Asset)
		{
			Valid(Asset);
			Asset.CooldownByLevel.EditorCurveData.AddKey(1.f, 1.f);
			Asset.CooldownByLevel.EditorCurveData.AddKey(2.f, 2.f);
			Asset.CooldownByLevel.EditorCurveData.AddKey(3.f, 2.f);
			Asset.FalloffByDistance.EditorCurveData.AddKey(0.f, 10.f);
			Asset.FalloffByDistance.EditorCurveData.AddKey(100.f, 0.f);
		},
		{});
	Expect(*this, *Rule, Id, TEXT("NaN key value in an unannotated curve"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.Curve.EditorCurveData.AddKey(0.f, NaN); },
		{ TEXT("not a finite number") });
	Expect(*this, *Rule, Id, TEXT("NaN key time"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.Curve.EditorCurveData.AddKey(NaN, 1.f); },
		{ TEXT("not a finite number") });
	Expect(*this, *Rule, Id, TEXT("NaN tangent"),
		[&](UGLFixtureDataAsset& Asset)
		{
			Valid(Asset);
			const FKeyHandle Key = Asset.Curve.EditorCurveData.AddKey(0.f, 0.f);
			Asset.Curve.EditorCurveData.AddKey(1.f, 1.f);
			Asset.Curve.EditorCurveData.GetKey(Key).LeaveTangent = NaN;
		},
		{ TEXT("tangent") });
	Expect(*this, *Rule, Id, TEXT("infinite key in a nested payload"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.Nested.Curve.EditorCurveData.AddKey(0.f, Infinity); },
		{ TEXT("Nested") });
	Expect(*this, *Rule, Id, TEXT("infinite key in a curve that must rise: one finding, not two"),
		[&](UGLFixtureDataAsset& Asset)
		{
			Valid(Asset);
			Asset.CooldownByLevel.EditorCurveData.AddKey(1.f, 1.f);
			Asset.CooldownByLevel.EditorCurveData.AddKey(2.f, Infinity);
			Asset.CooldownByLevel.EditorCurveData.AddKey(3.f, 2.f);
		},
		{ TEXT("not a finite number") });
	Expect(*this, *Rule, Id, TEXT("reversal in a curve that must rise"),
		[&](UGLFixtureDataAsset& Asset)
		{
			Valid(Asset);
			Asset.CooldownByLevel.EditorCurveData.AddKey(1.f, 3.f);
			Asset.CooldownByLevel.EditorCurveData.AddKey(2.f, 1.f);
		},
		{ TEXT("must not fall") });
	Expect(*this, *Rule, Id, TEXT("rise in a curve that must fall"),
		[&](UGLFixtureDataAsset& Asset)
		{
			Valid(Asset);
			Asset.FalloffByDistance.EditorCurveData.AddKey(0.f, 1.f);
			Asset.FalloffByDistance.EditorCurveData.AddKey(100.f, 5.f);
		},
		{ TEXT("must not rise") });
	Expect(*this, *Rule, Id, TEXT("empty required curve"),
		[](UGLFixtureDataAsset&) {},
		{ TEXT("required curve has no keys") });
	Expect(*this, *Rule, Id, TEXT("required curve with only a default value: it has data"),
		[](UGLFixtureDataAsset& Asset) { Asset.RequiredCurve.EditorCurveData.DefaultValue = 5.f; },
		{});

	// External curves: the keys belong to the curve asset, which reports its own non-finite data once.
	const TStrongObjectPtr<UCurveFloat> EmptyRoot(NewObject<UCurveFloat>(GetTransientPackage()));   // reused across cases
	const TStrongObjectPtr<UCurveFloat> BrokenRoot(NewObject<UCurveFloat>(GetTransientPackage()));
	UCurveFloat* Empty = EmptyRoot.Get();
	UCurveFloat* Broken = BrokenRoot.Get();
	Broken->FloatCurve.AddKey(0.f, NaN);
	Expect(*this, *Rule, Id, TEXT("required curve pointing at an empty curve asset"),
		[Empty](UGLFixtureDataAsset& Asset) { Asset.RequiredCurve.ExternalCurve = Empty; },
		{ TEXT("external curve") });
	Expect(*this, *Rule, Id, TEXT("curve pointing at a broken curve asset: reported on that asset, not here"),
		[&](UGLFixtureDataAsset& Asset) { Valid(Asset); Asset.Curve.ExternalCurve = Broken; },
		{});
	ExpectOn(*this, *Rule, Id, TEXT("the broken curve asset itself"), Broken, { TEXT("FloatCurve") });

	UCurveTable* Table = NewObject<UCurveTable>(GetTransientPackage());
	Table->AddRichCurve(TEXT("C00")).AddKey(0.f, 1.f);
	Table->AddRichCurve(TEXT("C01")).AddKey(0.f, Infinity);
	ExpectOn(*this, *Rule, Id, TEXT("curve table with an infinite row"), Table, { TEXT("Row[C01]") });
	return true;
}

namespace GameplayLintRuleTests
{
	/**
	 * An in-memory Blueprint of Parent. AddComponents adds component nodes and sets their templates before the compile,
	 * as the editor does; SetDefaults changes the class defaults, which exist only after it (05 C2). The UpdateBudget
	 * tests run against the project's pack settings (Config/DefaultEditor.ini): the fixture budget client is a client,
	 * the fixture agent is in scope. Changing that config is meant to fail these tests.
	 */
	UBlueprint* MakeBlueprint(UClass* Parent, const TCHAR* Name, TFunctionRef<void(UBlueprint&)> AddComponents,
		TFunctionRef<void(UObject&)> SetDefaults = [](UObject&) {})
	{
		// A unique name, so running the tests twice in one editor session cannot collide with the first run's classes.
		const FName Unique = MakeUniqueObjectName(GetTransientPackage(), UBlueprint::StaticClass(), FName(Name));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, GetTransientPackage(), Unique, BPTYPE_Normal, TEXT("GameplayLintTests"));
		AddComponents(*Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefaults(*Blueprint->GeneratedClass->GetDefaultObject());
		return Blueprint;
	}

	UActorComponent& AddComponent(UBlueprint& Blueprint, UClass* Class, const TCHAR* Name)
	{
		USCS_Node* Node = Blueprint.SimpleConstructionScript->CreateNode(Class, Name);
		Blueprint.SimpleConstructionScript->AddNode(Node);
		return *Node->ComponentTemplate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintUpdateBudgetTest, "GameplayLint.Rules.UpdateBudget", GameplayLintRuleTests::Flags)
bool FGameplayLintUpdateBudgetTest::RunTest(const FString& Parameters)
{
	using namespace GameplayLintRuleTests;
	const TStrongObjectPtr<UEditorValidatorBase> Outside = NewRule(*this, TEXT("/Script/GameplayLintUpdateBudget.GLRule_TickOutsideBudget"));
	const TStrongObjectPtr<UEditorValidatorBase> Group = NewRule(*this, TEXT("/Script/GameplayLintUpdateBudget.GLRule_ClientTickGroup"));
	const TStrongObjectPtr<UEditorValidatorBase> Never = NewRule(*this, TEXT("/Script/GameplayLintUpdateBudget.GLRule_ClientNeverTicks"));
	if (!Outside || !Group || !Never)
	{
		return false;
	}
	UClass* Agent = AGLFixtureAgent::StaticClass();
	UClass* Client = UGLFixtureBudgetClient::StaticClass();
	UClass* Ticker = UGLFixtureTicker::StaticClass();
	auto NoComponents = [](UBlueprint&) {};

	// GL.UB.TickOutsideBudget (severity Warning: the asset stays valid).
	const TCHAR* OutsideId = TEXT("[GL.UB.TickOutsideBudget]");
	ExpectOn(*this, *Outside, OutsideId, TEXT("agent with an interval ticker (near-miss)"),
		MakeBlueprint(Agent, TEXT("BP_TestIntervalTicker"), [&](UBlueprint& B) { AddComponent(B, Ticker, TEXT("Ticker")).PrimaryComponentTick.TickInterval = 0.2f; }),
		{}, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("agent with a start-disabled ticker (near-miss)"),
		MakeBlueprint(Agent, TEXT("BP_TestDisabledTicker"), [&](UBlueprint& B) { AddComponent(B, Ticker, TEXT("Ticker")).PrimaryComponentTick.bStartWithTickEnabled = false; }),
		{}, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("agent with allow-listed animation and movement components (near-miss)"),
		MakeBlueprint(Agent, TEXT("BP_TestAllowList"), [](UBlueprint& B)
		{
			AddComponent(B, USkeletalMeshComponent::StaticClass(), TEXT("Mesh"));
			AddComponent(B, UProjectileMovementComponent::StaticClass(), TEXT("Movement"));
		}),
		{}, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("agent with a per-frame ticker"),
		MakeBlueprint(Agent, TEXT("BP_TestFrameTicker"), [&](UBlueprint& B) { AddComponent(B, Ticker, TEXT("Ticker")); }),
		{ TEXT("Component[Ticker]") }, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("start-disabled ticker that auto-activates: it ticks every frame"),
		MakeBlueprint(Agent, TEXT("BP_TestAutoTicker"), [&](UBlueprint& B)
		{
			UActorComponent& Template = AddComponent(B, Ticker, TEXT("Ticker"));
			Template.PrimaryComponentTick.bStartWithTickEnabled = false;
			Template.bAutoActivate = true;
		}),
		{ TEXT("Component[Ticker]") }, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("agent whose native parent ticks every frame"),
		MakeBlueprint(AGLFixtureTickingAgent::StaticClass(), TEXT("BP_TestTickingAgent"), NoComponents),
		{ TEXT("the actor itself ticks") }, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("plain actor with a per-frame ticker: not in the budget's scope"),
		MakeBlueprint(AActor::StaticClass(), TEXT("BP_TestOutOfScope"), [&](UBlueprint& B) { AddComponent(B, Ticker, TEXT("Ticker")); }),
		{}, false);
	ExpectOn(*this, *Outside, OutsideId, TEXT("plain actor that opted in with a client, plus a per-frame ticker"),
		MakeBlueprint(AActor::StaticClass(), TEXT("BP_TestOptedIn"), [&](UBlueprint& B)
		{
			AddComponent(B, Client, TEXT("Client"));
			AddComponent(B, Ticker, TEXT("Ticker"));
		}),
		{ TEXT("Component[Ticker]") }, false);

	// GL.UB.ClientTickGroup (Warning), on a client template and on a client subclass's defaults.
	const TCHAR* GroupId = TEXT("[GL.UB.ClientTickGroup]");
	ExpectOn(*this, *Group, GroupId, TEXT("client in the scheduler's group"),
		MakeBlueprint(Agent, TEXT("BP_TestClientGroupOk"), [&](UBlueprint& B) { AddComponent(B, Client, TEXT("Client")); }),
		{}, false);
	ExpectOn(*this, *Group, GroupId, TEXT("client template in TG_DuringPhysics"),
		MakeBlueprint(Agent, TEXT("BP_TestClientGroupBad"), [&](UBlueprint& B) { AddComponent(B, Client, TEXT("Client")).PrimaryComponentTick.TickGroup = TG_DuringPhysics; }),
		{ TEXT("Component[Client].PrimaryComponentTick.TickGroup") }, false);
	ExpectOn(*this, *Group, GroupId, TEXT("client subclass whose defaults tick in TG_PostPhysics"),
		MakeBlueprint(Client, TEXT("BPC_TestClientGroup"), NoComponents,
			[](UObject& Defaults) { CastChecked<UActorComponent>(&Defaults)->PrimaryComponentTick.TickGroup = TG_PostPhysics; }),
		{ TEXT("Default.PrimaryComponentTick.TickGroup") }, false);
	ExpectOn(*this, *Group, GroupId, TEXT("client that never ticks, in another group: only ClientNeverTicks reports it"),
		MakeBlueprint(Agent, TEXT("BP_TestClientGroupNever"), [&](UBlueprint& B)
		{
			UActorComponent& Template = AddComponent(B, Client, TEXT("Client"));
			Template.PrimaryComponentTick.TickGroup = TG_DuringPhysics;
			Template.PrimaryComponentTick.bStartWithTickEnabled = false;
		}),
		{}, false);

	// GL.UB.ClientNeverTicks (Error).
	const TCHAR* NeverId = TEXT("[GL.UB.ClientNeverTicks]");
	ExpectOn(*this, *Never, NeverId, TEXT("client template that starts disabled"),
		MakeBlueprint(Agent, TEXT("BP_TestClientNever"), [&](UBlueprint& B) { AddComponent(B, Client, TEXT("Client")).PrimaryComponentTick.bStartWithTickEnabled = false; }),
		{ TEXT("Start with Tick Enabled") });
	ExpectOn(*this, *Never, NeverId, TEXT("client subclass whose defaults start disabled"),
		MakeBlueprint(Client, TEXT("BPC_TestClientNever"), NoComponents,
			[](UObject& Defaults) { CastChecked<UActorComponent>(&Defaults)->PrimaryComponentTick.bStartWithTickEnabled = false; }),
		{ TEXT("Default.PrimaryComponentTick") });
	ExpectOn(*this, *Never, NeverId, TEXT("start-disabled client that auto-activates: it ticks"),
		MakeBlueprint(Agent, TEXT("BP_TestClientAuto"), [&](UBlueprint& B)
		{
			UActorComponent& Template = AddComponent(B, Client, TEXT("Client"));
			Template.PrimaryComponentTick.bStartWithTickEnabled = false;
			Template.bAutoActivate = true;
		}),
		{});
	ExpectOn(*this, *Never, NeverId, TEXT("client that ticks"),
		MakeBlueprint(Agent, TEXT("BP_TestClientTicks"), [&](UBlueprint& B) { AddComponent(B, Client, TEXT("Client")); }),
		{});
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
