// P10 GameplayLint - game target (standalone/packaged builds).
// The lint tooling is editor-only, so this target contains only the runtime game module.

using UnrealBuildTool;

public class GameplayLintTarget : TargetRules
{
	public GameplayLintTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("GameplayLint");
	}
}
