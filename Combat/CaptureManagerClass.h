#pragma once

#include "../Core/Definitions.h"
#include "../Containers/DynamicVectorClass.h"

class HouseClass;
class TechnoClass;

// ============================================================================
// CaptureManagerClass - 心理控制（俘虏）管理器
//
// 根据游戏行为，可知每个具备精神控制能力的单位各持有一个控制管理器，
// 记录“我控制了谁”与“被控制者的原属方”，并在控制者消亡或超载时
// 决定被控制单位的去向。
// ============================================================================
class CaptureManagerClass {
public:
    struct ControlNode {
        TechnoClass* Controllee;      // 被控制的单位
        HouseClass*  OriginalOwner;   // 被控制时的原属方
    };

    CaptureManagerClass() noexcept;
    virtual ~CaptureManagerClass();

    virtual bool CanCapture(TechnoClass* pTarget) const;
    virtual bool CaptureUnit(TechnoClass* pTarget);
    virtual void FreeUnit(TechnoClass* pTarget);
    virtual void FreeNode(int32 idx);
    virtual void ReleaseAll();
    virtual void DecideUnitFate(TechnoClass* pControllee);
    virtual void DrawLinks();
    virtual bool NeedsToDrawLinks() const;
    virtual int32 GetControlNodeCount() const;
    virtual bool HasNodes() const;
    virtual bool IsOverloading() const;
    virtual bool OutOfSpareLinks() const;
    virtual HouseClass* GetOrigOwner(TechnoClass* pControllee) const;

    TechnoClass* Controller;            // 控制者本体
    int32        MaxControlNodes;       // 备用控制节点容量（负数视为无限制）
    DynamicVectorClass<ControlNode> Nodes;
};
