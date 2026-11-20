// GameplayLintCore - UGameplayLintCommandlet.

#include "GameplayLintCommandlet.h"

#include "DataValidationChangelist.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "GameplayLintReportSubsystem.h"
#include "GameplayLintSettings.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayLintCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogGameplayLint, Log, All);

namespace GameplayLintCommandlet
{
	const TCHAR* ResultName(EDataValidationResult Result)
	{
		switch (Result)
		{
		case EDataValidationResult::Valid:   return TEXT("Valid");
		case EDataValidationResult::Invalid: return TEXT("Invalid");
		default:                             return TEXT("NotValidated");
		}
	}

	/**
	 * Reads the pseudo-changelist. One change per line: "M <package>", "D <package>", "F <file>" (modified non-asset
	 * file), "X <file>" (deleted non-asset file). Returns the number of changes read, or -1 if the file is unreadable.
	 */
	int32 ReadChanges(const FString& File, UDataValidationChangelist& Changelist)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *File))
		{
			return -1;
		}
		int32 Count = 0;
		for (const FString& Raw : Lines)
		{
			const FString Line = Raw.TrimStartAndEnd();
			if (Line.Len() < 3 || Line[1] != TCHAR(' '))
			{
				continue;   // blank lines and comments
			}
			const FString Value = Line.Mid(2).TrimStartAndEnd();
			switch (Line[0])
			{
			case TCHAR('M'): Changelist.ModifiedPackageNames.Add(FName(*Value)); break;
			case TCHAR('D'): Changelist.DeletedPackageNames.Add(FName(*Value)); break;
			case TCHAR('F'): Changelist.ModifiedFiles.Add(Value); break;
			case TCHAR('X'): Changelist.DeletedFiles.Add(Value); break;
			default: continue;
			}
			++Count;
		}
		Changelist.Description = FText::FromString(FPaths::GetCleanFilename(File));
		return Count;
	}
}

UGameplayLintCommandlet::UGameplayLintCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The exit code is this commandlet's verdict (Main below), not "did anything anywhere log an error"
	// (LaunchEngineLoop.cpp:4217-4224): validation errors are logged on purpose, they are the result.
	UseCommandletResultAsExitCode = true;
}

int32 UGameplayLintCommandlet::Main(const FString& FullCommandLine)
{
	MainStart = FPlatformTime::Seconds();
	FullCommandLineCopy = FullCommandLine;

	// FParse handles quoted values, which file paths with spaces need. The stock switches are parsed by the base class.
	FString PathList;
	if (FParse::Value(*FullCommandLine, TEXT("Paths="), PathList, /*bShouldStopOnSeparator*/ false))   // a comma-separated list
	{
		PathList.ParseIntoArray(Paths, TEXT(","), true);
	}
	FParse::Value(*FullCommandLine, TEXT("Changes="), ChangesFile);
	if (!ChangesFile.IsEmpty() && Paths.Num() > 0)
	{
		// The engine's changelist path gathers its own assets (changed packages plus every validator's dependents).
		UE_LOG(LogGameplayLint, Warning, TEXT("-Paths applies to full scans only; it is ignored with -Changes."));
		Paths.Reset();
	}
	if (!FParse::Value(*FullCommandLine, TEXT("Report="), ReportFile))
	{
		ReportFile = FPaths::ProjectSavedDir() / TEXT("GameplayLint") / TEXT("Report.json");
	}
	FString FailOnText;
	if (FParse::Value(*FullCommandLine, TEXT("FailOn="), FailOnText))
	{
		FailOn = FailOnText.Equals(TEXT("Never"), ESearchCase::IgnoreCase) ? EFailOn::Never
			: (FailOnText.Equals(TEXT("Warning"), ESearchCase::IgnoreCase) ? EFailOn::Warning : EFailOn::Error);
	}

	const int32 Code = Super::Main(FullCommandLine);   // 0 = validation ran, 2 = it could not run
	if (Code == 0 && bFailThresholdReached)
	{
		UE_LOG(LogGameplayLint, Display, TEXT("Validation found problems at or above -FailOn: exit code 3."));
		return 3;
	}
	return Code;
}

