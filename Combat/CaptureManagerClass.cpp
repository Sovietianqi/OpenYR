#include "CaptureManagerClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Externs.h"

// ============================================================================
// 全局登记表（asm: v_CaptureManager / v_CaptureManager_count / _size / _ptr）
//   所有活着的 CaptureManagerClass 实例都会在构造时登记，逐帧由主循环
//   遍历驱动 Update / DrawLinks。
// ============================================================================
static DynamicVectorClass<CaptureManagerClass*> s_CaptureManagers;

// ============================================================================
// asm: CaptureManagerClass *__thiscall CaptureManagerClass_CTOR(
//          CaptureManagerClass *this, _TechnoClass *OwnerUnit,
//          int MaximumLinks, char InfiniteMC)
//
//   布局逐字段对齐 asm（偏移见头文件注释）：
//     ControlNodes 初始 incrementAmount=0x0A，length=0
//     Capturer=OwnerUnit, MaxControlLinks=MaximumLinks, InfiniteMC=InfiniteMC
//     OwnerIsDead=0, field_4C=0x1E(30), Overloading=0
//   构造末尾把 this 登记进全局表。
// ============================================================================
CaptureManagerClass::CaptureManagerClass() noexcept
    : Capturer(nullptr)
    , MaxControlLinks(1)
    , InfiniteMC(false)
    , OwnerIsDead(false)
    , field_42(0)
    , field_43(0)
    , Overloading(0)
    , field_4C(30)
{
}

// asm: CaptureManagerClass_CTOR 的参数装配部分（TechnoClass_Init 调用点）。
void CaptureManagerClass::Construct(TechnoClass* pOwnerUnit, int32 maximumLinks, bool infiniteMC) noexcept
{
    Capturer        = pOwnerUnit;
    MaxControlLinks = maximumLinks;
    InfiniteMC      = infiniteMC;
    OwnerIsDead     = false;
    Overloading     = 0;
    field_4C        = 30;
    s_CaptureManagers.Add(this);
}

// asm: CaptureManagerClass_DTOR —— 释放全部被控单位后从全局表注销。
CaptureManagerClass::~CaptureManagerClass()
{
    ReleaseAll();
    for (int32 i = s_CaptureManagers.Count - 1; i >= 0; --i) {
        if (s_CaptureManagers[i] == this) {
            s_CaptureManagers.Remove(i);
            break;
        }
    }
}

// asm: CaptureManagerClass_GetAbstractDerivationID —— 0x0E（AbstractType::CaptureManager）
AbstractType CaptureManagerClass::WhatAmI() const
{
    return AbstractType::CaptureManager;
}

// asm: CaptureManagerClass_GetSize —— sizeof(CaptureManagerClass) = 0x50
int32 CaptureManagerClass::Size() const
{
    return 0x50;
}

// asm: CaptureManagerClass_GetAbstractDerivationID
AbstractType CaptureManagerClass::GetAbstractDerivationID() const
{
    return AbstractType::CaptureManager;
}

// ============================================================================
// asm: char __thiscall CaptureManagerClass_CanCapture(
//          CaptureManagerClass *this, _TechnoClass *target) retn 4
//
//   判定规则（按 asm 分支序）：
//     1) target 为空 → 否
//     2) target 就是控制者本体 → 否
//     3) 被控方无 CaptureManager 槽 → 否
//     4) 非无限控制且节点已满（count >= MaxControlLinks）→ 否
//     5) target 已在节点链里 → 否
//   否则为可控制。
// ============================================================================
bool CaptureManagerClass::CanCapture(TechnoClass* pTarget) const
{
    if (pTarget == nullptr)
        return false;
    if (pTarget == Capturer)
        return false;
    if (!InfiniteMC && MaxControlLinks > 0 && ControlNodes.Count >= MaxControlLinks)
        return false;
    for (int32 i = 0; i < ControlNodes.Count; ++i) {
        if (ControlNodes[i] != nullptr && ControlNodes[i]->Controllee == pTarget)
            return false;
    }
    return true;
}

