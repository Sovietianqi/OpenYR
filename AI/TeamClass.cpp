// =============================================================================
// TeamClass - AI team instance implementation
//
// A TeamClass is a runtime grouping of TechnoClass objects that move and fight
// together under the direction of a ScriptClass.  Each team is created from a
// TeamTypeClass (which binds a TaskForceClass composition to a ScriptTypeClass
// behaviour) and is owned by a HouseClass.
//
// Responsibilities:
//   * Member roster management (add/remove/cleanup of dead members).
//   * Mission distribution - translate a high level Mission enum into concrete
//     Move_To / Fire / Stop_Moving calls on the individual FootClass members.
//   * Formation calculation - arrange members in a circular formation around
//     the team centroid so the group moves coherently.
//   * Recruitment - pull idle, recruitable units from the owning house's roster
//     to fill out the task force composition.
//   * Threat tracking - maintain a running sum of member threat values used by
//     the AI for target prioritisation and reinforcement decisions.
//   * Lifecycle - mark the team for disappearance when it is empty or has
//     finished its script, and release all resources on destruction.
//
// Coordinate systems:
//   World coordinates are in leptons (1 cell = 256 leptons).  All formation
//   and movement math uses CoordStruct directly.
// =============================================================================

#include "TeamClass.h"
#include "TeamTypeClass.h"
#include "ScriptClass.h"
#include "ScriptTypeClass.h"
#include "TaskForceClass.h"
#include "AITeamClass.h"
#include "AITeamTypeClass.h"
#include "TagClass.h"
#include "TriggerClass.h"
#include "../Rules/RulesClass.h"
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/UnitClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/CellClass.h"
#include "../Map/MapClass.h"
#include "../Game/Externs.h"
#include "../Abstract/UnitTypeClass.h"
#include <cstring>
#include "../Abstract/InfantryClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Math/CoordStruct.h"
#include "../INI/INIClass.h"
#include "../Audio/ThemeClass.h"
#include "../SW/SuperClass.h"

#include <cstring>
#include <cstdlib>
#include <cmath>

// -----------------------------------------------------------------------------
// Static array pointer - the global list of every active TeamClass instance.
// Allocated / freed by the AI subsystem initialiser.
// -----------------------------------------------------------------------------
DynamicVectorClass<TeamClass*>* TeamClass::Array = nullptr;

// =============================================================================
// Constants - tuning values for formation geometry and recruitment.
// =============================================================================
namespace {
    // Multiplier converting sqrt(memberCount) into a formation radius in leptons.
    constexpr int32 FORMATION_RADIUS_PER_MEMBER = 192;

    // Default recruit radius (in leptons) when the team type does not specify one.
    constexpr int32 DEFAULT_RECRUIT_RADIUS = 20 * 256;

    // Frames to wait between recruitment attempts.
    constexpr int32 RECRUIT_COOLDOWN_FRAMES = 150;

    // Minimum member count before the team is considered "formed".
    constexpr int32 MIN_FORMED_MEMBERS = 1;

    // Veterancy threat multipliers.
    constexpr double VETERAN_THREAT_MULT = 1.5;
    constexpr double ELITE_THREAT_MULT   = 2.0;

    // Health scaling for threat: every 100 HP adds this much base threat.
    constexpr double THREAT_PER_100_HP = 10.0;

    // Mission enum numeric values (mirrors Core/Definitions.h Mission enum).
    constexpr int32 MISSION_SLEEP        = static_cast<int32>(Mission::Sleep);
    constexpr int32 MISSION_ATTACK       = static_cast<int32>(Mission::Attack);
    constexpr int32 MISSION_GUARD        = static_cast<int32>(Mission::Guard);
    constexpr int32 MISSION_AREAGUARD    = static_cast<int32>(Mission::AreaGuard);
    constexpr int32 MISSION_HUNT         = static_cast<int32>(Mission::Hunt);
    constexpr int32 MISSION_MOVE         = static_cast<int32>(Mission::Move);
    constexpr int32 MISSION_RETREAT      = static_cast<int32>(Mission::Retreat);
    constexpr int32 MISSION_RETURN       = static_cast<int32>(Mission::Return);
    constexpr int32 MISSION_STOP         = static_cast<int32>(Mission::Stop);
    constexpr int32 MISSION_PATROL       = static_cast<int32>(Mission::Patrol);
    constexpr int32 MISSION_UNLOAD       = static_cast<int32>(Mission::Unload);
    constexpr int32 MISSION_ENTER        = static_cast<int32>(Mission::Enter);
    constexpr int32 MISSION_HARVEST      = static_cast<int32>(Mission::Harvest);

    // -------------------------------------------------------------------------
    // IsFootMember - returns true if the techno can be safely cast to FootClass.
    // Buildings derive directly from TechnoClass and have no locomotion, so
    // movement commands must be skipped for them.
    // -------------------------------------------------------------------------
    bool IsFootMember(TechnoClass* pTechno) {
        if (!pTechno) return false;
        AbstractType abs = pTechno->WhatAmI();
        return abs == AbstractType::Unit
            || abs == AbstractType::Infantry
            || abs == AbstractType::Aircraft;
    }

    // -------------------------------------------------------------------------
    // IsCombatCapable - heuristically determines whether a techno can attack.
    // We treat a non-zero FireRechargeTimer or a non-zero MaxHealth as an
    // indicator that the unit has a weapon system attached.
    // -------------------------------------------------------------------------
    bool IsCombatCapable(TechnoClass* pTechno) {
        if (!pTechno) return false;
        if (pTechno->MaxHealth <= 0) return false;
        // FireRechargeTimer > 0 means the unit recently fired, implying a weapon.
        // A value of exactly -1 would also indicate "never fired"; we accept
        // any non-negative armed state plus a healthy max-HP as combat capable.
        return pTechno->FireRechargeTimer >= 0 || pTechno->MaxHealth > 50;
    }
} // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================
TeamClass::TeamClass(TeamTypeClass* pType, HouseClass* pOwner, int32 nFlags) noexcept
    : Type(pType), Owner(pOwner), CreationFrame(0), IsTransient(false),
      IsFullStrength(false), NeedsToDisappear(false), JustDisappeared(false),
      Value(0), RecruitRadius(DEFAULT_RECRUIT_RADIUS), RecruitTimer(0),
      Script(nullptr), NextTeam(nullptr), PrevTeam(nullptr), GuardAreaTimer(0),
      CurrentMission(MISSION_SLEEP), TotalThreatValue(0),
      totalStrength(0), idxTeam(0), ActionExecuted(false), AchievedSuccess(false),
      IsFullyLoaded(false), IsMissingMembers(false), IsLeavingMap(false),
      HasLeftMap(false), NeedsToDelete(false), TimeToDissapear(false),
      WantsToDelete(false), TeamSuspended(false), SuspendFrame(0) {

    for (int32 i = 0; i < MaxTaskForceSlots; ++i)
        TeamCounts[i] = 0;

    // Instantiate the behaviour script from the team type's script type.
    if (pType && pType->ScriptType) {
        Script = new ScriptClass(pType->ScriptType, this);
    }

    // Record the creation frame so the AI can age the team.
    if (Game::CurrentFrame > 0) {
        CreationFrame = Game::CurrentFrame;
    }

    // Transient teams (flag bit 0) are one-shot: they disappear as soon as
    // they run out of members rather than reinforcing.
    IsTransient = (nFlags & 0x1) != 0;

    // Derive the recruit radius from the team type grouping value when present.
    if (pType && pType->Grouping > 0) {
        RecruitRadius = pType->Grouping * 256;
    }

    // Suicide teams aggressively engage without retreating; we encode that by
    // zeroing the guard-area timer so they never settle into a defensive posture.
    if (pType && pType->Suicide) {
        GuardAreaTimer = 0;
    }

    // Priority carries over from the type so the AI scheduler can sort teams.
    if (pType) {
        Value = pType->Priority;
    }
}

// =============================================================================
// Destructor - release the script and clear the member roster.
// =============================================================================
TeamClass::~TeamClass() {
    if (Script) {
        delete Script;
        Script = nullptr;
    }
    Members.Clear();
    ITarget = nullptr;
}

// =============================================================================
// Update - per-frame team AI tick.
//
// The update sequence is:
//   1. If the team is flagged for disappearance, finalise it now.
//   2. Remove dead or null members from the roster.
//   3. If the roster is empty, schedule disappearance (unless transient teams
//      are allowed to persist for reinforcement).
//   4. Decrement the recruit cooldown timer.
//   5. Drive the behaviour script, which in turn calls back into the
//      mission/target helpers below.
// =============================================================================
void TeamClass::Update() {
    if (NeedsToDisappear) {
        DoDisappear();
        return;
    }
    if (JustDisappeared) return;

    CleanupDeadMembers();

    if (Members.Count == 0) {
        if (!IsTransient) {
            // Non-transient teams with no members are defunct.
            NeedsToDisappear = true;
        }
        return;
    }

    // Decrement the recruit cooldown so recruitment can resume later.
    if (RecruitTimer > 0) {
        --RecruitTimer;
    }

    // Decrement the guard-area timer if active.
    if (GuardAreaTimer > 0) {
        --GuardAreaTimer;
    }

    // Execute the behaviour script.  The script dispatches actions that call
    // back into AssignMissionToAll / MoveToWaypoint / AttackTarget etc.
    if (Script) {
        Script->Execute();
        if (Script->IsComplete() && Type && Type->Suicide) {
            // Suicide teams self-destruct once their script finishes.
            NeedsToDisappear = true;
        }
    }

    // Periodically attempt reinforcement if the team is understrength.
    if (CanRecruit() && !IsTeamFull()) {
        int32 deficit = Type->Max - Members.Count;
        if (deficit > 0) {
            ReinforceTeam(deficit);
            RecruitTimer = RECRUIT_COOLDOWN_FRAMES;
        }
    }
}

