#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class BlitterClass;
class DSurface;

static constexpr int32 MAX_EMPULSES = 16;

class EMPulseClass {
public:
    EMPulseClass();
    ~EMPulseClass();

    void Initialize(const CoordStruct& pos, int32 radius, int32 duration, int32 damage);
    void Update();
    void CreatePulseRing();
    void ApplyEMPDisable();
    void ApplyEMPToCell(const CellStruct& cell, int32 distance);
    bool IsEMPImmune(ObjectClass* pObj) const;
    void RenderEffect(BlitterClass* blitter, DSurface* surface);

    void SetRadius(int32 radius);
    void SetDuration(int32 duration);
    void SetDisableDuration(int32 duration);
    void SetDamageAmount(int32 damage);
    void SetPulseColor(const ColorStruct& color);
    void SetMaxPulses(int32 count);
    void SetPulseInterval(int32 interval);
    void SetOwnerHouse(int32 house);
    void SetPosition(const CoordStruct& pos);

    int32 GetCurrentRadius() const;
    int32 GetCurrentDuration() const;
    bool IsActivePulse() const;
    bool HasExpandedFully() const;

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_EndEffect (0x4C58B0)
    //
    //   void __thiscall EMPulseClass_EndEffect(EMPulseClass *this)
    //
    //   脉冲结束时的清场：以脉冲中心为原点，在半径 r 的正方形邻域内扫描，
    //   对每个落在 [0,0x200) 网格内、且到中心的平方距离 <= r² 的格子，
    //   清掉格子上的 cfEMP 标志位。半径取 this+0x28，中心取 this+0x24/+0x26
    //   （两个 int16）。
    //
    //   逐指令对应：
    //     eax = [esi+28h] (radius) / ebx = -eax / ebp = eax*eax
    //     outer(bx): for (bx = -r; bx <= r; ++bx)
    //     inner(di): for (di = -r; di <= r; ++di)
    //       ax = centerX + di ; cx = centerY + bx
    //       if ax in [0,0x200) && cx in [0,0x200) && CellExists(ax,cx):
    //         if (ax-cx0)^2 + (cx-cy0)^2 <= r^2:
    //           cell = Coord_Cell(ax,cx) ; cell->Flags &= ~cfEMP
    // ------------------------------------------------------------------------
    static void End_Effect(int16 centerX, int16 centerY, int32 radius);

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_Initialize (0x4C5560, retn 4: a2)
    //
    //   构造后由 CTOR 调用的"施加 EMP"例程（不是逐帧动画）：
    //     if (Frame >= CreateFrame + Duration) return      ; 已过期
    //     r² = Radius²
    //     A) 遍历 vec_Aircrafts：对满足 (airborne && !returning && 距离 < r*256)
    //        的飞机调用 vt[+3DCh](a2)（即 AircraftClass::EMPulse）。
    //     B) 以中心格为原点，在 [-r,r]² 方形邻域扫描：
    //          格落在 [0,0x200) 网格内 && CellExists && (dx²+dy² <= r²)：
    //            有建筑 -> 写 EMPDuration / 触发 IonBlast 动画
    //            否则找 techno -> 若为 Unit/Infantry 且非 EMP 免疫 -> 施加 EMP。
    // ------------------------------------------------------------------------
    void ApplyEffects(int32 a2);

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_Delete —— 归零除说明字段外的全部状态（重放用）。
    // ------------------------------------------------------------------------
    void Delete();

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_UpdateAll (0x4C54A0) —— 逆序遍历 vec_EMPulses，
    //   对 Frame >= CreateFrame + Duration 的事件调用 vt[+20h](1) 销毁。
    // ------------------------------------------------------------------------
    static void UpdateAll();

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_DeleteAll (0x4C5470) —— 清空 vec_EMPulses。
    // ------------------------------------------------------------------------
    static void DeleteAll();

    // asm: EMPulseClass::ComputeCRC（vt+34h）—— 直接复用本类的 GetCRC 累积器。
    void ComputeCRC(CRCEngine& crc) const;

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_Init_CellCoords (0x4C5210 区) —— 把默认格坐标
    //   （EMPulseClass_Default_CellCoords，wXY）清零。静态初始化入口。
    // ------------------------------------------------------------------------
    static void Init_CellCoords();

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_Init_RoomCoords (0x4C5230 区) —— 把默认三维坐标
    //   （Default_X/Y/Z）清零。静态初始化入口。
    // ------------------------------------------------------------------------
    static void Init_RoomCoords();

    // ------------------------------------------------------------------------
    // asm: EMPulseClass_GetCRC (retn 4) —— 先 AbstractClass_GetCRC，再依次把
    //   [24h](wXY 两个字) / [28h](radius) / [2Ch](CreateFrame) / [30h](duration)
    //   喂给 CRC_WORD/CRC_DWORD。OpenYR 以文件内 CRC 累积器等价。
    // ------------------------------------------------------------------------
    uint32 GetCRC(uint32 crc) const;

    // asm: EMPulseClass_GetSize —— mov eax,34h ; retn
    int32 GetSize() const;

    // asm: EMPulseClass_GetAbstractDerivationID —— mov eax,37h ; retn
    int32 GetAbstractDerivationID() const;

    CoordStruct Position;
    int32 Radius;
    int32 MaxRadius;
    int32 Duration;
    int32 MaxDuration;
    int32 ExpandSpeed;
    int32 CurrentRadius;
    int32 CurrentDuration;
    bool IsActive;
    bool IsExpanding;
    bool HasExpanded;
    ColorStruct PulseColor;
    uint8 PulseAlpha;
    uint8 FlashAlpha;
    int32 RingThickness;
    int32 PulseCount;
    int32 MaxPulses;
    int32 PulseInterval;
    int32 PulseTimer;
    int32 CurrentPulse;
    int32 DamageAmount;
    int32 DisableDuration;
    int32 OwnerHouse;

    // asm: EMPulseClass +0x2C —— 创建时的帧戳（UpdateAll 用它 + Duration 判过期）
    int32 CreateFrame;
};

class EMPulseManagerClass {
public:
    EMPulseManagerClass();
    ~EMPulseManagerClass();

    static EMPulseManagerClass* GetInstance();

    int32 CreateEMPulse(const CoordStruct& pos, int32 radius, int32 duration, int32 damage);
    bool RemoveEMPulse(int32 index);
    void RemoveAllPulses();
    void UpdateAllPulses();
    void RenderAllEffects(BlitterClass* blitter, DSurface* surface);
    int32 GetActiveCount() const;
    EMPulseClass* GetEMPulse(int32 index) const;

    EMPulseClass* Pulses[MAX_EMPULSES];
    int32 ActiveCount;
};