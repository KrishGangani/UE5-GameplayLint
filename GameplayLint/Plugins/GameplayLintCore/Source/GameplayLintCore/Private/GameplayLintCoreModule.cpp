// GameplayLintCore - module registration.

#include "Modules/ModuleManager.h"

// No startup or shutdown logic: the engine discovers rules by reflection, so nothing is registered here.
IMPLEMENT_MODULE(FDefaultModuleImpl, GameplayLintCore);
