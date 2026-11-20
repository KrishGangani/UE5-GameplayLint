// GameplayLintCore - UGameplayLintReportSubsystem.

#include "GameplayLintReportSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayLintReportSubsystem)

void UGameplayLintReportSubsystem::Begin()
{
	Findings.Reset();
	CheckTimes.Reset();
	Dependents.Reset();
	bOpen = true;
}

void UGameplayLintReportSubsystem::End()
{
	bOpen = false;
}

void UGameplayLintReportSubsystem::RecordFinding(const FGameplayLintFinding& Finding)
{
	if (bOpen)
	{
		Findings.Add(Finding);
	}
}

void UGameplayLintReportSubsystem::RecordCheckTime(FName RuleId, double Milliseconds)
{
	if (bOpen)
	{
		CheckTimes.FindOrAdd(RuleId).Add(Milliseconds);
	}
}

void UGameplayLintReportSubsystem::RecordDependents(FName RuleId, int32 AssetsAdded, double Milliseconds)
{
	if (bOpen)
	{
		FDependentStats& Stats = Dependents.FindOrAdd(RuleId);
		++Stats.Calls;
		Stats.AssetsAdded += AssetsAdded;
		Stats.Milliseconds += Milliseconds;
	}
}
