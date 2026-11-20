// P10 GameplayLint - editor-only benchmark module. It will hold the commandlet that creates the seeded
// test corpus (assets with known defects plus clean controls) reproducibly from a seed.
// Nothing depends on this module, so it has no public headers and only private dependencies.

using UnrealBuildTool;

public class GameplayLintBench : ModuleRules
{
	public GameplayLintBench(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
