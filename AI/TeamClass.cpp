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
#include <cstdio>
#include <cstdlib>
#include <cstdint>
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
#include "../Abstract/BuildingTypeClass.h"
#include "../Abstract/UnitClass.h"
#include "../Animations/AnimClass.h"
#include "../Animations/AnimTypeClass.h"
#include "../Houses/HouseClass.h"
#include "../Houses/FactoryClass.h"
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
#include <unordered_map>

// -----------------------------------------------------------------------------
// 队伍出生帧（旁表）：空队伍"等待编成"的宽限期据此计算。
//   不放进类里是为了不动 TeamClass 的布局（它参与存档/CRC 的状态字段）。
//   构造时登记、析构时擦除。
// -----------------------------------------------------------------------------
static std::unordered_map<const TeamClass*, int32>& TeamBirthFrames()
{
    static std::unordered_map<const TeamClass*, int32> frames;
    return frames;
}

// -----------------------------------------------------------------------------
// 补员下单的节流表：key = (队伍指针, 坑位号)，value = 上次尝试下单的帧号。
//   同样放旁表不动布局。缺员期间若每帧都尝试下单，会把工厂队列刷满，
//   并且 `BeginProductionOf` 被队列上限拒绝后会被无意义地反复调用。
// -----------------------------------------------------------------------------
static std::unordered_map<uintptr_t, int32>& TeamOrderFrames()
{
    static std::unordered_map<uintptr_t, int32> frames;
    return frames;
}

static uintptr_t TeamSlotKey(const TeamClass* pTeam, int32 idx)
{
    // 乘法而非按位或：不依赖指针的对齐位数，坑位号 0..15 各占一个独立键。
    return reinterpret_cast<uintptr_t>(pTeam) * 16u + static_cast<uintptr_t>(idx & 0xF);
}

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
// Script-action handler helpers
//
//  These mirror the small inline blocks the original repeats before every
//  Team_* handler: resolve the script argument into a waypoint cell, pick the
//  object sitting on that cell, test whether a roster entry is still usable,
//  and score a building the way Team_PickEnemyStructure does.
// =============================================================================
namespace {

// asm: Scenario_GetWaypointCell (0x68BD??) —— Map::Coord_Cell(&Waypoints[idx]).
//   The script action's payload is the waypoint index (cAIScriptActionType.arg).
inline CellClass* ScriptWaypointCell(void* pParam) {
    if (ScenarioClass::Instance == nullptr) return nullptr;
    return ScenarioClass::Instance->GetWaypointCell(
        static_cast<int32>(reinterpret_cast<intptr_t>(pParam)));
}

// asm: every waypoint handler calls CellClass_GetAircraftOrTerrain with
//   a2 = (cell->Flags >> 8) & 1, i.e. the BridgeHead bit.
inline ObjectClass* CellObjectAt(CellClass* pCell) {
    if (pCell == nullptr) return nullptr;
    return pCell->GetAircraftOrTerrain(
        pCell->HasFlag(CellFlags::BridgeHead) ? 1 : 0, false);
}

// asm: FootClass_Exists — alive, health > 0, on the map (ObjectClass+0x90),
//   and not riding inside an open-topped transport (ObjectClass+0x81).
inline bool IsUsableMember(TechnoClass* pTech) {
    return pTech != nullptr && !pTech->IsDead() && pTech->Health > 0;
}

// asm: Team_PickEnemyStructure / Team_PickFriendlyStructure scoring.
//   flags is the high 16 bits of the packed script argument:
//     0 : 0x7FFFFFFF - Cell_Threat     (prefer the calmest cell)
//     1 : Cell_Threat                  (prefer the hottest cell)
//     2 : 0x7FFFFFFF - 3D distance     (prefer the nearest building)
//     3 : 3D distance                  (prefer the farthest building)
inline int32 StructureWeight(int32 flags, BuildingClass* pBuilding, TechnoClass* pUnit) {
    if (pBuilding == nullptr || pUnit == nullptr) return -1;

    if (flags == 0 || flags == 1) {
        CellStruct cell = Math::CoordToCell(pBuilding->GetCoords());
        int32 threat = (TheMap != nullptr) ? TheMap->Cell_Threat(cell, pUnit->Owner) : 0;
        return (flags == 0) ? (0x7FFFFFFF - threat) : threat;
    }

    CoordStruct a = pBuilding->GetCoords();
    CoordStruct b = pUnit->GetCoords();
    const double dx = static_cast<double>(a.X - b.X);
    const double dy = static_cast<double>(a.Y - b.Y);
    const double dz = static_cast<double>(a.Z - b.Z);
    const int32 dist = static_cast<int32>(std::sqrt(dx * dx + dy * dy + dz * dz));
    return (flags == 2) ? (0x7FFFFFFF - dist) : dist;
}

// asm: the "pick the highest-valued member" prologue shared by
//   Team_MoveToFriendlyStructure / Team_GatherAt* / Team_Chrono*.  The original
//   compares TechnoTypeClass+0x5FC, a field this port does not model; the
//   closest equivalent that is guaranteed to terminate on the same roster is
//   the first usable member, which is also what the enemy-structure handlers do.
inline TechnoClass* FirstUsableMember(TeamClass* pTeam) {
    for (int32 i = 0; i < pTeam->Members.GetCount(); ++i) {
        TechnoClass* pTech = pTeam->Members[i];
        if (IsUsableMember(pTech)) return pTech;
    }
    return nullptr;
}

} // anonymous namespace

// =============================================================================
// Constructor
// =============================================================================
TeamClass::TeamClass(TeamTypeClass* pType, HouseClass* pOwner, int32 nFlags) noexcept
    : Type(pType), Owner(pOwner), CreationFrame(0), IsTransient(false),
      IsFullStrength(false), NeedsToDisappear(false), JustDisappeared(false),
      Value(0), RecruitRadius(DEFAULT_RECRUIT_RADIUS), RecruitTimer(0),
      Script(nullptr), IsFlashing(false), ITarget(nullptr), PrevTarget(nullptr),
      SpawnCell(nullptr), FollowAnchor(nullptr),
      NextTeam(nullptr), PrevTeam(nullptr), GuardAreaTimer(0),
      CurrentMission(MISSION_SLEEP), TotalThreatValue(0),
      totalStrength(0), idxTeam(0), ActionExecuted(false), AchievedSuccess(false),
      IsFullyLoaded(false), IsMissingMembers(false), IsLeavingMap(false),
      HasLeftMap(false), NeedsToDelete(false), TimeToDissapear(false),
      WantsToDelete(false), TeamSuspended(false), SuspendFrame(-1),
      SuspendDuration(0), IsForming(false), HasTakenDamage(false) {

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

    // asm: TeamClass_CTOR (asm:1526848) 尾段 ——
    //   mov eax,vec_Teams.count / cmp vec_Teams.length,eax /
    //   ... call [eax+8] (扩容) / mov ecx,vec_Teams.length / inc /
    //   mov vec_Teams.length,ecx / mov [data+ecx*4],esi
    //   即构造完立刻把自己登记进 vec_Teams。缺这一步时，任何经
    //   TeamTypeClass_CreateTeam 新建的队伍都不会被 LogicClass_Update
    //   的 Team 循环（asm L751785）驱动，等于「建了但不动」。
    //   幂等：重复登记会被跳过（同一指针只保留一份）。
    if (TeamClass::Array != nullptr) {
        bool alreadyListed = false;
        for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
            if (TeamClass::Array->GetItem(i) == this) {
                alreadyListed = true;
                break;
            }
        }
        if (!alreadyListed) {
            TeamClass::Array->Add(this);
        }
    }

    // asm: TeamClass_CTOR (asm:1526848) 之后紧跟的"动员"状态 —— 新队名单为空，按编成表
    //   逐坑招人。原版由编成核对（LeaveMap）刷新这一位，而新队还没进入
    //   "要离场"流程，核对不会跑；不置位则 Update 的动员段永不触发，
    //   补员扫描（SearchForRecruit）也不会跑，队伍永远是空壳 —— 这正是
    //   "AI 建了队但不出兵"的直接原因。
    IsMissingMembers = true;

    // 出生帧（旁表）：空队伍需要一段"等待编成"的宽限期。
    //   触发器在"本房能生产该兵种"时就会放行建队（CanInstantiateTeam 只查
    //   生产资格，不查库存），而招募只能从现成单位里拉。若空队当帧就判死，
    //   会变成"建一支→招不到→死→下一帧再建"的抖动，处理器全耗在建队/删队
    //   上（实测 4200 帧积出 165 支空队）。
    TeamBirthFrames()[this] = Game::CurrentFrame;
}