void UGameplayLintCommandlet::FilterAssetsToValidate(TArray<FAssetData>& AssetDataList)
{
	Super::FilterAssetsToValidate(AssetDataList);   // the stock engine-content filter
	if (Paths.Num() == 0)
	{
		return;
	}
	AssetDataList.RemoveAll([this](const FAssetData& AssetData)
	{
		const FString PackageName = AssetData.PackageName.ToString();
		for (const FString& Path : Paths)
		{
			// "/Game/GLSeed" matches "/Game/GLSeed/..." but not "/Game/GLSeedOld/..."
			const FString Folder = Path.EndsWith(TEXT("/")) ? Path : Path + TEXT("/");
			if (PackageName.StartsWith(Folder))
			{
				return false;
			}
		}
		return true;
	});
}

void UGameplayLintCommandlet::SetupValidationSettings(FValidateAssetsSettings& Settings)
{
	Super::SetupValidationSettings(Settings);   // usecase Commandlet, excluded directories skipped
	Settings.bCollectPerAssetDetails = true;    // the report scores every asset, including the stock configuration
	Settings.bSilent = true;                    // no progress dialog in a headless run
}

void UGameplayLintCommandlet::ValidateAssets(const TArray<FAssetData>& AssetDataList, const FValidateAssetsSettings& InSettings, FValidateAssetsResults& OutResults)
{
	UGameplayLintReportSubsystem* Report = GEditor->GetEditorSubsystem<UGameplayLintReportSubsystem>();
	check(Report);
	Report->Begin();
	ValidateStart = FPlatformTime::Seconds();

	if (ChangesFile.IsEmpty())
	{
		Super::ValidateAssets(AssetDataList, InSettings, OutResults);
	}
	else
	{
		// The engine's changelist path: modified assets + every validator's dependents + referencers of deleted packages.
		UDataValidationChangelist* Changelist = NewObject<UDataValidationChangelist>();
		ChangeLines = GameplayLintCommandlet::ReadChanges(ChangesFile, *Changelist);
		if (ChangeLines < 0)
		{
			UE_LOG(LogGameplayLint, Error, TEXT("Cannot read the change list %s"), *ChangesFile);
			bChangesUnreadable = true;   // nothing was validated: the run must not look like a clean pass (exit code 2)
		}
		else
		{
			UEditorValidatorSubsystem* Validation = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
			check(Validation);
			Validation->ValidateChangelist(Changelist, InSettings, OutResults);
		}
	}

	ValidateEnd = FPlatformTime::Seconds();
	Report->End();
}

bool UGameplayLintCommandlet::ProcessValidationResults(FValidateAssetsResults& Results)
{
	if (bChangesUnreadable)
	{
		return false;   // validation could not run: exit code 2
	}
	switch (FailOn)
	{
	case EFailOn::Error:   bFailThresholdReached = Results.NumInvalid > 0; break;
	case EFailOn::Warning: bFailThresholdReached = Results.NumInvalid + Results.NumWarnings > 0; break;
	default:               bFailThresholdReached = false;
	}

	if (!WriteReport(Results))
	{
		UE_LOG(LogGameplayLint, Error, TEXT("Cannot write the report %s"), *ReportFile);
		return false;   // the run's result is unknown to CI: exit code 2
	}
	UE_LOG(LogGameplayLint, Display, TEXT("Report written to %s"), *ReportFile);
	return true;
}

