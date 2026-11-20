// GameplayLintDataRules - module registration.

#include "Modules/ModuleManager.h"

// No startup or shutdown logic: the engine discovers this pack's rules by reflection.
IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintDataRules);
