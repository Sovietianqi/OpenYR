#include "MPTeams.h"
#include "../Houses/HouseClass.h"
#include "../Network/MPObserverTeam.h"

#include <cstring>

// ============================================================================
// MPTeams implementations
// ============================================================================

// asm: NameNodeTypes -- {data, length} in .data
static HouseClass** g_NameNodeTypesData   = nullptr;
static int32        g_NameNodeTypesLength = 0;

HouseClass** NameNodeTypes_Data()   { return g_NameNodeTypesData; }
int32        NameNodeTypes_Length() { return g_NameNodeTypesLength; }

void NameNodeTypes_Set(HouseClass** pData, int32 length)
{
    g_NameNodeTypesData   = pData;
    g_NameNodeTypesLength = length;
}

// ============================================================================
// MPTeam_SetUnicodeString  (asm: MPTeam_SetUnicodeString, 0x7B6730)
// ============================================================================
MPTeam* MPTeam::Set_Unicode_String(const wchar_t* pSource)
{
    // asm: mov dword ptr [esi], 0
    Name = nullptr;

    // asm: mov eax, [edi] / test eax,eax / jz done
    if (pSource != nullptr) {
        // asm: push eax / call _wcslen / lea eax,[eax+eax+2] / push eax
        //      call new
        const size_t chars = wcslen(pSource) + 1;
        const size_t bytes = chars * 2;
        wchar_t* pBuf = new wchar_t[bytes];

        // asm: mov [esi], eax
        Name = pBuf;

        // asm: push [edi] / push eax / call _wcscpy
        wmemcpy(pBuf, pSource, chars);
    }

    // asm: done: mov eax, esi / retn 4
    return this;
}

// ============================================================================
// MPTeam_SetString  (asm: MPTeam_SetString, 0x7B46B0)
// ============================================================================
MPTeam* MPTeam::Set_String(const char* pSource)
{
    // asm: mov dword ptr [ebx], 0
    Name = nullptr;

    // asm: mov edi, [esi] / test edi,edi / jz done
    if (pSource != nullptr) {
        // asm: or ecx,0FFFFFFFFh / xor eax,eax / repne scasb / not ecx
        //      push ecx / call new
        // The original allocates strlen+1 bytes (the `not ecx` on a
        // repne-scasb that consumed the terminator yields len+1).
        const size_t len = strlen(pSource);
        char* pBuf = new char[len + 1];

        // asm: mov [ebx], edx
        Name = reinterpret_cast<wchar_t*>(pBuf);

        // asm: rep movsd / rep movsb -- byte copy of len+1
        memcpy(pBuf, pSource, len + 1);
    }

    // asm: done: mov eax, ebx / retn 4
    return this;
}

// ============================================================================
// MPObserverTeam_Func1  (asm: MPObserverTeam_func1, 0x5D8C90)
//
// Pure vtable forwarding to MPObserverTeam_IsTeamIncluded:
//   mov eax,[esp+idx] / push eax / call MPObserverTeam_IsTeamIncluded / retn 4
//
// MPObserverTeam_IsTeamIncluded 本身只做一次**全局表**边界判定（不读 this）：
//   test eax,eax / jl false
//   cmp eax, NameNodeTypes.length / jge false
//   mov al,1
// 即"这个下标是不是一个合法的玩家位"。原先的空壳返回 false 会让观察者队伍
// 永远报告空，这里按 asm 落地。
// ============================================================================
bool MPObserverTeam_Func1(void* pTeam, int32 idx)
{
    (void)pTeam;                                    // asm: this 从不被读取
    if (idx < 0)                                    // asm: test eax,eax ; jl
        return false;
    return idx < g_NameNodeTypesLength;             // asm: cmp NameNodeTypes.length
}

// ============================================================================
// MPSiegeDefenderTeam_IsIncluded  (asm: MPSiegeDefenderTeam_IsIncluded)
// ============================================================================
bool MPSiegeDefenderTeam_IsIncluded(int32 idx)
{
    // asm: mov edx, NameNodeTypes.length / test edx,edx / jle return_true
    //      mov eax, 1
    if (g_NameNodeTypesLength <= 0)
        return true;                       // asm: eax = 1

    // asm: mov esi, NameNodeTypes.data / mov edi, [esp+arg]
    // loop_5CAE8E:
    //   cmp edi, ecx / jz next            ; slot matches this index -> skip
    //   mov ebx, [esi] / cmp [ebx+6Bh], eax(1) / jz return_false
    //   ; [house+6Bh] == 1 == AI house -> the team is not included
    for (int32 i = 0; i < g_NameNodeTypesLength; ++i) {
        if (i == idx)
            continue;                  // asm: cmp edi,ecx / jz next

        const HouseClass* pHouse = g_NameNodeTypesData[i];   // asm: mov ebx,[esi]
        if (pHouse != nullptr && !pHouse->IsHumanPlayer)     // asm: cmp [ebx+6Bh],1 / jz
            return false;
    }

    // asm: return_true: xor al,al is the FALSE path; the fall-through with
    // eax=1 (true) happens when no AI house rejects inclusion.
    return true;
}
