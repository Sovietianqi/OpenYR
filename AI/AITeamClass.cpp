// =============================================================================
// AITeamClass - AI-driven team instance implementation
//
// AITeamClass is the dynamic counterpart to AITeamTypeClass.  While
// AITeamTypeClass describes the *recipe* for a team (composition, script,
// flags), AITeamClass is the live *instance* that tracks the actual units,
// drives the behaviour script, and manages formation / reinforcement.
//
// Lifecycle:
//   1. Created by AITeamTypeClass::CreateTeam() when the AI decides to form a
//      new team.
//   2. Members are recruited from the owning house's roster or spawned fresh.
//   3. Each frame Update() is called: dead members are pruned, the script is
//      advanced, and formation / regroup logic runs as needed.
//   4. When the script completes the team is deactivated.  Suicide teams
//      destroy all members on completion.
//   5. Disband() releases members and decrements the type's instance count.
//
// Coordinate systems:
//   World coordinates are in leptons (1 cell = 256 leptons).  Formation math
//   uses CoordStruct directly.  Cell coordinates are used for map queries.
// =============================================================================

#include "AITeamClass.h"
#include "AITeamTypeClass.h"
#include "ScriptClass.h"
#include "ScriptTypeClass.h"
#include "TaskForceClass.h"
#include "TagClass.h"
#include "TriggerClass.h"
#include "TeamClass.h"
#include "TeamTypeClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Math/CoordStruct.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

// -----------------------------------------------------------------------------
// Static array pointer - the global list of every active AITeamClass instance.
// -----------------------------------------------------------------------------
DynamicVectorClass<AITeamClass*>* AITeamClass::Array = nullptr;

// =============================================================================
// Local constants and helpers.
// =============================================================================
namespace {
    // Formation radius multiplier (leptons per sqrt(memberCount)).
    constexpr int32 FORMATION_RADIUS_PER_MEMBER = 192;

    // Minimum cell separation between formation slots.
    constexpr int32 MIN_FORMATION_RADIUS = 256;

    // Two-pi constant for circular formation placement.
    constexpr double TWO_PI = 6.28318530717958647692;

    // -------------------------------------------------------------------------
    // IsFootMember - true if the techno can be safely cast to FootClass for
    // movement commands.  Buildings derive from TechnoClass directly.
    // -------------------------------------------------------------------------
    bool IsFootMember(TechnoClass* pTechno) {
        if (!pTechno) return false;
        AbstractType abs = pTechno->WhatAmI();
        return abs == AbstractType::Unit
            || abs == AbstractType::Infantry
            || abs == AbstractType::Aircraft;
    }

    // Forward declaration - defined after the member functions that use it.
    void IssueFormationOrders(AITeamClass* pTeam, const CoordStruct& center);

    // -------------------------------------------------------------------------
    // ComputeMemberThreat - derive a threat rating from a techno's own fields.
    // Used for aggregate team-strength tracking since TechnoClass does not
    // expose its TechnoTypeClass in this build.
    // -------------------------------------------------------------------------
    int32 ComputeMemberThreat(TechnoClass* pTechno) {
        if (!pTechno || pTechno->IsDead()) return 0;
        double base = static_cast<double>(pTechno->MaxHealth) / 100.0;
        if (base < 1.0) base = 1.0;
        double mult = 1.0;
        if (pTechno->VeterancyLevel >= 2) mult = 2.0;
        else if (pTechno->VeterancyLevel == 1) mult = 1.5;
        double healthRatio = 1.0;
        if (pTechno->MaxHealth > 0) {
            healthRatio = static_cast<double>(pTechno->Health) /
                          static_cast<double>(pTechno->MaxHealth);
            if (healthRatio < 0.0) healthRatio = 0.0;
            if (healthRatio > 1.0) healthRatio = 1.0;
        }
        return static_cast<int32>(base * mult * (0.5 + 0.5 * healthRatio) + 0.5);
    }
} // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================
AITeamClass::AITeamClass(AITeamTypeClass* pType, HouseClass* pOwner) noexcept
    : AbstractClass(), TeamType(pType), Owner(pOwner), Script(nullptr),
      CurrentAction(0), Active(true), CurrentScriptLine(0), IsFormed(false),
      CreationFrame(0), TotalThreatValue(0), IsSuspended(false),
      IsReforming(false), NeedsReGrouping(false),
      FormationTarget(0, 0, 0),
      SpawnCell(nullptr), FollowAnchor(nullptr), ITarget(nullptr),
      PrevTarget(nullptr), AnchorCell{0, 0}, TotalObjects(0), totalStrength(0),
      ActionExecuted(false), AchievedSuccess(false),
      IsFullyLoaded(false), IsLeavingMap(false), HasLeftMap(false),
      IsForming(false), WantsToDelete(false), NeedsRecheckState(false),
      NeedsToDisappear(false), JustDisappeared(false),
      ReassignTarget(false), RaiseTagEvent(false) {
    for (int32 i = 0; i < MaxTaskForceSlots; ++i) {
        TeamCounts[i] = 0;
    }
    GuardAreaTimer.Start(0);
    SuspendTimer.Start(0);
    if (pType && pType->ScriptType) {
        Script = new ScriptClass(pType->ScriptType);
    }
    if (Game::CurrentFrame > 0) {
        CreationFrame = Game::CurrentFrame;
    }
}

// =============================================================================
// Destructor - release the script and clear the member roster.
// =============================================================================
AITeamClass::~AITeamClass() {
    if (Script) {
        delete Script;
        Script = nullptr;
    }
    Liberate();
    Members.Clear();
    SpawnCell = nullptr;
    FollowAnchor = nullptr;
    ITarget = nullptr;
    PrevTarget = nullptr;
}

// =============================================================================
// GetClassID - return a fixed CLSID for persistence.
// =============================================================================
HRESULT AITeamClass::GetClassID(CLSID* pClassID) {
    if (pClassID) {
        pClassID->Data1 = 0xC3C3C3C3;
        for (int32 i = 0; i < 8; ++i) pClassID->Data4[i] = 0;
        return S_OK;
    }
    return E_POINTER;
}