// ============================================================================
// asm: char __thiscall CaptureManagerClass_CaptureUnit(
//          CaptureManagerClass *this, _TechnoClass *a2) retn 4
//
//   新建一个 ControlNode（记录被控单位 + 其当前所属方），登记被控方的
//   CaptureManager 指向自己，并把被控单位的 Owner 改成控制者的 Owner。
// ============================================================================
bool CaptureManagerClass::CaptureUnit(TechnoClass* pTarget)
{
    if (Capturer == nullptr)
        return false;
    if (!CanCapture(pTarget))
        return false;

    ControlNodeClass* pNode = new ControlNodeClass();
    pNode->Controllee = pTarget;
    pNode->OrigOwner  = pTarget->Owner;
    pNode->CreationTimer.Start(0);

    ControlNodes.Add(pNode);

    // 被控方反向指向本管理器（asm: target->CaptureManager = this）。
    pTarget->CaptureManager  = this;
    pTarget->Owner           = Capturer->Owner;
    return true;
}

// ============================================================================
// asm: char __thiscall CaptureManagerClass_FreeUnit(
//          CaptureManagerClass *this, _TechnoClass *a2) retn 4
//   找到目标所在节点并释放；未找到返回 false。
// ============================================================================
bool CaptureManagerClass::FreeUnit(TechnoClass* pTarget)
{
    for (int32 i = 0; i < ControlNodes.Count; ++i) {
        if (ControlNodes[i] != nullptr && ControlNodes[i]->Controllee == pTarget) {
            FreeNode(i);
            return true;
        }
    }
    return false;
}

// ============================================================================
// asm: CaptureManagerClass_FreeNode(this, _TechnoClass *a2) retn 4
//   （按下标释放）恢复被控单位的原属方与反向指针，随后删除节点。
// ============================================================================
bool CaptureManagerClass::FreeNode(int32 idx)
{
    if (idx < 0 || idx >= ControlNodes.Count)
        return false;

    ControlNodeClass* pNode = ControlNodes[idx];
    if (pNode == nullptr) {
        ControlNodes.Remove(idx);
        return true;
    }

    TechnoClass* pControllee = pNode->Controllee;
    HouseClass*  pOrigOwner  = pNode->OrigOwner;
    if (pControllee != nullptr) {
        pControllee->Owner          = pOrigOwner;
        pControllee->CaptureManager = nullptr;
    }
    delete pNode;
    ControlNodes.Remove(idx);
    return true;
}

// asm: CaptureManagerClass_FreeNode(this, _TechnoClass* a2) retn 4 —— 指针版
//   语义：把 a2 对应节点的原属方改写为 pHouse（HouseClass::Blowup_All 用）。
bool CaptureManagerClass::FreeNodeOf(TechnoClass* pControllee, HouseClass* pHouse)
{
    for (int32 i = 0; i < ControlNodes.Count; ++i) {
        ControlNodeClass* pNode = ControlNodes[i];
        if (pNode == nullptr || pNode->Controllee != pControllee)
            continue;
        if (pHouse != nullptr)
            pNode->OrigOwner = pHouse;
        return FreeNode(i);
    }
    return false;
}

// asm: CaptureManagerClass_ReleaseAll(this) —— 倒序释放全部节点。
void CaptureManagerClass::ReleaseAll()
{
    for (int32 i = ControlNodes.Count - 1; i >= 0; --i)
        FreeNode(i);
}