bool UGameplayLintCommandlet::WriteReport(const FValidateAssetsResults& Results) const
{
	const UGameplayLintReportSubsystem* Report = GEditor->GetEditorSubsystem<UGameplayLintReportSubsystem>();
	const UGameplayLintSettings* Settings = GetDefault<UGameplayLintSettings>();

	FString Json;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("version"), 1);
	Writer->WriteValue(TEXT("commandline"), FullCommandLineCopy);
	Writer->WriteValue(TEXT("changes"), ChangesFile);
	Writer->WriteValue(TEXT("change_lines"), ChangeLines);
	Writer->WriteValue(TEXT("dependents_mode"), StaticEnum<EGameplayLintDependents>()->GetNameStringByValue(int64(Settings->GetDependents())));

	// Phase timings. Engine start-up is outside the validation phase, and it is reported separately.
	Writer->WriteObjectStart(TEXT("times_s"));
	Writer->WriteValue(TEXT("process_start_to_main"), MainStart - GStartTime);
	Writer->WriteValue(TEXT("main_to_validate"), ValidateStart - MainStart);
	Writer->WriteValue(TEXT("validate"), ValidateEnd - ValidateStart);
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("counts"));
	Writer->WriteValue(TEXT("requested"), Results.NumRequested);
	Writer->WriteValue(TEXT("checked"), Results.NumChecked);
	Writer->WriteValue(TEXT("valid"), Results.NumValid);
	Writer->WriteValue(TEXT("invalid"), Results.NumInvalid);
	Writer->WriteValue(TEXT("warnings"), Results.NumWarnings);
	Writer->WriteValue(TEXT("skipped"), Results.NumSkipped);
	Writer->WriteValue(TEXT("unable"), Results.NumUnableToValidate);
	Writer->WriteObjectEnd();

	// Per validator (engine and GameplayLint alike): FValidatorStatistics, which the stock commandlet only logs as counts.
	Writer->WriteArrayStart(TEXT("validators"));
	for (const TPair<FTopLevelAssetPath, FValidatorStatistics>& Pair : Results.ValidatorStatistics)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("class"), Pair.Key.ToString());
		Writer->WriteValue(TEXT("assets_validated"), Pair.Value.AssetsValidated);
		Writer->WriteValue(TEXT("assets_added"), Pair.Value.AssetsAddedForValidation);
		Writer->WriteValue(TEXT("seconds"), Pair.Value.TotalDurationSeconds);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	// Every validated asset, with every error and warning any check reported: how the stock configuration is scored.
	Writer->WriteArrayStart(TEXT("assets"));
	for (const TPair<FString, FValidateAssetsDetails>& Pair : Results.AssetsDetails)
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("asset"), Pair.Key);
		Writer->WriteValue(TEXT("result"), GameplayLintCommandlet::ResultName(Pair.Value.Result));
		Writer->WriteArrayStart(TEXT("errors"));
		for (const FText& Error : Pair.Value.ValidationErrors)
		{
			Writer->WriteValue(Error.ToString());
		}
		Writer->WriteArrayEnd();
		Writer->WriteArrayStart(TEXT("warnings"));
		for (const FText& Warning : Pair.Value.ValidationWarnings)
		{
			Writer->WriteValue(Warning.ToString());
		}
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	Writer->WriteArrayStart(TEXT("findings"));
	for (const FGameplayLintFinding& Finding : Report->GetFindings())
	{
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("rule"), Finding.RuleId.ToString());
		Writer->WriteValue(TEXT("asset"), Finding.Asset.ToString());
		Writer->WriteValue(TEXT("severity"), Finding.Severity == EGameplayLintSeverity::Error ? TEXT("Error") : TEXT("Warning"));
		Writer->WriteValue(TEXT("where"), Finding.Where);
		Writer->WriteValue(TEXT("message"), Finding.Message);
		Writer->WriteObjectEnd();
	}
	Writer->WriteArrayEnd();

	// Per rule: one time per asset checked. Medians and p99 are computed by Tools\score.py.
	Writer->WriteObjectStart(TEXT("check_times_ms"));
	for (const TPair<FName, TArray<double>>& Pair : Report->GetCheckTimes())
	{
		Writer->WriteArrayStart(Pair.Key.ToString());
		for (const double Milliseconds : Pair.Value)
		{
			Writer->WriteValue(Milliseconds);
		}
		Writer->WriteArrayEnd();
	}
	Writer->WriteObjectEnd();

	Writer->WriteObjectStart(TEXT("dependents"));
	for (const TPair<FName, UGameplayLintReportSubsystem::FDependentStats>& Pair : Report->GetDependents())
	{
		Writer->WriteObjectStart(Pair.Key.ToString());
		Writer->WriteValue(TEXT("calls"), Pair.Value.Calls);
		Writer->WriteValue(TEXT("assets_added"), Pair.Value.AssetsAdded);
		Writer->WriteValue(TEXT("ms"), Pair.Value.Milliseconds);
		Writer->WriteObjectEnd();
	}
	Writer->WriteObjectEnd();

	Writer->WriteObjectEnd();
	Writer->Close();
	// UTF-8 always: the default (AutoDetect) writes UTF-16 as soon as any message holds a non-ANSI character.
	return FFileHelper::SaveStringToFile(Json, *ReportFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
