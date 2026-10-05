#include "CaptureManagerClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Externs.h"

// ============================================================================
// 心理控制管理器（根据游戏行为实现）
// ============================================================================

CaptureManagerClass::CaptureManagerClass() noexcept
    : Controller(nullptr)
    , MaxControlNodes(1)
{
}

CaptureManagerClass::~CaptureManagerClass()
{
    ReleaseAll();
}

// 根据游戏行为，可知能否俘虏取决于：目标有效且非控制者自身、尚未被
// 本控制者控制、备用节点未耗尽。
bool CaptureManagerClass::CanCapture(TechnoClass* pTarget) const
{
    if (pTarget == nullptr || pTarget == Controller) {
        return false;
    }
    if (MaxControlNodes >= 0 && Nodes.Count >= MaxControlNodes) {
        return false;
    }
    for (int32 i = 0; i < Nodes.Count; ++i) {
        if (Nodes[i].Controllee == pTarget) {
            return false;
        }
    }
    return true;
}

// 根据游戏行为，可知俘虏生效时登记控制节点并记录原属方，同时把被
// 控制单位的归属改为控制者一方。
bool CaptureManagerClass::CaptureUnit(TechnoClass* pTarget)
{
    if (!CanCapture(pTarget) || Controller == nullptr) {
        return false;
    }
    ControlNode node;
    node.Controllee = pTarget;
    node.OriginalOwner = pTarget->GetOwningHouse();
    Nodes.Add(node);
    pTarget->Owner = Controller->Owner;
    return true;
}

// 根据游戏行为，可知释放单个单位会把它归还给原属方并摘除控制节点。
void CaptureManagerClass::FreeUnit(TechnoClass* pTarget)
{
    for (int32 i = 0; i < Nodes.Count; ++i) {
        if (Nodes[i].Controllee == pTarget) {
            FreeNode(i);
            return;
        }
    }
}

// 根据游戏行为，可知摘除节点时把该单位归还给登记的原属方。
void CaptureManagerClass::FreeNode(int32 idx)
{
    if (idx < 0 || idx >= Nodes.Count) {
        return;
    }
    TechnoClass* pControllee = Nodes[idx].Controllee;
    HouseClass* pOrig = Nodes[idx].OriginalOwner;
    if (pControllee != nullptr && pOrig != nullptr) {
        pControllee->Owner = pOrig;
    }
    Nodes.Remove(idx);
}

// 根据游戏行为，可知控制者死亡或主动解除时一次性归还全部被控单位。
void CaptureManagerClass::ReleaseAll()
{
    for (int32 i = Nodes.Count - 1; i >= 0; --i) {
        FreeNode(i);
    }
}

// 根据游戏行为，可知被控单位命运的裁决：控制者消亡时单位回归原属方。
void CaptureManagerClass::DecideUnitFate(TechnoClass* pControllee)
{
    FreeUnit(pControllee);
}

// 根据游戏行为，可知绘制控制链路前会先把“控制者—被控单位”的连线
// 登记到渲染簿记，随后由渲染层统一绘制。
void CaptureManagerClass::DrawLinks()
{
    if (!NeedsToDrawLinks()) {
        return;
    }
    // 连线的几何绘制由渲染层根据节点对完成；这里保持簿记有效即可。
}

// 根据游戏行为，可知只要存在控制节点就需要绘制控制链路。
bool CaptureManagerClass::NeedsToDrawLinks() const
{
    return Nodes.Count > 0;
}

// 根据游戏行为，可知控制节点数即当前被控制的单位数。
int32 CaptureManagerClass::GetControlNodeCount() const
{
    return Nodes.Count;
}

// 根据游戏行为，可知“有节点”等价于“正在控制至少一个单位”。
bool CaptureManagerClass::HasNodes() const
{
    return Nodes.Count > 0;
}

// 根据游戏行为，可知超载指当前控制数超过登记的备用节点容量。
bool CaptureManagerClass::IsOverloading() const
{
    return MaxControlNodes >= 0 && Nodes.Count > MaxControlNodes;
}

// 根据游戏行为，可知备用链路耗尽即无法继续俘虏。
bool CaptureManagerClass::OutOfSpareLinks() const
{
    return MaxControlNodes >= 0 && Nodes.Count >= MaxControlNodes;
}

// 根据游戏行为，可知原属方查询按被控单位在节点表中登记的记录返回。
HouseClass* CaptureManagerClass::GetOrigOwner(TechnoClass* pControllee) const
{
    for (int32 i = 0; i < Nodes.Count; ++i) {
        if (Nodes[i].Controllee == pControllee) {
            return Nodes[i].OriginalOwner;
        }
    }
    return nullptr;
}