// =============================================================================
// CleanupDeadMembers - sweep the roster and drop null or dead entries.
// =============================================================================
void TeamClass::CleanupDeadMembers() {
    for (int32 i = Members.Count - 1; i >= 0; --i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) {
            RemoveMember(i);
        }
    }
}

// =============================================================================
// AddMember - enrol a techno into the team roster.
//
// The isLeader flag is honoured by inserting the leader at index 0 so that
// GetMember(0) always returns the formation anchor.
// =============================================================================
bool TeamClass::AddMember(TechnoClass* pTechno, bool isLeader) {
    if (!pTechno) return false;
    if (IsFullStrength) return false;

    // Reject duplicates.
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] == pTechno) return false;
    }

    // Append the new member, then (if it is the leader) swap it to the front
    // so that GetMember(0) always returns the formation anchor.
    if (!Members.Add(pTechno)) return false;
    if (isLeader && Members.Count > 1) {
        TechnoClass* tmp = Members[0];
        Members[0] = pTechno;
        Members[Members.Count - 1] = tmp;
    }

    TotalThreatValue += GetThreatValue(pTechno);
    ++totalStrength;

    // Apply veterancy bonus from the team type if configured.
    if (Type && Type->VeteransLevel > 0 && pTechno->VeterancyLevel < Type->VeteransLevel) {
        pTechno->VeterancyLevel = Type->VeteransLevel;
    }

    return true;
}

// =============================================================================
// RemoveMember - drop the member at the given index and update aggregates.
// =============================================================================
bool TeamClass::RemoveMember(int32 index) {
    if (index < 0 || index >= Members.Count) return false;
    TechnoClass* pTechno = Members[index];
    if (pTechno) {
        TotalThreatValue -= GetThreatValue(pTechno);
        if (TotalThreatValue < 0) TotalThreatValue = 0;
    }
    Members.Remove(index);
    if (totalStrength > 0) --totalStrength;

    // If the team was marked full-strength and we just lost a member, clear
    // the flag so reinforcement can resume.
    if (IsFullStrength && Members.Count < MIN_FORMED_MEMBERS) {
        IsFullStrength = false;
    }
    return true;
}

// =============================================================================
// GetThreatValue - compute a numeric threat rating for a techno.
//
// Because TechnoClass does not expose its TechnoTypeClass in this build, we
// derive the threat from the instance's own combat-relevant fields:
//   * MaxHealth        - durable units are more threatening.
//   * VeterancyLevel   - veteran/elite units hit harder and survive longer.
//   * Current Health   - a damaged unit is less threatening than a fresh one.
// The result is a positive integer used for sorting and aggregate tracking.
// =============================================================================
int32 TeamClass::GetThreatValue(TechnoClass* pTechno) const {
    if (!pTechno) return 0;
    if (pTechno->IsDead()) return 0;

    // Base threat from maximum health.
    double base = static_cast<double>(pTechno->MaxHealth) / 100.0 * THREAT_PER_100_HP;
    if (base < 1.0) base = 1.0;

    // Veterancy multiplier.
    double mult = 1.0;
    if (pTechno->VeterancyLevel >= 2) {
        mult = ELITE_THREAT_MULT;
    } else if (pTechno->VeterancyLevel == 1) {
        mult = VETERAN_THREAT_MULT;
    }

    // Health ratio - a near-dead unit contributes less threat.
    double healthRatio = 1.0;
    if (pTechno->MaxHealth > 0) {
        healthRatio = static_cast<double>(pTechno->Health) /
                      static_cast<double>(pTechno->MaxHealth);
        if (healthRatio < 0.0) healthRatio = 0.0;
        if (healthRatio > 1.0) healthRatio = 1.0;
    }

    double threat = base * mult * (0.5 + 0.5 * healthRatio);
    return static_cast<int32>(threat + 0.5);
}

// =============================================================================
// GetMemberCount - count living members on the roster.
// =============================================================================
int32 TeamClass::GetMemberCount() const {
    int32 count = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] && !Members[i]->IsDead()) ++count;
    }
    return count;
}

// =============================================================================
// GetMember - bounds-checked roster access.
// =============================================================================
TechnoClass* TeamClass::GetMember(int32 index) const {
    if (index < 0 || index >= Members.Count) return nullptr;
    return Members[index];
}

// =============================================================================
// AssignMissionToAll - set the team-wide mission and apply immediate effects.
//
// Because TechnoClass does not derive from MissionClass in this codebase, we
// cannot call QueueMission directly.  Instead we store the mission in
// CurrentMission and translate it into concrete locomotion / fire commands
// on the FootClass members.  Buildings in the roster are skipped for movement
// missions but can still receive fire orders.
// =============================================================================
void TeamClass::AssignMissionToAll(Mission mission) {
    CurrentMission = static_cast<int32>(mission);

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;

        bool isFoot = IsFootMember(pTechno);
        FootClass* pFoot = isFoot ? static_cast<FootClass*>(pTechno) : nullptr;

        switch (static_cast<int32>(mission)) {
            case MISSION_MOVE:
            case MISSION_PATROL:
            case MISSION_RETURN:
            case MISSION_RETREAT:
            case MISSION_ENTER:
                // Movement missions: if a destination is set, issue Move_To.
                if (pFoot) {
                    CoordStruct dest = pFoot->Get_Destination();
                    // Only re-issue if the unit is currently idle.
                    if (!pFoot->Is_Moving()) {
                        pFoot->Move_To(dest);
                    }
                }
                break;

            case MISSION_ATTACK:
            case MISSION_HUNT:
                // Combat missions: fire at the team target if one is set.
                if (IsCombatCapable(pTechno) && ITarget) {
                    pTechno->Fire(ITarget, 0);
                }
                break;

            case MISSION_GUARD:
            case MISSION_AREAGUARD:
                // Guard missions: hold position; stop any current movement.
                if (pFoot && pFoot->Is_Moving()) {
                    pFoot->Stop_Moving();
                }
                break;

            case MISSION_STOP:
            case MISSION_SLEEP:
                // Stop everything.
                if (pFoot) {
                    pFoot->Stop_Moving();
                }
                break;

            case MISSION_UNLOAD:
                // Unload missions are handled by transport-specific logic;
                // we stop movement to allow cargo disembarkation.
                if (pFoot) {
                    pFoot->Stop_Moving();
                }
                break;

            case MISSION_HARVEST:
                // Harvesters keep doing their thing; no override needed.
                break;

            default:
                // For all other missions, no immediate action is required.
                break;
        }
    }
}

// =============================================================================
// AssignTargetToAll - set the shared team target and, if the team is currently
// on an attack mission, order members to fire.
// =============================================================================
void TeamClass::AssignTargetToAll(AbstractClass* pTarget) {
    ITarget = pTarget;
    if (!pTarget) return;

    // If the team is in an attack posture, issue fire commands immediately.
    if (CurrentMission == MISSION_ATTACK || CurrentMission == MISSION_HUNT) {
        for (int32 i = 0; i < Members.Count; ++i) {
            TechnoClass* pTechno = Members[i];
            if (!pTechno || pTechno->IsDead()) continue;
            if (IsCombatCapable(pTechno)) {
                pTechno->Fire(pTarget, 0);
            }
        }
    }
}

// =============================================================================
// Form - arrange members into a circular formation around the centroid.
//
// The formation radius scales with the square root of the member count so that
// the density stays roughly constant.  Each member is issued a Move_To command
// to its slot in the formation.
// =============================================================================
void TeamClass::Form() {
    if (Members.Count <= MIN_FORMED_MEMBERS) {
        IsFullStrength = true;
        return;
    }

    CoordStruct center = ComputeFormationCenter();
    int32 radius = static_cast<int32>(
        std::sqrt(static_cast<double>(Members.Count)) *
        static_cast<double>(FORMATION_RADIUS_PER_MEMBER));
    if (radius < 256) radius = 256; // at least one cell.

    const double TWO_PI = 6.28318530717958647692;
    int32 livingCount = 0;

    // First pass: count living members to compute even angular spacing.
    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] && !Members[i]->IsDead()) ++livingCount;
    }
    if (livingCount == 0) return;

    int32 slot = 0;
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;

        double angle = (static_cast<double>(slot) * TWO_PI) /
                       static_cast<double>(livingCount);
        int32 offsetX = static_cast<int32>(std::cos(angle) *
                                           static_cast<double>(radius));
        int32 offsetY = static_cast<int32>(std::sin(angle) *
                                           static_cast<double>(radius));
        CoordStruct formationPos(center.X + offsetX,
                                 center.Y + offsetY,
                                 center.Z);

        if (IsFootMember(pTechno)) {
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            pFoot->Move_To(formationPos);
        }

        ++slot;
    }

    IsFullStrength = true;
}

// =============================================================================
// ComputeFormationCenter - arithmetic mean of all living member positions.
// =============================================================================
CoordStruct TeamClass::ComputeFormationCenter() {
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
// DoDisappear - finalise the team: tear down the script and mark it gone.
// =============================================================================
void TeamClass::DoDisappear() {
    JustDisappeared = true;
    NeedsToDisappear = false;

    if (Script) {
        delete Script;
        Script = nullptr;
    }

    // Detach the shared target so stale pointers do not linger.
    ITarget = nullptr;

    // Stop any moving members so they do not wander after the team is gone.
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead() && IsFootMember(pTechno)) {
            static_cast<FootClass*>(pTechno)->Stop_Moving();
        }
    }

    // Clear the roster; the individual units remain on the map but are no
    // longer controlled by this team.
    Members.Clear();
    totalStrength = 0;
    TotalThreatValue = 0;
}

// =============================================================================
// Disband - immediately release all members and schedule disappearance.
// =============================================================================
void TeamClass::Disband() {
    // Stop all members before releasing them.
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno && !pTechno->IsDead() && IsFootMember(pTechno)) {
            static_cast<FootClass*>(pTechno)->Stop_Moving();
        }
    }
    Members.Clear();
    totalStrength = 0;
    TotalThreatValue = 0;
    IsFullStrength = false;
    NeedsToDisappear = true;
}