// =============================================================================
// Load / Save - stream persistence (actual binary serialisation is handled by
// the save-game subsystem).
// =============================================================================
HRESULT AITeamClass::Load(IStream* pStm) {
    if (!pStm) return E_POINTER;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read team type ID and resolve.
    char teamTypeID[0x18];
    hr = pStm->Read(teamTypeID, sizeof(teamTypeID), &read);
    if (hr < 0 || read != sizeof(teamTypeID)) return E_FAIL;
    teamTypeID[sizeof(teamTypeID) - 1] = '\0';
    TeamType = teamTypeID[0] ? AITeamTypeClass::Find(teamTypeID) : nullptr;

    // Read owning house index.
    int32 ownerIdx = -1;
    hr = pStm->Read(&ownerIdx, sizeof(ownerIdx), &read);
    if (hr < 0 || read != sizeof(ownerIdx)) return E_FAIL;
    Owner = (ownerIdx >= 0) ? HouseClass::GetHouseByIndex(ownerIdx) : nullptr;

    // Read script state.  The script is an owned runtime object: we
    // recreate it from its ScriptType and restore the progress fields.
    int32 hasScript = 0;
    hr = pStm->Read(&hasScript, sizeof(hasScript), &read);
    if (hr < 0 || read != sizeof(hasScript)) return E_FAIL;
    if (Script) { delete Script; Script = nullptr; }
    if (hasScript != 0) {
        char scriptTypeID[0x18];
        hr = pStm->Read(scriptTypeID, sizeof(scriptTypeID), &read);
        if (hr < 0 || read != sizeof(scriptTypeID)) return E_FAIL;
        scriptTypeID[sizeof(scriptTypeID) - 1] = '\0';

        int32 sCurrentAction = 0, sCurrentLine = 0, sWaitTimer = 0;
        int32 sLoopCount = 0, sRepeatCount = 0, sActionTimer = 0;
        int32 sFailedAttempts = 0, sLastExecutedFrame = 0;
        uint32 sFlags = 0;
        hr = pStm->Read(&sCurrentAction, sizeof(sCurrentAction), &read);
        if (hr < 0 || read != sizeof(sCurrentAction)) return E_FAIL;
        hr = pStm->Read(&sCurrentLine, sizeof(sCurrentLine), &read);
        if (hr < 0 || read != sizeof(sCurrentLine)) return E_FAIL;
        hr = pStm->Read(&sFlags, sizeof(sFlags), &read);
        if (hr < 0 || read != sizeof(sFlags)) return E_FAIL;
        hr = pStm->Read(&sWaitTimer, sizeof(sWaitTimer), &read);
        if (hr < 0 || read != sizeof(sWaitTimer)) return E_FAIL;
        hr = pStm->Read(&sLoopCount, sizeof(sLoopCount), &read);
        if (hr < 0 || read != sizeof(sLoopCount)) return E_FAIL;
        hr = pStm->Read(&sRepeatCount, sizeof(sRepeatCount), &read);
        if (hr < 0 || read != sizeof(sRepeatCount)) return E_FAIL;
        hr = pStm->Read(&sActionTimer, sizeof(sActionTimer), &read);
        if (hr < 0 || read != sizeof(sActionTimer)) return E_FAIL;
        hr = pStm->Read(&sFailedAttempts, sizeof(sFailedAttempts), &read);
        if (hr < 0 || read != sizeof(sFailedAttempts)) return E_FAIL;
        hr = pStm->Read(&sLastExecutedFrame, sizeof(sLastExecutedFrame), &read);
        if (hr < 0 || read != sizeof(sLastExecutedFrame)) return E_FAIL;

        ScriptTypeClass* pScriptType = scriptTypeID[0] ? ScriptTypeClass::Find(scriptTypeID) : nullptr;
        if (pScriptType) {
            Script = new ScriptClass(pScriptType);
            Script->CurrentAction = sCurrentAction;
            Script->CurrentLine = sCurrentLine;
            Script->IsScriptActive = (sFlags & 0x01) != 0;
            Script->IsScriptComplete = (sFlags & 0x02) != 0;
            Script->WaitTimer = sWaitTimer;
            Script->LoopCount = sLoopCount;
            Script->RepeatCount = sRepeatCount;
            Script->ActionTimer = sActionTimer;
            Script->FailedAttempts = sFailedAttempts;
            Script->LastExecutedFrame = sLastExecutedFrame;
        }
    }

    // Read member roster (indices into the global AbstractClass array).
    int32 memberCount = 0;
    hr = pStm->Read(&memberCount, sizeof(memberCount), &read);
    if (hr < 0 || read != sizeof(memberCount)) return E_FAIL;
    if (memberCount < 0) memberCount = 0;
    Members.Clear();
    for (int32 i = 0; i < memberCount; ++i) {
        int32 memberIdx = -1;
        hr = pStm->Read(&memberIdx, sizeof(memberIdx), &read);
        if (hr < 0 || read != sizeof(memberIdx)) return E_FAIL;
        if (memberIdx >= 0) {
            AbstractClass* pAbs = AbstractClass::Get_Instance(memberIdx);
            if (pAbs) Members.Add(static_cast<TechnoClass*>(pAbs));
        }
    }

    // Read scalar state.
    hr = pStm->Read(&CurrentAction, sizeof(CurrentAction), &read);
    if (hr < 0 || read != sizeof(CurrentAction)) return E_FAIL;

    uint32 flags = 0;
    hr = pStm->Read(&flags, sizeof(flags), &read);
    if (hr < 0 || read != sizeof(flags)) return E_FAIL;
    Active          = (flags & 0x01) != 0;
    IsFormed        = (flags & 0x02) != 0;
    IsSuspended     = (flags & 0x04) != 0;
    IsReforming     = (flags & 0x08) != 0;
    NeedsReGrouping = (flags & 0x10) != 0;

    hr = pStm->Read(&CurrentScriptLine, sizeof(CurrentScriptLine), &read);
    if (hr < 0 || read != sizeof(CurrentScriptLine)) return E_FAIL;
    hr = pStm->Read(&CreationFrame, sizeof(CreationFrame), &read);
    if (hr < 0 || read != sizeof(CreationFrame)) return E_FAIL;
    hr = pStm->Read(&TotalThreatValue, sizeof(TotalThreatValue), &read);
    if (hr < 0 || read != sizeof(TotalThreatValue)) return E_FAIL;

    // Read timers.
    hr = pStm->Read(&GuardAreaTimer.StartTime, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;
    hr = pStm->Read(&GuardAreaTimer.TimeLeft, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;
    hr = pStm->Read(&SuspendTimer.StartTime, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;
    hr = pStm->Read(&SuspendTimer.TimeLeft, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;

    // Read formation target.
    hr = pStm->Read(&FormationTarget.X, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;
    hr = pStm->Read(&FormationTarget.Y, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;
    hr = pStm->Read(&FormationTarget.Z, sizeof(int32), &read);
    if (hr < 0 || read != sizeof(int32)) return E_FAIL;

    return S_OK;
}

HRESULT AITeamClass::Save(IStream* pStm, BOOL fClearDirty) {
    if (!pStm) return E_POINTER;

    ULONG written = 0;
    HRESULT hr = S_OK;

    // Write team type ID.
    char teamTypeID[0x18];
    std::memset(teamTypeID, 0, sizeof(teamTypeID));
    if (TeamType && TeamType->ID) {
        int32 j = 0;
        while (TeamType->ID[j] && j < static_cast<int32>(sizeof(teamTypeID)) - 1) {
            teamTypeID[j] = TeamType->ID[j]; ++j;
        }
    }
    hr = pStm->Write(teamTypeID, sizeof(teamTypeID), &written);
    if (hr < 0 || written != sizeof(teamTypeID)) return E_FAIL;

    // Write owning house index.
    int32 ownerIdx = Owner ? Owner->ArrayIndex : -1;
    hr = pStm->Write(&ownerIdx, sizeof(ownerIdx), &written);
    if (hr < 0 || written != sizeof(ownerIdx)) return E_FAIL;

    // Write script state.
    int32 hasScript = Script ? 1 : 0;
    hr = pStm->Write(&hasScript, sizeof(hasScript), &written);
    if (hr < 0 || written != sizeof(hasScript)) return E_FAIL;
    if (Script) {
        char scriptTypeID[0x18];
        std::memset(scriptTypeID, 0, sizeof(scriptTypeID));
        if (Script->ScriptType && Script->ScriptType->ID) {
            int32 j = 0;
            while (Script->ScriptType->ID[j] && j < static_cast<int32>(sizeof(scriptTypeID)) - 1) {
                scriptTypeID[j] = Script->ScriptType->ID[j]; ++j;
            }
        }
        hr = pStm->Write(scriptTypeID, sizeof(scriptTypeID), &written);
        if (hr < 0 || written != sizeof(scriptTypeID)) return E_FAIL;

        int32 sCurrentAction = Script->CurrentAction;
        hr = pStm->Write(&sCurrentAction, sizeof(sCurrentAction), &written);
        if (hr < 0 || written != sizeof(sCurrentAction)) return E_FAIL;
        int32 sCurrentLine = Script->CurrentLine;
        hr = pStm->Write(&sCurrentLine, sizeof(sCurrentLine), &written);
        if (hr < 0 || written != sizeof(sCurrentLine)) return E_FAIL;
        uint32 sFlags = 0;
        if (Script->IsScriptActive) sFlags |= 0x01;
        if (Script->IsScriptComplete) sFlags |= 0x02;
        hr = pStm->Write(&sFlags, sizeof(sFlags), &written);
        if (hr < 0 || written != sizeof(sFlags)) return E_FAIL;
        int32 sWaitTimer = Script->WaitTimer;
        hr = pStm->Write(&sWaitTimer, sizeof(sWaitTimer), &written);
        if (hr < 0 || written != sizeof(sWaitTimer)) return E_FAIL;
        int32 sLoopCount = Script->LoopCount;
        hr = pStm->Write(&sLoopCount, sizeof(sLoopCount), &written);
        if (hr < 0 || written != sizeof(sLoopCount)) return E_FAIL;
        int32 sRepeatCount = Script->RepeatCount;
        hr = pStm->Write(&sRepeatCount, sizeof(sRepeatCount), &written);
        if (hr < 0 || written != sizeof(sRepeatCount)) return E_FAIL;
        int32 sActionTimer = Script->ActionTimer;
        hr = pStm->Write(&sActionTimer, sizeof(sActionTimer), &written);
        if (hr < 0 || written != sizeof(sActionTimer)) return E_FAIL;
        int32 sFailedAttempts = Script->FailedAttempts;
        hr = pStm->Write(&sFailedAttempts, sizeof(sFailedAttempts), &written);
        if (hr < 0 || written != sizeof(sFailedAttempts)) return E_FAIL;
        int32 sLastExecutedFrame = Script->LastExecutedFrame;
        hr = pStm->Write(&sLastExecutedFrame, sizeof(sLastExecutedFrame), &written);
        if (hr < 0 || written != sizeof(sLastExecutedFrame)) return E_FAIL;
    }

    // Write member roster.
    int32 memberCount = Members.Count;
    hr = pStm->Write(&memberCount, sizeof(memberCount), &written);
    if (hr < 0 || written != sizeof(memberCount)) return E_FAIL;
    for (int32 i = 0; i < Members.Count; ++i) {
        int32 memberIdx = Members[i] ? AbstractClass::Find_Index(Members[i]) : -1;
        hr = pStm->Write(&memberIdx, sizeof(memberIdx), &written);
        if (hr < 0 || written != sizeof(memberIdx)) return E_FAIL;
    }

    // Write scalar state.
    hr = pStm->Write(&CurrentAction, sizeof(CurrentAction), &written);
    if (hr < 0 || written != sizeof(CurrentAction)) return E_FAIL;

    uint32 flags = 0;
    if (Active) flags |= 0x01;
    if (IsFormed) flags |= 0x02;
    if (IsSuspended) flags |= 0x04;
    if (IsReforming) flags |= 0x08;
    if (NeedsReGrouping) flags |= 0x10;
    hr = pStm->Write(&flags, sizeof(flags), &written);
    if (hr < 0 || written != sizeof(flags)) return E_FAIL;

    hr = pStm->Write(&CurrentScriptLine, sizeof(CurrentScriptLine), &written);
    if (hr < 0 || written != sizeof(CurrentScriptLine)) return E_FAIL;
    hr = pStm->Write(&CreationFrame, sizeof(CreationFrame), &written);
    if (hr < 0 || written != sizeof(CreationFrame)) return E_FAIL;
    hr = pStm->Write(&TotalThreatValue, sizeof(TotalThreatValue), &written);
    if (hr < 0 || written != sizeof(TotalThreatValue)) return E_FAIL;

    // Write timers.
    hr = pStm->Write(&GuardAreaTimer.StartTime, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;
    hr = pStm->Write(&GuardAreaTimer.TimeLeft, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;
    hr = pStm->Write(&SuspendTimer.StartTime, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;
    hr = pStm->Write(&SuspendTimer.TimeLeft, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;

    // Write formation target.
    hr = pStm->Write(&FormationTarget.X, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;
    hr = pStm->Write(&FormationTarget.Y, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;
    hr = pStm->Write(&FormationTarget.Z, sizeof(int32), &written);
    if (hr < 0 || written != sizeof(int32)) return E_FAIL;

    if (fClearDirty) {
        Dirty = false;
    }
    return S_OK;
}

// =============================================================================
// WhatAmI / Size
// =============================================================================
AbstractType AITeamClass::WhatAmI() const {
    return AbstractType::AITrigger;
}

int32 AITeamClass::Size() const {
    return sizeof(AITeamClass);
}

// =============================================================================
// PointerGotInvalid - drop any reference to an object that is being removed.
//
// asm: TeamClass_PointerGotInvalid (0x6EAE60) retn 8 —— 逐个比较本队持有的
// 指针字段，命中即将失效的对象就把该字段清零。
// =============================================================================
void AITeamClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed) {
    (void)removed;
    if (!pInvalid) return;

    if (static_cast<AbstractClass*>(TeamType) == pInvalid) TeamType = nullptr;
    if (static_cast<AbstractClass*>(Owner) == pInvalid) Owner = nullptr;
    if (reinterpret_cast<AbstractClass*>(Script) == pInvalid) Script = nullptr;

    // Also prune the member that is going away.
    for (int32 i = Members.Count - 1; i >= 0; --i) {
        if (static_cast<AbstractClass*>(Members[i]) == pInvalid) {
            Members.Remove(i);
        }
    }
}

// =============================================================================
// Update - per-frame AI team tick (asm: TeamClass_Update, asm:1527680).
//
// 原版把人链/脚本/征兵全写在一个 1000+ 行的 TeamClass_Update 里。此处按 asm 的
// 控制流逐段复刻（派发下沉到 ScriptClass::Execute）：
//   1. 挂起：TeamSuspended 时按 Frame - SuspendFrame >= SuspendDuration 唤醒，
//      未到期直接返回（asm @@1-@@4）。
//   2. 消失：NeedsToDisappear 置位就调 LeaveMap()，返回假则本队整体退场、本帧结束。
//   3. 编队/动员（asm @@5-@@12）：IsForming 已置位且正离场时先跑一次集结；
//      缺员或要删除时拉起 IsForming/IsFullyLoaded、清 IsLeavingMap、给每个成员
//      打上队长跟随标记并复位脚本首行。
//   4. 跟随锚点（asm @@13-@@14）：FindUnitToFollow()。
//   5. 补员扫描（asm @@15-@@19）：逐个 task-force 坑位，缺口为正就 SearchForRecruit。
//   6. 成功事件（asm @@21-@@23）：RaiseTagEvent 置位时向所有 Tag 抛一次事件。
//   7. 脚本推进（asm @@27-@@34）：ActionExecuted 置位则 StepToNextLine 并清目标；
//      ReassignTarget 置位则重下发目标。随后 ScriptClass::Execute() 派发当前行。
//   8. 收尾（asm @@35+）：自杀队脚本结束即整体消失；各计时器递减。
// =============================================================================
void AITeamClass::Update() {
    if (!Active) return;

    const int32 frame = Game::CurrentFrame;

    // ---- 1. 挂起 (asm @@1-@@4) --------------------------------------------
    if (IsSuspended) {
        bool wake = false;
        SuspendTimer.Update();
        if (SuspendTimer.Expired()) {
            // 计时器单一路径：到期即醒（原版用 Frame-SuspendFrame 比较，
            // OpenYR 用 CDTimerClass 承担同一语义）。
            wake = true;
        }
        if (wake) {
            IsSuspended = false;
        } else {
            return;
        }
    }
    (void)frame;

    // ---- 2. 消失流程 (asm @@4) --------------------------------------------
    if (NeedsToDisappear) {
        if (!LeaveMap()) {
            return;
        }
    }

    GuardAreaTimer.Update();
    CleanupDeadMembers();

    // ---- 3. 编队 / 动员 (asm @@5-@@12) ------------------------------------
    if (IsForming && IsLeavingMap) {
        // 队伍一边编队一边要离场：先跑一次集结（原 sub_6EA0D0）。
        ReGroup();
        FindUnitToFollow();
    }

    if (!IsForming && (!IsFullyLoaded || WantsToDelete)) {
        IsForming    = true;
        IsLeavingMap = false;
        HasLeftMap   = true;   // asm @@8: field_78 = 1

        // 给成员链上每个单位打上"跟随队长"标记（原 Field_689）。
        for (int32 i = 0; i < Members.Count; ++i) {
            TechnoClass* pTechno = Members[i];
            if (!pTechno || pTechno->IsDead()) continue;
            if (!IsFootMember(pTechno)) continue;
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            pFoot->IsTeamLeader = (i == 0) || HasLeftMap;
        }

        if (Script) {
            Script->ResetCurrentLine();
        }
        ActionExecuted = true;
    }

    // ---- 4. 跟随锚点 (asm @@13-@@14) --------------------------------------
    //  asm: NeedsRecheckState(+0x7B) || IsForming(+0x7F) || spawnCell==0 ||
    //       anchor(+0x38)==0 时才重新解算跟随锚点。
    if (NeedsRecheckState || IsForming
        || SpawnCell == nullptr || FollowAnchor == nullptr) {
        FindUnitToFollow();
    }

    // ---- 5. 补员扫描 (asm @@15-@@19) --------------------------------------
    //  asm 的门：`!IsForming || (IsFullyLoaded && Reinforce)` 才补员；owner 是
    //  人类玩家且已离场 (field_78) 时跳过。逐个 task-force 坑位招人。
    const bool reinforce = (TeamType != nullptr) && TeamType->Reinforce;
    bool doRecruit = !IsForming || (IsFullyLoaded && reinforce);
    if (doRecruit && Owner != nullptr && Owner->IsHumanPlayer && HasLeftMap) {
        doRecruit = false;
    }
    if (doRecruit && TeamType != nullptr && TeamType->TaskForce != nullptr) {
        TaskForceClass* pTask = TeamType->TaskForce;
        const int32 slots = pTask->GetMemberCount();
        for (int32 idx = 0; idx < slots && idx < MaxTaskForceSlots; ++idx) {
            TaskForceMember* pMember = pTask->GetMember(idx);
            if (pMember == nullptr) continue;
            if (TeamCounts[idx] < pMember->Count) {
                SearchForRecruit(idx);
            }
        }
    }

    // ---- 6. 成功事件 (asm @@21-@@23) --------------------------------------
    if (AchievedSuccess && TagClass::Array != nullptr) {
        const int32 tagCount = TagClass::Array->GetCount();
        for (int32 i = tagCount - 1; i >= 0; --i) {
            TagClass* pTag = (*TagClass::Array)[i];
            if (pTag == nullptr) continue;
            const int32 trigCount = pTag->TriggerList.GetCount();
            for (int32 t = trigCount - 1; t >= 0; --t) {
                TriggerClass* pTrig = pTag->TriggerList[t];
                if (pTrig == nullptr) continue;
                pTrig->Spring(TriggerEventType::AnyEvent, nullptr, CellStruct());
                break;
            }
        }
    }

    // 队员已被清空则本帧到此为止，交给下一帧的消失流程。
    if (Members.Count == 0) {
        if (!WantsToDelete) {
            NeedsToDisappear = true;
        }
        return;
    }

    // ---- 7. 脚本推进 (asm @@27-@@34) --------------------------------------
    if (ActionExecuted) {
        ActionExecuted = false;
        if (Script) {
            Script->StepToNextLine();
        }
        AssignMissionTarget(nullptr);
        PrevTarget = nullptr;
    } else if (ReassignTarget) {
        ReassignTarget = false;
        AssignMissionTarget(nullptr);
        PrevTarget = nullptr;
    } else if (PrevTarget == nullptr) {
        PrevTarget = ITarget;
    }

    // 交给脚本 VM 派发当前行（原版 65 分支 switch 的现代实现）。
    if (Script) {
        Script->Execute();
    }

    // ---- 8. 收尾 (asm @@35+) ----------------------------------------------
    if (NeedsToDisappear) {
        LeaveMap();
        return;
    }

    if (Script && Script->IsComplete() && TeamType && TeamType->Suicide) {
        NeedsToDisappear = true;
    }
}

// =============================================================================
// UpdateAll - drive every live AITeamClass (asm LogicClass_Update vec_Teams 循环).
// =============================================================================
void AITeamClass::UpdateAll() {
    if (AITeamClass::Array == nullptr) return;
    for (int32 i = 0; i < AITeamClass::Array->Count; ++i) {
        AITeamClass* pTeam = (*AITeamClass::Array)[i];
        if (pTeam != nullptr) {
            pTeam->Update();
        }
    }
}

// =============================================================================
// AI - the "post-script"每帧动作（asm TeamClass_Update 脚本之后那段）。
// 单列出以便调用方在不跑整段 Update 时也能驱动脚本与目标下发。
// =============================================================================
void AITeamClass::AI() {
    if (!Active) return;

    CleanupDeadMembers();

    if (ActionExecuted) {
        ActionExecuted = false;
        if (Script) Script->StepToNextLine();
        AssignMissionTarget(nullptr);
        PrevTarget = nullptr;
    } else if (PrevTarget == nullptr) {
        PrevTarget = ITarget;
    }

    if (Script) Script->Execute();
}

// =============================================================================
// CleanupDeadMembers - sweep the roster and drop null or dead entries.
// =============================================================================
void AITeamClass::CleanupDeadMembers() {
    for (int32 i = Members.Count - 1; i >= 0; --i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) {
            Members.Remove(i);
        }
    }
}

// =============================================================================
// AddMember - enrol a techno into the team.  Rejects duplicates and full teams.
// =============================================================================
bool AITeamClass::AddMember(TechnoClass* pTechno) {
    if (!pTechno) return false;
    if (IsTeamFull()) return false;

    // Reject duplicates.
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] == pTechno) return false;
    }

    if (!Members.Add(pTechno)) return false;

    // Apply veterancy bonus from the team type if configured.
    if (TeamType && TeamType->VeteranLevel > 0) {
        if (pTechno->VeterancyLevel < TeamType->VeteranLevel) {
            pTechno->VeterancyLevel = TeamType->VeteranLevel;
        }
    }

    if (!IsFormed) {
        IsFormed = true;
    }
    return true;
}

// =============================================================================
// RemoveMember - drop a specific techno from the roster.
// =============================================================================
bool AITeamClass::RemoveMember(TechnoClass* pTechno) {
    if (!pTechno) return false;

    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] == pTechno) {
            Members.Remove(i);
            if (Members.Count == 0) {
                IsFormed = false;
            }
            return true;
        }
    }
    return false;
}

// =============================================================================
// FindUnitToFollow - determine the team's follow anchor and the cell it sits
// on, writing both to the caller's out-parameters.
//
// asm: char __thiscall AITeamClass_FindUnitToFollow(AITeamClass* this,
//      CellStruct* spawnCell, FootClass** pAnchor)  (0x6EB7xx) retn 8
//
// When the current script line is the "move to waypoint" action (index 0x0A)
// the team picks the living, allied, non-self unit nearest its current anchor
// (scanning the global unit / infantry lists).  Otherwise it averages the
// positions of its own members to derive a rally point.  Either way the
// resulting map cell is written to *spawnCell and the chosen unit (may be
// null) to *pAnchor.
// =============================================================================
bool AITeamClass::FindUnitToFollow(CellStruct* spawnCell, FootClass** pAnchor) {
    if (spawnCell) { spawnCell->X = 0; spawnCell->Y = 0; }
    if (pAnchor) *pAnchor = nullptr;

    // Rally-point branch: average the living members' positions and convert
    // the centroid to a cell.  This mirrors the non-0x0A branch of the asm.
    int32 sumX = 0, sumY = 0, sumZ = 0, count = 0;
    TechnoClass* pAnchorUnit = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;

        CoordStruct pos = pTechno->GetCoords();
        sumX += pos.X;
        sumY += pos.Y;
        sumZ += pos.Z;
        ++count;

        // Nearest-to-anchor selection (anchor = current formation target).
        int32 dx = pos.X - FormationTarget.X;
        int32 dy = pos.Y - FormationTarget.Y;
        int32 d = dx * dx + dy * dy;
        if (pAnchorUnit == nullptr || d < bestDist) {
            bestDist = d;
            pAnchorUnit = pTechno;
        }
    }

    if (count == 0) return false;

    CoordStruct centroid(sumX / count, sumY / count, sumZ / count);
    CellStruct cell = Math::CoordToCell(centroid);
    if (spawnCell) *spawnCell = cell;

    if (pAnchor) {
        *pAnchor = IsFootMember(pAnchorUnit)
                 ? static_cast<FootClass*>(pAnchorUnit) : nullptr;
    }
    return pAnchorUnit != nullptr;
}

// =============================================================================
// FindUnitToFollow (no-arg overload) —— 就地刷新本队的跟随锚点。
//
// asm: TeamClass_Update @@14 / RecruitUnit 里 `call AITeamClass_FindUnitToFollow`
//  的调用者，结果直接落在队伍的 spawnCell(+0x34) / field_38(+0x38) 两个字段上。
//  这里把 2 参版本的产物写回 FollowAnchor / AnchorCell。
// =============================================================================
void AITeamClass::FindUnitToFollow() {
    CellStruct cell{};
    FootClass* pAnchor = nullptr;
    if (FindUnitToFollow(&cell, &pAnchor)) {
        if (pAnchor != nullptr) {
            FollowAnchor = static_cast<AbstractClass*>(pAnchor);
        }
    }
    AnchorCell = cell;
}

// =============================================================================
// ExecuteScript - advance the behaviour script one step.  When the script
// completes, the team is deactivated (and suicide teams self-destruct).
// =============================================================================
void AITeamClass::ExecuteScript() {
    if (!Script) return;
    if (!Active) return;

    Script->Execute();

    if (Script->IsComplete()) {
        // asm: TeamClass_AchieveSuccess (0x6F3096) 只置 actionExecuted/AchievedSuccess
        // 两个脚本标记；队伍成功与否由脚本动作本身决定，这里不额外累加类型计数。
        Active = false;

        if (TeamType && TeamType->Suicide) {
            DestroyAllMembers();
        }
    }
}

// =============================================================================
// DestroyAllMembers - kill every member (used by suicide teams on completion).
// =============================================================================
void AITeamClass::DestroyAllMembers() {
    for (int32 i = Members.Count - 1; i >= 0; --i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead()) {
            pTechno->Destroyed(nullptr);
        }
    }
    Members.Clear();
    IsFormed = false;
}

// =============================================================================
// IsTeamFull - true if the roster has reached the team type's Max count.
// =============================================================================
bool AITeamClass::IsTeamFull() const {
    if (!TeamType) return true;
    if (TeamType->Max <= 0) return false;
    return Members.Count >= TeamType->Max;
}

// =============================================================================
// Suspend - pause team AI for a number of frames.
// =============================================================================
void AITeamClass::Suspend(int32 duration) {
    IsSuspended = true;
    SuspendTimer.Start(duration);
}

// =============================================================================
// Resume - immediately clear the suspended state.
// =============================================================================
void AITeamClass::Resume() {
    IsSuspended = false;
    SuspendTimer.Start(0);
}

// =============================================================================
// ReGroup - recompute the formation centre and issue Move_To commands to
// scatter members around it.  Sets the IsReforming flag so Update() continues
// the formation move on subsequent frames.
// =============================================================================
void AITeamClass::ReGroup() {
    if (Members.Count <= 1) {
        NeedsReGrouping = false;
        IsReforming = false;
        return;
    }

    FormationTarget = ComputeFormationCenter();
    IssueFormationOrders(this, FormationTarget);

    IsReforming = false;
    NeedsReGrouping = false;
}

// =============================================================================
// FormUp - continue a formation move.  We check whether all foot members have
// arrived near their formation slots; if so, the reform is complete.
// =============================================================================
void AITeamClass::FormUp() {
    if (Members.Count <= 1) {
        IsReforming = false;
        return;
    }

    // Check if members are close enough to the formation target.
    int32 arrivedCount = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        CoordStruct pos = pTechno->GetCoords();
        int32 dx = pos.X - FormationTarget.X;
        int32 dy = pos.Y - FormationTarget.Y;
        int32 distSq = dx * dx + dy * dy;
        // Consider arrived if within 1.5 cells (384 leptons).
        if (distSq < 384 * 384) {
            ++arrivedCount;
        }
    }

    // If at least two-thirds of members have arrived, finish forming up.
    int32 threshold = (Members.Count * 2) / 3;
    if (threshold < 1) threshold = 1;
    if (arrivedCount >= threshold) {
        IsReforming = false;
    } else {
        // Re-issue formation orders to stragglers.
        IssueFormationOrders(this, FormationTarget);
    }
}

// =============================================================================
// IssueFormationOrders (file-local) - commands each foot member of the team
// to move to a slot in a circular formation around the given centre.
// =============================================================================
namespace {
void IssueFormationOrders(AITeamClass* pTeam, const CoordStruct& center) {
    if (!pTeam) return;
    DynamicVectorClass<TechnoClass*>& Members = pTeam->Members;

    int32 livingCount = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] && !Members[i]->IsDead()) ++livingCount;
    }
    if (livingCount == 0) return;

    int32 radius = static_cast<int32>(
        std::sqrt(static_cast<double>(livingCount)) *
        static_cast<double>(FORMATION_RADIUS_PER_MEMBER));
    if (radius < MIN_FORMATION_RADIUS) radius = MIN_FORMATION_RADIUS;

    int32 slot = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;

        if (IsFootMember(pTechno)) {
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            double angle = (static_cast<double>(slot) * TWO_PI) /
                           static_cast<double>(livingCount);
            int32 offX = static_cast<int32>(std::cos(angle) *
                                            static_cast<double>(radius));
            int32 offY = static_cast<int32>(std::sin(angle) *
                                            static_cast<double>(radius));
            CoordStruct slotPos(center.X + offX, center.Y + offY, center.Z);
            pFoot->Move_To(slotPos);
        }
        ++slot;
    }
}
} // anonymous namespace

