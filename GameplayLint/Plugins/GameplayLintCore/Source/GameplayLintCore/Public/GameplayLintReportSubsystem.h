// GameplayLintCore - where rules report while a machine-readable report is being collected.

#pragma once

#include "EditorSubsystem.h"
#include "GameplayLintTypes.h"
#include "GameplayLintReportSubsystem.generated.h"

#define UE_API GAMEPLAYLINTCORE_API

/**
 * Collects findings and timings while a report is open.
 *
 * The engine, not GameplayLint, creates the rule objects and calls them, so rules need a well-known place to report
 * to; an editor subsystem lives exactly as long as the editor session and is found with GEditor->GetEditorSubsystem.
 * Outside an open report (normal editing, validate-on-save) rules produce Message Log entries only and nothing is
 * kept. The owner of a report (the commandlet) calls Begin before validation and End after it.
 */
UCLASS(MinimalAPI)
class UGameplayLintReportSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/** Dependent-gathering totals of one rule in one incremental run. */
	struct FDependentStats
	{
		int32 Calls = 0;
		int32 AssetsAdded = 0;
		double Milliseconds = 0.0;
	};

	/** Opens a report and clears the previous one. */
	UE_API void Begin();

	/** Closes the report. Its data stays readable until the next Begin. */
	UE_API void End();

	bool IsOpen() const { return bOpen; }

	UE_API void RecordFinding(const FGameplayLintFinding& Finding);
	UE_API void RecordCheckTime(FName RuleId, double Milliseconds);
	UE_API void RecordDependents(FName RuleId, int32 AssetsAdded, double Milliseconds);

	const TArray<FGameplayLintFinding>& GetFindings() const { return Findings; }
	const TMap<FName, TArray<double>>& GetCheckTimes() const { return CheckTimes; }
	const TMap<FName, FDependentStats>& GetDependents() const { return Dependents; }

private:
	bool bOpen = false;
	TArray<FGameplayLintFinding> Findings;

	/** Per rule: one duration per asset it checked, in milliseconds (medians and p99 are computed by Tools\score.py). */
	TMap<FName, TArray<double>> CheckTimes;

	TMap<FName, FDependentStats> Dependents;
};

#undef UE_API