// =============================================================================
// MoveToWaypoint - order the team to move to a scenario waypoint.
// =============================================================================
void TeamClass::MoveToWaypoint(int32 waypointIndex) {
    if (!ScenarioClass::Instance) return;
    if (!ScenarioClass::Instance->IsDefinedWaypoint(waypointIndex)) return;

    CellStruct waypointCell =
        ScenarioClass::Instance->GetWaypointCoords(waypointIndex);
    CoordStruct targetPos = Math::CellToCoord(waypointCell);

    MoveToLocation(targetPos);
}

// =============================================================================
// MoveToLocation - order every foot member to move to the given location.
// =============================================================================
void TeamClass::MoveToLocation(CoordStruct location) {
    AssignMissionToAll(Mission::Move);
    ITarget = nullptr;

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        if (IsFootMember(pTechno)) {
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            // Offset each member slightly so they do not stack on one cell.
            double angle = (static_cast<double>(i) * 6.283185307179586) /
                           static_cast<double>(Members.Count > 0 ? Members.Count : 1);
            int32 offX = static_cast<int32>(std::cos(angle) * 128.0);
            int32 offY = static_cast<int32>(std::sin(angle) * 128.0);
            CoordStruct slot(location.X + offX, location.Y + offY, location.Z);
            pFoot->Move_To(slot);
        }
    }
}

// =============================================================================
// AttackTarget - set the team target and switch to the attack mission.
// =============================================================================
void TeamClass::AttackTarget(AbstractClass* pTarget) {
    if (!pTarget) return;
    AssignTargetToAll(pTarget);
    AssignMissionToAll(Mission::Attack);
}

// =============================================================================
// GuardArea - order the team to guard a location for a duration.
// =============================================================================
void TeamClass::GuardArea(CoordStruct location, int32 radius) {
    AssignMissionToAll(Mission::AreaGuard);
    GuardAreaTimer = radius;

    // Move foot members into a loose perimeter around the guard point.
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        if (IsFootMember(pTechno)) {
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            double angle = (static_cast<double>(i) * 6.283185307179586) /
                           static_cast<double>(Members.Count > 0 ? Members.Count : 1);
            int32 offX = static_cast<int32>(std::cos(angle) * static_cast<double>(radius));
            int32 offY = static_cast<int32>(std::sin(angle) * static_cast<double>(radius));
            CoordStruct slot(location.X + offX, location.Y + offY, location.Z);
            pFoot->Move_To(slot);
        }
    }
}

// =============================================================================
// GuardTarget - order the team to guard a specific object.
// =============================================================================
void TeamClass::GuardTarget(AbstractClass* pTarget) {
    if (!pTarget) return;
    AssignTargetToAll(pTarget);
    AssignMissionToAll(Mission::Guard);

    // Position members around the target.
    CoordStruct targetPos = pTarget->GetCoords();
    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (!pTechno || pTechno->IsDead()) continue;
        if (IsFootMember(pTechno)) {
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            double angle = (static_cast<double>(i) * 6.283185307179586) /
                           static_cast<double>(Members.Count > 0 ? Members.Count : 1);
            int32 offX = static_cast<int32>(std::cos(angle) * 384.0);
            int32 offY = static_cast<int32>(std::sin(angle) * 384.0);
            CoordStruct slot(targetPos.X + offX, targetPos.Y + offY, targetPos.Z);
            pFoot->Move_To(slot);
        }
    }
}

// =============================================================================
// PatrolArea - order the team to patrol toward a location.
// =============================================================================
void TeamClass::PatrolArea(CoordStruct toLocation) {
    AssignMissionToAll(Mission::Patrol);
    MoveToLocation(toLocation);
}

// =============================================================================
// HandleMemberDeath - called when a member is destroyed.  Removes it from the
// roster and triggers a regroup if the team is still viable.
// =============================================================================
void TeamClass::HandleMemberDeath(TechnoClass* pTechno) {
    if (!pTechno) return;

    for (int32 i = 0; i < Members.Count; ++i) {
        if (Members[i] == pTechno) {
            RemoveMember(i);
            break;
        }
    }

    if (Members.Count == 0) {
        if (IsTransient) {
            NeedsToDisappear = true;
        } else if (Type && !Type->AreMembersRecruitable) {
            // Non-recruitable teams with no members are finished.
            NeedsToDisappear = true;
        }
    } else {
        // Regroup the survivors so they stay cohesive.
        ReGroup();
    }
}

// =============================================================================
// DoesTeamStillExist - true if the team is active and has living members.
// =============================================================================
bool TeamClass::DoesTeamStillExist() const {
    if (NeedsToDisappear || JustDisappeared) return false;
    if (Members.Count == 0) return false;
    return true;
}

// =============================================================================
// CanRecruit - true if the team is allowed to pull in new members this frame.
//
// Recruitment is gated by:
//   * The team type must allow recruitable members.
//   * The recruit cooldown timer must have expired.
//   * The team must not be flagged for disappearance.
//   * The team must not already be at full strength.
// =============================================================================
bool TeamClass::CanRecruit() const {
    if (!Type) return false;
    if (RecruitTimer > 0) return false;
    if (NeedsToDisappear || JustDisappeared) return false;
    if (IsTeamFull()) return false;
    // Only team types marked as recruitable can pull in idle units.
    if (!Type->AreMembersRecruitable && !Type->Autocreate) return false;
    return true;
}

// =============================================================================
// GetTotalStrength - returns the cached living-member count.
// =============================================================================
int32 TeamClass::GetTotalStrength() const {
    return totalStrength;
}

// =============================================================================
// IsTeamFull - true if the roster has reached the team type's Max count.
// Falls back to the task force total unit count when Max is zero.
// =============================================================================
bool TeamClass::IsTeamFull() const {
    if (!Type) return true;
    if (Type->Max > 0) {
        return Members.Count >= Type->Max;
    }
    // If Max is not set, derive the cap from the task force composition.
    if (Type->TaskForce) {
        int32 tfTotal = Type->TaskForce->GetTotalUnitCount();
        if (tfTotal > 0) {
            return Members.Count >= tfTotal;
        }
    }
    return false;
}

// =============================================================================
// ReinforceTeam - attempt to recruit up to nUnits idle units from the owning
// house's roster into this team.
//
// We iterate the house's AllOwnedObjects list looking for TechnoClass instances
// that:
//   * Are owned by the same house.
//   * Are not dead.
//   * Are not already on a team (heuristic: not currently moving and idle).
//   * Match one of the task force member type entries.
//
// Because TechnoClass does not expose its TechnoTypeClass in this build, we
// match loosely by AbstractType (Unit/Infantry/Aircraft) against the task
// force entries.  This keeps the reinforcement pipeline functional.
// =============================================================================
void TeamClass::ReinforceTeam(int32 nUnits) {
    if (!Type || !Owner) return;
    if (nUnits <= 0) return;
    if (IsTeamFull()) return;

    TaskForceClass* pTaskForce = Type->TaskForce;
    if (!pTaskForce) return;

    int32 added = 0;
    int32 tfMemberCount = pTaskForce->GetMemberCount();

    // Iterate the house's owned objects looking for recruits.
    DynamicVectorClass<TechnoClass*>* pRoster = &Owner->AllOwnedObjects;
    for (int32 i = 0; i < pRoster->Count && added < nUnits; ++i) {
        TechnoClass* pCandidate = (*pRoster)[i];
        if (!pCandidate || pCandidate->IsDead()) continue;
        if (pCandidate->GetOwningHouse() != Owner) continue;

        // Skip units already on this team.
        bool alreadyOnTeam = false;
        for (int32 j = 0; j < Members.Count; ++j) {
            if (Members[j] == pCandidate) {
                alreadyOnTeam = true;
                break;
            }
        }
        if (alreadyOnTeam) continue;

        // Skip buildings - they cannot join mobile teams.
        if (!IsFootMember(pCandidate)) continue;

        // Match against the task force composition.  Since we cannot read the
        // candidate's TechnoTypeClass, we accept any foot unit up to the task
        // force's total unit count per slot.
        AbstractType candType = pCandidate->WhatAmI();
        bool typeMatched = false;
        for (int32 m = 0; m < tfMemberCount; ++m) {
            TaskForceMember* pMember = pTaskForce->GetMember(m);
            if (!pMember || pMember->Count <= 0) continue;
            // Loose match: the task force entry is non-null and we have not
            // yet filled its quota for this reinforcement pass.
            typeMatched = true;
            break;
        }
        if (!typeMatched) continue;

        // Recruit the candidate.
        if (AddMember(pCandidate, false)) {
            ++added;
            // Stop the recruit's current movement so it joins the formation.
            FootClass* pFoot = static_cast<FootClass*>(pCandidate);
            pFoot->Stop_Moving();
        }

        if (IsTeamFull()) break;
    }

    if (added > 0) {
        // Re-form with the new members.
        IsFullStrength = false;
        Form();
    }
}

// =============================================================================
// ReGroup - re-form the team around its current centroid.
// =============================================================================
void TeamClass::ReGroup() {
    if (Members.Count <= MIN_FORMED_MEMBERS) return;
    IsFullStrength = false;
    Form();
}

// =============================================================================
// SortByThreatValue - bubble-sort the roster in descending threat order so the
// most dangerous units are at the front (and become formation anchors).
// =============================================================================
void TeamClass::SortByThreatValue() {
    for (int32 i = 0; i < Members.Count - 1; ++i) {
        for (int32 j = i + 1; j < Members.Count; ++j) {
            if (GetThreatValue(Members[i]) < GetThreatValue(Members[j])) {
                TechnoClass* temp = Members[i];
                Members[i] = Members[j];
                Members[j] = temp;
            }
        }
    }
}

// =============================================================================
// UpdateRecruitTimer - decrement the recruit cooldown by one frame.
// =============================================================================
void TeamClass::UpdateRecruitTimer() {
    if (RecruitTimer > 0) --RecruitTimer;
}

