// P10 GameplayLint - primary game module (runtime). It will hold the gameplay data types that the
// seeded test corpus is made of (data assets, DataTable rows, components). It never depends on the
// lint plugins: the rules inspect game data, the game does not know the rules exist.

using UnrealBuildTool;

public class GameplayLint : ModuleRules
{
	public GameplayLint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
