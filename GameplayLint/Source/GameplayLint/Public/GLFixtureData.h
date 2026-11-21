// P10 GameplayLint - fixture data types: the gameplay data the seeded test corpus is made of.
// They stand in for a real game's data. Every place a broken reference can hide in real data has a field here:
// top level, nested struct, array element, map value, instanced sub-object, DataTable row, Blueprint default.
// The lint rules never include this header: they find these values by reflection, like any other project's data.

#pragma once

#include "Curves/CurveFloat.h"
#include "Engine/CurveTable.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "StructUtils/InstancedStruct.h"
#include "GLFixtureData.generated.h"

/** A tag, a row handle and a curve: the three kinds of reference the data rules check, reused at every nesting level. */
USTRUCT(BlueprintType)
struct FGLFixturePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FDataTableRowHandle Row;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FRuntimeFloatCurve Curve;
};

/** One DataTable row. Rows hold references too, and the M4 control mutation edits only Value. */
USTRUCT(BlueprintType)
struct FGLFixtureRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Fixture")
	int32 Value = 0;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FDataTableRowHandle Link;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FRuntimeFloatCurve Curve;

#if WITH_EDITOR
	/**
	 * Class-level validation, the kind a row struct owner writes. The engine runs it before any validator, and when it
	 * fails no validator runs on that table (EditorValidatorSubsystem.cpp:657-663): the corpus's masking set uses it.
	 */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override
	{
		if (Value < 0)
		{
			Context.AddError(NSLOCTEXT("GLFixture", "NegativeValue", "Value must not be negative"));
			return EDataValidationResult::Invalid;
		}
		return EDataValidationResult::Valid;
	}
#endif
};

/** An instanced sub-object: data that lives in its own UObject inside the owning asset's package. */
UCLASS(MinimalAPI, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UGLFixtureInstanced : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGLFixturePayload Payload;
};

/**
 * A gameplay data asset with every structural variant of the test corpus. The two annotated properties carry a
 * meaning only the programmer knows: LintRequired (must be set) and LintMonotonic (a curve that must not reverse).
 */
UCLASS(MinimalAPI)
class UGLFixtureDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGameplayTagContainer Tags;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FDataTableRowHandle Row;

	/** A loot entry without a row is a bug, not "no loot". */
	UPROPERTY(EditAnywhere, Category = "Fixture", meta = (LintRequired))
	FDataTableRowHandle RequiredRow;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FCurveTableRowHandle CurveRow;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FRuntimeFloatCurve Curve;

	/** A cooldown must never get shorter as the level rises. */
	UPROPERTY(EditAnywhere, Category = "Fixture", meta = (LintMonotonic = "Increasing"))
	FRuntimeFloatCurve CooldownByLevel;

	/** A curve that must not rise: the Decreasing case of GL.Data.Curve (tests only; the corpus leaves it empty). */
	UPROPERTY(EditAnywhere, Category = "Fixture", meta = (LintMonotonic = "Decreasing"))
	FRuntimeFloatCurve FalloffByDistance;

	/** A damage fall-off curve with no keys would deal the default value at every distance. */
	UPROPERTY(EditAnywhere, Category = "Fixture", meta = (LintRequired))
	FRuntimeFloatCurve RequiredCurve;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FGLFixturePayload Nested;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	TArray<FGLFixturePayload> List;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	TMap<FName, FGLFixturePayload> Map;

	UPROPERTY(EditAnywhere, Instanced, Category = "Fixture")
	TObjectPtr<UGLFixtureInstanced> Instanced;

	/**
	 * An instanced-struct payload. 5.8's property iterator walks into it (UnrealType.cpp:57-65), so it is a regular
	 * variant of the corpus, not an expected miss (01b, finding added in Phase 5).
	 */
	UPROPERTY(EditAnywhere, Category = "Fixture", meta = (BaseStruct = "/Script/GameplayLint.GLFixturePayload"))
	FInstancedStruct Extra;

	/**
	 * A project's own reference type: a table and a row name in two plain fields instead of an FDataTableRowHandle.
	 * Nothing tells a generic rule that these two fields belong together, so a missing row here is the corpus's
	 * held-out kind: an expected miss, reported separately (01 section 7.1).
	 */
	UPROPERTY(EditAnywhere, Category = "Fixture")
	TSoftObjectPtr<UDataTable> LootTable;

	UPROPERTY(EditAnywhere, Category = "Fixture")
	FName LootRow;
};

