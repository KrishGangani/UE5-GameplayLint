// P10 GameplayLint - editor-only benchmark module: the commandlet that creates the seeded test corpus
// (assets with known defects plus clean controls) reproducibly from a seed, and applies its mutations.
// Nothing depends on this module, so it has no public headers and only private dependencies.

using UnrealBuildTool;

public class GameplayLintBench : ModuleRules
{
	public GameplayLintBench(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine",
			"AssetRegistry",   // AssetCreated, and the "corpus folder must be empty" check
			"DataValidation",  // the tests' rule derives from UEditorValidatorBase: its vtable and ValidateLoadedAsset link here
			"GameplayTags",    // FGameplayTag values in the fixture data
			"Json",            // the ground-truth manifest
			"UnrealEd",        // FKismetEditorUtilities: creating and compiling Blueprints
			"GameplayLint",    // the fixture data types the corpus is made of
			"GameplayLintCore" // the framework under test (automation tests, Private/Tests)
		});
	}
}
