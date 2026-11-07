#pragma once

#include "../Core/Definitions.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/Timer.h"
#include "../Abstract/AbstractClass.h"

class HouseClass;
class TechnoClass;
class TechnoTypeClass;

// ============================================================================
// CaptureManagerClass - 心灵控制（俘虏）管理器
//
// asm 权威布局（CaptureManagerClass struc ; sizeof=0x50）：
//   +0x00 vt             vtable
//   +0x04 a              AbstractClass 基对象（IRTTITypeInfo/INoticeSink/...）
//   +0x24 ControlNodes   VectorPtrCapturedNodeClass（堆节点指针数组）
//   +0x3C MaxControlLinks
//   +0x40 InfiniteMC     byte
//   +0x41 OwnerIsDead    byte
//   +0x42 field_42       byte
//   +0x43 field_43       byte
//   +0x44 Overloading    dword
//   +0x48 Capturer       _TechnoClass*（控制者本体）
//   +0x4C field_4C       dword（帧倒计时）
//
// 每个具备精神控制能力的单位各持有一个控制管理器，记录"我控制了谁"与
// "被控制者原属方"，并在控制者消亡或超载时决定被控制单位的去向。
// ============================================================================

// asm: CapturedNodeClass（VectorPtrCapturedNodeClass 的元素），堆分配节点：
//   +0x00 Controllee       _TechnoClass*（被控制单位）
//   +0x04 OrigOwner        HouseClass*（被控制时的原属方）
//   +0x08 CreationTimer    CDTimerClass（StartTime/TimeLeft）
struct ControlNodeClass {
    TechnoClass* Controllee;
    HouseClass*  OrigOwner;
    CDTimerClass CreationTimer;
};

class CaptureManagerClass : public AbstractClass {
public:
    CaptureManagerClass() noexcept;
    virtual ~CaptureManagerClass();

    // asm: CaptureManagerClass_CTOR(OwnerUnit, MaximumLinks, InfiniteMC)
    // 由 TechnoClass_Init 在具备心灵控制能力时装配。
    void Construct(TechnoClass* pOwnerUnit, int32 maximumLinks, bool infiniteMC) noexcept;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual AbstractType GetAbstractDerivationID() const;
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // asm: CaptureManagerClass_IsOverloading(this, byte* a2) retn 4
    // 出参返回"是否正在超载读秒"。
    bool IsOverloading(bool* pTimerRunning) const;

    // asm: CaptureManagerClass_Update(this) —— 逐帧维护控制链。
    virtual void Update() override;

    // asm: CaptureManagerClass_PointerGotInvalid(this, void* a2) retn 4
    // 被控单位失效时从节点链中摘除对应节点。
    void PointerGotInvalid(AbstractClass* pInvalid);

    // asm: CaptureManagerClass_CanCapture(this, _TechnoClass* target) retn 4
    virtual bool CanCapture(TechnoClass* pTarget) const;

    // asm: CaptureManagerClass_CaptureUnit(this, _TechnoClass* a2) retn 4
    virtual bool CaptureUnit(TechnoClass* pTarget);

    // asm: CaptureManagerClass_FreeUnit(this, _TechnoClass* a2) retn 4
    bool FreeUnit(TechnoClass* pTarget);

    // asm: CaptureManagerClass_ReleaseAll(this)
    void ReleaseAll();

    // asm: CaptureManagerClass_FreeNode(this, _TechnoClass* a2) retn 4
    bool FreeNode(int32 idx);

    // asm: CaptureManagerClass_DecideUnitFate(this, _FootClass* Unit) retn 4
    void DecideUnitFate(TechnoClass* pControllee);

    // asm: CaptureManagerClass_DrawLinks(this)
    void DrawLinks();

    // asm: CaptureManagerClass_NeedsToDrawLinks(this) retn 4
    bool NeedsToDrawLinks() const;

    // asm: CaptureManagerClass_OutOfSpareLinks(this)
    bool OutOfSpareLinks() const;

    // asm: CaptureManagerClass_HasNodes(this)
    bool HasNodes() const;

    // asm: CaptureManagerClass_GetControlNodeCount(this)
    int32 GetControlNodeCount() const;

    // asm: CaptureManagerClass_GetOrigOwner(this, _TechnoClass* target) retn 4
    HouseClass* GetOrigOwner(TechnoClass* pControllee) const;

    // asm: CaptureManagerClass_FreeNode(this, _TechnoClass* a2) retn 4
    // 语义：把 a2 对应节点的原属方改写为 pHouse（HouseClass::Blowup_All 用）。
    bool FreeNodeOf(TechnoClass* pControllee, HouseClass* pHouse);

    TechnoClass*  Capturer;          // +0x48 控制者本体
    int32         MaxControlLinks;   // +0x3C 最大控制链数（负数视为无限制）
    bool          InfiniteMC;        // +0x40 无限心灵控制
    bool          OwnerIsDead;       // +0x41 控制者已亡
    uint8         field_42;          // +0x42
    uint8         field_43;          // +0x43
    int32         Overloading;       // +0x44 超载读秒
    int32         field_4C;          // +0x4C 帧倒计时（默认 30）

    // +0x24 被控节点链（堆节点指针）。
    DynamicVectorClass<ControlNodeClass*> ControlNodes;
};
