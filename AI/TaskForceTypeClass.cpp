#include "TaskForceTypeClass.h"

#include <cstdio>

// ============================================================================
// TaskForceTypeClass implementations
// ============================================================================

// asm: taskforceOutputFormatter (shared static scratch)
char TaskForceTypeClass::OutputFormatter[128];

// ----------------------------------------------------------------------------
// asm: TaskForceTypeClass_FormatOutput (0x6A54A0)
// ----------------------------------------------------------------------------
const char* TaskForceTypeClass::Format_Output() const
{
    // asm: add eax, 24h -- the display name is stored behind the ID base
    const char* pName = (IDBase != nullptr) ? (IDBase + 0x24) : "";

    // asm: push eax / push ecx / push offset aDS ("%d,%s") / push buffer
    //      call _sprintf
    std::snprintf(OutputFormatter, sizeof(OutputFormatter),
                  "%d,%s", MembersCount, pName);

    // asm: mov eax, offset taskforceOutputFormatter
    return OutputFormatter;
}
