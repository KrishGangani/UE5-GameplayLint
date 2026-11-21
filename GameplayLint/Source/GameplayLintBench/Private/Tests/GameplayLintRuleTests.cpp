// P10 GameplayLint - automation tests for the rule packs, on in-memory objects (03, S5 onwards).
// Run: Tools\run_tests.bat (editor, headless), filter "GameplayLint".
//
// The packs have no public headers (nothing may depend on a pack, 02 section 3), so the tests find each rule class
// by its path, as the engine's validator discovery does, and drive it through the engine's own entry point.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Curves/CurveFloat.h"
#include "EditorValidatorBase.h"
#include "Engine/CurveTable.h"
#include "Engine/DataTable.h"
#include "GameplayTagsManager.h"
#include "GLFixtureData.h"
#include "GLTestHelpers.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"

#include <limits>

namespace GameplayLintRuleTests
{
	using GameplayLintTestHelpers::Flags;
	using GameplayLintTestHelpers::MakeTag;

	/** A new instance of the rule class at ClassPath, or null (and a test error) if the pack isn't loaded. */
	UEditorValidatorBase* NewRule(FAutomationTestBase& Test, const TCHAR* ClassPath)
	{
		UClass* Class = FindObject<UClass>(nullptr, ClassPath);
		Test.TestNotNull(FString::Printf(TEXT("rule class %s is loaded"), ClassPath), Class);
		return Class ? NewObject<UEditorValidatorBase>(GetTransientPackage(), Class) : nullptr;
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
	UEditorValidatorBase* Rule = NewRule(*this, TEXT("/Script/GameplayLintDataRules.GLRule_UnknownTag"));
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
	UEditorValidatorBase* Rule = NewRule(*this, TEXT("/Script/GameplayLintDataRules.GLRule_DanglingRow"));
	if (!Rule)
	{
		return false;
	}
	const TCHAR* Id = TEXT("[GL.Data.DanglingRow]");

	// Unnamed: a second run in the same editor session gets fresh objects.
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
	Table->RowStruct = FGLFixtureRow::StaticStruct();
	Table->AddRow(TEXT("R00"), FGLFixtureRow());
	UCurveTable* Curves = NewObject<UCurveTable>(GetTransientPackage());
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
	UEditorValidatorBase* Rule = NewRule(*this, TEXT("/Script/GameplayLintDataRules.GLRule_Curve"));
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
	UCurveFloat* Empty = NewObject<UCurveFloat>(GetTransientPackage());
	UCurveFloat* Broken = NewObject<UCurveFloat>(GetTransientPackage());
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

#endif // WITH_DEV_AUTOMATION_TESTS
