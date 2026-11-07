#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

// ------------------------------------------------------------------------
// 根据游戏行为，可知光盘面负责探测盘符、请求换盘与记录所需盘号。
// ------------------------------------------------------------------------
class CD {
public:
    static bool Inserted(int32 diskNumber);
    static bool IsInserted();
    static int32 GetCDIndex();
    static bool SwapToDisk(int32 diskNumber);
    static void SetRequiredCDIndex(int32 index);

    // ========================================================================
    // asm: Force_CD_Available (真实入口 ≈0x4A8277)
    //
    //   ; char __thiscall CD::ForceAvailable(void* this, int cdIndex)
    //   this = ecx ; retn 4
    //   调用者：CD::Is_Available+24 直接 call；并经 vtable 被 6+ 处虚派发。
    //
    //   铁证（asm 行 37854 的 vt_CDClass 结构）：
    //     { ForceAvailable(+0) ; FoundCdNumber(+4) ; SwapToDisk(+8) }
    //   Force_CD_Available 即本类虚表槽 0；this+4 即 FoundCdNumber；
    //   this->[vt+4] / [vt+8] 即后两槽。
    //
    //   （AsmComplex5 原标注 0x4A8260 在 asm 中未找到；由 loc_4A829B = +0x24
    //     反推真实入口 ≈0x4A8277。批注把 this 说成 "CD monitor 对象"，
    //     实为 CD 实例。）
    //
    //   —— 语义（asm 逐句）：
    //     cd == -2                         -> true
    //     cd >= 0 -> cd = 2
    //     idx = Get_CD_Index(RequiredCDNumber, 0x3C)
    //     if (idx > -1 && (cd == idx || cd == -1)) 且 RequiredCDNumber != 0
    //                                      -> finalize
    //     否则若 OldRequiredCDNumber != 0 && != RequiredCDNumber:
    //       idx = Get_CD_Index(old, 0x258)
    //       if (cd == idx) -> finalize ; else if (cd == -1) -> true
    //     timeout = 0x78
    //     loop:
    //       found = -1
    //       轮询 CDDataClass.counter 个盘符（round-robin 起点 [field_6C]）：
    //         idx = Get_CD_Index(drive, timeout)
    //         if (idx > -1 && (cd == idx || cd == -1 || cd == -2)) {
    //             found = drive ; break ;
    //         }
    //       if (found > 0):
    //         CD_SetRequiredCDIndex(found) ; sub_47AAC0() ; finalize
    //       answered = [this]->vt[+4](cd)        ; 提示插盘
    //       if (!answered) return false
    //       timeout = 0x12C
    //
    //   注：CD 类在本项目是纯静态桩（Inserted/IsInserted/GetCDIndex/
    //   SwapToDisk/SetRequiredCDIndex），暂无实例与 vtable。铁律①：
    //   此处固化 ForceAvailable 的**完整判定流程**（这部分只依赖
    //   Get_CD_Index 与全局 CD 状态，语义可确证），把依赖实例虚槽的
    //   "提示插盘"一步保留为 asm 调用点注释。
    // ========================================================================
    static bool ForceAvailable(int32 cdIndex);
    static bool Is_Available(int32 cdIndex);
    static int32 Get_Index(int32 drive, int32 timeout);

    // asm: 与 vtable 槽 +4 / +8 对应
    static int32  Get_Found_Cd_Number();
    static void   Set_Found_Cd_Number(int32 n);
};