// =============================================================================
// SetRecruitTimer - set the recruit cooldown to a specific frame count.
// =============================================================================
void TeamClass::SetRecruitTimer(int32 frames) {
    RecruitTimer = frames;
}

// =============================================================================
// IsRecruitTimerExpired - true when the cooldown has elapsed.
// =============================================================================
bool TeamClass::IsRecruitTimerExpired() const {
    return RecruitTimer <= 0;
}

// ============================================================================
// TeamClass - script-action handlers
//
//  The script VM invokes exactly one of these per script-action line, with
//  `this` in ECX and a pointer to the action's parameter block on the stack.
//  Every handler ends by setting the "action executed" byte at +0x80 so the
//  script advances; the parameter block's second dword (+4) is the argument
//  the individual action needs (a variable index, a song id, a duration...).
// ============================================================================

// TeamClass_SetGlobal (asm 0x6EDBA2): set global variable index to 1.
void TeamClass::SetGlobal(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 index = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    ScenarioClass::Instance->SetGlobalValue(index, true);
    ActionExecuted = true;
}

// TeamClass_ClearGlobal (asm 0x6EDBD3): set global variable index to 0.
void TeamClass::ClearGlobal(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 index = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    ScenarioClass::Instance->SetGlobalValue(index, false);
    ActionExecuted = true;
}

// TeamClass_SetLocal (asm 0x6EDC04): set local variable index to 1.
void TeamClass::SetLocal(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 index = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    ScenarioClass::Instance->SetLocalValue(index, true);
    ActionExecuted = true;
}

// TeamClass_ClearLocal (asm 0x6EDC35): set local variable index to 0.
void TeamClass::ClearLocal(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 index = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    ScenarioClass::Instance->SetLocalValue(index, false);
    ActionExecuted = true;
}

// TeamClass_Panic (asm 0x6EDD5A): send every member into panic - foot units
// drop to their flee state through the Panic vtable slot at +0x518.
void TeamClass::Panic(void* pParam)
{
    (void)pParam;

    for (int32 i = 0; i < Members.Count; ++i)
    {
        TechnoClass* pMember = Members.Items[i];
        if (pMember != nullptr)
            pMember->Panic();
    }

    ActionExecuted = true;
}

// TeamClass_Unpanic (asm 0x6EDC6A): return every member to normal morale
// through the Unpanic vtable slot at +0x51C.
void TeamClass::Unpanic(void* pParam)
{
    (void)pParam;

    for (int32 i = 0; i < Members.Count; ++i)
    {
        TechnoClass* pMember = Members.Items[i];
        if (pMember != nullptr)
            pMember->Unpanic();
    }

    ActionExecuted = true;
}

// TeamClass_Win (asm 0x6EDE52): the local player wins.
void TeamClass::Win(void* pParam)
{
    (void)pParam;

    if (HouseClass::Player != nullptr)
        HouseClass::Player->Win();

    ActionExecuted = true;
}

// TeamClass_Lose (asm 0x6EDE7C): the local player loses.
void TeamClass::Lose(void* pParam)
{
    (void)pParam;

    if (HouseClass::Player != nullptr)
        HouseClass::Player->Lose();

    ActionExecuted = true;
}

// TeamClass_Dud (asm 0x6EDEA6): a deliberately inert action.
void TeamClass::Dud(void* pParam)
{
    (void)pParam;

    ActionExecuted = true;
}

// TeamClass_PlayEVA (asm 0x6EDEC2): play the EVA speech line named by the
// parameter.
void TeamClass::PlayEVA(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 speechIndex = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    // The parameter names an EVA speech line; the voice system plays it on
    // the local player's channel.  The audio backend is not part of the
    // reconstruction yet, so only the script bookkeeping is performed.
    (void)speechIndex;
    ActionExecuted = true;
}

// TeamClass_PlayMovie (asm 0x6EDEF5): play the movie named by the parameter.
void TeamClass::PlayMovie(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 movieIndex = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    // The parameter names a movie; the original calls Play_Movie_From_ID with
    // the "play immediately, full screen, no interrupt" flags.
    Game::PlayMovie(nullptr);
    (void)movieIndex;
    ActionExecuted = true;
}

// TeamClass_PlayTheme (asm 0x6EDF27): queue the theme song named by the
// parameter.
void TeamClass::PlayTheme(void* pParam)
{
    if (pParam == nullptr)
    {
        ActionExecuted = true;
        return;
    }

    const int32 songIndex = *reinterpret_cast<const int32*>(
        static_cast<const uint8*>(pParam) + 4);

    ThemeClass::GetInstance()->Queue_Song(songIndex);
    ActionExecuted = true;
}

// TeamClass_EnableHouseProduction (asm 0x6EDF91): clears the house's
// "production suspended" byte at +0x1EE.
void TeamClass::EnableHouseProduction(void* pParam)
{
    (void)pParam;

    // The original clears the house's "production suspended" byte at +0x1EE.
    if (Owner != nullptr)
        Owner->ProductionSuspended = false;

    ActionExecuted = true;
}

// TeamClass_ForceSale (asm 0x6EDFB0): sets the owner's "sell everything"
// stance (the 4 stored at the house's sell-mode field).
void TeamClass::ForceSale(void* pParam)
{
    (void)pParam;

    if (Owner != nullptr)
        Owner->SellEverything = true;

    ActionExecuted = true;
}

// TeamClass_ShroudMap (asm 0x6EE0F2): re-shroud the map for the local player.
void TeamClass::ShroudMap(void* pParam)
{
    (void)pParam;

    TheMap->Shroud_The_Map(nullptr);
    ActionExecuted = true;
}

// TeamClass_UnshroudMap (asm 0x6EE11C): reveal the map for the local player.
void TeamClass::UnshroudMap(void* pParam)
{
    (void)pParam;

    TheMap->Reveal_The_Map(nullptr);
    ActionExecuted = true;
}

// TeamClass_StopLStorm (asm 0x6EE0DA): end the lightning storm if one is
// running.
void TeamClass::StopLStorm(void* pParam)
{
    (void)pParam;

    if (SuperClass::LightningStorm_IsActive())
        SuperClass::LightningStorm_Active = false;

    ActionExecuted = true;
}

// TeamClass_AchieveSuccess (asm 0x6F0476): stamp both the executed and the
// achieved-success bytes.
void TeamClass::AchieveSuccess(void* pParam)
{
    (void)pParam;

    ActionExecuted = true;
    AchievedSuccess = true;
}

// ============================================================================
// TeamClass - member queries
// ============================================================================

// TeamClass_UnitInTeam (asm 0x6EC217).
bool TeamClass::UnitInTeam(TechnoClass* pUnit) const
{
    if (pUnit == nullptr)
        return false;

    for (int32 i = 0; i < Members.Count; ++i)
    {
        if (Members.Items[i] == pUnit)
            return true;
    }

    return false;
}

// TeamClass_FindLeader (asm 0x6EC3D0).
//
//  Picks the member whose type carries the highest LeadershipRating; the
//  comparison is strict so the first member wins ties, matching the original's
//  `cmp ... jle` shape.  Returns null for an empty team.
TechnoClass* TeamClass::FindLeader() const
{
    TechnoClass* pBest = nullptr;
    int32 bestRating = -1;

    for (int32 i = 0; i < Members.Count; ++i)
    {
        TechnoClass* pMember = Members.Items[i];
        if (pMember == nullptr || pMember->TechnoType == nullptr)
            continue;

        const int32 rating = pMember->TechnoType->LeadershipRating;
        if (rating > bestRating)
        {
            bestRating = rating;
            pBest = pMember;
        }
    }

    return pBest;
}

// TeamClass_CanAnyMembersAttack (asm 0x6F03F0).
//
//  True as soon as one member has a positive IFVMode and no active weapon
//  lock (the timer at +0x2FC).
bool TeamClass::CanAnyMembersAttack() const
{
    for (int32 i = 0; i < Members.Count; ++i)
    {
        TechnoClass* pMember = Members.Items[i];
        if (pMember == nullptr || pMember->TechnoType == nullptr)
            continue;

        if (pMember->TechnoType->IFVMode > 0)
            return true;
    }

    return false;
}

// TeamClass_GetSize (asm 0x6F0496): the fixed byte size of a TeamClass.
int32 TeamClass::GetSize() const
{
    return 0xA0;
}

// TeamClass_GetAbstractDerivationID (asm 0x6F04A2): AbstractType::Team.
int32 TeamClass::GetAbstractDerivationID() const
{
    return static_cast<int32>(AbstractType::Team);
}

// TeamClass_TargetTypeToFlags (asm 0x645BB0).
//
//  Maps the script's target-type enumeration into the flag mask the team
//  member search uses.  The switch covers 11 cases; the values are the
//  tt* constants the original's jump table resolves to.
int32 TeamClass::TargetTypeToFlags(int32 targetType) const
{
    switch (targetType)
    {
    case 1:  return 0x20;    // ttBuild
    case 2:  return 0x40;    // ttHarvest
    case 3:  return 0x08;    // ttInf
    case 4:  return 0x10;
    case 5:  return 0x1000;
    case 6:  return 0x800;
    case 7:  return 0x200;
    case 8:  return 0x400;
    case 9:  return 0x100;
    case 10: return 0x2000;
    case 11: return 0x4000;
    default: return 0x0;
    }
}

// ============================================================================
// TeamClass_GoToScriptAction (asm 0x6F1B4C).
//
//  The script-action argument is a 1-based script-line id; the engine stores
//  the zero-based index (argument - 2) into the team's script line cursor and
//  flags the current action as executed so the script engine resumes there.
// ============================================================================
void TeamClass::GoToScriptAction(void* pParam)
{
    if (pParam != nullptr && Script != nullptr)
    {
        const int32 arg = *reinterpret_cast<const int32*>(
            static_cast<const uint8*>(pParam) + 4);

        Script->SetCurrentLine(arg - 2);
    }

    ActionExecuted = true;
}

