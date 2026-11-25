#include "HotkeyCommands.h"
#include "../Game/Externs.h"

#include <Abstract/ObjectClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/InfantryClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/BuildingClass.h>
#include <Rendering/TacticalClass.h>
#include <Rules/RulesClass.h>
#include <Audio/VocClass.h>
#include <Game/Game.h>

// ============================================================================
// 三条选择集命令（Cheer / Deploy / Stop）。
//
// asm 里它们都是薄壳：遍历 v_CurrentObjects，对每个被选对象问几个虚表槽，
// 全部放行后播一条确认语音。真正的状态变更由 TechnoClass_374（槽 +374h）
// 把 Mission 入队完成。
//
// 本工程没有按原版槽位重建 vtable，也没有 0x80 槽环形网络包队列，因此按
// **功能等价**落点：
//   [vt+0A0h] CanReceiveWaypoint -> TechnoClass::CanSetWaypoint()
//   [vt+09Ch] TechnoClass_9C     -> TechnoClass::Can_Be_Commanded()
//   [vt+374h] TechnoClass_374    -> TechnoClass::DispatchMission()
//   ObjectClass::Flags14 & 1     -> ObjectClass::Is_Selected()
// ============================================================================

namespace {

// v_CurrentObjects 在本工程的等价物是 TacticalClass::SelectedObjects。
inline int32 SelectionCount()
{
    return TacticalClass::SelectedObjects.Count;
}

inline ObjectClass* SelectionAt(int32 i)
{
    return TacticalClass::SelectedObjects[i];
}

inline bool IsPlayerControlled(const TechnoClass* pThis)
{
    if (pThis == nullptr || pThis->Owner == nullptr)
        return false;
    bool isPlayer = false;
    return pThis->Owner->Is_Player(&isPlayer) == S_OK && isPlayer;
}

// asm: 循环开头的 `test byte ptr [esi+14h], 1` —— 只处理"仍处于选中态"的对象。
inline bool AcceptsOrder(const ObjectClass* pObj)
{
    return pObj != nullptr && pObj->Is_Selected();
}

constexpr uint32 kCheerCooldownFrames = 30;   // asm: RulesData[+580h] 的帧冷却

int32 LastCheerFrame = 0;

} // namespace

// ============================================================================
// asm: CheerCommandClass_Execute   (asm:1666621)
//
//   if (Player_MuteSWLaunches) return;
//   if (RulesData[+580h] + LastCheerFrame >= Frame) return;
//   NetworkEvent ev;  AddEvent_Noopt(&ev, ev_ALLCHEER, PlayerPtr[+30h]);
//   ... 塞进 0x80 槽环形包队列 ...
//   LastCheerFrame = Frame;
//
// 本工程直接把欢呼落到本地：播 Rules->CheerSound 并刷新冷却时间戳。
// ============================================================================
void CheerCommandClass::Execute(uint32 flags)
{
    (void)flags;

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr)
        return;

    const int32 now = static_cast<int32>(Game::CurrentFrame);
    if (LastCheerFrame + static_cast<int32>(kCheerCooldownFrames) >= now)
        return;

    if (pRules->CheerSound >= 0)
        VocClass::PlayIndexAtPos(pRules->CheerSound, nullptr);

    LastCheerFrame = now;
}

