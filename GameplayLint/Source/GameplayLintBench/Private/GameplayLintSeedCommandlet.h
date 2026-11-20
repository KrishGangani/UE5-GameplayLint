// P10 GameplayLint - the seeded test corpus: assets with known defects, clean controls and their ground truth.

#pragma once

#include "Commandlets/Commandlet.h"
#include "GameplayLintSeedCommandlet.generated.h"

/**
 * Builds the test corpus reproducibly from a seed, or applies the incremental-test mutations to it.
 *
 *   -run=GameplayLintSeed -Seed=<n> -Assets=<N> -Manifest=<file>      create /Game/GLSeed and its ground truth
 *   -run=GameplayLintSeed -Mutate -Changes=<file>                       apply M1-M4 and list what changed
 *
 * The corpus folder must not exist yet (Tools\seed.py deletes it first). The rules never see this module or the
 * manifest: the manifest is what the scoring script compares their findings with (01 section 7.1).
 */
UCLASS()
class UGameplayLintSeedCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UGameplayLintSeedCommandlet();

	virtual int32 Main(const FString& Params) override;
};
