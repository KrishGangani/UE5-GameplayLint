// GameplayLintCore - the small value types every part of the framework shares.

#pragma once

#include "UObject/SoftObjectPath.h"
#include "GameplayLintTypes.generated.h"

/** How serious a finding is. Errors make the asset invalid (and fail CI by default); warnings inform. */
UENUM()
enum class EGameplayLintSeverity : uint8
{
	Warning,
	Error
};

/**
 * How a rule finds the assets that a change can break, in an incremental (changelist) run (02 section 6.2).
 * Off and Coarse exist to measure Precise against (configurations I1 and I2, 01 section 7.2).
 */
UENUM()
enum class EGameplayLintDependents : uint8
{
	/** Validate only what the change list names. */
	Off,
	/** Every asset that references a changed asset at all. */
	Coarse,
	/** Only assets whose recorded references point at something the change removed (searchable names, derived classes). */
	Precise
};

/** One finding of one rule on one asset. Stored by path, never by pointer, so a report outlives unloaded assets. */
USTRUCT()
struct FGameplayLintFinding
{
	GENERATED_BODY()

	/** Stable rule ID, for example GL.Data.DanglingRow. CI gates and trends on it. */
	UPROPERTY()
	FName RuleId;

	/** The asset the engine validated (for a Blueprint: the Blueprint asset). */
	UPROPERTY()
	FSoftObjectPath Asset;

	UPROPERTY()
	EGameplayLintSeverity Severity = EGameplayLintSeverity::Error;

	/** Where inside the asset, as a readable property path: Payloads[3].Loot, Component[Ticker], ... */
	UPROPERTY()
	FString Where;

	UPROPERTY()
	FString Message;
};
