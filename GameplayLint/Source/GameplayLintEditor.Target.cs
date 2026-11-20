// P10 GameplayLint - editor target (the editor, commandlets and the lint tooling run here).

using UnrealBuildTool;

public class GameplayLintEditorTarget : TargetRules
{
	public GameplayLintEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "GameplayLint", "GameplayLintBench" });
	}
}