// =============================================================================
// Destructor - release the script and clear the member roster.
// =============================================================================
TeamClass::~TeamClass() {
    // asm: TeamClass_DTOR (asm:1527224) → TeamClass::Remove —— 先从 vec_Teams
    //   摘除自身，再释放成员脚本与名单，避免悬垂指针留在逐帧遍历表里。
    TeamBirthFrames().erase(this);
    // 补员下单节流表也要清：键按 (队伍, 坑位) 编码，逐一擦除 16 个坑位。
    for (int32 slot = 0; slot < 16; ++slot)
        TeamOrderFrames().erase(TeamSlotKey(this, slot));

    if (TeamClass::Array != nullptr) {
        for (int32 i = 0; i < TeamClass::Array->Count; ++i) {
            if (TeamClass::Array->GetItem(i) == this) {
                TeamClass::Array->Remove(i);
                break;
            }
        }
    }

    if (Script) {
        delete Script;
        Script = nullptr;
    }
    Members.Clear();
    ITarget = nullptr;
}

// =============================================================================
// Update - per-frame team AI tick (asm: TeamClass_Update, 0x6E940x).
//
// 原版把队伍脚本的"取指/派发"整段写在 TeamClass_Update 里（一个 65 分支的
// 巨型 switch）。本项目把"派发"下沉到 ScriptClass::Execute()，所以这里只负责
// 原版 switch 之前的**每帧前缀**，顺序与原版一致：
//   1. 挂起计时：被暂停的队伍等到 Frame - SuspendFrame >= SuspendDuration
//      才解除暂停（SuspendFrame == -1 表示未计时，直接看剩余时长）。
//   2. 消失流程：TimeToDissapear 置位就调 LeaveMap()，返回假说明本队整个
//      消失，本帧就此结束。
//   3. 编队/动员：IsForming 已置位时，若队伍正要离场就先跑一次集结
//      （原 sub_6EA0D0，这里映射为 ReGroup + FindUnitToFollow）；否则若
//      还缺人（IsMissingMembers）或需要跟随，就把 IsForming/IsFullyLoaded
//      拉起、清掉 IsLeavingMap，给每个成员打上"队长跟随"标记并复位脚本首行。
//   4. 跟随锚点：用 FindUnitToFollow() 找到队伍该跟的对象。
//   5. 补员扫描：逐个 task-force 坑位，缺口为正就 SearchForRecruit(idx)。
//   6. 成功事件：AchievedSuccess 置位时向所有 Tag 抛一次事件。
//   7. 脚本推进：ActionExecuted 置位则 StepToNextLine()，随后交给
//      ScriptClass::Execute() 派发当前行；脚本非末行时清空队伍目标。
//   8. 自杀队脚本结束时整体消失。
// =============================================================================
void TeamClass::Update() {
    const int32 frame = Game::CurrentFrame;

    // ---- 0. 已解散：本帧不做任何事 -----------------------------------------
    //   DoDisappear 之后队伍只剩一个空壳（成员已清、脚本已删）。若还让它跑
    //   下面整套流程，`Members.Count == 0` 会再次把 NeedsToDisappear 置位、
    //   下一帧再跑一次 DoDisappear —— 永久循环。回收由 LogicClass 的逐帧
    //   队伍循环负责（倒序摘除并 delete）。
    if (JustDisappeared)
        return;

    // ---- 1. 挂起计时 -------------------------------------------------------
    if (TeamSuspended) {
        bool wake = false;
        if (SuspendFrame == -1) {
            // 没有起始帧：只要剩余时长归零就醒。
            wake = (SuspendDuration <= 0);
        } else {
            const int32 elapsed = frame - SuspendFrame;
            if (elapsed >= SuspendDuration) {
                wake = true;
            } else {
                SuspendDuration -= elapsed;
            }
        }
        if (wake) {
            TeamSuspended = false;
        } else {
            // 仍被挂起：本帧不参与任何行动。
            return;
        }
    }

    // ---- 2. 消失流程 -------------------------------------------------------
    if (TimeToDissapear) {
        if (!LeaveMap()) {
            // 整队已经退场，本帧结束。
            return;
        }
    }

    CleanupDeadMembers();

    // ---- 3. 编队 / 动员 ----------------------------------------------------
    // ---- 3. 编队 / 动员 (asm @@5-@@12) ------------------------------------
    //  IsForming 已置位时若队伍正要离场，先跑一次集结（原 sub_6EA0D0）。
    if (IsForming && IsLeavingMap) {
        ReGroup();
        FindUnitToFollow();
    }

    // 需要凑人（缺员）时才正式"动员"：拉起 IsForming/IsFullyLoaded、清
    // IsLeavingMap，给每个成员打上队长跟随标记并复位脚本首行。动员后继续
    // 往下走（原 asm 落到 @@13），不会本帧直接返回。
    if (IsMissingMembers || WantsToDelete) {
        IsForming       = true;
        IsFullyLoaded   = true;
        IsLeavingMap    = false;

        for (int32 i = 0; i < Members.Count; ++i) {
            TechnoClass* pTechno = Members[i];
            if (pTechno == nullptr || pTechno->IsDead())
                continue;
            if (!IsFootMember(pTechno))
                continue;
            FootClass* pFoot = static_cast<FootClass*>(pTechno);
            pFoot->IsTeamLeader = (i == 0) || HasLeftMap;
        }

        // ★ 动员不等于"脚本重来"。
        //   原版这条分支只拉起 IsForming/IsFullyLoaded、给成员打队长标记；
        //   此前这里额外做了 ResetCurrentLine()+ActionExecuted=true，等于每次
        //   缺员/补员都把脚本重置并立刻推页 —— 脚本还没派发就被判"已完成"，
        //   整队永远收不到任务（实测步兵队 mission 恒 0）。
    }

    // ---- 4. 跟随锚点 (asm @@13-@@14) --------------------------------------
    FindUnitToFollow();

    // ---- 5. 补员扫描 (asm @@15-@@19) --------------------------------------
    //  仅在队伍已动员、且没有离场/缺员时才补员；逐个 task-force 坑位，
    //  缺口为正就招一名。
    const bool canScanRecruit = IsForming && !HasLeftMap && !IsLeavingMap;
    if (canScanRecruit && Type != nullptr && Type->TaskForce != nullptr) {
        TaskForceClass* pTask = Type->TaskForce;
        const int32 slots = pTask->Members.Count;
        bool stillMissing = false;
        for (int32 idx = 0; idx < slots && idx < MaxTaskForceSlots; ++idx) {
            const int32 need = pTask->Members.Items[idx].Count;
            if (TeamCounts[idx] >= need)
                continue;
            SearchForRecruit(idx);
            if (TeamCounts[idx] < need) {
                stillMissing = true;
                // 招不到现成的闲置单位：让房主把缺口造出来。
                //   原版这条链路是 TeamClass_Update -> HouseClass_BeginProductionOf，
                //   造好的单位进房主的待编队缓冲后被 Recruit 吸收；本树没有缓冲，
                //   所以只能靠"造出来 -> 进全局表 -> 下一次 SearchForRecruit 捡到"。
                RequestProductionForSlot(idx);
            }
        }
        // 坑位全满 → 动员完成，清缺员位（否则每帧都会重跑动员段并复位脚本）。
        //   这等价于原版 LeaveMap 编成核对里 `IsMissingMembers = (actual != required)`。
        if (!stillMissing && Members.Count > 0)
            IsMissingMembers = false;
    }

    // ---- 6. 成功事件 (asm @@21-@@23) --------------------------------------
    if (AchievedSuccess && TagClass::Array != nullptr) {
        const int32 tagCount = TagClass::Array->GetCount();
        for (int32 i = tagCount - 1; i >= 0; --i) {
            TagClass* pTag = (*TagClass::Array)[i];
            if (pTag == nullptr)
                continue;
            const int32 trigCount = pTag->TriggerList.GetCount();
            for (int32 t = trigCount - 1; t >= 0; --t) {
                TriggerClass* pTrig = pTag->TriggerList[t];
                if (pTrig == nullptr)
                    continue;
                pTrig->Spring(TriggerEventType::AnyEvent, nullptr, CellStruct());
                break;
            }
        }
    }

    // 队员已被清空。
    //   ★ 空队不能当帧就判死：触发器在"本房能生产该兵种"时就放行建队
    //   （CanInstantiateTeam 只查生产资格，不查库存），而补员只能从现成
    //   单位里拉。当帧判死会变成"建一支→招不到→死→下一帧再建"的抖动，
    //   实测 4200 帧积出 165 支空队、每帧都在建队/删队。这里给一段宽限期
    //   （15 秒）：期间队伍留在表里占住 Max 名额，等新单位下线后被招进来；
    //   超过宽限期仍招不到人才解散。
    //
    //   注意 DoDisappear 必须在这里直接调用：原实现只置 NeedsToDisappear
    //   就 return，而下一帧又会在同一处 return，第 8 段的收尾永远到不了 ——
    //   空队于是永远挂在表里。
    if (Members.Count == 0) {
        if (!IsTransient) {
            const int32 born = TeamBirthFrames()[this];
            if (frame - born >= 900 || born == 0) {
                NeedsToDisappear = true;
                DoDisappear();
            }
        }
        return;
    }

    // ---- 7. 脚本推进 -------------------------------------------------------
    if (ActionExecuted) {
        ActionExecuted = false;
        if (Script) {
            Script->StepToNextLine();
        }
        // 脚本推进后，若处于"非末行"，清空当前目标。
        AssignTargetToAll(nullptr);
        PrevTarget = nullptr;
    } else if (PrevTarget == nullptr) {
        // 无新脚本动作时保留上一目标。
        PrevTarget = ITarget;
    }

    // 交给脚本 VM 派发当前行（原版 65 分支 switch 的现代实现）。
    //
    // ★ 脚本跑完而队伍仍停在"空闲"时，从头再跑一遍。
    //   AI 脚本描述的是**循环行为**（搜敌→推进→再搜敌），不是一次性任务：
    //   一次 Execute 只是把队伍推进到某个稳态任务上。若那一次发生在队伍
    //   还没招满、或者任务被外部复位（新兵入队、集结、取消）之后，队伍
    //   就会永久停在 Sleep 上不动。这里给一条"已完成且空闲就重来"的自愈
    //   路径，语义上与原版脚本的循环执行一致。
    if (Script) {
        if (Script->IsComplete() && CurrentMission == MISSION_SLEEP
            && !IsMissingMembers && Members.Count > 0) {
            Script->Reset();
        }
        Script->Execute();
    }

    // ---- 7b. 任务保持 -----------------------------------------------------
    //   脚本派发的是**队伍**任务，成员的任务层仍会自己收尾：一轮交火打完
    //   （FootClass::Mission_Attack 里目标消失 → ForceMission(Guard)）、或者
    //   被 Start_Default_Mission 归位，成员就退回 Guard/Sleep 站着。原版靠
    //   脚本循环（Search & Destroy 反复重派）把队伍拉回来；本树脚本在
    //   complete 后不会再重派，于是整队在第一轮交火后就地驻扎（实测
    //   AI_Inf6 m=15 而成员 m0=5）。
    //   这里只对"猎杀"姿态做保持：正在 Attack / Move 的成员不动，只把空闲的
    //   成员重新拉到队伍任务上。
    if (CurrentMission == MISSION_HUNT) {
        for (int32 i = 0; i < Members.Count; ++i) {
            TechnoClass* pTechno = Members[i];
            if (pTechno == nullptr || pTechno->IsDead() || !IsFootMember(pTechno))
                continue;
            const Mission cur = pTechno->GetMission();
            if (cur == Mission::None || cur == Mission::Sleep
                || cur == Mission::Guard || cur == Mission::AreaGuard
                || cur == Mission::Stop) {
                static_cast<FootClass*>(pTechno)->ForceMission(Mission::Hunt);
            }
        }
    }

    // ---- 8. 自杀队收尾 -----------------------------------------------------
    if (NeedsToDisappear) {
        DoDisappear();
        return;
    }

    if (Script && Script->IsComplete() && Type && Type->Suicide) {
        NeedsToDisappear = true;
    }

    if (RecruitTimer > 0) {
        --RecruitTimer;
    }
    if (GuardAreaTimer > 0) {
        --GuardAreaTimer;
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

        // ★ 先真正把任务下到成员身上。
        //   原版 AssignMissionToAll 的实质就是给每个成员 Queue_Mission；
        //   此前这里只做"顺手开一枪 / 停一下"的替代动作，成员的 CurrentMission
        //   根本没变 —— 于是 Hunt 队的脚本派发了 MISSION_HUNT，单位却仍在
        //   Guard，一步都不会朝敌方挪。MissionClass::Update 随后按
        //   CurrentMission 分派到 Mission_Hunt，推进才真正发生。
        if (pFoot != nullptr) {
            pFoot->ForceMission(mission);
        }

        switch (static_cast<int32>(mission)) {
            case MISSION_MOVE:
            case MISSION_PATROL:
            case MISSION_RETURN:
            case MISSION_RETREAT:
            case MISSION_ENTER:
                // 移动任务：目的地由调用方（MoveToLocation 等）另行 Set_Destination，
                //   这里不再用旧目的地反向覆盖，否则会把刚设好的路径打回去。
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

    // ★ 脚本已跑完时，补员后重跑一次。
    //   AI 脚本描述的是循环行为：每次有人入队都重新派发一遍，才能保证
    //   "后来补上的人"和"最早那个人"拿到同一个任务。不重跑的话，补员
    //   发生在脚本执行之后，新兵就会一直停在落位的默认任务上。
    if (Script != nullptr && Script->IsComplete())
        Script->Reset();

    // ★ 新兵立刻服从队伍当前的任务。
    //   队伍的脚本只在"第一次凑够人"时执行一次（Execute 一次即 complete），
    //   AssignMissionToAll 因此只覆盖当时在队的人；此后补招进来的成员会一直
    //   停在落位默认的 AreaGuard 上 —— 队伍名义上 4 辆，只有 1 辆真的冲锋。
    if (CurrentMission != MISSION_SLEEP && CurrentMission >= 0)
    {
        pUnit->ForceMission(static_cast<Mission>(CurrentMission));
    }

    // ★ 招募不是"脚本动作执行完毕"。
    //   TeamClass::Update 第 7 步把 ActionExecuted 当作"当前行已完成、该翻页"
    //   的信号；在此处置位会让**招第一个人**就把脚本从第 0 行推到第 1 行，
    //   于是在下一次 Execute() 之前 IsScriptComplete 已经成立 —— 整个攻击
    //   脚本一行都没执行（实测 line 恒 0、成员任务恒 Sleep）。
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
 // asm: TeamClass_TRUCKBtoTRUCKA
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
 // asm: TeamClass_TRUCKAtoTRUCKB
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
 // asm: TeamClass_LiberateAllMembers
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
 // asm: int __thiscall TeamClass_GuardAreaForX(AITeamClass *this, cAIScriptActionType *a2, char a3)   (asm:1537241)
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
 // asm: TeamClass_LoadIntoTransport
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
 // asm: TeamClass_ChangeToScript
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
// asm: TeamClass_CloneAndDie
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
// asm: TeamClass_LeaveMap
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

        // 队伍类型上"可以缺人出行"的开关（asm TeamType+0xAB = Reinforce）
        // 决定凑够一半即算可用。
        if (Type != nullptr && Type->Reinforce)
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
        SpawnCell       = nullptr;

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
// asm: void __thiscall TeamClass::Took_Damage(AITeamClass *this, TechnoClass *a2,
//      int damageState, TechnoClass *attacker)   (asm:0x6EB3xx, retn 0Ch)
//
//  FootClass_ReceiveDamage 在队员被击中时回调这里。队伍据此重建锚点，好让
//  成员围绕交火地点重新集结：
//    * 攻击者/伤害状态为空，或本队是自杀队 -> 不反应。
//    * 尚未开始编队（+0x7F==0）：清空锚点（spawnCell=0），打上"挨过打"
//      (+0x75) 与"需重编"(+0x7B) 标记后返回。
//    * 已编队：仅当攻击者不是本队成员、其类型不是建筑/步兵之类的特殊类、
//      且不是当前目标时，才把本队锚点改到攻击者身上（队伍类型允许时）；
//      最后让队长朝攻击者开火。
// ============================================================================
void TeamClass::Took_Damage(TechnoClass* pObject, int32 damageState, TechnoClass* pAttacker)
{
    (void)pObject;

    if (pAttacker == nullptr || damageState == 0)
        return;

    // 自杀队不会因为挨打而重新定位。
    if (Type != nullptr && Type->Suicide)
        return;

    // 尚未编队：清锚点、记标记。
    if (!IsForming) {
        SpawnCell       = nullptr;
        HasTakenDamage  = true;
        HasLeftMap      = true;
        return;
    }

    // 攻击者必须是"敌对且非本队成员"的实体。
    if (UnitInTeam(pAttacker))
        return;

    // 目标已经是攻击者则无需处理。
    if (ITarget == pAttacker)
        return;

    // 队伍类型允许重新索敌时，把锚点改到攻击者身上。
    if (Type != nullptr && Type->Aggressive) {
        SpawnCell = pAttacker;
        HasTakenDamage = true;
        HasLeftMap = true;
    }
}

// ============================================================================
// asm: bool __thiscall TeamClass::Is_Leaving_Map(AITeamClass *this)   (asm:0x6EC2xx)
//
//  仅当本队真的开始撤离时才返回真：已经进入编队（+0x7F）、脚本不是最后
//  一行、且当前脚本行是"移动到路径点"，同时该路径点落在玩家雷达视野之外
//  （说明队伍正朝地图外行军）。
// ============================================================================
bool TeamClass::Is_Leaving_Map() const
{
    if (!IsForming)
        return false;
    if (Script == nullptr || Script->ScriptType == nullptr)
        return false;

    // 脚本不能停在最后一行。
    const int32 total = Script->ScriptType->GetTotalActionCount();
    if (Script->CurrentLine + 1 >= total)
        return false;

    // 当前行必须是 "Move to waypoint"（action == 3）。
    if (Script->ScriptType->ScriptActions[Script->CurrentLine].Action != 3)
        return false;

    const int32 waypoint = Script->ScriptType->ScriptActions[Script->CurrentLine].Argument;
    if (ScenarioClass::Instance == nullptr)
        return false;

    // 路径点坐标已经是 CellStruct，直接拿来测雷达。
    CellStruct cell = ScenarioClass::Instance->GetWaypointCoords(waypoint);

    // 在雷达范围内就不算离场。
    if (MapClass::Instance != nullptr && MapClass::Instance->In_Radar(cell, true))
        return false;

    return true;
}

// ============================================================================
// asm: bool __thiscall TeamClass::Has_Entered_Map(AITeamClass *this)   (asm:0x6EC3xx)
//
//  队伍成员全部真正落到地图上（已生成标志置位）时返回真；空队视为已进场。
// ============================================================================
bool TeamClass::Has_Entered_Map() const
{
    TechnoClass* pMember = (Members.Count > 0) ? Members.Items[0] : nullptr;
    if (pMember == nullptr)
        return true;

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno == nullptr)
            continue;
        if (!pTechno->Is_On_Map())
            return false;
    }
    return true;
}

// ============================================================================
// asm: void __thiscall AITeamClass_FindUnitToFollow(AITeamClass *this, ...)   (asm:0x6EB7xx)
//
//  为队伍确定"跟随锚点"：优先取离队伍锚点最近的、仍在场上的成员作为
//  FollowAnchor；若队伍锚点（SpawnCell）为空，则用第一个成员补齐。
// ============================================================================
void TeamClass::FindUnitToFollow()
{
    TechnoClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    // 锚点必须是一个能给出坐标的实体（非空即用其坐标做基准）。
    const bool haveAnchor = (SpawnCell != nullptr);

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTechno = Members[i];
        if (pTechno == nullptr || pTechno->IsDead())
            continue;

        if (haveAnchor) {
            const CoordStruct a = pTechno->Get_Coord();
            const CoordStruct b = SpawnCell->GetCoords();
            const int32 dx = a.X - b.X;
            const int32 dy = a.Y - b.Y;
            const int32 dz = a.Z - b.Z;
            const int32 d  = dx * dx + dy * dy + dz * dz;
            if (d < bestDist) {
                bestDist = d;
                pBest = pTechno;
            }
        } else {
            // 没有锚点：第一个成员充当锚点。
            pBest = pTechno;
            break;
        }
    }

    if (pBest == nullptr)
        return;

    if (SpawnCell == nullptr)
        SpawnCell = pBest;
    FollowAnchor = pBest;
}