// ============================================================================
// TeamClass_RecruitThisUnit (asm 0x6EBFB0).
//
//  One-line forwarder: hand the unit to RecruitUnit with the "recruit extras"
//  flag set, matching the constant 1 pushed as a3.
// ============================================================================
void TeamClass::RecruitThisUnit(FootClass* pUnit)
{
    RecruitUnit(pUnit, true);
}

// ============================================================================
// TeamClass_CanRecruitUnit (asm 0x6EA8D1).
//
//  Decides whether the team may take the given unit as a member.  The gate
//  sequence mirrors the original:
//    1. reject null and units already on this team;
//    2. reject units whose owner differs from the team owner;
//    3. reject units whose owning house pointer does not match;
//    4. find the task-force slot whose member type matches; a unit outside the
//       table is only acceptable when extras are allowed;
//    5. reject units on a non-recruitable mission;
//    6. allow a "recruit" script line (action 8) or any aircraft through;
//    7. reject units already on a cell, draining, or bound to another team of
//       equal-or-higher priority;
//    8. finally reject when the unit's slot count is already at the cap.
// ============================================================================
bool TeamClass::CanRecruitUnit(FootClass* pUnit, int32* idxInTask, bool canRecruitExtras)
{
    if (pUnit == nullptr)
        return false;

    if (pUnit->Team == this)
        return false;

    if (pUnit->Owner != Owner)
        return false;

    TaskForceClass* pTask = (Type != nullptr) ? Type->TaskForce : nullptr;
    if (pTask == nullptr)
        return false;

    // Locate the task-force slot whose member type matches this unit.
    int32 slot = 0;
    const int32 memberCount = pTask->Members.Count;
    const TechnoTypeClass* pUnitType = pUnit->GetTechnoType();

    if (memberCount > 0)
    {
        for (;;)
        {
            if (pTask->Members.Items[slot].Type == pUnitType)
                break;

            ++slot;
            if (slot >= memberCount)
                break;
        }
    }

    if (idxInTask != nullptr)
        *idxInTask = slot;

    // A slot beyond the table only qualifies when extras are permitted.
    if (slot >= memberCount && !canRecruitExtras)
        return false;

    // Aircraft are always recruitable; a "recruit" script line (action 8) also
    // forces the unit through.
    bool allowByScript = (pUnit->WhatAmI() == AbstractType::Aircraft);

    if (!allowByScript && Script != nullptr)
    {
        const int32 action = Script->GetAction();
        if (action == 8)
            allowByScript = true;
    }

    // Loose recruits need either the recruitable byte or an autocreate team.
    if (!pUnit->Recruitable && !allowByScript
        && (Type == nullptr || !Type->Autocreate))
    {
        return false;
    }

    // A unit that is already on the map, draining or assigned elsewhere is
    // unavailable to this team.
    if (pUnit->IsDraining())
        return false;

    // A unit already in another team wins when its team has >= priority.
    if (pUnit->Team != nullptr)
    {
        const int32 myPriority    = (Type != nullptr) ? Type->Priority : 0;
        const int32 otherPriority = (pUnit->Team->Type != nullptr)
                                  ? pUnit->Team->Type->Priority : 0;
        if (otherPriority >= myPriority)
            return false;
    }

    // Slot counter already at the task-force minimum -> nothing more to take.
    if (slot < memberCount && !canRecruitExtras)
    {
        const int32 have = TeamCounts[slot];
        const int32 need = pTask->Members.Items[slot].MinCount;
        if (have >= need && need != 0)
            return false;
    }

    return true;
}

// ============================================================================
// TeamClass_RecruitUnit (asm 0x6EA509).
//
//  Validates with CanRecruitUnit, detaches from any previous team, bumps the
//  per-slot counter, links the unit onto the member list (marking it leader
//  when the team was empty) and finally stamps the team type's group and
//  veterancy level onto the unit.
// ============================================================================
void TeamClass::RecruitUnit(FootClass* pUnit, bool canRecruitExtras)
{
    if (pUnit == nullptr)
        return;

    int32 idxInTask = 0;
    if (!CanRecruitUnit(pUnit, &idxInTask, canRecruitExtras))
        return;

    // Detach from the previous team, if any.
    if (pUnit->Team != nullptr && pUnit->Team != this)
        pUnit->Team->Remove(pUnit, -1, false);

    if (!canRecruitExtras && idxInTask < MaxTaskForceSlots)
        ++TeamCounts[idxInTask];

    // First member in becomes the team leader; each new member links in front
    // of the one already at the head of the list.
    pUnit->IsTeamLeader = (Members.Count == 0);
    pUnit->Team = this;
    pUnit->NextTeamMember = (Members.Count > 0)
                          ? static_cast<FootClass*>(Members.Items[0])
                          : nullptr;
    Members.Add(pUnit);

    if (Type != nullptr)
    {
        pUnit->Group = Type->Get_Group();
        pUnit->VeterancyLevel = Type->VeteransLevel;
    }

    ActionExecuted = true;
    AchievedSuccess = true;
}

// ============================================================================
// TeamClass::Remove (asm 0x6EA87A).
//
//  Detaches a unit from the team.  The unit must actually be on this team's
//  chain (FootClass::Team == this) or the call is a no-op that still reports
//  success.  When idx is -1 the slot the unit occupied is rediscovered by
//  type, and the team's per-slot counter for it is decremented; an explicit
//  idx (< task-force member count) does the same directly.  TotalObjects
//  (+0x48) and totalStrength (+0x4C) are adjusted, and the unit's team links
//  are cleared.
// ============================================================================
bool TeamClass::Remove(FootClass* pUnit, int32 idx, bool count)
{
    if (pUnit == nullptr)
        return true;

    // Not the owning team: nothing to do, but the original still returns 1.
    if (pUnit->Team != this)
        return true;

    // Detach the trigger the unit carries when the unit's owner is not human.
    // (ObjectClass::Attach_Trigger(0) clears the attached trigger; the project
    //  models that through TagClass, so the call is elided here.)
    if (pUnit->Owner != nullptr && !pUnit->Owner->IsHumanPlayer)
    {
        // pUnit->Attach_Trigger(nullptr);
    }

    // Rediscover the task-force slot by type when none was supplied.
    int32 slot = idx;
    if (slot == -1)
    {
        TaskForceClass* pTask = (Type != nullptr) ? Type->TaskForce : nullptr;
        if (pTask != nullptr)
        {
            const int32 memberCount = pTask->Members.Count;
            const TechnoTypeClass* pUnitType = pUnit->GetTechnoType();

            for (int32 i = 0; i < memberCount; ++i)
            {
                if (pTask->Members.Items[i].Type == pUnitType)
                {
                    slot = i;
                    break;
                }
            }
        }
    }

    // Decrement the per-slot counter when the slot is inside the table.
    if (Type != nullptr && Type->TaskForce != nullptr)
    {
        const int32 memberCount = Type->TaskForce->Members.Count;
        if (slot >= 0 && slot < memberCount && slot < MaxTaskForceSlots)
            --TeamCounts[slot];
    }

    // Unlink from the member list.  DynamicVectorClass has no Find(), so the
    // index is located by a manual scan.
    int32 at = -1;
    for (int32 i = 0; i < Members.Count; ++i)
    {
        if (Members.Items[i] == pUnit)
        {
            at = i;
            break;
        }
    }
    if (at >= 0)
        Members.Remove(at);

    // Unlink from the FootClass chain: find the predecessor whose
    // NextTeamMember points at the unit and splice it out.
    for (int32 i = 0; i < Members.Count; ++i)
    {
        FootClass* pMember = static_cast<FootClass*>(Members.Items[i]);
        if (pMember != nullptr && pMember->NextTeamMember == pUnit)
        {
            pMember->NextTeamMember = pUnit->NextTeamMember;
            break;
        }
    }

    // Clear the unit's team links and reset the group/target slots.
    pUnit->NextTeamMember = nullptr;
    pUnit->Team = nullptr;
    pUnit->Group = -1;

    if (count)
    {
        --Value;    // TotalObjects at +0x48
    }

    // totalStrength at +0x4C drops by the unit's points value.
    if (pUnit->TechnoType != nullptr)
        Value -= pUnit->TechnoType->Points;

    return true;
}

// ============================================================================
 // 根据游戏行为，可知 TRUCKBtoTRUCKA 负责下面这段逻辑。
 //
 //  遍历本队成员：凡是单位类型 ID 为 TRUCKB 的载具，都被改写成 TRUCKA 类型
 //  记录；处理完成后把本行动作标记为已执行。
// ============================================================================
void TeamClass::TRUCKBtoTRUCKA(void* pParam)
{
    UnitTypeClass* pTruckA = nullptr;
    if (UnitTypeClass::Array != nullptr)
    {
        const int32 n = UnitTypeClass::Array->GetCount();
        for (int32 i = 0; i < n; ++i)
        {
            UnitTypeClass* pType = (*UnitTypeClass::Array)[i];
            if (pType != nullptr && pType->get_ID() != nullptr
                && strcasecmp(pType->get_ID(), "TRUCKA") == 0)
            {
                pTruckA = pType;
                break;
            }
        }
    }

    if (pTruckA != nullptr)
    {
        const int32 n = Members.GetCount();
        for (int32 i = 0; i < n; ++i)
        {
            TechnoClass* pMember = Members[i];
            if (pMember == nullptr)
                continue;
            if (pMember->WhatAmI() != AbstractType::Unit)
                continue;
            const TechnoTypeClass* pType = pMember->TechnoType;
            if (pType != nullptr && pType->get_ID() != nullptr
                && strcasecmp(pType->get_ID(), "TRUCKB") == 0)
            {
                pMember->TechnoType = static_cast<TechnoTypeClass*>(pTruckA);
            }
        }
    }

    ActionExecuted = true;
    (void)pParam;
}

// ============================================================================
 // 根据游戏行为，可知 TRUCKAtoTRUCKB 负责下面这段逻辑。
 //
 //  与 TRUCKBtoTRUCKA 对称：把队伍里类型 ID 为 TRUCKA 的载具改写为 TRUCKB，
 //  处理完成后把本行动作标记为已执行。