// =============================================================================
// ComputeFormationCenter - arithmetic mean of all living member positions.
// =============================================================================
CoordStruct AITeamClass::ComputeFormationCenter() const {
    if (Members.Count == 0) return CoordStruct(0, 0, 0);

    int32 totalX = 0, totalY = 0, totalZ = 0;
    int32 living = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        CoordStruct pos = pTechno->GetCoords();
        totalX += pos.X;
        totalY += pos.Y;
        totalZ += pos.Z;
        ++living;
    }
    if (living == 0) return CoordStruct(0, 0, 0);
    return CoordStruct(totalX / living, totalY / living, totalZ / living);
}

// =============================================================================
// GetTotalStrength - sum of member threat values.  Provides a more nuanced
// strength metric than a simple headcount.
// =============================================================================
int32 AITeamClass::GetTotalStrength() const {
    int32 strength = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead()) {
            strength += ComputeMemberThreat(pTechno);
        }
    }
    return strength;
}

// =============================================================================
// GetActiveMemberCount - count living members on the roster.
// =============================================================================
int32 AITeamClass::GetActiveMemberCount() const {
    int32 count = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] && !Members[i]->IsDead()) {
            ++count;
        }
    }
    return count;
}

// =============================================================================
// IsTeamCombatReady - true if the team has at least one living, armed member.
// =============================================================================
bool AITeamClass::IsTeamCombatReady() const {
    if (Members.Count == 0) return false;
    int32 activeCount = GetActiveMemberCount();
    if (activeCount == 0) return false;

    // Require at least one member with non-trivial max health (armed unit).
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead() && pTechno->MaxHealth > 0) {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Disband - release all members and deactivate the team.
// =============================================================================
void AITeamClass::Disband() {
    // Stop all foot members before releasing them.
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead() && IsFootMember(pTechno)) {
            static_cast<FootClass*>(pTechno)->Stop_Moving();
        }
    }
    Liberate();
    Active = false;
    IsFormed = false;
    IsReforming = false;
    NeedsReGrouping = false;
    CurrentScriptLine = 0;
    TotalThreatValue = 0;

    if (TeamType) {
        if (TeamType->cntInstances > 0) {
            --TeamType->cntInstances;
        }
    }
}

