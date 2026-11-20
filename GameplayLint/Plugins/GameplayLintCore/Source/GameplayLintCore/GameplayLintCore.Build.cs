// GameplayLintCore - the reusable framework (editor only). It must not depend on any rule pack or game module.

using UnrealBuildTool;

public class GameplayLintCore : ModuleRules
{
	public GameplayLintCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Public: our public headers derive from these modules' classes, so every rule pack needs their include paths.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine",
			"DataValidation",      // UEditorValidatorBase, UDataValidationCommandlet
			"DeveloperSettings",   // UDeveloperSettings (UGameplayLintSettings)
			"EditorSubsystem"      // UEditorSubsystem (UGameplayLintReportSubsystem)
		});

		// Private: used only inside our .cpp files.
		PrivateDependencyModuleNames.AddRange(new string[] {
			"Json",            // the commandlet's report
			"SourceControl",   // DataValidationChangelist.h includes its changelist headers
			"UnrealEd"         // GEditor
		});
	}
}