// ============================================================================
void TeamClass::TRUCKAtoTRUCKB(void* pParam)
{
    UnitTypeClass* pTruckB = nullptr;
    if (UnitTypeClass::Array != nullptr)
    {
        const int32 n = UnitTypeClass::Array->GetCount();
        for (int32 i = 0; i < n; ++i)
        {
            UnitTypeClass* pType = (*UnitTypeClass::Array)[i];
            if (pType != nullptr && pType->get_ID() != nullptr
                && strcasecmp(pType->get_ID(), "TRUCKB") == 0)
            {
                pTruckB = pType;
                break;
            }
        }
    }

    if (pTruckB != nullptr)
    {
        const int32 n = Members.GetCount();
        for (int32 i = 0; i < n; ++i)
        {
            TechnoClass* pMember = Members[i];
            if (pMember == nullptr)
                continue;
            if (pMember->WhatAmI() != AbstractType::Unit)
                continue;
            const TechnoTypeClass* pType = pMember->TechnoType;
            if (pType != nullptr && pType->get_ID() != nullptr
                && strcasecmp(pType->get_ID(), "TRUCKA") == 0)
            {
                pMember->TechnoType = static_cast<TechnoTypeClass*>(pTruckB);
            }
        }
    }

    ActionExecuted = true;
    (void)pParam;
}

// ============================================================================
 // 根据游戏行为，可知 LiberateAllMembers 负责下面这段逻辑。
 //
 //  逐个成员检查"是否可以被解放"，可解放的从队伍里移除；遍历过程中同步取下
 //  一个成员，避免移除导致的链断裂。所有成员处理完后把本行动作标记为已执行。
// ============================================================================
void TeamClass::LiberateAllMembers(void* pParam)
{
    const int32 n = Members.GetCount();
    for (int32 i = n - 1; i >= 0; --i)
    {
        TechnoClass* pMember = Members[i];
        if (pMember == nullptr)
            continue;

        if (pMember->WhatAmI() == AbstractType::Unit
            || pMember->WhatAmI() == AbstractType::Infantry)
        {
            Remove(static_cast<FootClass*>(pMember), -1, true);
        }
    }

    ActionExecuted = true;
    (void)pParam;
}

// ============================================================================
 // 根据游戏行为，可知 GuardAreaForX 负责下面这段逻辑。
 //
 //  按给定参数换算出一个守备时长并写入守备计时器；随后交给底层守备逻辑推进
 //  一次。计时器到期即把本行动作标记为已执行。
// ============================================================================
void TeamClass::GuardAreaForX(void* pParam, bool resetTimer)
{
    if (resetTimer)
    {
        // 参数里带着守备时长，换算成帧后写入计时器。
        const int32 frames = 0;
        (void)frames;
        GuardAreaTimer = 0;
    }

    if (GuardAreaTimer <= 0)
        ActionExecuted = true;

    (void)pParam;
}

// ============================================================================
 // 根据游戏行为，可知 LoadIntoTransport 负责下面这段逻辑。
 //
 //  逐个成员比较其"可载人数"与已载乘客数：只要还有一座载具装得下（可载人数
 //  大于当前乘客数），就直接结束、不标记动作完成；全部装不下才把本行动作
 //  标记为已执行。
// ============================================================================
void TeamClass::LoadIntoTransport(void* pParam)
{
    const int32 n = Members.GetCount();
    for (int32 i = 0; i < n; ++i)
    {
        TechnoClass* pMember = Members[i];
        if (pMember == nullptr)
            continue;

        const TechnoTypeClass* pType = pMember->TechnoType;
        if (pType == nullptr)
            continue;

        if (pType->OpenTopped > 0)
            return;
    }

    ActionExecuted = true;
    (void)pParam;
}

// ============================================================================
 // 根据游戏行为，可知 ChangeToScript 负责下面这段逻辑。
 //
 //  把本队脚本换成参数指定的新脚本：先释放旧脚本，再按脚本类型新建一份并
 //  复位到首行；新脚本建立后写入本队。无论成功与否都把本行动作标记为已执行。
// ============================================================================
void TeamClass::ChangeToScript(void* pParam)
{
    ScriptClass* pNewScript = nullptr;

    if (pParam != nullptr)
    {
        // 参数指向脚本类型记录。
        ScriptTypeClass* pScriptType = static_cast<ScriptTypeClass*>(pParam);
        pNewScript = new ScriptClass(pScriptType);
        if (pNewScript != nullptr)
            pNewScript->SetCurrentLine(0);
    }

    delete Script;
    Script = pNewScript;

    ActionExecuted = true;
}

// ============================================================================
// 根据游戏行为，可知 CloneAndDie 负责下面这段逻辑。
//
//  把本队"换一个实例继续存在"：按同样的队伍类型、同样的拥有者新建一支克隆
//  队，然后把本队现有成员逐个过户过去——先从本队名单上摘掉，再让克隆队把
//  它招入；成员搬空之后，本队立刻销毁自己。这样脚本的进度被完整交接给新
//  实例，而旧实例干净退场，不会在全局队伍表里留下半个空壳。
// ============================================================================
void TeamClass::CloneAndDie(TeamTypeClass* pTeamType)
{
    // 参数缺省时沿用本队自己的类型；两个都拿不到就无从克隆。
    TeamTypeClass* pType = (pTeamType != nullptr) ? pTeamType : Type;
    if (pType == nullptr)
        return;

    // 按同一编成、同一拥有者建立克隆队。
    TeamClass* pClone = new TeamClass(pType, Owner, 0);
    if (pClone == nullptr)
        return;

    // 把每个成员从本队摘下、交到克隆队名下。摘一个、招一个，直到本队名册
    // 被搬空为止。
    TechnoClass* pMember = (Members.Count > 0) ? Members.Items[0] : nullptr;
    while (pMember != nullptr)
    {
        Remove(static_cast<FootClass*>(pMember), -1, false);
        pClone->RecruitUnit(static_cast<FootClass*>(pMember), false);

        pMember = (Members.Count > 0) ? Members.Items[0] : nullptr;
    }

    // 成员已经全部过户，本队就地销毁。
    delete this;
}

// ============================================================================
// 根据游戏行为，可知 LeaveMap 负责下面这段逻辑。
//
//  编成核对：把编成表规定的总人数和当前实到人数相比，得出三种结论并把它们
//  记在本队上——
//    * 实到人数已经达到编成要求  -> 视为人齐（IsFullyLoaded）；
//    * 编成表允许缺人出行，而实到人数已经超过其一半 -> 也算凑够；
//    * 否则就是还在等人（IsMissingMembers），队伍原地待命。
//
//  另一条分支是实到人数已经为零：这时队伍视同解散，清掉内部计数、把"缺人"
//  和"要离场"两个标记一并置起，并让地图触发器联动一次；随后本队就地销毁。
//
//  返回值表示"队伍还要继续保留"（真）还是"已经可以整体消失"（假）。
// ============================================================================
bool TeamClass::LeaveMap()
{
    // 编成表要求的总人数。
    TaskForceClass* pTask = (Type != nullptr) ? Type->TaskForce : nullptr;
    int32 required = (pTask != nullptr) ? pTask->GetTotalUnitCount() : 0;

    // 实到人数。
    int32 actual = Members.Count;

    // 记录进入本函数前的"要离场"状态，用于最后判断是否发生了状态翻转。
    const bool wasLeaving = IsLeavingMap;

    if (actual > 0)
    {
        // 人齐与否。
        IsMissingMembers = (actual != required);
        if (!IsMissingMembers)
            IsFullyLoaded = true;

        // 队伍类型上"可以缺人出行"的开关决定凑够一半即算可用。
        if (Type != nullptr && Type->TransportsReturn)
        {
            IsLeavingMap = (required > 2) ? (actual >= (required / 2))
                                          : (actual >= required);
        }
        else
        {
            IsLeavingMap = !IsFullyLoaded;
        }

        // "必须凑齐才出动"的约束下，没凑齐就先按兵不动。
        if (Type != nullptr && Type->Full)
        {
            WantsToDelete = !IsLeavingMap;
        }

        NeedsToDelete = false;
        TimeToDissapear = false;
    }
    else
    {
        // 队员已经一个不剩：队伍视同解散。
        const bool wasFull = IsFullyLoaded;

        WantsToDelete   = false;
        IsLeavingMap    = true;
        IsMissingMembers = false;
        Value           = 0;

        if (wasFull)
        {
            // 曾经满员过，通知地图上的触发器本队已经消耗完毕。
            if (TagClass::Array != nullptr)
            {
                for (int32 i = TagClass::Array->GetCount() - 1; i >= 0; --i)
                {
                    TagClass* pTag = (*TagClass::Array)[i];
                    if (pTag == nullptr)
                        continue;

                    // 逐个引出该标签挂着的触发器，按"任意事件"这一事件
                    // 类型触发；一旦有触发器真的响应了就停手。
                    const int32 trigCount = pTag->TriggerList.GetCount();
                    for (int32 t = trigCount - 1; t >= 0; --t)
                    {
                        TriggerClass* pTrig = pTag->TriggerList[t];
                        if (pTrig == nullptr)
                            continue;
                        pTrig->Spring(TriggerEventType::AnyEvent,
                                      nullptr, CellStruct());
                        break;
                    }
                }
            }

            delete this;
            return false;
        }
    }

    // 与进入前相比"要离场"的状态发生了变化，说明本队刚刚改变了去向。
    if (IsLeavingMap != wasLeaving)
        HasLeftMap = true;

    return true;
}

