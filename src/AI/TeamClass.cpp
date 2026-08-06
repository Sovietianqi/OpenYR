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
#include "../Abstract/TechnoClass.h"
#include "../Abstract/FootClass.h"
#include "../Abstract/TechnoTypeClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/AircraftClass.h"
#include "../Houses/HouseClass.h"
#include "../Game/Game.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Math/CoordStruct.h"
#include "../INI/INIClass.h"

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
      totalStrength(0), idxTeam(0) {

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
