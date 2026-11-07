#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// TaskForceTypeClass - a named task force grouping (units + counts) used by
// AI team types.  Original layout:
//   +0x00  int32  MembersCount  (first dword, formatted as "%d")
//   +0x04  char*  IDBase        (name lives at IDBase + 0x24)
// ============================================================================

class TaskForceTypeClass {
public:
    TaskForceTypeClass() noexcept : MembersCount(0), IDBase(nullptr) {}

    // ------------------------------------------------------------------------
    // asm: TaskForceTypeClass_FormatOutput (0x6A54A0)
    //
    //   mov eax, [ecx+4]        ; IDBase
    //   mov ecx, [ecx]          ; MembersCount
    //   add eax, 24h            ; name = IDBase + 0x24
    //   push eax / push ecx
    //   push offset aDS         ; "%d,%s"
    //   push offset taskforceOutputFormatter
    //   call _sprintf
    //   mov eax, offset taskforceOutputFormatter / retn
    //
    // Returns a pointer to the shared static scratch buffer.
    // ------------------------------------------------------------------------
    const char* Format_Output() const;

    int32  MembersCount;   // asm +0x00
    char*  IDBase;         // asm +0x04 (the name string lives at +0x24)

    // asm: taskforceOutputFormatter -- shared static scratch buffer
    static char OutputFormatter[128];
};
