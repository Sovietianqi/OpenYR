#pragma once

// =============================================================================
// Special/RadSiteClass.h - 辐射区对象（核弹 / 辐射武器留下的持续伤害区域）
//
// 权威来源：asm 片段 /workspace/asm_export/RadSiteClass.asm.txt，
// 以及主 asm 中的结构定义：
//
//   RadSiteClass struc ; (sizeof=0x74, mappedto_155)
//     vt                    dd ?   ; +0x00
//     a     AbstractClass ?        ; +0x04（AbstractClass 基类子对象）
//     LightSource           dd ?   ; +0x24  关联光源对象指针
//     RadLevelStartTime     dd ?   ; +0x28  辐射等级衰减节拍起始帧
//     field_2C              dd ?   ; +0x2C  （保留）
//     RadLevelDelay         dd ?   ; +0x30  衰减间隔（帧）
//     RadLightStartTime     dd ?   ; +0x34  光源强度衰减节拍起始帧
//     field_38              dd ?   ; +0x38  （保留）
//     RadLightDelay         dd ?   ; +0x3C  光源衰减间隔（帧）
//     pos         wXY ?            ; +0x40  中心坐标（两个 int16：X/Y）
//     Spread                dd ?   ; +0x44  扩散半径（格）
//     SpreadInLeptons       dd ?   ; +0x48  扩散半径（leptons）
//     RadLevel              dd ?   ; +0x4C  当前辐射等级
//     CurrentLevelDecrement dd ?   ; +0x50  每次衰减量（等级/帧）
//     CurrentIntensity      dd ?   ; +0x54  当前光源强度
//     CurrentTint   RGBd ?         ; +0x58  R/G/B（+58/+5C/+60）
//     CurrentLightDecrement dd ?   ; +0x64  光源强度每帧衰减量
//     CurrentLightStage     dd ?   ; +0x68  光源初始强度（衰减基准）
//     StartRadLevel         dd ?   ; +0x6C  起始辐射剂量
//     CurrentRadLevel       dd ?   ; +0x70  剩余辐射剂量（逐帧递减，<=0 销毁）
//   RadSiteClass ends
//
// asm 进程符号与本类成员的对应：
//   RadSiteClass_CTOR                  <-> RadSiteClass::RadSiteClass
//   RadSiteClass::~RadSiteClass        <-> RadSiteClass::~RadSiteClass
//   RadSite_SetBaseCell                <-> RadSiteClass::SetBaseCell
//   RadSite_SetCellSpread              <-> RadSiteClass::SetCellSpread
//   RadSite_SetRadLevel                <-> RadSiteClass::SetRadLevel
//   RadSiteClass_GetCurrentLevel       <-> RadSiteClass::GetCurrentLevel
//   RadSiteClass_Add                   <-> RadSiteClass::Add
//   RadSiteClass_Radiate               <-> RadSiteClass::Radiate
//   RadSiteClass_Update                <-> RadSiteClass::Update
//   RadSiteClass_IncreaseLevel         <-> RadSiteClass::IncreaseLevel
//   RadSiteClass_DecreaseLevel         <-> RadSiteClass::DecreaseLevel
//   sub_65BB50                         <-> RadSiteClass::DecreaseAppliedCells
//   RadSiteClass_GetSize               <-> RadSiteClass::Size
//   RadSiteClass_GetCRC                <-> RadSiteClass::ComputeCRC
//   RadSiteClass_GetClassID            <-> RadSiteClass::GetClassID
//   RadSiteClass_Load                  <-> RadSiteClass::Load
//   RadSiteClass_Save                  <-> RadSiteClass::Save
//   RadSiteClass_GetAbstractDerivationID <-> RadSiteClass::GetAbstractDerivationID
//
// 派生自 ObjectClass（asm: 其 `a AbstractClass ?` 基类子对象即抽象层；
// asm 的 new/delete 与 vtable 四指针布局与 AbstractClass 系一致，本项目
// 落到 AbstractClass，语义等价：只有坐标 / 身份 / 逐帧 Update 三个面）。
// =============================================================================

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class CellClass;
class BlitterClass;
class DSurface;

// ============================================================================
// RGBd - asm 中 CurrentTint 三字节块（R/G/B），逐字节访问用。
// ============================================================================
struct RadTintStruct {
    int32 R;    // asm +0x58
    int32 G;    // asm +0x5C
    int32 B;    // asm +0x60
};

class RadSiteClass : public AbstractClass {
public:
    static const AbstractType AbsID = AbstractType::RadSite;

    // asm: vec_RadSites —— 全局辐射区池。CTOR 把自己挂进去，DTOR 摘除。
    // 定义在 Game/Globals.cpp。
    static DynamicVectorClass<RadSiteClass*>* Array;

    // ========================================================================
    // 构造 / 析构
    // ========================================================================
    //
    // asm: RadSiteClass_CTOR —— 地基调用 AbstractClass_CTOR，清零 0x24..0x70
    //   （RadLevelStartTime/RadLightStartTime 取当前 Frame），最后把自己
    //   Push 进 vec_RadSites。
    RadSiteClass() noexcept;

    // asm: RadSiteClass::~RadSiteClass —— 中心格清 RadSite、Detach、释放光源、
    //   从 vec_RadSites 摘除、销毁抽象基类。
    virtual ~RadSiteClass() override;

    // ========================================================================
    // 身份
    // ========================================================================
    virtual AbstractType WhatAmI() const override { return AbstractType::RadSite; }

    // asm: RadSiteClass_GetSize —— 返回 0x74。
    virtual int32 Size() const override { return 0x74; }