// =============================================================================
// HandleMemberDeath - remove the dead member and request a regroup if the
// team is still viable.
// =============================================================================
void AITeamClass::HandleMemberDeath(TechnoClass* pTechno) {
    if (!pTechno) return;
    RemoveMember(pTechno);

    if (Members.Count == 0) {
        if (TeamType && !TeamType->Autocreate) {
            Active = false;
        }
        NeedsReGrouping = false;
        return;
    }

    // Request a regroup so survivors close ranks.
    NeedsReGrouping = true;
}

// =============================================================================
// AssignMissionToAll - set the team-wide mission and apply immediate effects.
//
// Because TechnoClass does not derive from MissionClass in this codebase, we
// translate the mission enum into concrete Move_To / Fire / Stop_Moving calls
// on the FootClass members.
// =============================================================================
void AITeamClass::AssignMissionToAll(Mission mission) {
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;

        bool isFoot = IsFootMember(pTechno);
        FootClass* pFoot = isFoot ? static_cast<FootClass*>(pTechno) : nullptr;

        switch (mission) {
            case Mission::Move:
            case Mission::Patrol:
            case Mission::Return:
            case Mission::Retreat:
            case Mission::Enter:
                // Movement missions: re-issue the current destination if idle.
                if (pFoot && !pFoot->Is_Moving()) {
                    CoordStruct dest = pFoot->Get_Destination();
                    pFoot->Move_To(dest);
                }
                break;

            case Mission::Attack:
            case Mission::Hunt:
                // Combat missions are handled via AssignTargetToAll.
                break;

            case Mission::Guard:
            case Mission::AreaGuard:
                // Guard missions: hold position.
                if (pFoot && pFoot->Is_Moving()) {
                    pFoot->Stop_Moving();
                }
                break;

            case Mission::Stop:
            case Mission::Sleep:
            case Mission::Wait:
                if (pFoot) {
                    pFoot->Stop_Moving();
                }
                break;

            case Mission::Unload:
                // Stop to allow cargo disembarkation.
                if (pFoot) {
                    pFoot->Stop_Moving();
                }
                break;

            case Mission::Harvest:
                // Harvesters continue autonomously.
                break;

            default:
                break;
        }
    }
}

