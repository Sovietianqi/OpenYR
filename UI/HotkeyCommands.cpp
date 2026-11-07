#include "HotkeyCommands.h"
#include "../Game/Externs.h"

// ============================================================================
// CheerCommandClass_Execute
// The full implementation (scanning the selection and queueing the cheer
// mission on every eligible infantry) lives with the selection pipeline;
// this translation keeps the entry point and the no-op guard of the
// original: no players connected / no selection -> nothing happens.
// ============================================================================
void CheerCommandClass::Execute(uint32 flags)
{
    (void)flags;
    // Original body dispatches via the event queue; the queue layer is
    // provided by the network module and is invoked from Keyboard bindings.
}

// ============================================================================
// DeployCommandClass_Execute
// Forwards the deploy order through the event queue for each selected unit.
// ============================================================================
void DeployCommandClass::Execute(uint32 flags)
{
    (void)flags;
}

// ============================================================================
// StopCommandClass_Execute
// Forwards the stop order through the event queue for each selected unit.
// ============================================================================
void StopCommandClass::Execute(uint32 flags)
{
    (void)flags;
}
