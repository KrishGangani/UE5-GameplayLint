// P10 GameplayLint - automation tests for the core framework: the walker and the rule contract (03, S4).
// Run: Tools\run_tests.bat (editor, headless), filter "GameplayLint".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/DataTable.h"
#include "GameplayLintWalker.h"
#include "GLFixtureData.h"
#include "GLTestRule.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"

namespace GameplayLintTests
{
	/** Editor context; one filter (ProductFilter), as the test macro requires. */
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** A tag by name, known to the dictionary or not (the same path the seeding commandlet uses). */
	FGameplayTag MakeTag(const TCHAR* Name)
	{
		FGameplayTag Tag;
		Tag.FromExportString(Name, PPF_SerializedAsImportText);
		return Tag;
	}

	/** Every non-None tag the walker finds in Asset, with its path. */
	TMap<FString, FString> TagsIn(UObject* Asset)
	{
		TMap<FString, FString> Found;
		GameplayLint::ForEachValue(Asset, FGameplayTag::StaticStruct(), [&Found](const GameplayLint::FValue& Value)
		{
			const FName Name = static_cast<const FGameplayTag*>(Value.Data)->GetTagName();
			if (!Name.IsNone())
			{
				Found.Add(Name.ToString(), Value.Path);
			}
		});
		return Found;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintWalkerEveryVariantTest, "GameplayLint.Walker.EveryVariant", GameplayLintTests::Flags)
bool FGameplayLintWalkerEveryVariantTest::RunTest(const FString& Parameters)
{
	using namespace GameplayLintTests;
	UGLFixtureDataAsset* Asset = NewObject<UGLFixtureDataAsset>(GetTransientPackage());
	Asset->Tag = MakeTag(TEXT("Test.Top"));
	Asset->Tags.AddTagFast(MakeTag(TEXT("Test.Container")));
	Asset->Nested.Tag = MakeTag(TEXT("Test.Nested"));
	Asset->List.AddDefaulted(2);
	Asset->List[1].Tag = MakeTag(TEXT("Test.Array"));
	Asset->Map.Add(TEXT("K")).Tag = MakeTag(TEXT("Test.Map"));
	Asset->Instanced = NewObject<UGLFixtureInstanced>(Asset);
	Asset->Instanced->Payload.Tag = MakeTag(TEXT("Test.Instanced"));
	FGLFixturePayload Extra;
	Extra.Tag = MakeTag(TEXT("Test.InstancedStruct"));
	Asset->Extra = FInstancedStruct::Make(Extra);

	const TMap<FString, FString> Found = TagsIn(Asset);
	const TCHAR* Expected[] = { TEXT("Test.Top"), TEXT("Test.Container"), TEXT("Test.Nested"), TEXT("Test.Array"),
		TEXT("Test.Map"), TEXT("Test.Instanced"), TEXT("Test.InstancedStruct") };
	TestEqual(TEXT("one value per seeded tag, no duplicates (ParentTags is transient and skipped)"), Found.Num(), int32(UE_ARRAY_COUNT(Expected)));
	for (const TCHAR* Name : Expected)
	{
		TestTrue(FString::Printf(TEXT("found %s"), Name), Found.Contains(Name));
	}
	if (const FString* Path = Found.Find(TEXT("Test.Array")))
	{
		TestTrue(FString::Printf(TEXT("array path names the element: %s"), **Path), Path->Contains(TEXT("List")) && Path->Contains(TEXT("[1]")));
	}
	if (const FString* Path = Found.Find(TEXT("Test.Instanced")))
	{
		TestTrue(FString::Printf(TEXT("sub-object path starts at its property: %s"), **Path), Path->StartsWith(TEXT("Instanced")));
	}
	AddInfo(FString::Printf(TEXT("Paths: %s"), *FString::JoinBy(Found, TEXT(", "), [](const TPair<FString, FString>& Pair) { return Pair.Key + TEXT("=") + Pair.Value; })));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintWalkerDataTableTest, "GameplayLint.Walker.DataTableRows", GameplayLintTests::Flags)
bool FGameplayLintWalkerDataTableTest::RunTest(const FString& Parameters)
{
	using namespace GameplayLintTests;
	UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
	Table->RowStruct = FGLFixtureRow::StaticStruct();
	FGLFixtureRow Row;
	Row.Tag = MakeTag(TEXT("Test.Row0"));
	Table->AddRow(TEXT("R00"), Row);
	Row.Tag = MakeTag(TEXT("Test.Row1"));
	Table->AddRow(TEXT("R01"), Row);

	const TMap<FString, FString> Found = TagsIn(Table);
	TestEqual(TEXT("one value per row"), Found.Num(), 2);
	const FString* Path = Found.Find(TEXT("Test.Row1"));
	TestTrue(TEXT("row path names the row"), Path && Path->StartsWith(TEXT("Row[R01].")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintWalkerMetadataTest, "GameplayLint.Walker.Metadata", GameplayLintTests::Flags)
bool FGameplayLintWalkerMetadataTest::RunTest(const FString& Parameters)
{
	UGLFixtureDataAsset* Asset = NewObject<UGLFixtureDataAsset>(GetTransientPackage());
	Asset->List.AddDefaulted(1);
	TMap<FString, FString> Monotonic;
	TArray<FString> Required;
	GameplayLint::ForEachValue(Asset, FRuntimeFloatCurve::StaticStruct(), [&](const GameplayLint::FValue& Value)
	{
		if (const FString* Direction = GameplayLint::FindMetaData(Value, TEXT("LintMonotonic")))
		{
			Monotonic.Add(Value.Path, *Direction);
		}
		if (GameplayLint::FindMetaData(Value, TEXT("LintRequired")))
		{
			Required.Add(Value.Path);
		}
	});
	TestEqual(TEXT("exactly one LintMonotonic curve"), Monotonic.Num(), 1);
	TestEqual(TEXT("it is CooldownByLevel, increasing"), Monotonic.FindRef(TEXT("CooldownByLevel")), FString(TEXT("Increasing")));
	TestEqual(TEXT("exactly one LintRequired curve"), Required.Num(), 1);
	TestTrue(TEXT("it is RequiredCurve"), Required.Num() == 1 && Required[0] == TEXT("RequiredCurve"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayLintRuleContractTest, "GameplayLint.Rule.Contract", GameplayLintTests::Flags)
bool FGameplayLintRuleContractTest::RunTest(const FString& Parameters)
{
	UGLTestRule* Rule = NewObject<UGLTestRule>();
	struct FCase
	{
		const TCHAR* Name;
		EDataValidationResult Expected;
		int32 Errors;
		int32 Warnings;
	};
	const FCase Cases[] = {
		{ TEXT("GoodObject"), EDataValidationResult::Valid, 0, 0 },
		{ TEXT("BadObject"), EDataValidationResult::Invalid, 1, 0 },
		{ TEXT("WarnObject"), EDataValidationResult::Valid, 0, 1 },
	};
	for (const FCase& Case : Cases)
	{
		UObject* Object = NewObject<UGLFixtureInstanced>(GetTransientPackage(), Case.Name);
		FDataValidationContext Context(false, EDataValidationUsecase::Script, {});
		const EDataValidationResult Result = Rule->ValidateLoadedAsset(FAssetData(Object), Object, Context);
		TestEqual(FString::Printf(TEXT("%s: result"), Case.Name), int32(Result), int32(Case.Expected));
		TestEqual(FString::Printf(TEXT("%s: errors"), Case.Name), int32(Context.GetNumErrors()), Case.Errors);
		TestEqual(FString::Printf(TEXT("%s: warnings"), Case.Name), int32(Context.GetNumWarnings()), Case.Warnings);
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			const FString Text = Issue.TokenizedMessage.IsValid() ? Issue.TokenizedMessage->ToText().ToString() : Issue.Message.ToString();
			TestTrue(FString::Printf(TEXT("%s: message carries the rule ID: %s"), Case.Name, *Text), Text.Contains(TEXT("[GL.Test.Rule]")));
		}
	}
	TestFalse(TEXT("the test rule never runs in real scans"), Rule->IsEnabled());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
