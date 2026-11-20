// GameplayLintDataRules - a rule pack (editor only). It depends on the core framework and nothing depends on it:
// the engine finds its rules by reflection, so the pack has no public headers and only private dependencies.

using UnrealBuildTool;

public class GameplayLintDataRules : ModuleRules
{
	public GameplayLintDataRules(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// DataValidation: every module that defines a rule links the validator base itself (the link rule, 02 section 3).
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DataValidation", "GameplayTags", "GameplayLintCore" });
	}
}