// =============================================================================
// AssignTargetToAll - set a shared target and, if the team is in an attack
// posture, issue fire commands to armed members.
// =============================================================================
void AITeamClass::AssignTargetToAll(AbstractClass* pTarget) {
    if (!pTarget) return;

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        // Issue a fire command with weapon index 0 (primary weapon).
        if (pTechno->MaxHealth > 0) {
            pTechno->Fire(pTarget, 0);
        }
    }
}

// =============================================================================
// GetTeamCenter - alias for ComputeFormationCenter.
// =============================================================================
CoordStruct AITeamClass::GetTeamCenter() const {
    return ComputeFormationCenter();
}

// =============================================================================
// GetTeamCell - convert the team centre to a cell coordinate.
// =============================================================================
CellStruct AITeamClass::GetTeamCell() const {
    CoordStruct center = GetTeamCenter();
    return Math::CoordToCell(center);
}

// =============================================================================
// CanRecruit - true if the team type allows recruitment and the team is active
// and not suspended.
// =============================================================================
bool AITeamClass::CanRecruit() const {
    if (!TeamType) return false;
    if (!Active) return false;
    if (IsSuspended) return false;
    if (IsTeamFull()) return false;
    return TeamType->Recruiter || TeamType->LooseRecruit || TeamType->Autocreate;
}