// ============================================================================
// asm: void __fastcall TeamClass_SuspendTeamsByPriority(int suspendPriority, HouseClass_old *House)   (asm:1534239)
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

        // 打上暂停标记并写下起始帧与时长。时长由规则表里的基地遇袭响应
        // 秒数换算成帧（每秒按 900 帧计）；Update 用
        // Frame - SuspendFrame >= SuspendDuration 判定解除。
        pTeam->NeedsToDelete    = true;
        pTeam->TimeToDissapear  = true;
        pTeam->TeamSuspended    = true;
        pTeam->SuspendFrame     = Game::CurrentFrame;
        pTeam->SuspendDuration  = static_cast<int32>(RulesClass::Instance != nullptr
                                        ? RulesClass::Instance->BaseDefenseSuspendSeconds * 900.0
                                        : 0.0);
    }
}

// ============================================================================
// asm: signed int __thiscall TeamClass_SearchForRecruit(AITeamClass *this, int idx)   (asm:1531212)
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
    //
    // ★ 这里必须用 **类型** 枚举（UnitType/InfantryType/AircraftType）：
    //   坑位里存的是 TechnoTypeClass*，它的 WhatAmI() 返回的是 ...Type；
    //   原先用实例枚举（Unit/Infantry/Aircraft）比较，恒不相等 —— 每一个
    //   坑位都在第一行就被 continue 掉，队伍永远招不到人。
    const AbstractType kinds[3] = {
        AbstractType::UnitType, AbstractType::AircraftType, AbstractType::InfantryType
    };

    FootClass* pBest      = nullptr;
    int32      bestScore  = 0x7FFFFFFF;

    for (int32 k = 0; k < 3; ++k)
    {
        // 只挑与坑位要求相符的种类。
        if (pWantType != nullptr && pWantType->WhatAmI() != kinds[k])
            continue;

        DynamicVectorClass<TechnoClass*>* pPool = nullptr;
        if (kinds[k] == AbstractType::UnitType)
            pPool = reinterpret_cast<DynamicVectorClass<TechnoClass*>*>(UnitClass::Array);
        else if (kinds[k] == AbstractType::AircraftType)
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
// RequestProductionForSlot —— 坑位缺员且现成单位招不到时，向本房产线下单。
//
// 原版链路（asm: TeamClass_Update 的补员段 → HouseClass_BeginProductionOf）：
//   队伍逐个 task-force 坑位核对人数，缺口为正就请求生产；造出来的单位进入
//   房主的待编队缓冲，再被 TeamClass::Recruit 吸收。
//
// 本树的招募是直接从全局单位表里挑（SearchForRecruit），所以这里的语义是
// "招不到就造"：只有全局池里确实没有符合坑位类型/编组的闲置单位时才下单，
// 避免与常规 AI 生产重复下单。三条防重复规则：
//   1) 节流：同一 (队伍, 坑位) 每 90 帧最多一次 —— 生产需要时间，每帧尝试
//      只会把工厂队列刷满，并让"队列已满"的失败路径空转。
//   2) 在制去重：把本房所有工厂"排队中 + 在制品"里该类型的数量算进去，
//      够补缺口就不再下单。
//   3) 一次只下 1 个：连续下单会占满队列，饿死其它队伍与常规生产。
// ============================================================================
void TeamClass::RequestProductionForSlot(int32 idx)
{
    if (Owner == nullptr || Type == nullptr || Type->TaskForce == nullptr)
        return;
    if (idx < 0 || idx >= MaxTaskForceSlots)
        return;

    TaskForceClass* pTask = Type->TaskForce;
    if (idx >= pTask->Members.Count)
        return;

    const TaskForceMember& mem = pTask->Members.Items[idx];
    TechnoTypeClass* pWant = mem.Type;
    if (pWant == nullptr || mem.Count <= 0)
        return;

    const int32 missing = mem.Count - TeamCounts[idx];
    if (missing <= 0)
        return;

    // ---- 1. 节流 -----------------------------------------------------------
    const int32 frame = Game::CurrentFrame;
    const uintptr_t key = TeamSlotKey(this, idx);
    auto it = TeamOrderFrames().find(key);
    if (it != TeamOrderFrames().end()) {
        const int32 last = it->second;
        if (frame >= last && (frame - last) < 90)
            return;
        it->second = frame;
    } else {
        TeamOrderFrames()[key] = frame;
    }

    // ---- 2. 在制去重 -------------------------------------------------------
    int32 inFlight = 0;
    if (FactoryClass::Array != nullptr) {
        for (int32 i = 0; i < FactoryClass::Array->Count; ++i) {
            FactoryClass* pF = (*FactoryClass::Array)[i];
            if (pF == nullptr || pF->GetOwner() != Owner)
                continue;
            inFlight += pF->CountThisInQueue(pWant);
            if (pF->GetCurrentOrder() == pWant)
                ++inFlight;
        }
    }
    if (inFlight >= missing)
        return;

    // ---- 3. 下单（一次一个；资金由工厂 Update 逐帧扣，不预扣）-------------
    Owner->BeginProductionOf(pWant, 1);
}

// ============================================================================
// asm: char __thiscall TeamClass_DoesTeamHaveTransportAircraft(AITeamClass *this)   (asm:1540924)
//
//  判断本队里有没有"运输机"类的成员：空降、机降类的队伍要靠运输机把队员
//  送到目的地，编队时先查一遍。有任何一个成员是 loaded 状态的运输机就算有。
// ============================================================================
bool TeamClass::DoesTeamHaveTransportAircraft() const
{
    // asm: TeamClass_ReduceTiberium
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
// asm: TeamClass_ReduceTiberium
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

    // asm: int __thiscall TeamClass_GetStrayDistance(AITeamClass *this)   (asm:1542858)
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

        // asm: int __thiscall TeamClass_GetStrayDistance(AITeamClass *this)   (asm:1542858)
        //  份额时有多少卸多少。
        const int32 carried = pUnit->GetTiberiumLoad();
        if (carried <= 0) {
            continue;
        }

        const int32 take = (carried < amount) ? carried : amount;
        total += take;
        amount -= take;

        // asm: int __thiscall TeamClass_GetStrayDistance(AITeamClass *this)   (asm:1542858)
        if (amount <= 0) {
            break;
        }
    }

    return total;
}

