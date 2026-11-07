#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class HouseClass;

// ============================================================================
// Command classes recovered from the vtable procs
// CheerCommandClass / DeployCommandClass / StopCommandClass all expose an
// `Executer` slot that immediately forwards to the matching `Execute`
// implementation (asm: retn 4 thunks at 0x4AB940 / 0x4ABA50 / 0x5688F0).
// ============================================================================
class CheerCommandClass {
public:
    // asm: CheerCommandClass_Executer -> CheerCommandClass_Execute
    static void Executer(uint32 flags) { Execute(flags); }

    // Causes every owned infantry unit to play its cheer animation.
    static void Execute(uint32 flags);
};

class DeployCommandClass {
public:
    // asm: DeployCommandClass_Executer -> DeployCommandClass_Execute
    static void Executer(uint32 flags) { Execute(flags); }

    // Issues the deploy/undeploy order to the currently selected units.
    static void Execute(uint32 flags);
};

class StopCommandClass {
public:
    // asm: StopCommandClass_Executer -> StopCommandClass_Execute
    static void Executer(uint32 flags) { Execute(flags); }

    // Halts the currently selected units' current activity.
    static void Execute(uint32 flags);
};