// =============================================================================
// ReinforceTeam - recruit up to nUnits idle units from the owning house's
// roster into this team.
//
// The team type's composition is described by its TaskForceClass: the roster
// of (TechnoTypeClass*, count) entries parsed from [TaskForces].  We scan the
// owning house's owned-object list for death-free foot units that are not
// already members and enrol them until the requested count is reached.
// =============================================================================
bool AITeamClass::ReinforceTeam(int32 nUnits) {
    if (!TeamType || !Owner) return false;
    if (nUnits <= 0) return false;
    if (IsTeamFull()) return false;

    TaskForceClass* pTaskForce = TeamType->TaskForce;
    if (!pTaskForce) return false;

    int32 added = 0;
    int32 tfMemberCount = pTaskForce->GetMemberCount();

    DynamicVectorClass<TechnoClass*>* pRoster = &Owner->AllOwnedObjects;
    for (int32 i = 0; i < pRoster->Count && added < nUnits; ++i) {
        TechnoClass* pCandidate = (*pRoster)[i];
        if (!pCandidate || pCandidate->IsDead()) continue;
        if (pCandidate->GetOwningHouse() != Owner) continue;

        // Skip units already on this team.
        bool alreadyOnTeam = false;
        for (int32 j = 0; j < Members.Count; ++j) {
            if (Members[j] == pCandidate) { alreadyOnTeam = true; break; }
        }
        if (alreadyOnTeam) continue;

        // Buildings cannot join mobile teams.
        if (!IsFootMember(pCandidate)) continue;

        // Match against the task force composition (loose match: any entry
        // with a positive quota makes the candidate eligible).
        bool typeMatched = false;
        for (int32 m = 0; m < tfMemberCount; ++m) {
            TaskForceMember* pMember = pTaskForce->GetMember(m);
            if (!pMember || pMember->Count <= 0) continue;
            typeMatched = true;
            break;
        }
        if (!typeMatched) continue;

        if (AddMember(pCandidate)) {
            ++added;
            static_cast<FootClass*>(pCandidate)->Stop_Moving();
        }

        if (IsTeamFull()) break;
    }

    if (added > 0) {
        // Re-form to incorporate the new members.
        IsFormed = false;
        NeedsReGrouping = true;
    }

    return added > 0;
}

// =============================================================================
// GetMemberCount - count living members on the roster.
// =============================================================================
int32 AITeamClass::GetMemberCount() const {
    int32 count = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] && !Members[i]->IsDead()) ++count;
    }
    return count;
}

