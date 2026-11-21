// GameplayLintUpdateBudget - a rule pack (editor only). It depends on the core framework and nothing depends on it:
// the engine finds its rules by reflection, so the pack has no public headers and only private dependencies.

using UnrealBuildTool;

public class GameplayLintUpdateBudget : ModuleRules
{
	public GameplayLintUpdateBudget(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// DataValidation: every module that defines a rule links the validator base itself (the link rule, 02 section 3).
		// DeveloperSettings: the pack's settings class.
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DataValidation", "DeveloperSettings", "GameplayLintCore" });
	}
}
