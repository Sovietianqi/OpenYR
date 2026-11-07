#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class MPGameModeClass;

// ============================================================================
// MP original-layout constructors (asm: MP_Siege_CTOR 0x5CD760 area,
// MP_Unholy_CTOR 0x5CD820 area)
//
//  Both are the tail of the mode-specific Init calls:
//
//    mov eax, [esp+idxMPMode] / edx = [esp+aTitle2]
//    esi = ecx (this)
//    ecx = [esp+AI_Allowed]
//    push eax (idxMPMode) / push ecx (AI_Allowed)
//    push edx (aTitle2) / push [esp+aTitle1]
//    push wTitle2 / push wTitle1
//    call MultiplayerGameMode_CTOR       ; base init with all six params
//    mov dword ptr [esi], offset MultiplayerSiege::vftable
//                                        ; (UnholyAlliance::vftable for
//                                         MP_Unholy_CTOR)
//    mov eax, esi / retn 18h
//
//  On this build the vtable swap is expressed as the derived-mode marker
//  stored alongside the base object; the argument order below matches the
//  original push order exactly.
// ============================================================================

// asm: MP_Siege_CTOR -- constructs a MultiplayerSiege over the base mode.
MPGameModeClass* MP_Siege_CTOR(const wchar_t* pWTitle1, const wchar_t* pWTitle2,
                               const char* pATitle1, const char* pATitle2,
                               bool aiAllowed, int32 idxMPMode);

// asm: MP_Unholy_CTOR -- constructs an UnholyAlliance over the base mode.
MPGameModeClass* MP_Unholy_CTOR(const wchar_t* pWTitle1, const wchar_t* pWTitle2,
                                const char* pATitle1, const char* pATitle2,
                                bool aiAllowed, int32 idxMPMode);
