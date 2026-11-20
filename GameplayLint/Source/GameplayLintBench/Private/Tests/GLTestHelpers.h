// P10 GameplayLint - helpers shared by the automation tests of the bench module (05 E3, F4).

#pragma once

#include "GameplayTagContainer.h"
#include "Misc/AutomationTest.h"

namespace GameplayLintTestHelpers
{
	/** Editor context; one filter (ProductFilter), as the test macro requires. */
	inline constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	/** A tag by name, known to the dictionary or not (the same path the seeding commandlet uses). */
	inline FGameplayTag MakeTag(const TCHAR* Name)
	{
		FGameplayTag Tag;
		Tag.FromExportString(Name, PPF_SerializedAsImportText);
		return Tag;
	}
}