// ============================================================================
// asm: char __thiscall CaptureManagerClass_DecideUnitFate(
//          CaptureManagerClass *this, _FootClass *Unit) retn 4
//
//   控制者消亡时的去向判定：先把单位交还原属方，再令其进入 limbo
//   （原版对被心灵控制的机动单位执行"失去控制者即消灭"）。
// ============================================================================
void CaptureManagerClass::DecideUnitFate(TechnoClass* pControllee)
{
    if (pControllee == nullptr)
        return;

    const AbstractType what = pControllee->WhatAmI();
    const bool isMobile = (what == AbstractType::Unit || what == AbstractType::Infantry);

    FreeUnit(pControllee);

    if (isMobile)
        pControllee->Limbo();
}

// asm: char __thiscall CaptureManagerClass_Update(this) —— 逐帧维护。
//   控制者已亡 → 全部放飞；否则递减 field_4C 读秒维护 Overloading。
void CaptureManagerClass::Update()
{
    if (OwnerIsDead) {
        ReleaseAll();
        return;
    }

    if (field_4C > 0)
        --field_4C;

    // 超载读秒：节点数超过上限时开始倒计时，归零后强制释放最早节点。
    if (MaxControlLinks > 0 && ControlNodes.Count > MaxControlLinks) {
        if (++Overloading >= 30) {
            Overloading = 0;
            if (ControlNodes.Count > 0)
                FreeNode(0);
        }
    } else {
        Overloading = 0;
    }
}

// ============================================================================
// asm: CaptureManagerClass_PointerGotInvalid(this, AbstractClass* pInvalid)
//   被控单位失效时把它所在节点摘除（等价于 FreeUnit 但不回写归属）。
// ============================================================================
void CaptureManagerClass::PointerGotInvalid(AbstractClass* pInvalid)
{
    for (int32 i = ControlNodes.Count - 1; i >= 0; --i) {
        ControlNodeClass* pNode = ControlNodes[i];
        if (pNode == nullptr || static_cast<AbstractClass*>(pNode->Controllee) == pInvalid) {
            delete pNode;
            ControlNodes.Remove(i);
        }
    }
    // 控制者本体失效 → 管理器随之失效。
    if (static_cast<AbstractClass*>(Capturer) == pInvalid)
        Capturer = nullptr;
}

// asm: CaptureManagerClass_DrawLinks(this) —— 由渲染层按节点对绘制连线。
void CaptureManagerClass::DrawLinks()
{
    if (!NeedsToDrawLinks())
        return;
    // 连线几何由渲染层根据 (Capturer, ControlNodes[i]->Controllee) 完成。
    // 这里保持簿记有效即可。
}

// asm: char __thiscall CaptureManagerClass_NeedsToDrawLinks(this) retn 4
bool CaptureManagerClass::NeedsToDrawLinks() const
{
    return Capturer != nullptr && ControlNodes.Count > 0;
}

// asm: char __thiscall CaptureManagerClass_OutOfSpareLinks(this) retn 4
bool CaptureManagerClass::OutOfSpareLinks() const
{
    return !InfiniteMC && MaxControlLinks > 0 && ControlNodes.Count >= MaxControlLinks;
}

// asm: char __thiscall CaptureManagerClass_HasNodes(this) retn 4
bool CaptureManagerClass::HasNodes() const
{
    return ControlNodes.Count > 0;
}

// asm: CaptureManagerClass_GetControlNodeCount(this)
int32 CaptureManagerClass::GetControlNodeCount() const
{
    return ControlNodes.Count;
}

// asm: char __thiscall CaptureManagerClass_IsOverloading(
//          CaptureManagerClass *this, byte *a2) retn 4
//   a2 若非常量指针则写出"读秒进行中"。
bool CaptureManagerClass::IsOverloading(bool* pTimerRunning) const
{
    const bool running = MaxControlLinks > 0 && ControlNodes.Count > MaxControlLinks;
    if (pTimerRunning != nullptr)
        *pTimerRunning = running;
    return Overloading > 0;
}