    // asm: RadSiteClass_GetCRC —— 转调 AbstractClass_GetCRC（retn 4，单参）。
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // asm: RadSiteClass_GetAbstractDerivationID —— 返回 0x45 (69)。
    int32 GetAbstractDerivationID() const { return 0x45; }

    // asm: Update 尾部 `test [esi+70h],eax; jg skip; call vtable[+0x20]`
    //   —— CurrentRadLevel（+0x70）<= 0 即自毁。本项目的驱动循环按此谓词
    //   把已死辐射区清出 Array（同 WaveClass::IsDead 的处理方式）。
    bool IsDead() const { return CurrentRadLevel <= 0; }

    // asm: RadSiteClass_GetClassID —— 成功时回写 CLSID_RadSiteClass，
    //   失败（pClassID == null）返回 0x80004003。
    virtual HRESULT GetClassID(CLSID* pClassID) override;

    // asm: RadSiteClass_Load —— AbstractClass_Load 后重设 vtable 四指针、
    //   复位计时字段，并把 +0x24（LightSource 槽）交给 Swizzle 修复。
    virtual HRESULT Load(IStream* pStm) override;

    // asm: RadSiteClass_Save —— 直接转调 AbstractClass_Save（retn 0Ch）。
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    // ========================================================================
    // 设置 / 查询（asm 的自由函数形态，落到成员方法）
    // ========================================================================
    //
    // asm: RadSite_SetBaseCell —— [this+40h] = 传入格子坐标（wXY）。
    void SetBaseCell(const CellStruct& cell);

    // asm: RadSite_SetCellSpread —— Spread = spread；
    //   SpreadInLeptons = (spread << 8) + 0x80。
    void SetCellSpread(int32 spread);

    // asm: RadSite_SetRadLevel —— RadLevel = level；
    //   StartRadLevel = CurrentRadLevel = RulesData->RadDurationMultiple * level。
    void SetRadLevel(int32 level);

    // asm: RadSiteClass_GetCurrentLevel —— StartRadLevel <= 0 时返回 0，
    //   否则返回 CurrentRadLevel * RadLevel / StartRadLevel。
    int32 GetCurrentLevel() const;

    // ========================================================================
    // 主循环
    // ========================================================================
    //
    // asm: RadSiteClass_Update —— 每帧：
    //   1) --CurrentRadLevel；CurrentRadLevel <= 0 由外层遍历判定销毁；
    //   2) RadLevelStartTime/RadLevelDelay 节拍到点则 DecreaseLevel 并重置；
    //   3) RadLightStartTime/RadLightDelay 节拍到点则按比例刷新光源
    //      tint/intensity；CurrentRadLevel <= 0 时经 vtable +0x20 自毁。
    virtual void Update() override;

    // ========================================================================
    // 辐射施加
    // ========================================================================
    //
    // asm: RadSiteClass_Add —— 先对既有覆盖格做一次“清扫”（DecreaseAppliedCells），
    //   然后 RadLevel += amount，并重算 StartRadLevel/CurrentRadLevel 与
    //   RadDurationMultiple 的乘积，最后重新 Radiate。
    void Add(int32 amount);

    // asm: RadSiteClass_Radiate —— 刷新计时字段与光源（首次建 LightSource，
    //   否则 ChangeLevels），计算 tint/intensity 与两档衰减量，再 IncreaseLevel。
    void Radiate();

    // asm: RadSiteClass_IncreaseLevel —— 以中心为原点、Spread 为半径的正方形
    //   邻域逐格扫描，按 (SpreadInLeptons - dist)/SpreadInLeptons * RadLevel
    //   的线性衰减，调用 CellClass_RadLevel_Increase 累加到各格的辐射剂量。
    void IncreaseLevel();

    // asm: RadSiteClass_DecreaseLevel —— 同扫描，额外除以 CurrentLevelDecrement；
    //   调用 CellClass_RadLevel_Decrease。
    void DecreaseLevel();

    // asm: sub_65BB50 —— DecreaseLevel 的“清扫”变体（Add 内部先调一次）。
    void DecreaseAppliedCells();

    // ========================================================================
    // 数据成员（语义对齐 asm 字段名，不要求字节偏移一致）
    // ========================================================================
    void*               LightSource;             // asm +0x24  关联光源
    int32               RadLevelStartTime;       // asm +0x28
    int32               field_2C;                // asm +0x2C
    int32               RadLevelDelay;           // asm +0x30
    int32               RadLightStartTime;       // asm +0x34
    int32               field_38;                // asm +0x38
    int32               RadLightDelay;           // asm +0x3C

    CellStruct          pos;                     // asm +0x40  中心坐标
    int32               Spread;                  // asm +0x44  扩散半径（格）
    int32               SpreadInLeptons;         // asm +0x48  扩散半径（leptons）
    int32               RadLevel;                // asm +0x4C
    int32               CurrentLevelDecrement;   // asm +0x50
    int32               CurrentIntensity;        // asm +0x54
    RadTintStruct       CurrentTint;             // asm +0x58  R/G/B
    int32               CurrentLightDecrement;   // asm +0x64
    int32               CurrentLightStage;       // asm +0x68
    int32               StartRadLevel;           // asm +0x6C
    int32               CurrentRadLevel;         // asm +0x70

private:
    // 局部工具：逐格扫描的公共实现（Increase/Decrease 共用）。
    //   op == +1 累加剂量，op == -1 衰减剂量（并额外除以 CurrentLevelDecrement）。
    void ApplyToCells(int32 op);
};
