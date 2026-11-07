#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// StartSpotClass - one entry of the multiplayer starting-spot / player
// handshake table.  Original layout (all accesses are dword):
//   +0x28  NameNode               embedded name-node sub-object (init/assign)
//   +0x4B  int32  CountrySlot    (raw)
//   +0x4F  int32  CountrySentinel(-2 = "not set" -> return -2)
//   +0x53  int32  ColourSlot
//   +0x57  int32  ColourSentinel(-2 = "not set" -> return -2)
//   +0x5B  int32  StartSlot
//   +0x5F  int32  StartSlotSentinel(-2 -> fall through, then check -1)
//   +0x7B  int32  field_7B       (-1 on the bot records)
//   sizeof = 0x85
//
//  IsNotDummy compares the entry pointer against the seven CSF_* reserved
//  sentinels (OpenSlot, ClosedSlot, Waiting, Observer, EasyAI, NormAI,
//  HardAI) -- any of those means the slot is a "dummy".
// ============================================================================

class StartSpotClass {
public:
    StartSpotClass() noexcept
        : CSFName{}
        , CountrySlot(-2)
        , CountrySentinel(-2)
        , ColourSlot(-2)
        , ColourSentinel(-2)
        , StartSlot(-2)
        , StartSlotSentinel(-2)
        , ReservedA(0)
        , ReservedB(0)
        , field_6B(0)
        , HouseIndex(-1)
        , CountryFlag(0)
    {
    }

    // ------------------------------------------------------------------------
    // asm: StartSpotClass_IsNotDummy (0x5E9420)
    //   test ecx,ecx / jz false
    //   cmp ecx, CSF_OpenSlot_   / jz false
    //   cmp ecx, CSF_ClosedSlot_ / jz false
    //   cmp ecx, CSF_Waiting_    / jz false
    //   cmp ecx, CSF_Observer_   / jz false
    //   cmp ecx, CSF_EasyAI_     / jz false
    //   cmp ecx, CSF_NormAI_     / jz false
    //   cmp ecx, CSF_HardAI_     / jz false
    //   mov eax,1 / retn
    // ------------------------------------------------------------------------
    static bool Is_Not_Dummy(const StartSpotClass* pEntry);

    // ------------------------------------------------------------------------
    // asm: StartSpotClass_GetCountry (0x696F10)
    //   edx = [ecx+4Fh] / eax = -2 / cmp edx,eax / jz ret
    //   eax = [ecx+4Bh]
    // ------------------------------------------------------------------------
    int32 Get_Country() const
    {
        if (CountrySentinel == -2)          // asm: cmp edx,0FFFFFFFEh / jz
            return -2;                      // asm: mov eax,0FFFFFFFEh
        return CountrySlot;                 // asm: mov eax,[ecx+4Bh]
    }

    // ------------------------------------------------------------------------
    // asm: StartingSpotClass_GetColour (0x696F20)
    //   edx = [ecx+57h] / cmp -2 / jz ret / eax = [ecx+53h]
    // ------------------------------------------------------------------------
    int32 Get_Colour() const
    {
        if (ColourSentinel == -2)           // asm: cmp edx,0FFFFFFFEh / jz
            return -2;
        return ColourSlot;                  // asm: mov eax,[ecx+53h]
    }

    // ------------------------------------------------------------------------
    // asm: StartingSpotClass_GetStartSlot (0x696F40)
    //   mov edx,[ecx+5Fh] / mov eax,-2 / cmp edx,eax
    //   jnz read                    ; 哨兵不是 -2 -> 直接读槽位
    //   cmp [ecx+5Bh],-1 / jz ret   ; 槽位也是"未设" -> **eax 仍是 -2**
    // read: mov eax,[ecx+5Bh]
    //
    // 注意 jz 那一支返回的是 -2（初值），不是 -1。
    // ------------------------------------------------------------------------
    int32 Get_Start_Slot() const
    {
        if (StartSlotSentinel == -2 && StartSlot == -1)
            return -2;                      // asm: jz locret（eax = -2）
        return StartSlot;                   // asm: mov eax,[ecx+5Bh]
    }

    // ------------------------------------------------------------------------
    // asm: sub_696F90 (0x696F90, retn 4) —— 写国家槽。
    //   a2 == -2 : [ecx+4Bh] = 0  ; [ecx+4Fh] = -2（"未设"哨兵）
    //   否则      : [ecx+4Bh] = a2 ; [ecx+4Fh] = -1（已设）
    // Assign_Houses 的第一段对 CountrySlot == -3 的记录调 Set_Country(0)。
    // ------------------------------------------------------------------------
    void Set_Country(int32 a2)
    {
        if (a2 == -2) {
            CountrySlot = 0;
            CountrySentinel = -2;
        } else {
            CountrySlot = a2;
            CountrySentinel = -1;
        }
    }