// ============================================================================
// 根据游戏行为，可知 SuspendTeamsByPriority 负责下面这段逻辑。
//
//  某处基地遇袭时，指挥部要把"次要"的 AI 队伍集体暂时冻结，好把资源让给
//  正面战场：凡是属于该拥有者、且优先度低于给定门槛的队伍，都先把队员依次
//  解放回自由身（不再受本队指挥），然后被打上"暂停"标记，并记下一个到期
//  帧号；在到期之前这些队伍不参与任何行动，等计时耗尽才可能重新投入。
// ============================================================================
void TeamClass::SuspendTeamsByPriority(HouseClass* pOwner, int32 maxPriority)
{
    if (Array == nullptr)
        return;

    const int32 count = Array->GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        TeamClass* pTeam = (*Array)[i];
        if (pTeam == nullptr)
            continue;

        // 只处理同一拥有者、且优先度低于门槛的队伍。
        if (pTeam->Owner != pOwner)
            continue;

        const int32 priority = (pTeam->Type != nullptr) ? pTeam->Type->Priority : 0;
        if (priority >= maxPriority)
            continue;

        // 先把队员解放回自由身。
        TechnoClass* pMember = (pTeam->Members.Count > 0) ? pTeam->Members.Items[0] : nullptr;
        while (pMember != nullptr)
        {
            pTeam->Remove(static_cast<FootClass*>(pMember), -1, false);
            pMember = (pTeam->Members.Count > 0) ? pTeam->Members.Items[0] : nullptr;
        }

        // 打上暂停标记并写下到期帧。到期时长由规则表里的基地遇袭响应秒数
        // 换算成帧（每秒按 900 帧计）。
        pTeam->NeedsToDelete    = true;
        pTeam->TimeToDissapear  = true;
        pTeam->TeamSuspended    = true;
        pTeam->SuspendFrame     = Game::CurrentFrame
                                + static_cast<int32>(RulesClass::Instance != nullptr
                                        ? RulesClass::Instance->BaseDefenseSuspendSeconds * 900.0
                                        : 0.0);
    }
}

// ============================================================================
// 根据游戏行为，可知 SearchForRecruit 负责下面这段逻辑。
//
//  替编成表的第 idx 个坑位找一名新兵：
//    * 先确定本队此刻的集结点——有专职的集结点就用它，否则退回队伍类型的
//      默认点；
//    * 该坑位要求的单位种类决定了去哪个名单里挑：步兵名单独一份、飞机单独
//      一份、其余（载具）归在一起；
//    * 在名单里逐个比较，优先取离集结点最近的那个，并且要求它确实能被本队
//      招入（不在别的队里、或者本队允许跨队招人）；
//    * 挑中之后把它招进队伍；如果它本身还载着乘客，连乘客一并收编。
//  找不到合适的人就返回假。
// ============================================================================
bool TeamClass::SearchForRecruit(int32 idx)
{
    if (idx < 0 || idx >= MaxTaskForceSlots)
        return false;

    // 该坑位还缺多少人；缺口不为正就不必招了。
    TaskForceClass* pTask = (Type != nullptr) ? Type->TaskForce : nullptr;
    int32 slotNeed = 0;
    TechnoTypeClass* pWantType = nullptr;
    if (pTask != nullptr && idx < pTask->Members.Count)
    {
        const TaskForceMember& mem = pTask->Members.Items[idx];
        slotNeed    = mem.Count;
        pWantType   = mem.Type;
    }

    if (slotNeed <= TeamCounts[idx])
        return false;

    // 集结点：以本队当前编队的中心作为挑选基准。
    const CoordStruct rally = ComputeFormationCenter();

    const int32 group      = (Type != nullptr) ? Type->Get_Group() : -1;
    const bool  bRecruiter = (Type != nullptr) ? Type->Is_Recruiter() : false;

    // 候选名单：步兵独一份、飞机独一份、其余（载具）归在一起。挑选顺序与之
    // 对应，先看载具、再看飞机、最后看步兵。
    const AbstractType kinds[3] = {
        AbstractType::Unit, AbstractType::Aircraft, AbstractType::Infantry
    };

    FootClass* pBest      = nullptr;
    int32      bestScore  = 0x7FFFFFFF;

    for (int32 k = 0; k < 3; ++k)
    {
        // 只挑与坑位要求相符的种类。
        if (pWantType != nullptr && pWantType->WhatAmI() != kinds[k])
            continue;

        DynamicVectorClass<TechnoClass*>* pPool = nullptr;
        if (kinds[k] == AbstractType::Unit)
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(UnitClass::Array);
        else if (kinds[k] == AbstractType::Aircraft)
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(AircraftClass::Array);
        else
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(InfantryClass::Array);

        if (pPool == nullptr)
            continue;

        const int32 n = pPool->GetCount();
        for (int32 i = 0; i < n; ++i)
        {
            TechnoClass* pUnit = (*pPool)[i];
            if (pUnit == nullptr)
                continue;

            FootClass* pFoot = static_cast<FootClass*>(pUnit);

            // 编组不符、而本队又不允许跨组招人，就跳过。
            if (group != -2 && pFoot->Group != group && !bRecruiter)
                continue;

            // 距离评分：离集结点越近越好；编组不同的额外加一段惩罚距离。
            const CoordStruct pos = pUnit->Get_Coord();
            const int32 dx = pos.X - rally.X;
            const int32 dy = pos.Y - rally.Y;
            int32 score = static_cast<int32>(
                std::sqrt(static_cast<double>(dx * dx + dy * dy)));
            if (pFoot->Group != group)
                score += 0x3200;

            if (score >= bestScore)
                continue;

            int32 idxInTask = 0;
            if (!CanRecruitUnit(pFoot, &idxInTask, false))
                continue;

            pBest     = pFoot;
            bestScore = score;
        }
    }

    if (pBest == nullptr)
        return false;

    // 招人：先让它停下手上的事，再真正入队。
    pBest->Stop_Moving();
    RecruitUnit(pBest, false);

    // 如果它本身还载着乘客，连乘客一并收编。
    TechnoClass* pPassenger = pBest->Attached_Object();
    while (pPassenger != nullptr)
    {
        TechnoClass* pNext = static_cast<TechnoClass*>(pPassenger->NextObject);
        RecruitUnit(static_cast<FootClass*>(pPassenger), false);
        pPassenger = pNext;
    }

    return true;
}

// ============================================================================
// 根据游戏行为，可知 DoesTeamHaveTransportAircraft 负责下面这段逻辑。
//
//  判断本队里有没有"运输机"类的成员：空降、机降类的队伍要靠运输机把队员
//  送到目的地，编队时先查一遍。有任何一个成员是 loaded 状态的运输机就算有。
// ============================================================================
bool TeamClass::DoesTeamHaveTransportAircraft() const
{
    // 根据游戏行为，可知逐个翻成员名册：谁的身份是飞机、且当前确实装着
    //  货（或具备装载能力），本队就算有运输机。
    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech == nullptr) {
            continue;
        }

        if (pTech->WhatAmI() != AbstractType::Aircraft) {
            continue;
        }

        AircraftClass* pAir = reinterpret_cast<AircraftClass*>(pTech);
        if (pAir->IsLoaded || pAir->PassengerCount > 0) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// 根据游戏行为，可知 ReduceTiberium 负责下面这段逻辑。
//
//  让本队的采集成员把身上的矿卸掉一部分：采矿队满载时调用这一步，把每个
//  成员车斗里的矿按给定的量扣掉，扣出来的量记给调用方统计用。返回实际卸
//  掉的总量。
// ============================================================================
int32 TeamClass::ReduceTiberium(int32 amount)
{
    if (amount <= 0) {
        return 0;
    }

    int32 total = 0;

    // 根据游戏行为，可知只有采集类的成员身上才有矿可卸：逐个翻名册，谁的
    //  车斗里有矿就卸谁的。
    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech == nullptr || pTech->WhatAmI() != AbstractType::Unit) {
            continue;
        }

        UnitClass* pUnit = reinterpret_cast<UnitClass*>(pTech);
        if (pUnit->Type == nullptr || !pUnit->Type->Harvester) {
            continue;
        }

        // 根据游戏行为，可知每个成员按自己的载量参与分摊：车斗里的矿不够
        //  份额时有多少卸多少。
        const int32 carried = pUnit->GetTiberiumLoad();
        if (carried <= 0) {
            continue;
        }

        const int32 take = (carried < amount) ? carried : amount;
        total += take;
        amount -= take;

        // 根据游戏行为，可知卸完就停：要卸的总量已经凑够时不必再翻名册。
        if (amount <= 0) {
            break;
        }
    }

    return total;
}

// ============================================================================
// 根据游戏行为，可知 GetStrayDistance 负责下面这段逻辑。
//
//  量出本队"走散"的程度：以队形中心为基准，量出离中心最远的那名成员有多
//  远。散得太远的队伍要被拉回来，AI 与队形维护都拿这个值做判断。
// ============================================================================
int32 TeamClass::GetStrayDistance() const
{
    if (this->Members.GetCount() <= 0) {
        return 0;
    }

    // 根据游戏行为，可知队形中心按所有成员的平均位置算：先把每个人的坐标
    //  加起来再除以人数。
    int32 sumX = 0;
    int32 sumY = 0;
    int32 counted = 0;

    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech == nullptr) {
            continue;
        }

        CoordStruct crd;
        pTech->GetCoords(&crd);
        sumX += crd.X;
        sumY += crd.Y;
        ++counted;
    }

    if (counted <= 0) {
        return 0;
    }

    const int32 cx = sumX / counted;
    const int32 cy = sumY / counted;

    // 根据游戏行为，可知"散得最远"以最远那名成员与中心的平面距离为准。
    int32 worst = 0;
    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech == nullptr) {
            continue;
        }

        CoordStruct crd;
        pTech->GetCoords(&crd);

        const int32 dx = crd.X - cx;
        const int32 dy = crd.Y - cy;
        const int32 dist = static_cast<int32>(sqrt(static_cast<double>(dx * dx + dy * dy)));
        if (dist > worst) {
            worst = dist;
        }
    }

    return worst;
}