// ============================================================================
// asm: DeployCommandClass_Execute   (asm:1666043)
//
// 阶段 A：选择集首项是"有驻军的己方建筑" -> 直接发 Deploy（卸载/开门）。
// 阶段 B：统计选择集里"已部署 / 未部署"两类是否同时存在。
// 阶段 C：逐个过命令门，能收就发 Deploy；混编时跳过已部署的那一类。
// 收尾：只要有一项被受理就播 VoiceDeploy。
// ============================================================================
void DeployCommandClass::Execute(uint32 flags)
{
    (void)flags;

    const int32 n = SelectionCount();
    if (n <= 0)
        return;

    bool fired = false;

    // ── 阶段 A ────────────────────────────────────────────────────────────
    ObjectClass* pFirst = SelectionAt(0);
    if (pFirst != nullptr && pFirst->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBld = static_cast<BuildingClass*>(pFirst);
        if (pBld->GetOccupantCount() > 0 && IsPlayerControlled(pBld))
        {
            if (AcceptsOrder(pBld) && pBld->DispatchMission(Mission::Deploy))
                fired = true;
            goto play_voice;
        }
    }

    {
        // ── 阶段 B ────────────────────────────────────────────────────────
        bool deployed   = false;
        bool undeployed = false;

        for (int32 i = 0; i < n; ++i)
        {
            ObjectClass* pObj = SelectionAt(i);
            if (pObj == nullptr)
                continue;

            const AbstractType kind = pObj->WhatAmI();
            if (kind == AbstractType::Infantry)
            {
                InfantryClass* pInf = static_cast<InfantryClass*>(pObj);
                if (!pInf->IsDeployer())
                    continue;
                if (pInf->IsDeployed_Anim()) {
                    deployed = true;
                    if (undeployed) break;
                } else {
                    undeployed = true;
                    if (deployed) break;
                }
            }
            else if (kind == AbstractType::Unit)
            {
                UnitClass* pUnit = static_cast<UnitClass*>(pObj);
                if (!pUnit->IsDeployable())
                    continue;
                if (pUnit->IsDeployed()) {
                    deployed = true;
                    if (undeployed) break;
                } else {
                    undeployed = true;
                    if (deployed) break;
                }
            }
        }

        // ── 阶段 C ────────────────────────────────────────────────────────
        for (int32 i = 0; i < n; ++i)
        {
            ObjectClass* pObj = SelectionAt(i);
            if (!AcceptsOrder(pObj))
                continue;

            TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);
            if (!pTechno->CanSetWaypoint() && !pTechno->Can_Be_Commanded())
                continue;

            const AbstractType kind = pObj->WhatAmI();
            if (kind == AbstractType::Infantry)
            {
                InfantryClass* pInf = static_cast<InfantryClass*>(pObj);
                if (deployed && undeployed && pInf->IsDeployed_Anim())
                    continue;                    // 混编时已部署的不再重复展开
            }
            else if (kind == AbstractType::Unit)
            {
                UnitClass* pUnit = static_cast<UnitClass*>(pObj);
                if (deployed && undeployed && pUnit->IsDeployed())
                    continue;
            }

            if (pTechno->DispatchMission(Mission::Deploy))
                fired = true;
        }
    }

play_voice:
    if (fired)
    {
        RulesClass* pRules = RulesClass::Instance;
        if (pRules != nullptr && pRules->VoiceDeploy >= 0)
            VocClass::PlayIndexAtPos(pRules->VoiceDeploy, nullptr);
    }
}

// ============================================================================
// asm: StopCommandClass_Execute   (asm:1666537)
//
//   n = v_CurrentObjects.ActiveCount;  if (n <= 0) return;
//   ok = true;
//   for (i...) {
//       o = data[i];  if (!o) continue;
//       if ((o->Flags14 & 1) == 0) continue;
//       if (!o->[vt+0A0h]() && !o->[vt+09Ch]()) continue;
//       ok = o->[vt+374h](Mission::Stop);
//       if (!ok) return;          // 任一拒绝 -> 整条命令作废，不播语音
//   }
//   if (!ok) return;
//   VocClass_Play(RulesData[+720h], 1.0f, 0x2000, 0);
// ============================================================================
void StopCommandClass::Execute(uint32 flags)
{
    (void)flags;

    const int32 n = SelectionCount();
    if (n <= 0)
        return;

    bool ok = true;

    for (int32 i = 0; i < n; ++i)
    {
        ObjectClass* pObj = SelectionAt(i);
        if (!AcceptsOrder(pObj))
            continue;

        TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);
        if (!pTechno->CanSetWaypoint() && !pTechno->Can_Be_Commanded())
            continue;

        ok = pTechno->DispatchMission(Mission::Stop);
        if (!ok)
            return;
    }

    if (!ok)
        return;

    RulesClass* pRules = RulesClass::Instance;
    if (pRules != nullptr && pRules->VoiceStop >= 0)
        VocClass::PlayIndexAtPos(pRules->VoiceStop, nullptr);
}