    // ---- original offsets -------------------------------------------------
    // asm +0x28h: the embedded name-node.  Its display name is what
    // Assign_Houses copies with `wcsncpy(house+1602Ah, node, 0x15)` — 21 wide
    // characters including the terminator; HouseClass::CSFName is the same
    // 21-element buffer at +1602Ah.
    wchar_t CSFName[0x15];
    int32 CountrySlot;        // asm +0x4B
    int32 CountrySentinel;    // asm +0x4F
    int32 ColourSlot;         // asm +0x53
    int32 ColourSentinel;     // asm +0x57
    int32 StartSlot;          // asm +0x5B
    int32 StartSlotSentinel;  // asm +0x5F
    int32 ReservedA;          // asm +0x63  (idxTeam —— 队伍号)
    int32 ReservedB;          // asm +0x67  (field_67)
    // asm +0x6B —— 剧本主房标志：值为 -1 时 Assign_Houses 把新建的这幢房
    //   记成全局 someHouse。
    int32 field_6B;
    // asm +0x6F —— 该记录对应的 HouseClass::ArrayIndex（Assign_Houses 回填）。
    int32 HouseIndex;
    // asm +0x6B —— 进度屏的「国家被强制成 obsi」标志：
    //   ProgressScreenClass_DrawPlayerSpot 在 Session 3/4 下若见到 -1，
    //   就把 Get_Country() 的结果改成 0FFFFFFFDh(-3)，即观察者图标。
    int32 CountryFlag;        // asm +0x6B
};

// ============================================================================
// asm: StartSpot_Create_Potato    (0x796EC0)
// asm: StartSpot_Create_PotatoToo (0x796FA0)
// asm: StartSpot_Create_OlCruddy  (0x796DC0)
//
// 三个多人「假玩家(bot)」记录创建函数。三者地址经 asm 核实**均真实存在**
// 且与标注一致，是 IDA 命名符号（非 sub_）。真实签名均为 `void(void)`：
//
//   StartSpot_Create_Potato proc near  ; CODE XREF: sub_7970D0+9A…
//
// 调用者：sub_7970D0（先 GetComputerNameA，对硬编码主机名 "STEVE-T3" /
//         "STEVE-T2" 做 __strcmpi 判断后按分支调用三者——Westwood
//         开发者机器的彩蛋）。
//
// 公共骨架（asm 逐句）：
//   obj = new BYTE[0x85] ; if ok: sub_53ECB0(obj+28h)      ; 名字节点 init
//   sub_735060(obj, <name>, -1)                            ; 赋显示名
//   [obj+7Bh] = -1
//   [obj+4Bh] = country ; [obj+53h] = colour
//   a2 = 0 ; a3 = {0Ah, 02h, X, Y, 0, 0}
//   sub_53ED20(obj+28h, &a2, &a3, 0)                       ; 赋坐标
//   NameNodeTypes 追加（DynamicVectorClass 增长语义）
//   dword_B7790C[gPlayerCount] = 0x64 ; ++gPlayerCount
//   PotatoToo 额外把 12 字节坐标节点拷进全局 stru_A8BB1C。
//
//   Potato    : "Potato",    country 0, colour 0, a3 = 0A 02 14 61
//   PotatoToo : "PotatoToo", country 1, colour 2, a3 = 0A 02 50 FA
//   Ol Cruddy : "Ol Cruddy", country 0, colour 3, a3 = 0A 02 51 52
//
// 铁律①（宁可留白，不写错）：依赖三个尚未落地的引擎例程
//   sub_53ECB0 (名字节点 init) / sub_735060 (赋名) / sub_53ED20 (赋坐标)
//   以及全局 NameNodeTypes 向量、dword_B7790C 玩家表、stru_A8BB1C 镜像，
//   此处只保留 asm 调用点与原偏移，不臆造实现。
// ============================================================================
StartSpotClass* StartSpot_Create_Potato();
StartSpotClass* StartSpot_Create_PotatoToo();
StartSpotClass* StartSpot_Create_OlCruddy();

// asm: sub_7970D0 -- the GetComputerNameA / "STEVE-T3"|"STEVE-T2" dispatcher
// that drives the three creators above.
void StartSpot_Create_DevBots();

// ----------------------------------------------------------------------------
// asm sentinels: CSF_OpenSlot_ ... CSF_HardAI_ -- reserved table entries.
// The build reproduces them as distinguishable sentinel addresses.
// ----------------------------------------------------------------------------
StartSpotClass* Get_CSF_OpenSlot();
StartSpotClass* Get_CSF_ClosedSlot();
StartSpotClass* Get_CSF_Waiting();
StartSpotClass* Get_CSF_Observer();
StartSpotClass* Get_CSF_EasyAI();
StartSpotClass* Get_CSF_NormAI();
StartSpotClass* Get_CSF_HardAI();
