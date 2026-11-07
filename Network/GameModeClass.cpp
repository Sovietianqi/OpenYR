#include "GameModeClass.h"
#include "../Game/EngineGlobals.h"

// ============================================================================
// asm: GameMode_WorkWithColourIdx (0x69B5F0)
//
//   char __thiscall GameMode_WorkWithColourIdx(GameModeClass *this, int a2)
//
//   asm 逐段：
//     push ebp / ebp = [esp+4+a2] / push esi
//     cmp ebp,0FFFFFFFEh / push edi / jz loc_69B657        ; idx == -2 -> false
//     edi = [ecx+284Ch]  (StartSpots.count)
//     xor edx,edx / test edi,edi / jle loc_69B642          ; count<=0 -> 固定槽
//     esi = [ecx+2840h]  (StartSpots.data)
//   loc_69B61E:                                            ; 遍历开始
//     eax = [esi]                                          ; StartSpotClass*
//     cmp dword ptr [eax+57h],0FFFFFFFEh / jnz loc_69B633
//     cmp dword ptr [eax+53h],0FFFFFFFFh / jnz loc_69B633
//     eax = 0FFFFFFFEh / jmp loc_69B636
//   loc_69B633:
//     eax = [eax+53h]
//   loc_69B636:
//     cmp eax,ebp / jz loc_69B65F                          ; 命中 -> true
//     inc edx / add esi,4 / cmp edx,edi / jl loc_69B61E
//   loc_69B642:                                            ; 固定槽
//     xor eax,eax / add ecx,84h
//     ... cmp [ecx],ebp / jz true / inc eax / add ecx,4 / cmp eax,8 / jl loop
//   loc_69B657: xor al,al                                  ; false
//   loc_69B65F: mov al,1                                   ; true
// ============================================================================
bool GameModeClass::Work_With_Colour_Idx(int32 idx)
{
    // asm: cmp ebp,0FFFFFFFEh / jz loc_69B657
    if (idx == -2)
        return false;                                   // asm: xor al,al

    // asm: edi = [ecx+284Ch] / xor edx,edx / test edi,edi / jle loc_69B642
    const int32 count = StartSpots.count;
    if (count > 0)
    {
        // asm: esi = [ecx+2840h]
        StartSpotClass** pData = StartSpots.data;

        for (int32 i = 0; i < count; ++i)               // asm: loc_69B61E
        {
            // asm: mov eax,[esi]
            const StartSpotClass* pSpot = pData[i];
            if (pSpot == nullptr)
                continue;

            // asm: cmp [eax+57h],-2 / jnz use53
            //      cmp [eax+53h],-1 / jnz use53
            //      eax = -2
            int32 colour;
            if (pSpot->ColourSentinel == -2 &&
                pSpot->ColourSlot == -1)
            {
                colour = -2;                            // asm: mov eax,0FFFFFFFEh
            }
            else                                        // asm: loc_69B633
            {
                colour = pSpot->ColourSlot;             // asm: mov eax,[eax+53h]
            }

            // asm: loc_69B636: cmp eax,ebp / jz loc_69B65F
            if (colour == idx)
                return true;
        }
    }

    // asm: loc_69B642: xor eax,eax / add ecx,84h
    //      cmp [ecx],ebp / jz true / inc eax / add ecx,4 / cmp eax,8 / jl loop
    const int32* pFixed = &field_84;
    for (int32 i = 0; i < 8; ++i)
    {
        if (pFixed[i] == idx)
            return true;
    }

    return false;                                       // asm: loc_69B657
}

// ============================================================================
// asm: GameModeClass_LoadCallStrings (0x6977B0)
//
//   asm 逐段：
//     push ebx / push esi / push edi
//     edi = pVec / esi = counter
//   loc_6977CE:
//     ecx = [off_83ED04 + esi*4]              ; CSF 名称
//     cmp ecx,MenuColorLinks / jae done        ; 表尾
//     push 251h / push "Session.CPP"           ; 调试元数据
//     call Get_String
//     test eax,eax / jz next                   ; 解析失败跳过
//     push eax / call __wcsdup
//     test eax,eax / jz next
//     ; 内联 vector 追加（loc_697801 扩容 / loc_697818 追加）
//     inc esi / jmp loc_6977CE
//   done:
//     pop edi / pop esi / pop ebx / retn
//
//   原版的 off_83ED04 指针表在 dump 中不含完整边界，这里按原版引用的
//   TXT_CWAIT_* lobby 名单复原表内容；其它语义（Get_String 返回空则跳过、
//   wcsdup 后入队）保持与原版一致。
// ============================================================================
static const char* const s_CallStringNames[] =
{
    "TXT_CWAIT_STAR70", "TXT_CWAIT_STAR71", "TXT_CWAIT_STAR72",
    "TXT_CWAIT_STAR73", "TXT_CWAIT_STAR74", "TXT_CWAIT_STAR75",
    "TXT_CWAIT_STAR76", "TXT_CWAIT_STAR77",
};

bool GameModeClass::Load_Call_Strings(DynamicVectorClass<wchar_t*>* pVec)
{
    if (pVec == nullptr)
        return false;

    const int32 count = static_cast<int32>(
        sizeof(s_CallStringNames) / sizeof(s_CallStringNames[0]));

    int32 pushed = 0;

    for (int32 i = 0; i < count; ++i)               // asm: loc_6977CE
    {
        // asm: ecx = [off_83ED04 + esi*4]
        const char* pName = s_CallStringNames[i];

        // asm: call Get_String / test eax,eax / jz next
        const char* pNarrow = EngineGlobals::Get_String(pName);
        if (pNarrow == nullptr)
            continue;                               // asm: 原版同样不插入

        // asm: push eax / call __wcsdup
        // 引擎侧 Get_String 在本 build 返回窄串，这里按宽字符契约加宽。
        wchar_t wide[256];
        size_t k = 0;
        for (; k < 255 && pNarrow[k] != '\0'; ++k)
            wide[k] = static_cast<wchar_t>(static_cast<unsigned char>(pNarrow[k]));
        wide[k] = L'\0';

        // asm: 内联扩容 + 追加（等价于 vector 的 Add）
        wchar_t* pDup = static_cast<wchar_t*>(
            YRMemory::Allocate((k + 1) * sizeof(wchar_t)));
        if (pDup == nullptr)
            continue;                               // asm: 分配失败跳过
        for (size_t j = 0; j <= k; ++j)
            pDup[j] = wide[j];

        pVec->Add(pDup);                            // asm: loc_697818
        ++pushed;
    }

    return pushed > 0;
}
