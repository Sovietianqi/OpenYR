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
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/TechnoTypeClass.h"
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
      FormationTarget(0, 0, 0) {
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
    Members.Clear();
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
// Update - per-frame AI team tick.
//
// Sequence:
//   1. If suspended, count down the suspend timer and skip processing.
//   2. Update the guard-area timer.
//   3. Prune dead members.
//   4. If the roster is empty, deactivate non-autocreate teams.
//   5. If regrouping is requested, re-form the team.
//   6. If the team is mid-reform, continue forming up.
//   7. Otherwise, advance the behaviour script.
// =============================================================================
void AITeamClass::Update() {
    if (!Active) return;

    // Handle suspension: count down the timer and resume when it expires.
    if (IsSuspended) {
        SuspendTimer.Update();
        if (SuspendTimer.Expired()) {
            IsSuspended = false;
        } else {
            return;
        }
    }

    GuardAreaTimer.Update();

    CleanupDeadMembers();

    if (Members.Count == 0) {
        // No members left.  Autocreate teams persist (they will recruit new
        // members); other teams deactivate.
        if (TeamType && !TeamType->Autocreate) {
            Active = false;
        }
        return;
    }

    // Re-aggregate the threat value after cleanup.
    TotalThreatValue = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        TotalThreatValue += ComputeMemberThreat(Members[i]);
    }

    // Process any pending regroup request.
    if (NeedsReGrouping) {
        ReGroup();
        NeedsReGrouping = false;
    }

    // Continue an in-progress formation move.
    if (IsReforming) {
        FormUp();
        return;
    }

    ExecuteScript();
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
// FindUnitToFollow - return the formation anchor (first living member).
// =============================================================================
TechnoClass* AITeamClass::FindUnitToFollow() {
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead()) {
            return pTechno;
        }
    }
    return nullptr;
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
        if (TeamType) {
            TeamType->RegisterSuccess();
        }
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
    Members.Clear();
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
// ReinforceTeam - spawn or recruit up to nUnits members from the team type's
// task force composition.
//
// For each task force entry we call AITeamTypeClass::SpawnUnit to create a new
// unit of the specified type for the owning house.  Spawned units are added
// to the roster immediately.
// =============================================================================
bool AITeamClass::ReinforceTeam(int32 nUnits) {
    if (!TeamType || !Owner) return false;
    if (nUnits <= 0) return false;
    if (IsTeamFull()) return false;

    int32 added = 0;
    int32 tfCount = TeamType->TaskForceMembers.Count;

    for (int32 i = 0; i < tfCount && added < nUnits; ++i) {
        TechnoTypeClass* pType = TeamType->TaskForceMembers[i];
        if (!pType) continue;

        int32 desired = 1;
        if (i < TeamType->TaskForceCounts.Count) {
            desired = TeamType->TaskForceCounts[i];
            if (desired < 1) desired = 1;
        }

        for (int32 j = 0; j < desired && added < nUnits; ++j) {
            if (IsTeamFull()) break;

            FootClass* pUnit = TeamType->SpawnUnit(pType, Owner);
            if (pUnit && AddMember(pUnit)) {
                ++added;
            }
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