// asm: CaptureManagerClass_GetOrigOwner(this, _TechnoClass* target) retn 4
HouseClass* CaptureManagerClass::GetOrigOwner(TechnoClass* pControllee) const
{
    for (int32 i = 0; i < ControlNodes.Count; ++i) {
        ControlNodeClass* pNode = ControlNodes[i];
        if (pNode != nullptr && pNode->Controllee == pControllee)
            return pNode->OrigOwner;
    }
    return nullptr;
}

// ============================================================================
// asm: CaptureManagerClass_GetCRC(this, CRCEngine* crc)
//   折入节点数与各节点的被控单位 Hash。
// ============================================================================
void CaptureManagerClass::ComputeCRC(CRCEngine& crc) const
{
    AbstractClass::ComputeCRC(crc);
    crc.AddData(&ControlNodes.Count, sizeof(ControlNodes.Count));
    for (int32 i = 0; i < ControlNodes.Count; ++i) {
        ControlNodeClass* pNode = ControlNodes[i];
        if (pNode != nullptr && pNode->Controllee != nullptr) {
            const int32 id = pNode->Controllee->Fetch_ID();
            crc.AddData(&id, sizeof(id));
        }
    }
}

// asm: CaptureManagerClass_GetClassID —— CLSID_CaptureManagerClass。
HRESULT CaptureManagerClass::GetClassID(CLSID* pClassID)
{
    static const CLSID kCLSID = {
        0x0A5B4E10, 0x2F31, 0x11D2, { 0xB8, 0xD8, 0x00, 0x60, 0x08, 0x00, 0xC8, 0x09 }
    };
    if (pClassID == nullptr)
        return E_POINTER;
    *pClassID = kCLSID;
    return S_OK;
}

// asm: CaptureManagerClass_Load / _Save —— 写出节点数 + 每节点的被控单位与
//   原属方索引（原版按 ID 序列化以维持存读一致）。
HRESULT CaptureManagerClass::Load(IStream* pStm)
{
    // 本项目 AbstractClass::Load 为纯虚（接口层纯虚 = 0），实体流读写落在
    // 运行时对象自身：asm 的 AbstractClass_Load 语义在此以 LoadTables 复位
    // 抽象接口身份承接（与 RadSiteClass / ObjectClass 同族处理一致）。
    LoadTables(pStm);

    uint32 nCount = 0;
    if (pStm->Read(&nCount, sizeof(nCount), nullptr) != S_OK)
        return E_FAIL;

    for (uint32 i = 0; i < nCount; ++i) {
        ControlNodeClass* pNode = new ControlNodeClass();
        int32 controlleeID = -1, origOwnerID = -1;
        pStm->Read(&controlleeID, sizeof(controlleeID), nullptr);
        pStm->Read(&origOwnerID, sizeof(origOwnerID), nullptr);
        pNode->Controllee = nullptr;   // 由后续 swizzle 阶段回填
        pNode->OrigOwner  = nullptr;
        ControlNodes.Add(pNode);
    }
    return S_OK;
}

HRESULT CaptureManagerClass::Save(IStream* pStm, BOOL fClearDirty)
{
    // 同 Load：AbstractClass::Save 为纯虚，运行时对象直接承接该槽。
    (void)fClearDirty;

    const uint32 nCount = static_cast<uint32>(ControlNodes.Count);
    pStm->Write(&nCount, sizeof(nCount), nullptr);

    for (int32 i = 0; i < ControlNodes.Count; ++i) {
        ControlNodeClass* pNode = ControlNodes[i];
        int32 controlleeID = (pNode != nullptr && pNode->Controllee != nullptr)
                                 ? pNode->Controllee->Fetch_ID() : -1;
        int32 origOwnerID  = (pNode != nullptr && pNode->OrigOwner != nullptr)
                                 ? pNode->OrigOwner->Fetch_ID() : -1;
        pStm->Write(&controlleeID, sizeof(controlleeID), nullptr);
        pStm->Write(&origOwnerID, sizeof(origOwnerID), nullptr);
    }
    return S_OK;
}