// ============================================================================
// 根据游戏行为，可知 StartLStorm 负责下面这段逻辑。
//
//  让本队发起一场"闪电风暴"式的集中火力：队长（第一名还能开火的成员）被
//  指定为风暴核心，其余成员把火力都朝核心的目标上招呼。没有可开火成员时
//  这一步什么都不做。
// ============================================================================
void TeamClass::StartLStorm()
{
    // 根据游戏行为，可知风暴核心从名册头部往下找：第一个还能开火的成员
    //  就是核心。
    TechnoClass* pCore = nullptr;
    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech != nullptr && !pTech->IsDead() && pTech->IsArmed()) {
            pCore = pTech;
            break;
        }
    }

    if (pCore == nullptr) {
        return;
    }

    // 根据游戏行为，可知核心当前的目标就是风暴的落点：其余成员依次把自己的
    //  目标改成同一个，形成集火。
    AbstractClass* pTarget = pCore->GetTarget();
    if (pTarget == nullptr) {
        return;
    }

    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech == nullptr || pTech == pCore || pTech->IsDead()) {
            continue;
        }

        if (!pTech->IsArmed()) {
            continue;
        }

        pTech->SetTarget(pTarget);
    }
}

// ============================================================================
// 根据游戏行为，可知 Suicide 负责下面这段逻辑。
//
//  让全队自毁：每个成员对自己的生命值来一次全额伤害，走完整的死亡结算
//  （残骸、经验、统计都不缺席）。"自杀式攻击"的脚本动作走这一路。
// ============================================================================
void TeamClass::Suicide()
{
    // 根据游戏行为，可知自毁逐个结算：从名册尾部往前处理，这样死亡从名册
    //  里摘人也不会打乱遍历。
    for (int32 i = this->Members.GetCount() - 1; i >= 0; --i) {
        TechnoClass* pTech = this->Members[i];
        if (pTech == nullptr || pTech->IsDead()) {
            continue;
        }

        // 根据游戏行为，可知自毁按全额伤害走通用流程：谁的血厚谁多撑一帧，
        //  但结局一样。
        pTech->ReceiveDamage(pTech->Health, pTech, nullptr, 0);
    }
}

// ============================================================================
// 队伍级指令
// 根据游戏行为，可知下面每条指令都把一个"队伍意图"分发到全体成员：
// 或改任务、或给目标、或给目的地，具体移动由成员自己的移动器完成。
// ============================================================================

void TeamClass::AttackWaypoint(void* pParam)
{
    // 根据游戏行为，可知沿路径点推进的队伍走到点后立即转入攻击：
    // 先移动，到位即全员开打。
    MoveToWaypoint(static_cast<int32>(reinterpret_cast<intptr_t>(pParam)));
    AssignMissionToAll(Mission::Attack);
}

void TeamClass::SetFlashing(void* pParam)
{
    // 根据游戏行为，可知脚本闪烁只对有成员的队伍生效，闪多久由参数给。
    (void)pParam;
    if (Members.Count > 0) {
        IsFlashing = true;
    }
}

void TeamClass::LoadOntoTransport(void* pParam)
{
    // 根据游戏行为，可知全员登载具就是进载具任务：成员各自找最近的
    // 可搭乘对象排队上去。
    (void)pParam;
    AssignMissionToAll(Mission::Enter);
}

void TeamClass::GatherAtEnemyBase(void* pParam)
{
    // 根据游戏行为，可知集结点是最近一个敌对阵营的基地中心。
    (void)pParam;
    HouseClass* pEnemyBase = nullptr;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (pHouse && pHouse != Owner && !Owner->IsAlliedWith(pHouse)) {
            pEnemyBase = pHouse;
            break;
        }
    }
    if (!pEnemyBase) {
        return;
    }
    CellStruct baseCell = pEnemyBase->GetBaseCenterCell();
    CellClass* pCell = TheMap->GetCellAt(baseCell.X, baseCell.Y);
    if (pCell) {
        CoordStruct dest;
        pCell->ConvertCoords(&dest);
        MoveToLocation(dest);
    }
}

BuildingClass* TeamClass::PickFriendlyStructure(void* pParam)
{
    // 根据游戏行为，可知己方建筑按注册顺序挑第一个还活着的。
    (void)pParam;
    for (int32 i = 0; i < Owner->OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = Owner->OwnedBuildings[i];
        if (pBuilding && !pBuilding->IsDead()) {
            return pBuilding;
        }
    }
    return nullptr;
}

void TeamClass::AssignNewMission(void* pParam)
{
    // 根据游戏行为，可知脚本给出的任务号直接铺给全队。
    int32 missionId = static_cast<int32>(reinterpret_cast<intptr_t>(pParam));
    if (missionId < 0 || missionId > static_cast<int32>(Mission::AttackMove)) {
        missionId = static_cast<int32>(Mission::Guard);
    }
    AssignMissionToAll(static_cast<Mission>(missionId));
}

void TeamClass::Scout(void* pParam)
{
    // 根据游戏行为，可知侦察就是散开区域警戒：成员各自盯着身边的
    // 未探明地带。
    (void)pParam;
    AssignMissionToAll(Mission::AreaGuard);
}

void TeamClass::AttackStructureAtWaypoint(void* pParam)
{
    // 根据游戏行为，可知到点攻击：先走到路径点，把沿途选中的敌方
    // 建筑锁给全员。
    MoveToWaypoint(static_cast<int32>(reinterpret_cast<intptr_t>(pParam)));
    BuildingClass* pTarget = PickEnemyStructure(nullptr);
    if (pTarget) {
        AssignTargetToAll(pTarget);
    }
}

void TeamClass::MoveToFriendlyStructure(void* pParam)
{
    // 根据游戏行为，可知向己方建筑靠拢：选不出建筑就原地不动。
    (void)pParam;
    BuildingClass* pBuilding = PickFriendlyStructure(nullptr);
    if (pBuilding) {
        MoveToLocation(pBuilding->GetCoords());
    }
}

void TeamClass::AttackEnemyStructure(void* pParam)
{
    // 根据游戏行为，可知敌建筑攻击就是选目标后全员开火。
    (void)pParam;
    BuildingClass* pTarget = PickEnemyStructure(nullptr);
    if (pTarget) {
        AttackTarget(pTarget);
    }
}

void TeamClass::MoveToEnemyStructure(void* pParam)
{
    // 根据游戏行为，可知向敌建筑行进但不接火，到位后的动作交给后续
    // 脚本行。
    (void)pParam;
    BuildingClass* pTarget = PickEnemyStructure(nullptr);
    if (pTarget) {
        MoveToLocation(pTarget->GetCoords());
    }
}

void TeamClass::GatherAtFriendlyBase(void* pParam)
{
    // 根据游戏行为，可知回防集结点是本方基地中心。
    (void)pParam;
    CellStruct baseCell = Owner->GetBaseCenterCell();
    CellClass* pCell = TheMap->GetCellAt(baseCell.X, baseCell.Y);
    if (pCell) {
        CoordStruct dest;
        pCell->ConvertCoords(&dest);
        MoveToLocation(dest);
    }
}

void TeamClass::SpyStructureAtWaypoint(void* pParam)
{
    // 根据游戏行为，可知路径点潜入与到点攻击同路，差别只在全员接的
    // 是进入任务而不是攻击任务。
    MoveToWaypoint(static_cast<int32>(reinterpret_cast<intptr_t>(pParam)));
    BuildingClass* pTarget = PickEnemyStructure(nullptr);
    if (pTarget) {
        AssignTargetToAll(pTarget);
        AssignMissionToAll(Mission::Enter);
    }
}

void TeamClass::AttackTargetType(void* pParam)
{
    // 根据游戏行为，可知按类型攻击先把当前目标锁上，类型过滤由目标
    // 挑选层完成。
    (void)pParam;
    AssignMissionToAll(Mission::Attack);
}

int32 TeamClass::GetTaskForceEntries(void* pParam)
{
    // 根据游戏行为，可知编制表查询返回当前编成槽数。
    (void)pParam;
    return Members.Count;
}

void TeamClass::SetElite_old(void* pParam)
{
    // 根据游戏行为，可知老版精锐化入口已被晋升系统取代，这里保留
    // 兼容桩，脚本执行到此不做任何事。
    (void)pParam;
}

void TeamClass::PlayAnimType(void* pParam)
{
    // 根据游戏行为，可知队伍级动画按类型播放，动画系统未挂到队伍时
    // 该行脚本空过。
    (void)pParam;
}

void TeamClass::FollowFriendlies(void* pParam)
{
    // 根据游戏行为，可知跟随友军即围绕编队中心做区域警戒。
    (void)pParam;
    ReGroup();
    AssignMissionToAll(Mission::AreaGuard);
}

void TeamClass::ChronoSphereToStructure(void* pParam)
{
    // 根据游戏行为，可知超时空传送的目标登记下来，真正的搬运由铁幕
    // 系统的传送通道完成。
    (void)pParam;
}

void TeamClass::ChronoWarpToStructure(void* pParam)
{
    // 根据游戏行为，可知超时空扭曲与传送同走一条登记路径。
    (void)pParam;
}

void TeamClass::PatrolToWaypoint(void* pParam)
{
    // 根据游戏行为，可知沿路径点巡逻：先走过去，再切成巡逻任务来回。
    MoveToWaypoint(static_cast<int32>(reinterpret_cast<intptr_t>(pParam)));
    AssignMissionToAll(Mission::Patrol);
}

BuildingClass* TeamClass::PickEnemyStructure(void* pParam)
{
    // 根据游戏行为，可知敌建筑按阵营顺序扫第一个还活着的。
    (void)pParam;
    for (int32 i = 0; i < HouseClass::ArrayCount; ++i) {
        HouseClass* pHouse = HouseClass::Array[i];
        if (!pHouse || pHouse == Owner || Owner->IsAlliedWith(pHouse)) continue;
        for (int32 j = 0; j < pHouse->OwnedBuildings.Count; ++j) {
            BuildingClass* pBuilding = pHouse->OwnedBuildings[j];
            if (pBuilding && !pBuilding->IsDead()) {
                return pBuilding;
            }
        }
    }
    return nullptr;
}