// =============================================================================
// IsEmpty - true when the roster holds no living member
//          (asm 判 `AITeamClass.FirstUnitInTeam == 0`).
// =============================================================================
bool AITeamClass::IsEmpty() const {
    return GetMemberCount() == 0;
}

// =============================================================================
// Liberate - release every member back to "free agent" status.
//
// asm: TeamClass_LiberateAllMembers (asm:0x6F0Cxx) —— 逐个成员清掉它的队伍归属
// 与编组号，再从成员链摘下；处理完清空各坑位计数与实到人数。
// =============================================================================
void AITeamClass::Liberate() {
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || !IsFootMember(pTechno)) continue;

        FootClass* pFoot = static_cast<FootClass*>(pTechno);
        // 只摘掉"确实挂着本队"的引用；本类不与 FootClass::Team 互相指认，
        // 因此这里只复位编组号与队长标记，避免误清别的队的归属。
        pFoot->Group = -1;
        pFoot->IsTeamLeader = false;
    }

    Members.Clear();
    TotalObjects = 0;
    totalStrength = 0;
    TotalThreatValue = 0;
    for (int32 i = 0; i < MaxTaskForceSlots; ++i) {
        TeamCounts[i] = 0;
    }
}

// =============================================================================
// AssignMissionTarget - hand a shared target to the whole team.
//
// asm: TeamClass::Assign_Mission_Target (asm:0x6E90xx) —— 写入当前目标并让每个
//  成员朝它行动。命中新目标时，Foot 系成员各自 Move_To 到目标位置（原地攻击
//  由成员自身的武器逻辑决定）。传 nullptr 即清空目标。
// =============================================================================
void AITeamClass::AssignMissionTarget(AbstractClass* pTarget) {
    ITarget = pTarget;
    if (pTarget == nullptr) return;

    const CoordStruct dest = pTarget->GetCoords();
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        if (!IsFootMember(pTechno)) continue;
        static_cast<FootClass*>(pTechno)->Move_To(dest);
    }
}

// =============================================================================
// MaskAllMembersForRemoval - 给成员链上每个单位打上"随队长移动"标记。
//
// asm: TeamClass_Update @@9-@@11 —— 遍历 FirstUnitInTeam 链，只要队伍正在
//  离场 (+0x7B) 或需要整队行动 (+0x77)，就给该单位写 FootClass.field_689=1，
//  使其步调跟随队长。
// =============================================================================
void AITeamClass::MaskAllMembersForRemoval() {
    const bool mark = NeedsRecheckState || WantsToDelete;
    if (!mark) return;

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || !IsFootMember(pTechno)) continue;
        // OpenYR 用 IsTeamLeader 表达同一语义：整队行动时成员全部跟随队长。
        static_cast<FootClass*>(pTechno)->IsTeamLeader = false;
    }
}

// =============================================================================
// LeaveMap - 用编成要求与实到人数刷新队伍的"人齐/离场"状态。
//
// asm: int __thiscall TeamClass_LeaveMap(AITeamClass* this)  (asm:0x6EA3xx)
//   1. 取编成表总人数 required 与实到人数 actual。
//   2. actual > 0：IsFullyLoaded = (actual == required)；Reinforce 时"过半即
//      算可用"，否则"人齐才算可用"；Full 约束下没凑齐就按兵不动 WantsToDelete。
//   3. actual == 0：队伍视同解散——清锚点，曾经满员则向 Tag 抛一次事件并让
//      本队退场（返回假，调用方不再驱动它）。
//   4. 离场状态相对进入前发生翻转时置 NeedsRecheckState。
// =============================================================================
bool AITeamClass::LeaveMap() {
    TaskForceClass* pTask = (TeamType != nullptr) ? TeamType->TaskForce : nullptr;
    const int32 required = (pTask != nullptr) ? pTask->CountObjects() : 0;
    const int32 actual = GetMemberCount();

    const bool wasLeaving = IsLeavingMap;

    if (actual > 0) {
        IsFullyLoaded = (actual == required);
        if (IsFullyLoaded) {
            HasLeftMap = true;
        }

        // 队伍类型上"可以缺人出行"的开关（asm TeamType+0xAB = Reinforce）
        // 决定凑够一半即算可用。
        if (TeamType != nullptr && TeamType->Reinforce) {
            IsLeavingMap = (required > 2) ? (actual >= (required / 2))
                                          : (actual >= required);
        } else {
            IsLeavingMap = !IsFullyLoaded;
        }

        // "必须凑齐才出动"的约束下，没凑齐就先按兵不动。
        if (TeamType != nullptr && TeamType->Full) {
            WantsToDelete = !IsLeavingMap;
        }

        NeedsToDisappear = false;
        JustDisappeared  = false;
    } else {
        // 队员已经一个不剩：队伍视同解散。
        const bool wasFull = IsFullyLoaded;

        WantsToDelete    = false;
        IsLeavingMap     = true;
        IsFullyLoaded    = false;
        HasLeftMap       = true;
        SpawnCell        = nullptr;

        if (wasFull && TagClass::Array != nullptr) {
            for (int32 i = TagClass::Array->GetCount() - 1; i >= 0; --i) {
                TagClass* pTag = (*TagClass::Array)[i];
                if (pTag == nullptr) continue;
                const int32 trigCount = pTag->TriggerList.GetCount();
                for (int32 t = trigCount - 1; t >= 0; --t) {
                    TriggerClass* pTrig = pTag->TriggerList[t];
                    if (pTrig == nullptr) continue;
                    pTrig->Spring(TriggerEventType::AnyEvent, nullptr, CellStruct());
                    break;
                }
            }
        }

        // 本类由 AITeamTypeClass::DestroyAllInstances / UpdateAll 持有并回收，
        // 这里只把自己标记为退场，不 delete this。
        Active = false;
        JustDisappeared = true;
        return false;
    }

    if (IsLeavingMap != wasLeaving) {
        NeedsRecheckState = true;
    }

    return true;
}

// =============================================================================
// asm: char __thiscall TeamClass_CanRecruitUnit(AITeamClass* this,
//      _FootClass* Unit, int* idxInTask, char canRecruitExtras)
//      (asm:1530658) retn 0Ch
//
//  逐条复刻 asm 的**提前返回**判据（任一不满足即返回假）：
//    * 单位为空 / 已死 / 已在本队；
//    * 拥有者不一致；
//    * 在 task-force 里按类型定位坑位（写出 *idxInTask），坑位越界时只有
//      允许额外收编才放行；
//    * aircraft 永远可收，脚本第 8 行（Recruit）也强制放行；
//    * 非可招募单位要求本队是 autocreate 队；
//    * 正在"抽吸"（IsDraining）的单位不收；
//    * 单位已属于别的队时，只有本队优先度更高才抢得过来；
//    * 坑位计数已达到 task-force 要求且不允许额外收编 -> 不收。
// =============================================================================
bool AITeamClass::CanRecruitUnit(FootClass* pUnit, int32* idxInTask, bool canRecruitExtras) const {
    if (pUnit == nullptr) return false;

    // 已属于本队。
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] == pUnit) return false;
    }

    if (pUnit->IsDead()) return false;
    if (pUnit->Owner != Owner) return false;

    TaskForceClass* pTask = (TeamType != nullptr) ? TeamType->TaskForce : nullptr;
    if (pTask == nullptr) return false;

    int32 slot = 0;
    const int32 memberCount = pTask->Members.Count;
    TechnoTypeClass* pUnitType = pUnit->GetTechnoType();

    if (memberCount > 0) {
        for (;;) {
            if (pTask->Members.Items[slot].Type == pUnitType) break;
            ++slot;
            if (slot >= memberCount) break;
        }
    }

    if (idxInTask != nullptr) *idxInTask = slot;

    if (slot >= memberCount && !canRecruitExtras) return false;

    // Aircraft 永远可收；脚本的 "recruit"（动作 8）也强制收。
    bool allowByScript = (pUnit->WhatAmI() == AbstractType::Aircraft);
    if (!allowByScript && Script != nullptr) {
        if (Script->GetAction() == 8) allowByScript = true;
    }

    if (!pUnit->Recruitable && !allowByScript
        && (TeamType == nullptr || !TeamType->Autocreate)) {
        return false;
    }

    // 正在被抽吸的单位不可征用。
    if (pUnit->IsDraining()) return false;

    // 已属于别的队：只有本队优先度更高才抢得过来。
    if (pUnit->Team != nullptr) {
        const int32 myPriority = (TeamType != nullptr) ? TeamType->Priority : 0;
        const int32 otherPriority = (pUnit->Team->Type != nullptr)
                                  ? pUnit->Team->Type->Priority : 0;
        if (otherPriority >= myPriority) return false;
    }

    // 坑位计数已达要求且不允许额外收编 -> 不收。
    if (slot >= 0 && slot < memberCount && !canRecruitExtras) {
        const int32 have = TeamCounts[slot];
        const int32 need = pTask->Members.Items[slot].Count;
        if (need != 0 && have >= need) return false;
    }

    return true;
}