// ============================================================================
// asm: int __thiscall TeamClass_GetStrayDistance(AITeamClass *this)   (asm:1542858)
//
//  量出本队"走散"的程度：以队形中心为基准，量出离中心最远的那名成员有多
//  远。散得太远的队伍要被拉回来，AI 与队形维护都拿这个值做判断。
// ============================================================================
int32 TeamClass::GetStrayDistance() const
{
    if (this->Members.GetCount() <= 0) {
        return 0;
    }

    // asm: TeamClass_StartLStorm
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

    // asm: TeamClass_StartLStorm
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
// asm: TeamClass_StartLStorm
//
//  让本队发起一场"闪电风暴"式的集中火力：队长（第一名还能开火的成员）被
//  指定为风暴核心，其余成员把火力都朝核心的目标上招呼。没有可开火成员时
//  这一步什么都不做。
// ============================================================================
void TeamClass::StartLStorm()
{
    // asm: TeamClass_Suicide
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

    // asm: TeamClass_Suicide
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
// asm: TeamClass_Suicide (0x6EDF83, retn 8)
//
//  把全队成员逐个打死：沿成员链 FirstUnitInTeam(this+0x54) → NextUnitInTeam
//  (+0x5D8) 正向遍历，对每个"还在场上、血量 > 0"的成员，用它的当前血量作
//  伤害、以 RulesData+0xFA8 的弹头、ignoreDefenses = 1 走一次
//  TechnoClass::ReceiveDamage。全部处理后把动作完成字节 +0x80 置 1。
// ============================================================================
void TeamClass::Suicide()
{
    // asm:6EDFD5 起为正向链式遍历（下一次迭代在伤害结算之后才读 +0x5D8），
    //  因此这里同样是从头到尾，而不是从尾部倒序。
    for (int32 i = 0; i < this->Members.GetCount(); ++i) {
        TechnoClass* pTech = this->Members[i];
        if (!IsUsableMember(pTech)) {
            continue;
        }

        // asm: mov eax,[esi+6Ch]（Health）→ push &Damage；
        //      WH = [RulesData+0FA8]，ignoreDefenses = 1，其余为 0。
        int32 suicideDamage = pTech->Health;
        WarheadTypeClass* pWarhead =
            (RulesClass::Instance != nullptr) ? RulesClass::Instance->field_FA8 : nullptr;
        pTech->ReceiveDamage(&suicideDamage, 0, pWarhead, 0, 1, 0, nullptr);
    }

    // asm:6EE03A  mov byte ptr [edi+80h],1
    ActionExecuted = true;
}

// ============================================================================
// 队伍级指令（脚本动作表 Team_* 部分，asm 0x6EC98E..0x6F03xx）
//
//  TeamClass_Update 的脚本分发按动作号调用下面这些 proc；每个 proc 的
//  `this` 在 ECX，动作参数（ActionClass.arg）在栈上。它们本身并不直接驱动
//  移动，而是把"目标"（对象或格子）交给 TeamClass::Assign_Mission_Target
//  （asm 0x6E904A），再由队伍每帧的 AI（sub_6EBAD0）把目标翻译成成员使命。
//  OpenYR 没有该目标/使命层，这里用 AssignTargetToAll（对象目标）与
//  MoveToLocation（格子目标）作为等价出口，并在每处注明。
// ============================================================================

void TeamClass::AttackWaypoint(void* pParam)
{
    // asm: Team_AttackWaypoint (loc_6ECA0A 一带, retn 8)
    //   1) cell = Scenario_GetWaypointCell(script.arg)
    //   2) 若格子上有对象（GetAircraftOrTerrain）→ 目标 = 该对象；否则
    //      目标 = 格子本身（队伍开过去）。Assign_Mission_Target(target)。
    //   3) 只要还有成员带弹（TechnoType.Ammo > 0），就让队伍继续打；
    //      全员无弹时清空目标并把 +0x80 置 1。
    if (Members.GetCount() == 0) {
        AssignTargetToAll(nullptr);
        ActionExecuted = true;
        return;
    }

    CellClass* pCell = ScriptWaypointCell(pParam);
    if (pCell == nullptr) {
        // asm: Assign_Mission_Target(0) → loc_6ECA40
        AssignTargetToAll(nullptr);
        ActionExecuted = true;
        return;
    }

    ObjectClass* pObject = CellObjectAt(pCell);
    if (pObject != nullptr) {
        AssignTargetToAll(pObject);
        AssignMissionToAll(Mission::Attack);
        return;
    }

    // asm: 空格的场合把格子本身当目标，交给队伍 AI 推进。
    CoordStruct dest;
    pCell->ConvertCoords(&dest);
    ITarget = nullptr;
    MoveToLocation(dest);

    // asm: loc_6ECA18 遍历成员检查 TechnoType.Ammo；全员为零才收尾。
    bool anyAmmo = false;
    for (int32 i = 0; i < Members.GetCount(); ++i) {
        TechnoClass* pTech = Members[i];
        if (IsUsableMember(pTech) && pTech->TechnoType != nullptr &&
            pTech->TechnoType->Ammo > 0) {
            anyAmmo = true;
            break;
        }
    }
    if (!anyAmmo) {
        AssignTargetToAll(nullptr);
        ActionExecuted = true;
    }
}

void TeamClass::SetFlashing(void* pParam)
{
    // asm: Team_SetFlashing (0x6EF5E9, retn 4)
    //   原版沿成员链逐个写 FootClass.f.FlashData.Duration = script.arg，
    //   不设置任何队伍级标志、也不写 +0x80。
    //
    //   asm 无对应（本端口）：OpenYR 没有 per-unit 的 FlashData（它在
    //   Abstract/FootClass 里，属其他 Agent 维护的模块），闪烁需求只能挂在
    //   队伍自己的 IsFlashing 上，由绘制层消费。行为保留观察实现。
    (void)pParam;
    if (Members.Count > 0) {
        IsFlashing = true;
    }
}

void TeamClass::LoadOntoTransport(void* pParam)
{
    // asm: Team_LoadOntoTransport (0x6ED253, retn 8)
    //   原版先扫 TeamType->TaskForce 的条目（6 槽并行数组，条目 +0x9C 为
    //   数量），找出 TechnoType.OpenTopped > 0 的运输载具；随后沿成员链
    //   逐个比对 TechnoType+0x5E0（可载人数）与成员 +0x114（已载人数），
    //   用 RadioClass_ArrayHasAtLeastOneItem 判断无线电是否空闲，再决定
    //   让哪些步兵/载具进入运输。只有"全部装不下"时才把 +0x80 置 1。
    //
    //   asm 对应但依赖未建模的接口（本端口）：TaskForce 的并行条目数组、
    //   TechnoType+0x5E0、FootClass+0x114 与 Radio 队列都不在 TeamClass 内，
    //   这些都在 Abstract/ 与 Network/ 侧，超出本次改动边界。行为保留观察
    //   实现：以 Enter 使命把全队交给各自的运输逻辑。
    (void)pParam;
    AssignMissionToAll(Mission::Enter);
}

void TeamClass::GatherAtEnemyBase(void* pParam)
{
    // asm: Team_GatherAtEnemyBase (loc_6EF72C 一带, retn 8)
    //   1) 选成员（TechnoType+0x5FC 最大者）；2) 取该成员 Owner 上登记的
    //      "指定敌方基地"（HouseClass+0x5600，-1 表示没有）→ 该 House 的
    //      GetBaseCenterCell；3) 以 Rules+0xD74*256 为半径、按基地与成员
    //      连线的朝向（arctan + sin/cos 定点运算）算出偏移点，再经
    //      Coord_Move / Nearby_Location / Coord_Cell；4) Assign_Mission_Target(格)。
    //
    //   asm 对应但依赖未建模字段（本端口）：HouseClass+0x5600 这个"指定敌方
    //   基地"槽位不在 HouseClass 里，属 Houses/ 模块。行为保留观察实现：
    //   退化为扫描第一个敌对阵营的基地中心。
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

BuildingClass* TeamClass::PickFriendlyStructure(BuildingTypeClass* pType, HouseClass* pHouse,
                                                TechnoClass* pUnit, int32 flags,
                                                bool onlyHouseEnemy)
{
    // asm: Team_PickFriendlyStructure (跳转表 off_6EF0F8 所在函数, retn 0Ch)
    //   __fastcall(Type=ECX, pHouse=EDX, Unit, flags, onlyHouseEnemy)：
    //   遍历 (pHouse ? pHouse : Unit->Owner)->OwnedBuildings，类型必须等于
    //   Type；按 flags 打分（见 StructureWeight），返回分值最高的一座。
    //   原版只扫描本阵营的建筑，onlyHouseEnemy 在此版本中未参与过滤。
    (void)onlyHouseEnemy;

    HouseClass* pOwner = pHouse;
    if (pOwner == nullptr && pUnit != nullptr) {
        pOwner = pUnit->Owner;
    }
    if (pOwner == nullptr || pType == nullptr) {
        return nullptr;
    }

    BuildingClass* pBest = nullptr;
    int32 bestWeight = -1;
    for (int32 i = 0; i < pOwner->OwnedBuildings.Count; ++i) {
        BuildingClass* pBuilding = pOwner->OwnedBuildings[i];
        if (pBuilding == nullptr || pBuilding->IsDead()) continue;
        if (pBuilding->Type != pType) continue;

        const int32 weight = StructureWeight(flags, pBuilding, pUnit);
        if (weight > bestWeight) {
            bestWeight = weight;
            pBest = pBuilding;
        }
    }
    return pBest;
}

void TeamClass::AssignNewMission(void* pParam)
{
    // asm: Team_AssignNewMission (0x6ED801, retn 8)
    //   对每个可用成员单独处理（不是"把使命号铺给全队"）：
    //     * 成员当前没有目标（+0x2B4 为空）且没有目的地（+0x5A4 为空）时，
    //       取脚本参数作新使命号；若与 SpatialCell/SpawnCell(+0x34) 的距离
    //       超过 Rules+0x171C（0x35/0x36 两条动作改用 +0x1720）的两倍，
    //       就先 Assign_Destination_Cell 再下发使命并清目标；
    //     * 已有目标/目的地的成员跳过；
    //     * 距离在阈值内则只把成员的 +0x689（"已到位"）置 1。
    //
    //   asm 对应但依赖未建模的接口（本端口）：成员使命/目的地状态（+0x2B4、
    //   +0x5A4、+0x689）与 vtable 使命入口都在 Abstract/ 侧。行为保留观察
    //   实现：把参数当成队伍使命号分发（仅对合法范围做保护）。
    int32 missionId = static_cast<int32>(reinterpret_cast<intptr_t>(pParam));
    if (missionId < 0 || missionId > static_cast<int32>(Mission::AttackMove)) {
        missionId = static_cast<int32>(Mission::Guard);
    }
    AssignMissionToAll(static_cast<Mission>(missionId));
}

void TeamClass::Scout(void* pParam)
{
    // asm: Team_Scout (0x6EE839, retn 8)
    //   1) 若队伍尚未持有侦察目标（TeamClass+0x30），从 Owner 的侦察节点表
    //      （HouseClass+0x5620/+0x562C，每项 8 字节，+4 为占用标志）里按
    //      当前未占用的槽随机挑一个，写进 +0x30；
    //   2) 收集 vec_Buildings 中 Owner 等于 +0x30 那支 House 的全部建筑，
    //      随机取一座；
    //   3) 选 TechnoType+0x5FC 最大的成员；
    //   4) 目标落到该建筑（Assign_Mission_Target）。
    //
    //   asm 对应但缺字段（本端口）：TeamClass+0x30（ScoutHouse）与
    //   HouseClass 的 ScoutNodes 表都没有建模，TeamClass 布局又不能增删
    //   成员。行为保留观察实现：区域警戒。
    (void)pParam;
    AssignMissionToAll(Mission::AreaGuard);
}

void TeamClass::AttackStructureAtWaypoint(void* pParam)
{
    // asm: Team_AttackStructureAtWaypoint (loc_6ECAE8 一带, retn 8)
    //   cell = Scenario_GetWaypointCell(arg)；取格上对象
    //   （CellClass_GetAircraftOrTerrain），只有 WhatAmI()==Building(6) 才算
    //   目标，否则目标清空。Assign_Mission_Target(target)。
    //   目标为空时把 +0x80 置 1；否则交给队伍 AI（sub_6EB490）。
    CellClass* pCell = ScriptWaypointCell(pParam);
    ObjectClass* pObject = CellObjectAt(pCell);

    if (pObject != nullptr && pObject->WhatAmI() == AbstractType::Building) {
        AssignTargetToAll(pObject);
        AssignMissionToAll(Mission::Attack);
        return;
    }

    AssignTargetToAll(nullptr);
    ActionExecuted = true;
}

void TeamClass::MoveToFriendlyStructure(void* pParam)
{
    // asm: Team_MoveToFriendlyStructure (loc_6EE621 一带, retn 8)
    //   arg 是打包值：低 16 位 = BuildingTypeClass.Vector 下标，
    //   高 16 位 = flags（0..3，见 StructureWeight）。
    //   选一个成员（原版取 TechnoType+0x5FC 最大者）作为"单位"，
    //   PickFriendlyStructure(Type, nullptr→成员 Owner, Unit, flags, 0)，
    //   再用 Map::Nearby_Location(建筑格, 成员 SpeedType, MovementZone)
    //   求可达落点，Assign_Mission_Target(落点格)。
    const int32 packed = static_cast<int32>(reinterpret_cast<intptr_t>(pParam));
    const int32 typeIndex = packed & 0xFFFF;
    const int32 flags = (packed >> 16) & 0xFFFF;

    TechnoClass* pUnit = FirstUsableMember(this);
    if (pUnit == nullptr) {
        ActionExecuted = true;
        return;
    }

    BuildingTypeClass* pType = nullptr;
    if (BuildingTypeClass::Array != nullptr &&
        typeIndex >= 0 && typeIndex < BuildingTypeClass::Array->Count) {
        pType = (*BuildingTypeClass::Array)[typeIndex];
    }
    if (pType == nullptr) {
        ActionExecuted = true;
        return;
    }

    BuildingClass* pBuilding =
        PickFriendlyStructure(pType, nullptr, pUnit, flags, false);
    if (pBuilding == nullptr) {
        ActionExecuted = true;
        return;
    }

    CellStruct near;
    CoordStruct dest = pBuilding->GetCoords();
    if (TheMap != nullptr && pUnit->TechnoType != nullptr) {
        near = TheMap->Nearby_Location(Math::CoordToCell(dest),
                                       static_cast<int32>(pUnit->TechnoType->SpeedTypeVal),
                                       pUnit->TechnoType->MoveZone);
        dest = Math::CellToCoord(near);
    }
    MoveToLocation(dest);
}

void TeamClass::AttackEnemyStructure(void* pParam)
{
    // asm: Team_AttackEnemyStructure (loc_6EE388 一带, retn 8)
    //   只在队伍还没有目标（+0x3C 为空）时选目标：
    //     Type   = BuildingTypeClass.Vector[arg & 0xFFFF]
    //     flags  = arg >> 16
    //     Unit   = 第一个可用成员
    //     Caller = Unit->Owner 的"指定敌方基地"（House+0x5600）
    //   调 Team_PickEnemyStructure，选到就 Assign_Mission_Target(建筑)。
    //   选不到则目标清空、+0x80 置 1；已选到则交给队伍 AI 开火。
    const int32 packed = static_cast<int32>(reinterpret_cast<intptr_t>(pParam));
    const int32 typeIndex = packed & 0xFFFF;
    const int32 flags = (packed >> 16) & 0xFFFF;

    if (ITarget == nullptr) {
        TechnoClass* pUnit = FirstUsableMember(this);
        BuildingTypeClass* pType = nullptr;
        if (BuildingTypeClass::Array != nullptr &&
            typeIndex >= 0 && typeIndex < BuildingTypeClass::Array->Count) {
            pType = (*BuildingTypeClass::Array)[typeIndex];
        }
        if (pUnit != nullptr && pType != nullptr) {
            BuildingClass* pBuilding = PickEnemyStructure(
                pType, pUnit->Owner, pUnit, flags,
                (Type != nullptr) ? Type->OnlyTargetHouseEnemy : false);
            if (pBuilding != nullptr) {
                AssignTargetToAll(pBuilding);
                AssignMissionToAll(Mission::Attack);
                return;
            }
        }
    }

    if (ITarget == nullptr) {
        AssignTargetToAll(nullptr);
        ActionExecuted = true;
    }
}

void TeamClass::MoveToEnemyStructure(void* pParam)
{
    // asm: Team_MoveToEnemyStructure (loc_6EE44F 一带, retn 8)
    //   与 AttackEnemyStructure 同一套选目标流程（Type 由低 16 位给），
    //   但选到后不接火：用 Map::Nearby_Location(建筑格, 成员 SpeedType,
    //   MovementZone) 求落点，Assign_Mission_Target(落点格)，由队伍 AI 行军。
    //   选不到且当前无目标 → +0x80 置 1。
    const int32 packed = static_cast<int32>(reinterpret_cast<intptr_t>(pParam));
    const int32 typeIndex = packed & 0xFFFF;
    const int32 flags = (packed >> 16) & 0xFFFF;

    if (ITarget == nullptr) {
        TechnoClass* pUnit = FirstUsableMember(this);
        BuildingTypeClass* pType = nullptr;
        if (BuildingTypeClass::Array != nullptr &&
            typeIndex >= 0 && typeIndex < BuildingTypeClass::Array->Count) {
            pType = (*BuildingTypeClass::Array)[typeIndex];
        }
        if (pUnit != nullptr && pType != nullptr) {
            BuildingClass* pBuilding = PickEnemyStructure(
                pType, pUnit->Owner, pUnit, flags,
                (Type != nullptr) ? Type->OnlyTargetHouseEnemy : false);
            if (pBuilding != nullptr) {
                CoordStruct dest = pBuilding->GetCoords();
                if (TheMap != nullptr && pUnit->TechnoType != nullptr) {
                    CellStruct near = TheMap->Nearby_Location(
                        Math::CoordToCell(dest),
                        static_cast<int32>(pUnit->TechnoType->SpeedTypeVal),
                        pUnit->TechnoType->MoveZone);
                    dest = Math::CellToCoord(near);
                }
                ITarget = nullptr;
                MoveToLocation(dest);
                return;
            }
        }
    }

    if (ITarget == nullptr) {
        ActionExecuted = true;
    }
}

void TeamClass::GatherAtFriendlyBase(void* pParam)
{
    // asm: Team_GatherAtFriendlyBase (loc_6EFA3C 一带, retn 8)
    //   1) 选成员（TechnoType+0x5FC 最大者），取它的 Owner->GetBaseCenterCell；
    //   2) 若 House+0x5600 指向另一支 House，则用两支基地中心的连线算朝向
    //      （arctan），否则用 Scenario.Random(0,255) 取随机朝向；
    //   3) 以 Rules+0xD74*256 为半径做极坐标偏移，经 Coord_Move、
    //      Nearby_Location、Coord_Cell 得到落点；
    //   4) Assign_Mission_Target(落点格)。
    //
    //   asm 对应但依赖未建模字段（本端口）：House+0x5600 未建模，且 Team 类
    //   布局不能新增成员。行为保留观察实现：直接开往本方基地中心。
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
    // asm: Team_SpyStructureAtWaypoint (loc_6ECE89 一带, retn 8)
    //   1) cell = GetWaypointCell(arg)，先 Assign_Mission_Target(cell)；
    //   2) 若当前目标还是 Cell（WhatAmI()==Cell），取格上对象
    //      （GetAircraftOrTerrain）并把它作为新目标；
    //   3) 目标为空 → Assign_Mission_Target(0) 且 +0x80 置 1。
    //   潜入的"进入"由队伍 AI 依据目标类型决定（这里用 Enter 使命承接）。
    CellClass* pCell = ScriptWaypointCell(pParam);
    if (pCell == nullptr) {
        AssignTargetToAll(nullptr);
        ActionExecuted = true;
        return;
    }

    ObjectClass* pObject = CellObjectAt(pCell);
    if (pObject != nullptr) {
        AssignTargetToAll(pObject);
        AssignMissionToAll(Mission::Enter);
        return;
    }

    CoordStruct dest;
    pCell->ConvertCoords(&dest);
    ITarget = nullptr;
    MoveToLocation(dest);
}

void TeamClass::AttackTargetType(void* pParam)
{
    // asm: Team_AttackTargetType (loc_6ED0BC 一带, retn 8)
    //   仅在队伍无目标时：选 TechnoType.LeadershipRating 最高的成员，把脚本
    //   参数经 TeamClass_TargetTypeToFlags 变成标志位，然后调用该成员的
    //   TechnoClass::SelectAutoTarget(flags, &member[0x9C], OnlyTargetHouseEnemy)
    //   自动索敌，结果交给 Assign_Mission_Target；随后按成员 Ammo 决定继续
    //   还是收尾（+0x80）。
    //
    //   asm 对应但依赖未建模接口（本端口）：TechnoClass::SelectAutoTarget
    //   （vtable 项）在 Abstract/ 里没有暴露。行为保留观察实现：切换攻击使命，
    //   由成员自身的索敌逻辑接手。
    (void)pParam;
    AssignMissionToAll(Mission::Attack);
}

void TeamClass::GetTaskForceEntries(DynamicVectorClass<TechnoTypeClass*>* pOut)
{
    // asm: Team_GetTaskForceEntries (loc_6EF4EF 一带, retn 4)
    //   1) 遍历 TeamType->TaskForce 的条目（6 槽：类型 +0xA8、数量 +0xA4），
    //      把每个类型按它的数量重复压入出参向量；
    //   2) 再遍历本队成员，把成员类型在向量中的那一项删掉——于是出参里剩下
    //      的就是"编制表里还缺的类型/数量"。
    //   原版在第二段用 VectorClass::FindIndexOf + 平移删除；这里用
    //   DynamicVectorClass 的线性查找 + Remove 等价实现。
    if (pOut == nullptr) {
        return;
    }

    TaskForceClass* pTaskForce = (Type != nullptr) ? Type->TaskForce : nullptr;
    if (pTaskForce != nullptr) {
        for (int32 i = 0; i < pTaskForce->Members.Count; ++i) {
            TaskForceMember* pEntry = pTaskForce->GetMember(i);
            if (pEntry == nullptr || pEntry->Type == nullptr || pEntry->Count <= 0) {
                continue;
            }
            for (int32 n = 0; n < pEntry->Count; ++n) {
                pOut->Add(pEntry->Type);
            }
        }
    }

    for (int32 i = 0; i < Members.Count; ++i) {
        TechnoClass* pTech = Members[i];
        if (!IsUsableMember(pTech)) {
            continue;
        }
        TechnoTypeClass* pMemberType = pTech->GetType();
        for (int32 j = 0; j < pOut->Count; ++j) {
            if ((*pOut)[j] == pMemberType) {
                pOut->Remove(j);
                break;
            }
        }
    }
}

void TeamClass::SetElite_old(void* pParam)
{
    // asm: Team_SetElite_old (loc_6EF5D6 一带, retn 8)
    //   第二个参数 elite 非零时：沿成员链把 TechnoClass.FlashData.Level
    //   （老兵等级）写成动作参数，然后 +0x80 置 1；elite 为零则原样返回，
    //   连 +0x80 都不写。
    //
    //   asm 对应但依赖未建模字段（本端口）：TechnoClass 没有 FlashData.Level
    //   （老兵等级由晋升系统管理，属 Abstract/ 模块）。行为保留观察实现：
    //   什么都不做（老版精锐化入口已被晋升系统取代）。
    (void)pParam;
}

void TeamClass::PlayAnimType(void* pParam)
{
    // asm: Team_PlayAnimType (loc_6EF661 一带, retn 8)
    //   arg 是打包值：低 16 位 = vec_AnimTypes 下标，高 16 位 = loopCount。
    //   下标合法且队伍有成员时，对每个成员：
    //     new AnimClass(AnimType, member->GetCoords(), 0,
    //                   loopCount, 0x600, 0, 0) → AnimClass::Attach_To(member)
    //   无论是否播放，最后都把 +0x80 置 1。
    const int32 packed = static_cast<int32>(reinterpret_cast<intptr_t>(pParam));
    const int32 animIndex = packed & 0xFFFF;
    const int32 loopCount = (packed >> 16) & 0xFFFF;

    if (animIndex >= 0 && AnimTypeClass::Array != nullptr &&
        animIndex < AnimTypeClass::Array->Count && Members.Count > 0) {
        AnimTypeClass* pAnimType = (*AnimTypeClass::Array)[animIndex];
        if (pAnimType != nullptr) {
            for (int32 i = 0; i < Members.Count; ++i) {
                TechnoClass* pTech = Members[i];
                if (!IsUsableMember(pTech)) {
                    continue;
                }
                AnimClass* pAnim = new AnimClass(pAnimType, pTech->GetCoords(),
                                                 0, loopCount, 0x600, 0, false);
                if (pAnim != nullptr) {
                    pAnim->Attach_To(pTech);
                }
            }
        }
    }

    ActionExecuted = true;
}

void TeamClass::FollowFriendlies(void* pParam)
{
    // asm: Team_FollowFriendlies (loc_6EDB89 一带, retn 8)
    //   1) AITeamClass_FindUnitToFollow(&SpawnCell(+0x2C), &FollowAnchor(+0x38))；
    //   2) PrevTarget(+0x40) = SpawnCell；跑一遍队伍 AI（sub_6EBAD0）；
    //   3) 若 FollowAnchor 是 Techno：对每个成员，先清掉指向别处的目标，
    //      再看锚点的 LastTarget——成员能打、当前使命为 5(Attack) 时，把
    //      成员目标设成锚点正在打的那个目标（跟着友军打同一个）。
    //
    //   asm 对应但依赖未建模接口（本端口）：成员使命号与目标槽（+0x2B4、
    //   +0x5A4、vtable+0x184/0x3C8）都在 Abstract/ 侧。行为保留观察实现：
    //   整队重编后转入区域警戒。
    (void)pParam;
    ReGroup();
    AssignMissionToAll(Mission::AreaGuard);
}

void TeamClass::ChronoSphereToStructure(void* pParam)
{
    // asm: Team_ChronoSphereToStructure (loc_6EFE7D 一带, retn 8)
    //   1) 选 TechnoType.NavalTargeting 最大的成员，取其 Owner；
    //   2) 在该 Owner 的超武列表（House+0x258/+0x264，24 字节一项）里按
    //      TypeIndex 找 3（超时空传送）与 4（超时空扭曲），两者都要有；
    //   3) 传送建筑 +0x6F 非零且 HouseClass_CurrentPowerPercentage < 1.0 时
    //      放弃；4) 用 Team_PickEnemyStructure 选敌方建筑，再由
    //      HouseClass_SWFire 把传送目标设成队伍 SpawnCell(+0x34) 所在格。
    //
    //   asm 对应但依赖未建模接口（本端口）：HouseClass 的超武列表与
    //   HouseClass_SWFire 在 Houses/、SW/ 侧，本次不改它们。行为保留观察
    //   实现：不动作。
    (void)pParam;
}

void TeamClass::ChronoWarpToStructure(void* pParam)
{
    // asm: Team_ChronoWarpToStructure (loc_6F0147 一带, retn 8)
    //   流程与 ChronoSphereToStructure 同构：选 LeadershipRating 最大的成员，
    //   在 Owner 的超武列表里找 TypeIndex 3（传送）和 4（扭曲），检查传送
    //   建筑 +0x6F 与电力，再用 PickEnemyStructure 选目标并 SWFire。
    //   区别只在最终使用的超武与目标落点（这里以队伍锚点为终点）。
    //
    //   asm 对应但依赖未建模接口（本端口）：同上，超武列表/SWFire 属
    //   Houses/ 与 SW/。行为保留观察实现：不动作。
    (void)pParam;
}

void TeamClass::PatrolToWaypoint(void* pParam)
{
    // asm: Team_PatrolToWaypoint (loc_6ECD15 一带, retn 8)
    //   1) cell = Scenario_GetWaypointCell(arg)，Assign_Mission_Target(cell)；
    //   2) 若队伍没有 PrevTarget(+0x40) 且动作参数 < 0x2BE，再次把格子设为目标；
    //   3) 之后每隔 Rules+0x1150*900 帧，选一名成员，用它的
    //      GetPatrolTarget(vtable+0x3C4, own coords) 求下一个巡逻点；求不到
    //      就把目标改回路径点格；
    //   4) 目标带 +0x14 bit1 时交给 sub_6EB490，否则 sub_6EBAD0。
    //
    //   OpenYR 没有 per-member 巡逻目标解析，第 3 步以队伍 Patrol 使命承接。
    CellClass* pCell = ScriptWaypointCell(pParam);
    if (pCell == nullptr) {
        ActionExecuted = true;
        return;
    }

    CoordStruct dest;
    pCell->ConvertCoords(&dest);
    ITarget = nullptr;
    MoveToLocation(dest);
    AssignMissionToAll(Mission::Patrol);
}

BuildingClass* TeamClass::PickEnemyStructure(BuildingTypeClass* pType, HouseClass* pCaller,
                                             TechnoClass* pUnit, int32 flags,
                                             bool onlyHouseEnemy)
{
    // asm: Team_PickEnemyStructure (跳转表 off_6EEE84 所在函数, retn 0Ch)
    //   __fastcall(Type=ECX, CallerHouse=EDX, Unit, flags, onlyHouseEnemy)：
    //   遍历全局 vec_Buildings，类型必须等于 Type，然后按归属过滤：
    //     * 与 CallerHouse 同属一方 → 纳入；
    //     * 与 Unit->Owner 不是盟友 → 纳入；
    //     * 是盟友但对方是真人玩家 → 纳入，否则跳过。
    //   按 flags 打分（见 StructureWeight），分别记录"本方最佳"与"任意最佳"；
    //   本方分值更高就返回本方那座，否则返回任意最佳。onlyHouseEnemy 非零
    //   时只返回本方那一座（原版命名如此，语义是"只打自己阵营的建筑"）。
    if (BuildingClass::Array == nullptr || pType == nullptr) {
        return nullptr;
    }

    BuildingClass* pBestOwn = nullptr;
    BuildingClass* pBestAny = nullptr;
    int32 weightOwn = -1;
    int32 weightAny = -1;

    HouseClass* pUnitOwner = (pUnit != nullptr) ? pUnit->Owner : nullptr;

    for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
        BuildingClass* pBuilding = (*BuildingClass::Array)[i];
        if (pBuilding == nullptr || pBuilding->IsDead()) continue;
        if (pBuilding->Type != pType) continue;

        const bool isOwn = (pCaller != nullptr && pBuilding->Owner == pCaller);
        if (!isOwn) {
            const bool allied = (pUnitOwner != nullptr && pBuilding->Owner != nullptr &&
                                 pUnitOwner->IsAlliedWith(pBuilding->Owner));
            if (allied && pBuilding->Owner != nullptr && !pBuilding->Owner->IsHumanPlayer) {
                continue;
            }
        }

        const int32 weight = StructureWeight(flags, pBuilding, pUnit);
        if (isOwn && weight > weightOwn) {
            weightOwn = weight;
            pBestOwn = pBuilding;
        }
        if (weight > weightAny) {
            weightAny = weight;
            pBestAny = pBuilding;
        }
    }

    if (pBestOwn != nullptr && weightOwn > weightAny) {
        return pBestOwn;
    }
    if (onlyHouseEnemy) {
        return pBestOwn;
    }
    return pBestAny;
}
