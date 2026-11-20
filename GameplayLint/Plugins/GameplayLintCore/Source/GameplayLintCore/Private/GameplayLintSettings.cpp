// GameplayLintCore - UGameplayLintSettings.

#include "GameplayLintSettings.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayLintSettings)

namespace GameplayLintSettings
{
	/** True if Id equals Prefix or starts with "Prefix.", so "GL.UB" matches "GL.UB.ClientTickGroup" but not "GL.UBX". */
	bool MatchesPrefix(const FString& Id, const FString& Prefix)
	{
		return Id.Equals(Prefix, ESearchCase::IgnoreCase)
			|| (Id.StartsWith(Prefix, ESearchCase::IgnoreCase) && Id.Len() > Prefix.Len() && Id[Prefix.Len()] == TCHAR('.'));
	}

	bool MatchesAny(const FString& Id, const TArray<FString>& Prefixes)
	{
		for (const FString& Prefix : Prefixes)
		{
			if (MatchesPrefix(Id, Prefix))
			{
				return true;
			}
		}
		return false;
	}
}

void UGameplayLintSettings::PostInitProperties()
{
	Super::PostInitProperties();   // loads the saved config first

	// Only the class default object is ever asked (GetDefault<>), so only it reads the command line.
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	// bShouldStopOnSeparator = false: the value is a comma-separated list, and FParse would stop at the first comma (Parse.cpp:298).
	FString Rules;
	if (FParse::Value(FCommandLine::Get(), TEXT("GameplayLintRules="), Rules, /*bShouldStopOnSeparator*/ false))
	{
		TArray<FString> Parts;
		Rules.ParseIntoArray(Parts, TEXT(","), true);
		CommandLineRules = MoveTemp(Parts);
	}

	FString Mode;
	if (FParse::Value(FCommandLine::Get(), TEXT("GameplayLintDependents="), Mode))
	{
		// The enum's reflection data knows its names; the lookup is case-insensitive by default (Class.h:2857-2869).
		const int64 Value = StaticEnum<EGameplayLintDependents>()->GetValueByNameString(Mode);
		if (Value != INDEX_NONE)
		{
			CommandLineDependents = EGameplayLintDependents(Value);
		}
		else
		{
			// A typo must not silently run as another configuration: keep the saved setting and say so.
			UE_LOG(LogTemp, Warning, TEXT("GameplayLint: unknown -GameplayLintDependents=%s (Off, Coarse or Precise); using the saved setting."), *Mode);
		}
	}
}

bool UGameplayLintSettings::IsRuleEnabled(FName RuleId) const
{
	const FString Id = RuleId.ToString();
	if (CommandLineRules.IsSet())
	{
		const TArray<FString>& Prefixes = CommandLineRules.GetValue();
		if (Prefixes.Num() == 1 && Prefixes[0].Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			return false;
		}
		if (Prefixes.Num() == 1 && Prefixes[0].Equals(TEXT("All"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		return GameplayLintSettings::MatchesAny(Id, Prefixes);
	}
	return bEnabled && !GameplayLintSettings::MatchesAny(Id, DisabledRules);
}

EGameplayLintDependents UGameplayLintSettings::GetDependents() const
{
	return CommandLineDependents.Get(Dependents);
}
