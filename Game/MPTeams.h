#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include <wchar.h>

class HouseClass;

// ============================================================================
// MPTeams - multiplayer team bookkeeping helpers
//
//   MPTeam             -- a named team slot (unicode + ansi string pair)
//   MPObserverTeam     -- the observer bucket (existing Network/ class)
//   MPSiegeDefenderTeam-- siege-mode defender bucket
// ============================================================================

// ----------------------------------------------------------------------------
// MPTeam - the original stores a single wchar_t* at +0x00; both setters
// duplicate the incoming string into a fresh allocation and return the
// object itself (fluent chaining).
// ----------------------------------------------------------------------------
class MPTeam {
public:
    MPTeam() noexcept : Name(nullptr) {}

    // ------------------------------------------------------------------------
    // asm: MPTeam_SetUnicodeString (0x7B6730)
    //
    //   edi = [esp+a2] / esi = ecx / dword [esi] = 0
    //   eax = [edi] / test eax,eax / jz done
    //   push eax / call _wcslen
    //   lea eax, [eax+eax+2]     ; (chars+1) * 2 bytes
    //   push eax / call new
    //   mov [esi], eax / push [edi] / push eax / call _wcscpy
    // done:
    //   mov eax, esi / retn 4
    // ------------------------------------------------------------------------
    MPTeam* Set_Unicode_String(const wchar_t* pSource);

    // ------------------------------------------------------------------------
    // asm: MPTeam_SetString (0x7B46B0) - ansi variant, same shape:
    //   edi = [esi] / repne scasb / not ecx / push ecx / call new
    //   then rep movsd / rep movsb copy.
    // ------------------------------------------------------------------------
    MPTeam* Set_String(const char* pSource);

    wchar_t* Name;   // asm +0x00
};

// ----------------------------------------------------------------------------
// asm: MPObserverTeam_func1 (0x7EE090 area) - vtable slot forwarding to
// MPObserverTeam_IsTeamIncluded with the raw member index.
// ----------------------------------------------------------------------------
bool MPObserverTeam_Func1(void* pTeam, int32 idx);

// ----------------------------------------------------------------------------
// MPSiegeDefenderTeam - siege-mode defender bucket.
// asm MPSiegeDefenderTeam_IsIncluded (0x5CAE70):
//   edx = NameNodeTypes.length / ecx = 0 / eax = 1
//   test edx,edx / jle return_true
//   esi = NameNodeTypes.data / edi = [esp+arg]
// loop_5CAE8E:
//   cmp edi, ecx / jz next          ; entry matches index -> included
//   ebx = [esi] / cmp [ebx+6Bh], eax (1) / jz return_false
//                                   ; entry is an AI house -> excluded
// next:
//   inc ecx / add esi,4 / cmp ecx,edx / jl loop
//   return true
// ----------------------------------------------------------------------------
bool MPSiegeDefenderTeam_IsIncluded(int32 idx);

// ----------------------------------------------------------------------------
// asm global: NameNodeTypes -- {HouseClass** data; int32 length} table used
// by the lobby name list.  Exposed so the siege logic and color lookup share
// the same instance.
// ----------------------------------------------------------------------------
HouseClass** NameNodeTypes_Data();
int32        NameNodeTypes_Length();
void         NameNodeTypes_Set(HouseClass** pData, int32 length);
