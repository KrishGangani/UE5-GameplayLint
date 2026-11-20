// GameplayLintCore - the reusable framework (editor only). It must not depend on any rule pack or game module.

using UnrealBuildTool;

public class GameplayLintCore : ModuleRules
{
	public GameplayLintCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// DataValidation is public: the rule base class (a public header) derives from its UEditorValidatorBase.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DataValidation" });
	}
}