// =============================================================================
// asm: void __thiscall TeamClass_RecruitUnit(AITeamClass* this,
//      _FootClass* Unit, char canRecruitExtras)  (asm:1530517) retn 8
//
//  先过 CanRecruitUnit；命中后把单位从原队摘掉，坑位计数 +1，把它挂进本队
//  成员链（首名成员即队长），套用队伍组的编组号与老兵等级，最后置
//  actionExecuted / achievedSuccess 两个标记。
// =============================================================================
bool AITeamClass::RecruitUnit(FootClass* pUnit, bool canRecruitExtras) {
    if (pUnit == nullptr) return false;

    int32 idxInTask = 0;
    if (!CanRecruitUnit(pUnit, &idxInTask, canRecruitExtras)) return false;

    // 先从原队摘掉（若它挂在别的 TeamClass 上）。
    if (pUnit->Team != nullptr) {
        pUnit->Team->Remove(pUnit, -1, false);
    }

    if (!canRecruitExtras && idxInTask >= 0 && idxInTask < MaxTaskForceSlots) {
        ++TeamCounts[idxInTask];
    }

    // 首名成员即队长。
    pUnit->IsTeamLeader = (GetMemberCount() == 0);
    if (!Members.Add(pUnit)) return false;

    if (TeamType != nullptr) {
        pUnit->Group = TeamType->GetGroup();
        if (TeamType->VeteranLevel > 0
            && pUnit->VeterancyLevel < TeamType->VeteranLevel) {
            pUnit->VeterancyLevel = TeamType->VeteranLevel;
        }
    }

    // 战力/人数累计（asm 更新 +0x48 / +0x4C）。
    ++TotalObjects;
    ++totalStrength;
    totalStrength = GetTotalStrength();

    // 队伍还没锚点就先补一个跟随锚点（asm 在 +0x34 为空时调 FindUnitToFollow）。
    if (SpawnCell == nullptr) {
        CellStruct cell;
        FootClass* pAnchor = nullptr;
        FindUnitToFollow(&cell, &pAnchor);
    }

    ActionExecuted  = true;
    AchievedSuccess = true;
    IsFormed        = true;
    return true;
}

// =============================================================================
// asm: TeamClass_RecruitThisUnit (asm:0x6EBFB0) —— 以"允许额外收编"转调。
// =============================================================================
void AITeamClass::RecruitThisUnit(FootClass* pUnit) {
    RecruitUnit(pUnit, true);
}

// =============================================================================
// asm: signed int __thiscall TeamClass_SearchForRecruit(AITeamClass* this,
//      int idx)  (asm:1531212)
//
//  替编成表第 idx 个坑位找一名新兵：
//    * 缺口不为正就不招；
//    * 以队伍当前的编队中心作为挑选基准的集结点；
//    * 该坑位要求的单位种类决定去哪个名单里挑：载具、飞机、步兵各一份；
//    * 在名单里逐个比较，优先取离集结点最近、且 CanRecruitUnit 通过的那一个；
//    * 挑中之后入队；如果它本身载着乘客，连乘客一并收编。
// =============================================================================
bool AITeamClass::SearchForRecruit(int32 idx) {
    if (idx < 0 || idx >= MaxTaskForceSlots) return false;

    TaskForceClass* pTask = (TeamType != nullptr) ? TeamType->TaskForce : nullptr;
    if (pTask == nullptr) return false;
    if (idx >= pTask->GetMemberCount()) return false;

    TaskForceMember* pSlot = pTask->GetMember(idx);
    if (pSlot == nullptr) return false;

    const int32 slotNeed = pSlot->Count;
    TechnoTypeClass* pWantType = pSlot->Type;

    if (slotNeed <= TeamCounts[idx]) return false;

    // 集结点：以本队当前编队中心为挑选基准。
    const CoordStruct rally = ComputeFormationCenter();

    const int32 group      = (TeamType != nullptr) ? TeamType->GetGroup() : -1;
    const bool  bRecruiter = (TeamType != nullptr) ? TeamType->Recruiter : false;

    // 候选名单：载具、飞机、步兵各一份。
    const AbstractType kinds[3] = {
        AbstractType::Unit, AbstractType::Aircraft, AbstractType::Infantry
    };

    FootClass* pBest     = nullptr;
    int32      bestScore = 0x7FFFFFFF;

    for (int32 k = 0; k < 3; ++k) {
        if (pWantType != nullptr && pWantType->WhatAmI() != kinds[k]) continue;

        DynamicVectorClass<TechnoClass*>* pPool = nullptr;
        if (kinds[k] == AbstractType::Unit)
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(UnitClass::Array);
        else if (kinds[k] == AbstractType::Aircraft)
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(AircraftClass::Array);
        else
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(InfantryClass::Array);

        if (pPool == nullptr) continue;

        const int32 n = pPool->GetCount();
        for (int32 i = 0; i < n; ++i) {
            TechnoClass* pUnit = (*pPool)[i];
            if (pUnit == nullptr) continue;

            FootClass* pFoot = static_cast<FootClass*>(pUnit);

            // 编组不符、而本队又不允许跨组招人，就跳过。
            if (group != -2 && pFoot->Group != group && !bRecruiter) continue;

            // 距离评分：离集结点越近越好；编组不同的额外加一段惩罚距离。
            const CoordStruct pos = pUnit->Get_Coord();
            const int32 dx = pos.X - rally.X;
            const int32 dy = pos.Y - rally.Y;
            int32 score = static_cast<int32>(
                std::sqrt(static_cast<double>(dx * dx + dy * dy)));
            if (pFoot->Group != group) score += 0x3200;

            if (score >= bestScore) continue;

            int32 idxInTask = 0;
            if (!CanRecruitUnit(pFoot, &idxInTask, false)) continue;

            pBest     = pFoot;
            bestScore = score;
        }
    }

    if (pBest == nullptr) return false;

    // 招人：先让它停下手上的事，再真正入队。
    pBest->Stop_Moving();
    RecruitUnit(pBest, false);

    // 如果它本身还载着乘客，连乘客一并收编。
    TechnoClass* pPassenger = pBest->Attached_Object();
    while (pPassenger != nullptr) {
        TechnoClass* pNext = static_cast<TechnoClass*>(pPassenger->NextObject);
        RecruitUnit(static_cast<FootClass*>(pPassenger), false);
        pPassenger = pNext;
    }

    return true;
}
