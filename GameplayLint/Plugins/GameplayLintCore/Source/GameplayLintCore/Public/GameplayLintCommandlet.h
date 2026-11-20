// GameplayLintCore - the CI entry point: the engine's Data Validation commandlet plus what CI needs.

#pragma once

#include "DataValidationCommandlet.h"
#include "GameplayLintCommandlet.generated.h"

#define UE_API GAMEPLAYLINTCORE_API

struct FValidateAssetsResults;
struct FValidateAssetsSettings;

/**
 * -run=GameplayLint: the stock -run=DataValidation (same gathering, same validation path, same engine validators),
 * plus four things it lacks (01b #7):
 *   -Paths=/Game/A,/Game/B   validate only assets under these package paths (full scans; ignored with -Changes,
 *                            where the engine's changelist path gathers the assets itself)
 *   -Changes=<file>          validate a pseudo-changelist read from a file (one change per line: "M <package>",
 *                            "D <package>", "F <file>" for a modified non-asset file, "X <file>" for a deleted one),
 *                            through the engine's changelist path, so every validator's dependents are added
 *   -Report=<file>           write a JSON report: counts, per-asset results, findings, per-validator and per-asset
 *                            rule times, dependent statistics, phase timings
 *   -FailOn=Error|Warning|Never   exit code 3 if any validated asset has a problem at or above this level
 *                            (default Error); 0 otherwise; 2 if validation could not run (the engine's meaning, and
 *                            an unreadable -Changes file)
 * Incremental runs need -BUILDMACHINE: the changelist object's own check then skips source control
 * (DataValidationChangelist.cpp:97-102), which a pseudo-changelist from a file has none of.
 */
UCLASS(MinimalAPI)
class UGameplayLintCommandlet : public UDataValidationCommandlet
{
	GENERATED_BODY()

public:
	UE_API UGameplayLintCommandlet(const FObjectInitializer& ObjectInitializer);

	UE_API virtual int32 Main(const FString& FullCommandLine) override;

protected:
	UE_API virtual void FilterAssetsToValidate(TArray<FAssetData>& AssetDataList) override;
	UE_API virtual void SetupValidationSettings(FValidateAssetsSettings& Settings) override;
	UE_API virtual void ValidateAssets(const TArray<FAssetData>& AssetDataList, const FValidateAssetsSettings& InSettings, FValidateAssetsResults& OutResults) override;
	UE_API virtual bool ProcessValidationResults(FValidateAssetsResults& Results) override;

private:
	enum class EFailOn : uint8 { Never, Warning, Error };

	bool WriteReport(const FValidateAssetsResults& Results) const;

	FString FullCommandLineCopy;
	TArray<FString> Paths;
	FString ChangesFile;
	FString ReportFile;
	EFailOn FailOn = EFailOn::Error;
	bool bFailThresholdReached = false;
	bool bChangesUnreadable = false;

	// Phase timings (FPlatformTime::Seconds): Main entered, validation started and ended.
	double MainStart = 0.0;
	double ValidateStart = 0.0;
	double ValidateEnd = 0.0;
	int32 ChangeLines = 0;
};

#undef UE_API
