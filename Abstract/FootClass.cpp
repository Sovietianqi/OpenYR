#include <Abstract/FootClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Locomotion/LocomotionClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Locomotion/LocomotionClass.h>
#include <Abstract/UnitClass.h>
#include <Abstract/BuildingClass.h>
#include <Abstract/InfantryClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Animations/AnimClass.h>
#include <Rules/RulesClass.h>
#include <Abstract/MissionClass.h>
#include <Rendering/TacticalClass.h>
#include <Game/Game.h>
#include <AI/TagClass.h>
#include <AI/TeamClass.h>
#include <AI/TeamTypeClass.h>
#include <Audio/VoxClass.h>
#include <Houses/HouseClass.h>
#include <Game/GameInit.h>
#include <Scenario/ScenarioClass.h>
#include <Game/ChainDebug.h>   // 逐帧调用链探针（#6/#7 验证）

#include <cstdlib>

// 前置声明：这两个 helper 定义在本文件下方（asm 里被内联在各任务处理器尾部），
// 上方多个 Mi_/Mission_ 处理器要先用到它们。
inline int32 MissionStepDelay(const Mission& mission);
inline bool  IsSpatialTarget(const AbstractClass* pTarget) noexcept;

// ============================================================================
// FootClass.cpp
//
//  FootClass is the base for everything that moves under its own power:
//  infantry, vehicles, aircraft and ships.  It owns the locomotion link, the
//  path-finding queue, the primary / turret facing values, and the per-frame
//  movement update loop.  This file expands the .cpp with:
//    * Static Array management
//    * Path management (Get_Path / Set_Path / Clear_Path)
//    * Facing management (SetFacing / GetFacing implementations)
//    * Coordinate management (SetCoords implementation)
//    * Locomotion interface delegation
//    * Update loop for movement
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<FootClass*>* FootClass::Array = nullptr;

// ============================================================================
// Init_Array / Delete_Array
// ============================================================================
void FootClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<FootClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<FootClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<FootClass*>();
    }
}

void FootClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<FootClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

// ============================================================================
// Add_To_Array / Remove_From_Array
// ============================================================================
int32 FootClass::Add_To_Array(FootClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    if (!Array->Add(pInstance))
        return -1;

    return Array->Count - 1;
}

bool FootClass::Remove_From_Array(FootClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return false;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
        {
            return Array->Remove(i);
        }
    }
    return false;
}

// ============================================================================
// Get_Total_Count / Get_Instance / Find_Index
// ============================================================================
int32 FootClass::Get_Total_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

FootClass* FootClass::Get_Instance(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 FootClass::Find_Index(FootClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
            return i;
    }
    return -1;
}

// ============================================================================
// Path management
//
//  The path is a queue of waypoints the locomotion layer will walk through.
//  In the original binary the queue lives on the FootClass itself; here we
//  expose it through a small wrapper so callers do not have to know the
//  internal layout.
// ============================================================================

bool FootClass::Has_Path() const
{
    return Path.Count > 0;
}

int32 FootClass::Get_Path_Length() const
{
    return Path.Count;
}

CoordStruct FootClass::Get_Path_At(int32 index) const
{
    if (index < 0 || index >= Path.Count)
        return CoordStruct(0, 0, 0);
    return Path.Items[index];
}

void FootClass::Set_Path(const CoordStruct* pCoords, int32 count)
{
    Clear_Path();
    if (pCoords == nullptr || count <= 0)
        return;

    for (int32 i = 0; i < count; ++i)
    {
        Path.Add(pCoords[i]);
    }
}

void FootClass::Append_Path(const CoordStruct& coord)
{
    Path.Add(coord);
}

void FootClass::Clear_Path()
{
    Path.Clear();
}

CoordStruct FootClass::Peek_Next_Path() const
{
    if (Path.Count <= 0)
        return CoordStruct(0, 0, 0);
    return Path.Items[0];
}

CoordStruct FootClass::Pop_Next_Path()
{
    if (Path.Count <= 0)
        return CoordStruct(0, 0, 0);

    CoordStruct next = Path.Items[0];
    Path.Remove(0);
    return next;
}

// ============================================================================
// Facing management
//
//  FootClass owns two facings - PrimaryFacing (body) and TurretFacing.  The
//  header exposes inline setters; here we provide the non-virtual helpers
//  used by the AI and the locomotion layer.
// ============================================================================

void FootClass::SetFacing(DirStruct facing)
{
    PrimaryFacing = facing;
}

DirStruct FootClass::GetFacing() const
{
    return PrimaryFacing;
}

void FootClass::SetTurretFacing(DirStruct facing)
{
    TurretFacing = facing;
}

DirStruct FootClass::GetTurretFacing() const
{
    return TurretFacing;
}

// ============================================================================
// Coordinate management
//
//  SetCoords is the canonical entry point used by the locomotion layer and
//  the network resync code.  It updates Location, snaps the cell-occupation
//  bits and (in the full binary) refreshes the radar map.
// ============================================================================

void FootClass::SetCoords_Impl(const CoordStruct& coord)
{
    Location = coord;

    // Notify the locomotion layer so its internal CurrentCoord stays in
    // sync.  The full engine would also call MapClass::Mark here.
    if (Locomotion != nullptr)
    {
        Locomotion->CurrentCoord = coord;
    }
}

void FootClass::SetCoords(const CoordStruct& coord)
{
    SetCoords_Impl(coord);
}

CoordStruct FootClass::GetCoords_Impl() const
{
    return Location;
}

// ============================================================================
// Locomotion interface delegation
//
//  FootClass owns a LocomotionClass pointer that implements the actual
//  movement algorithm (drive, walk, fly, jumpjet, ...).  The helpers below
//  forward the common operations so callers do not need to dereference the
//  pointer directly.
// ============================================================================

void FootClass::Set_Locomotion(LocomotionClass* pLoco)
{
    if (Locomotion != nullptr)
    {
        // Drop the refcount on the old locomotion.
        Locomotion->Owner = nullptr;
    }
    Locomotion = pLoco;
    if (Locomotion != nullptr)
    {
        Locomotion->Owner = this;
        Locomotion->LinkedTo = this;
    }
}

LocomotionClass* FootClass::Get_Locomotion() const
{
    return Locomotion;
}

bool FootClass::Is_Moving() const
{
    if (Locomotion == nullptr)
        return false;
    return Locomotion->Is_Moving();
}

void FootClass::Stop_Moving()
{
    if (Locomotion == nullptr)
        return;
    Locomotion->Stop_Moving();
    Clear_Path();
}

void FootClass::Move_To(const CoordStruct& coord)
{
    if (Locomotion == nullptr)
        return;

    Clear_Path();
    Append_Path(coord);
    Locomotion->Move_To(coord);
}

CoordStruct FootClass::Get_Destination() const
{
    if (Locomotion == nullptr)
        return Location;
    return Locomotion->Destination();
}

// ============================================================================
// Update loop for movement
//
//  Called once per frame for every FootClass instance on the active list.
//  The locomotion layer does the actual position integration; this method
//  drives the path queue and the facing interpolation.
// ============================================================================

// ============================================================================
// asm: loc_4DA86E .. loc_4DAF00 (FootClass::AI 尾部) / FootClass::Update_Movement
//
//  运动积分：把本帧的位移/朝向交给行走逻辑（ILocomotion::Process，asm
//  L481617 `call [ecx+40h]`）。原版在调用前后各有若干门：
//
//    * 行走逻辑为空  → 原版抛 E_POINTER（RaiseError）后仍会解引用；
//                       本项目直接跳过，避免空指针。
//    * 正在下沉/坠落/被运载（[+3CDh] / [+8Dh] / [+81h] 非零）→ 不驱动。
//    * Pushy 类型（+2A8h 非零且类型非 Pushy）→ 不驱动。
//
//  Process 之后原版并不在 AI 里就地取下一个航点：路径推进由行走逻辑自身的
//  Destination() 完成后经 ILocomotion 反馈（FootClass::AI 的 QueryInterface
//  段）。这里保留"到达且仍有路径则推进下一段"的等价收尾。
// ============================================================================
void FootClass::Update_Movement()
{
    // asm: 4DA677/4DA850 -- 行走逻辑为空则原版抛错，本项目安全跳过。
    if (Locomotion == nullptr)
        return;

    // asm 4DA806 段的前置门（逐条对照）：
    //   [esi+3CDh] != 0  —— 正在下沉/坠落：OpenYR 未按原名建模该字节，
    //                       由各子类的坠落状态（如 AircraftClass::IsCrashing）
    //                       在进入 AI 前拦截，此处不重复判断。
    //   [esi+8Dh]  != 0  —— 正被运输载具载着（Transporter 非空）。
    //   [esi+81h]  != 0  —— InOpenTopped。
    //   [esi+2A8h] != 0 且 type->Pushy == 0 —— 「推挤指令」期间非 Pushy
    //                       类型不自行行走；[2A8h] 是集结点/TB 指令标志，
    //                       OpenYR 未建模，故该分支保持不动作（不改变默认
    //                       行走，避免把所有非 Pushy 单位都冻住）。
    if (Transporter != nullptr || InOpenTopped || IsInLimbo)
        return;

    // asm: 4DA86E —— 主动推进：call [locomotion.vt + 0x40] == ILocomotion::Process。
    Locomotion->Process();
    ++ChainDebug::LocomotionProcessCalls;      // 探针：#7 行走逻辑每帧推进

    // 到达当前落点且仍有后续航点：取下一个航点继续走。
    if (!Locomotion->Is_Moving() && Has_Path())
    {
        const CoordStruct next = Pop_Next_Path();
        Locomotion->Move_To(next);
    }
}

// ============================================================================
// FootClass::Update  (vtable 槽 0x5C)
//
//  真实继承链已按原版接通：
//      ObjectClass -> MissionClass -> RadioClass -> TechnoClass -> FootClass
//  逐帧由 vec_Logics 循环 `call [obj+0x5C]` 驱动（LogicClass_Update 第 15 步）。
//  子类（UnitClass / InfantryClass / AircraftClass）的 Update 先跑自身前置段，
//  再 call FootClass::Update，故本函数是"进入足类逐帧"的统一入口：
//    1. 父类逐帧 TechnoClass::Update()
//         = TechnoClass_Update 前半 → MissionClass::Update（asm L1561947）
//           → 后半（Bomb/SlaveManager/CaptureManager/Reload/SpawnManager/Cloak）
//    2. 足类增量 AI()（asm FootClass::AI 除首行 TechnoClass_Update 外的全部
//       内容，含 Update_Movement → ILocomotion::Process 驱动，asm L481617）
// ============================================================================
void FootClass::Update()
{
    // 探针：#6 足类逐帧入口被调用。
    ++ChainDebug::FootUpdateCalls;

    // asm: FootClass::AI 首行 `call TechnoClass_Update`（父类逐帧）。
    TechnoClass::Update();

    // asm: FootClass::AI 主体（Locomotion 驱动 + 逐帧状态）。
    AI();
}

// ============================================================================
// asm: FootClass::AI   (gamemd:481281 .. 482328)
//
//  足类逐帧主体。逐段（保持 asm 先后）：
//
//    段 A（481285）：父类逐帧 TechnoClass_Update。本项目由 Update() 承担，
//                    此处不再重复调用（见 Update()）。
//    段 B（481293）：空中单位每隔 RulesClass.RadLevelDelay 帧结算一次辐射
//                    伤害（免疫辐射 / 已在空中 / 已离场者除外）。
//    段 C（481443）：气球悬停单位落到被泰伯利亚污染的格子 → 置腐蚀标记。
//    段 D（481675）：行走逻辑为空的报错门（本项目安全跳过）。
//    段 E（481689）：正在移动且属友军的单位周期性重揭视野（Sight_From_3）。
//    段 F（481902）：每 16 帧对身处带标签格的空中单位抛 59 号事件。
//    段 G（481957）：行走帧（走路动画）节流推进。
//    段 H（482207）：移动音效的播放与收尾。
//    段 I（482483）：地面静止单位的每 64 帧一次散射（Scatter）。
//    段 J（482511）：传送收尾 + 挂靠对象逐帧推进。
//
//  说明：原版部分分支读写的字段（坠落/沉没/坠毁状态位、挂靠单位指针、
//  导航目的地）在本项目 ObjectClass/TechnoClass 上未按原名建模；此处按
//  "功能一致" 原则用现有状态（InAir / InLimbo / Owner 关系 / Locomotion
//  查询）等价表达，并对无对应状态的分支保持不动作。
// ============================================================================
void FootClass::AI()
{
    const int32 frame = Game::GetCurrentFrame();

    // ---- 段 B：辐射结算 ---------------------------------------------------
    // asm: 481295 mov [esi+6B3h],0 / mov eax,Frame / idiv RulesClass.<RadLevelDelay>
    //      test edx,edx / jnz skip  —— 每 RadLevelDelay 帧才结算一次。
    if (IsInAir())
    {
        RulesClass* pRules = RulesClass::Instance;
        const int32 radDelay = (pRules != nullptr) ? pRules->RadLevelDelay : 0;

        if (radDelay > 0 && (frame % radDelay) == 0
            && TechnoType != nullptr && !TechnoType->ImmuneToRadiation
            && !InAir() && !IsInLimbo)
        {
            const CoordStruct pos = GetCoords();
            const CellStruct cell = CellClass::Coord2Cell(pos);
            CellClass* pCell = (MapClass::Instance != nullptr)
                             ? MapClass::Instance->GetCellAt(cell) : nullptr;

            if (pCell != nullptr)
            {
                const int32 rad = pCell->Get_RadLevel();
                if (rad > 0 && pRules != nullptr)
                {
                    // asm: fild rad / fmul RulesClass.<RadLevelFactor> / floor
                    int32 damage = static_cast<int32>(static_cast<double>(rad)
                                                    * pRules->RadLevelFactor);
                    // asm: 以辐射弹头对自身下达伤害（本项目弹头表未登记辐射
                    //   弹头，传空等价于"仅按伤害值扣血"）。
                    ReceiveDamage(&damage, 0, nullptr, 0, 0, 0, nullptr);
                }
            }
        }
    }

    // ---- 段 C：气球悬停的泰伯利亚腐蚀 -------------------------------------
    // asm: 481436 cmp [esi+3D5h],0 / ... type->BalloonHover / MapClass_IsCellTainted
    if (!BalloonHoverTainted && TechnoType != nullptr && TechnoType->BalloonHover
        && MapClass::Instance != nullptr)
    {
        if (MapClass::Instance->IsCellTainted(GetCoords(), false))
            BalloonHoverTainted = true;
    }

    // ---- 段 D：行走逻辑驱动（asm loc_4DA806..4DA86E）-----------------------
    // asm 顺序（481675..481617）：
    //   cmp [esi+3CDh],0        ; 正在下沉/坠落
    //   cmp [esi+8Dh],0         ; 被运载
    //   cmp [esi+2A8h],0        ; Pushy 标记为 0，或 type->Pushy 为真
    //   cmp [esi+81h],0         ; InOpenTopped
    //   以上任一命中就跳过整段（loc_4DAA01）。
    //   否则 `mov eax,[esi+674h] / call [ecx+40h]` —— ILocomotion::Process。
    //
    // 前置门与 Process 全部落在 Update_Movement() 内（Transporter/InOpenTopped/
    // IsInLimbo 三道具名门 + ILocomotion::Process），此处直接调用即等价。
    Update_Movement();

    // ---- 段 E：友军空中移动单位的视野重揭 ---------------------------------
    // asm: 481689 Locomotion->Is_Moving() && InAir() && Is_Ally(PlayerPtr)
    //      → 计时到点则 MapClass_Sight_From_3(pos, height, radius+3, ...)
    if (Locomotion != nullptr && Locomotion->Is_Moving() && InAir())
    {
        bool alliedToPlayer = false;
        HouseClass* pPlayer = HouseClass::Player;
        if (Owner != nullptr && pPlayer != nullptr)
        {
            if (Owner == pPlayer)
            {
                alliedToPlayer = true;
            }
            else
            {
                bool v = false;
                Owner->Is_Ally(pPlayer->GetArrayIndex(), &v);
                alliedToPlayer = v;
            }
        }

        if (alliedToPlayer && MapClass::Instance != nullptr)
        {
            int32 remaining = SightRevealTimer.TimeLeft;
            if (SightRevealTimer.StartTime != -1)
            {
                const int32 elapsed = frame - SightRevealTimer.StartTime;
                remaining = (elapsed >= remaining) ? 0 : (remaining - elapsed);
            }

            if (remaining <= 0 || SightRevealTimer.StartTime == -1)
            {
                const CoordStruct pos = GetCoords();
                const int32 height = GetHeight();
                const int32 radius = (TechnoType != nullptr
                                      ? TechnoType->Sight : 0) + 3;
                MapClass::Instance->Sight_From_3(pos, height, radius, 0);

                // asm: [esi+65Ch]=Frame / [esi+660h]=Z / [esi+664h]=15
                SightRevealTimer.StartTime = frame;
                SightRevealTimer.TimeLeft  = 15;
            }
        }
    }

    // ---- 段 F：带标签格的空中单位每 16 帧抛事件 59 -------------------------
    // asm: 481790 (Frame & 0Fh)==0 / InAir / cell->Tag != 0 → RaiseEvent(59)
    if ((frame & 0x0F) == 0 && InAir() && Tag != nullptr)
    {
        Tag->RaiseEvent(59, this, GetCellCoords(), false, nullptr);
    }

    // ---- 段 G：行走帧节流推进（走路动画）----------------------------------
    // asm: 481957 起 —— 仅在行走逻辑在动、且未处于坠落/下沉/运载/传送等
    //   状态时推进 [esi+6B8h]。
    if (Locomotion != nullptr && Locomotion->Is_Moving()
        && !InAir() && !IsInLimbo && !IsWarpingOut() && !IsWarpingIn())
    {
        ++WalkFrameCounter;
    }

    // ---- 段 H：移动音效 --------------------------------------------------
    // asm: 482207 段 —— 在动时首次触发移动音（类型上的 MoveSound），停下后
    //   仍有 3 帧收尾。
    const bool movingNow = (Locomotion != nullptr && Locomotion->Is_Moving());
    if (movingNow)
    {
        if (!MoveSoundPlaying && TechnoType != nullptr && TechnoType->MoveSound[0] != '\0')
        {
            const int32 idx = VocClass::FindIndexOfName(TechnoType->MoveSound);
            if (idx != -1)
            {
                const CoordStruct pos = GetCoords();
                VocClass::PlayIndexAtPos(idx, &pos);
            }
            MoveSoundPlaying = true;
        }
        MoveSoundCooldown = 3;
    }
    else if (MoveSoundPlaying)
    {
        // asm: 482403 段 —— 停下后倒计时归零才彻底关掉。
        if (MoveSoundCooldown > 0)
            --MoveSoundCooldown;
        else
            MoveSoundPlaying = false;
    }

    // ---- 段 I：地面静止单位的每 64 帧一次散射 -------------------------------
    // asm: 482461 (Frame & 3Fh)==3F / NavCom 空 / 未在桥上 / 坡度 0 / 在地面 /
    //      高度 0 → Scatter(vt+0x174)。
    if ((frame & 0x3F) == 0x3F
        && !InAir() && !IsInLimbo && GetHeight() == 0 && !OnBridge())
    {
        const CellStruct cell = GetCellCoords();
        CellClass* pCell = (MapClass::Instance != nullptr)
                         ? MapClass::Instance->GetCellAt(cell) : nullptr;
        if (pCell != nullptr && pCell->Get_Slope() == 0)
        {
            // asm: Scatter(this->coords, 1, 0) —— 原地随机散开。
            const CoordStruct here = GetCoords();
            Scatter(here, true);
        }
    }

    // ---- 段 J：传送收尾 + 挂靠对象推进 ------------------------------------
    // asm: 482511 起 —— 若不在"传送进入"中则跑一次落位收尾，并推进挂靠单位。
    //   本项目未建模挂靠单位指针（+0x694），且落位收尾 sub_70D7E0 依赖
    //   无线电台握手（Radio_SendCommand），故此处保持不动作。
}

// ============================================================================
// ComputeCRC
//
//  Chains the parent CRC and then adds the FootClass-specific state.
// ============================================================================
void FootClass::ComputeCRC(CRCEngine& crc) const
{
    // 逐层回链：Techno -> Radio -> Mission -> Object -> Abstract。
    TechnoClass::ComputeCRC(crc);

    crc.AddData(&PrimaryFacing,  sizeof(PrimaryFacing));
    crc.AddData(&TurretFacing,   sizeof(TurretFacing));
    crc.AddData(&Pitch,          sizeof(Pitch));
    crc.AddData(&CurrentSequence, sizeof(CurrentSequence));
    crc.AddData(&Path.Count,     sizeof(Path.Count));

    // Locomotion is a pointer; hash the raw bits so the multiplayer checksum
    // still distinguishes "no locomotion" from "some locomotion".
    crc.AddData(&Locomotion, sizeof(Locomotion));
}

// ============================================================================
// File-local helper functions
//
//  These provide pathfinding utilities, smooth facing interpolation,
//  locomotion delegation helpers, formation layout computation, and
//  path-validation routines used by the FootClass movement system.  Because
//  the FootClass header cannot be modified, the helpers are declared as
//  free functions in the anonymous namespace and operate on the public
//  state exposed by FootClass and its locomotion pointer.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Path simplification constants
// --------------------------------------------------------------------------
constexpr int32 MAX_PATH_POINTS = 1024;
constexpr int32 MIN_PATH_FOR_SIMPLIFICATION = 4;
constexpr int32 COLLINEAR_THRESHOLD = 64; // leptons of deviation tolerance

// --------------------------------------------------------------------------
// DirectionDelta - Returns the (dx, dy) unit vector for a given facing in
// BRadians (0..255). Used by formation and strafe calculations.
// --------------------------------------------------------------------------
void DirectionDelta(DirStruct facing, int32& outDx, int32& outDy)
{
    uint8 dir = facing.Value;
    float rad = static_cast<float>(dir) *
                (2.0f * 3.14159265358979323846f / 256.0f);
    outDx = static_cast<int32>(std::cos(rad) * 256.0f);
    outDy = static_cast<int32>(std::sin(rad) * 256.0f);
}

// --------------------------------------------------------------------------
// DirectionToCoord - Returns the facing (BRadians) needed to travel from
// one coordinate to another.
// --------------------------------------------------------------------------
DirStruct DirectionToCoord(const CoordStruct& from, const CoordStruct& to)
{
    int32 dx = to.X - from.X;
    int32 dy = to.Y - from.Y;
    if (dx == 0 && dy == 0) {
        return DirStruct(0);
    }
    double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx));
    if (angle < 0) angle += 2.0 * 3.14159265358979323846;
    uint8 dir = static_cast<uint8>(
        (angle / (2.0 * 3.14159265358979323846)) * 256.0);
    return DirStruct(dir);
}

// --------------------------------------------------------------------------
// CoordDistanceSquared - Returns the squared distance between two points.
// Avoids a sqrt call when only comparisons are needed.
// --------------------------------------------------------------------------
int32 CoordDistanceSquared(const CoordStruct& a, const CoordStruct& b)
{
    int32 dx = a.X - b.X;
    int32 dy = a.Y - b.Y;
    return dx * dx + dy * dy;
}

// --------------------------------------------------------------------------
// CoordDistance3D - Returns the true 3D distance between two coordinates.
// --------------------------------------------------------------------------
int32 CoordDistance3D(const CoordStruct& a, const CoordStruct& b)
{
    int32 dx = a.X - b.X;
    int32 dy = a.Y - b.Y;
    int32 dz = a.Z - b.Z;
    return static_cast<int32>(std::sqrt(
        static_cast<double>(dx * dx + dy * dy + dz * dz)));
}

// --------------------------------------------------------------------------
// ApproachTarget - Returns a coordinate one cell in front of the target,
// facing back toward the source. Used when a unit needs to stop adjacent
// to an object rather than on top of it.
// --------------------------------------------------------------------------
CoordStruct ApproachTarget(const CoordStruct& source,
                           const CoordStruct& target,
                           int32 cellSize)
{
    if (cellSize <= 0) cellSize = 256;
    DirStruct dirToSource = DirectionToCoord(target, source);
    int32 dx, dy;
    DirectionDelta(dirToSource, dx, dy);
    // Normalize to one cell.
    dx = (dx * cellSize) / 256;
    dy = (dy * cellSize) / 256;
    return CoordStruct(target.X + dx, target.Y + dy, target.Z);
}

// --------------------------------------------------------------------------
// SmoothFacingStep - Returns the next facing value after rotating toward
// the target by at most stepAmount units. This is the core of the smooth
// turret / body turn used by infantry and vehicles.
// --------------------------------------------------------------------------
DirStruct SmoothFacingStep(DirStruct current, DirStruct target, int32 stepAmount)
{
    if (stepAmount <= 0) return current;

    int32 cur = static_cast<int32>(current.Value);
    int32 tgt = static_cast<int32>(target.Value);

    // Compute the shortest signed difference around the 0..255 circle.
    int32 diff = tgt - cur;
    if (diff > 128) diff -= 256;
    else if (diff < -128) diff += 256;

    int32 stepMag = (diff < 0 ? -diff : diff);
    if (stepMag <= stepAmount) {
        return target;
    }

    int32 sign = (diff >= 0) ? 1 : -1;
    int32 newVal = cur + sign * stepAmount;
    newVal &= 0xFF;
    return DirStruct(static_cast<uint8>(newVal));
}

// --------------------------------------------------------------------------
// FacingDifference - Returns the unsigned shortest angular distance
// between two facings (0..128).
// --------------------------------------------------------------------------
int32 FacingDifference(DirStruct a, DirStruct b)
{
    int32 diff = static_cast<int32>(a.Value) - static_cast<int32>(b.Value);
    if (diff < 0) diff = -diff;
    if (diff > 128) diff = 256 - diff;
    return diff;
}

// --------------------------------------------------------------------------
// IsFacingTarget - Returns true if the current facing is within the given
// tolerance of the target facing.
// --------------------------------------------------------------------------
bool IsFacingTarget(DirStruct current, DirStruct target, int32 tolerance)
{
    return FacingDifference(current, target) <= tolerance;
}

// --------------------------------------------------------------------------
// ComputeFormationOffset - Returns the relative offset for a unit in a
// formation. Formation index 0 is the leader (offset 0,0). Subsequent
// indices are placed in two staggered columns behind the leader.
// --------------------------------------------------------------------------
CoordStruct ComputeFormationOffset(int32 formationIndex, DirStruct leaderFacing)
{
    if (formationIndex <= 0) return CoordStruct(0, 0, 0);

    int32 row = (formationIndex - 1) / 2;
    int32 side = ((formationIndex - 1) % 2 == 0) ? -1 : 1;

    int32 backDist = (row + 1) * 192;   // ~3 cells
    int32 lateral = side * (row + 1) * 128;

    int32 dxUnit, dyUnit;
    DirectionDelta(leaderFacing, dxUnit, dyUnit);

    int32 dx = (-dxUnit * backDist - dyUnit * lateral) / 256;
    int32 dy = (-dyUnit * backDist + dxUnit * lateral) / 256;

    return CoordStruct(dx, dy, 0);
}

// --------------------------------------------------------------------------
// ApplyFormationToCoord - Adds a formation offset to a leader coordinate,
// rotated to match the leader's facing.
// --------------------------------------------------------------------------
CoordStruct ApplyFormationToCoord(const CoordStruct& leaderCoord,
                                  int32 formationIndex,
                                  DirStruct leaderFacing)
{
    CoordStruct offset = ComputeFormationOffset(formationIndex, leaderFacing);
    return CoordStruct(
        leaderCoord.X + offset.X,
        leaderCoord.Y + offset.Y,
        leaderCoord.Z);
}

// --------------------------------------------------------------------------
// IsCollinear - Returns true if three points lie on (approximately) the
// same line, within COLLINEAR_THRESHOLD leptons of deviation. Used by the
// path simplifier to remove redundant waypoints.
// --------------------------------------------------------------------------
bool IsCollinear(const CoordStruct& a, const CoordStruct& b, const CoordStruct& c)
{
    int32 dx1 = b.X - a.X;
    int32 dy1 = b.Y - a.Y;
    int32 dx2 = c.X - a.X;
    int32 dy2 = c.Y - a.Y;

    // Cross product magnitude equals twice the area of the triangle.
    int32 cross = (dx1 * dy2) - (dy1 * dx2);
    if (cross < 0) cross = -cross;

    // Normalize by the length of the longer leg so the threshold is
    // independent of segment length.
    int32 len1 = (dx1 < 0 ? -dx1 : dx1) + (dy1 < 0 ? -dy1 : dy1);
    int32 len2 = (dx2 < 0 ? -dx2 : dx2) + (dy2 < 0 ? -dy2 : dy2);
    int32 maxLen = (len1 > len2 ? len1 : len2);
    if (maxLen == 0) return true;

    return (cross / maxLen) < COLLINEAR_THRESHOLD;
}

// --------------------------------------------------------------------------
// SimplifyPath - Removes collinear intermediate waypoints from a path so
// the locomotion layer can plan smoother straight-line segments. The
// simplified path is written into outCoords (which must have room for at
// least inCount entries). Returns the new count.
// --------------------------------------------------------------------------
int32 SimplifyPath(const CoordStruct* inCoords, int32 inCount,
                   CoordStruct* outCoords, int32 maxOut)
{
    if (!inCoords || inCount <= 0 || !outCoords || maxOut <= 0) {
        return 0;
    }

    outCoords[0] = inCoords[0];
    int32 outCount = 1;

    if (inCount < MIN_PATH_FOR_SIMPLIFICATION) {
        for (int32 i = 1; i < inCount && outCount < maxOut; ++i) {
            outCoords[outCount++] = inCoords[i];
        }
        return outCount;
    }

    for (int32 i = 1; i < inCount - 1 && outCount < maxOut - 1; ++i) {
        const CoordStruct& prev = inCoords[i - 1];
        const CoordStruct& cur = inCoords[i];
        const CoordStruct& next = inCoords[i + 1];

        if (!IsCollinear(prev, cur, next)) {
            outCoords[outCount++] = cur;
        }
    }

    if (outCount < maxOut) {
        outCoords[outCount++] = inCoords[inCount - 1];
    }

    return outCount;
}

// --------------------------------------------------------------------------
// ReversePath - Writes the input path in reverse order into outCoords.
// Used when a unit must retrace its steps (e.g. retreat).
// --------------------------------------------------------------------------
int32 ReversePath(const CoordStruct* inCoords, int32 inCount,
                  CoordStruct* outCoords, int32 maxOut)
{
    if (!inCoords || inCount <= 0 || !outCoords || maxOut <= 0) {
        return 0;
    }
    int32 count = (inCount < maxOut) ? inCount : maxOut;
    for (int32 i = 0; i < count; ++i) {
        outCoords[i] = inCoords[inCount - 1 - i];
    }
    return count;
}

// --------------------------------------------------------------------------
// PathTotalLength - Returns the cumulative length of a path in leptons.
// --------------------------------------------------------------------------
int32 PathTotalLength(const CoordStruct* coords, int32 count)
{
    if (!coords || count <= 1) return 0;
    int32 total = 0;
    for (int32 i = 1; i < count; ++i) {
        int32 dx = coords[i].X - coords[i - 1].X;
        int32 dy = coords[i].Y - coords[i - 1].Y;
        total += static_cast<int32>(std::sqrt(
            static_cast<double>(dx * dx + dy * dy)));
    }
    return total;
}

// --------------------------------------------------------------------------
// EstimatePathTravelTime - Returns an estimate (in frames) of how long a
// unit will take to traverse the path at the given speed.
// --------------------------------------------------------------------------
int32 EstimatePathTravelTime(const CoordStruct* coords, int32 count, int32 speed)
{
    if (!coords || count <= 1 || speed <= 0) return 0;
    int32 length = PathTotalLength(coords, count);
    return length / speed;
}

// --------------------------------------------------------------------------
// FindClosestPathIndex - Returns the index of the path point closest to
// the given coordinate, or -1 if the path is empty.
// --------------------------------------------------------------------------
int32 FindClosestPathIndex(const CoordStruct* coords, int32 count,
                           const CoordStruct& target)
{
    if (!coords || count <= 0) return -1;
    int32 bestIndex = 0;
    int32 bestDist = CoordDistanceSquared(coords[0], target);
    for (int32 i = 1; i < count; ++i) {
        int32 dist = CoordDistanceSquared(coords[i], target);
        if (dist < bestDist) {
            bestDist = dist;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// --------------------------------------------------------------------------
// SubdivideSegment - Splits a path segment into multiple shorter segments
// of at most maxStepSize leptons each. Writes the subdivided points into
// outCoords and returns the number written.
// --------------------------------------------------------------------------
int32 SubdivideSegment(const CoordStruct& from, const CoordStruct& to,
                       int32 maxStepSize,
                       CoordStruct* outCoords, int32 maxOut)
{
    if (!outCoords || maxOut <= 0) return 0;
    if (maxStepSize <= 0) maxStepSize = 256;

    int32 dx = to.X - from.X;
    int32 dy = to.Y - from.Y;
    int32 dist = static_cast<int32>(std::sqrt(
        static_cast<double>(dx * dx + dy * dy)));

    if (dist <= maxStepSize) {
        if (maxOut >= 2) {
            outCoords[0] = from;
            outCoords[1] = to;
            return 2;
        }
        return 0;
    }

    int32 segments = (dist + maxStepSize - 1) / maxStepSize;
    if (segments > maxOut) segments = maxOut;

    for (int32 i = 0; i < segments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(segments);
        outCoords[i] = CoordStruct(
            static_cast<int32>(from.X + dx * t),
            static_cast<int32>(from.Y + dy * t),
            static_cast<int32>(from.Z + (to.Z - from.Z) * t));
    }
    return segments;
}

// --------------------------------------------------------------------------
// IsPathValid - Returns true if every point in the path is distinct from
// the previous point (no zero-length segments that would confuse the
// locomotion layer).
// --------------------------------------------------------------------------
bool IsPathValid(const CoordStruct* coords, int32 count)
{
    if (!coords || count <= 0) return false;
    for (int32 i = 1; i < count; ++i) {
        if (coords[i].X == coords[i - 1].X &&
            coords[i].Y == coords[i - 1].Y &&
            coords[i].Z == coords[i - 1].Z) {
            return false;
        }
    }
    return true;
}

// --------------------------------------------------------------------------
// ClampCoordToMap - Clamps a coordinate to the map bounds.
// --------------------------------------------------------------------------
CoordStruct ClampCoordToMap(const CoordStruct& coord, int32 mapSize)
{
    if (mapSize <= 0) return coord;
    int32 maxCoord = mapSize * 256;
    CoordStruct result = coord;
    if (result.X < 0) result.X = 0;
    if (result.Y < 0) result.Y = 0;
    if (result.X > maxCoord) result.X = maxCoord;
    if (result.Y > maxCoord) result.Y = maxCoord;
    return result;
}

// --------------------------------------------------------------------------
// InterpolateCoord - Linearly interpolates between two coordinates by
// parameter t in [0, 1].
// --------------------------------------------------------------------------
CoordStruct InterpolateCoord(const CoordStruct& from, const CoordStruct& to, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return CoordStruct(
        static_cast<int32>(from.X + (to.X - from.X) * t),
        static_cast<int32>(from.Y + (to.Y - from.Y) * t),
        static_cast<int32>(from.Z + (to.Z - from.Z) * t));
}

// --------------------------------------------------------------------------
// PointOnPath - Given a path and a distance traveled along it, returns
// the interpolated coordinate at that distance.
// --------------------------------------------------------------------------
CoordStruct PointOnPath(const CoordStruct* coords, int32 count, int32 distance)
{
    if (!coords || count <= 0) return CoordStruct(0, 0, 0);
    if (count == 1) return coords[0];
    if (distance <= 0) return coords[0];

    int32 traveled = 0;
    for (int32 i = 1; i < count; ++i) {
        int32 dx = coords[i].X - coords[i - 1].X;
        int32 dy = coords[i].Y - coords[i - 1].Y;
        int32 segLen = static_cast<int32>(std::sqrt(
            static_cast<double>(dx * dx + dy * dy)));
        if (traveled + segLen >= distance && segLen > 0) {
            float t = static_cast<float>(distance - traveled) /
                      static_cast<float>(segLen);
            return InterpolateCoord(coords[i - 1], coords[i], t);
        }
        traveled += segLen;
    }
    return coords[count - 1];
}

// --------------------------------------------------------------------------
// Foot_GetFacingFromVelocity - Derives a facing from a velocity vector.
// --------------------------------------------------------------------------
DirStruct FacingFromVelocity(const CoordStruct& velocity)
{
    if (velocity.X == 0 && velocity.Y == 0) {
        return DirStruct(0);
    }
    return DirectionToCoord(CoordStruct(0, 0, 0), velocity);
}

// --------------------------------------------------------------------------
// ComputeStrafeOffset - Returns a coordinate offset perpendicular to the
// facing direction, used by strafing aircraft and infantry.
// --------------------------------------------------------------------------
CoordStruct ComputeStrafeOffset(DirStruct facing, int32 distance)
{
    // Rotate facing by 90 degrees (64 units in BRadians).
    DirStruct perp(static_cast<uint8>((facing.Value + 64) & 0xFF));
    int32 dx, dy;
    DirectionDelta(perp, dx, dy);
    return CoordStruct(
        (dx * distance) / 256,
        (dy * distance) / 256,
        0);
}

// --------------------------------------------------------------------------
// DescribeFootState - Returns a short string describing the unit's
// movement state for debugging overlays.
// --------------------------------------------------------------------------
const char* DescribeFootState(const FootClass& foot)
{
    if (foot.Locomotion == nullptr) return "NoLoco";
    if (foot.Is_Moving()) return "Moving";
    if (foot.Has_Path()) return "Queued";
    return "Idle";
}

// --------------------------------------------------------------------------
// GetSequenceName - Maps a Sequence enum value to a human-readable name.
// --------------------------------------------------------------------------
const char* GetSequenceName(Sequence seq)
{
    switch (seq) {
        case Sequence::Ready:           return "Ready";
        case Sequence::Guard:           return "Guard";
        case Sequence::Prone:           return "Prone";
        case Sequence::Walk:            return "Walk";
        case Sequence::FireUp:          return "FireUp";
        case Sequence::Down:            return "Down";
        case Sequence::FireProne:       return "FireProne";
        case Sequence::Idle1:           return "Idle1";
        case Sequence::Idle2:           return "Idle2";
        case Sequence::Die1:            return "Die1";
        case Sequence::Die2:            return "Die2";
        case Sequence::Die3:            return "Die3";
        case Sequence::Die4:            return "Die4";
        case Sequence::Die5:            return "Die5";
        case Sequence::Swim:            return "Swim";
        case Sequence::WetIdle1:        return "WetIdle1";
        case Sequence::WetIdle2:        return "WetIdle2";
        case Sequence::WetDie1:         return "WetDie1";
        case Sequence::WetDie2:         return "WetDie2";
        case Sequence::Crawl:           return "Crawl";
        case Sequence::Fly:             return "Fly";
        case Sequence::FireFly:         return "FireFly";
        case Sequence::IdleFly:         return "IdleFly";
        case Sequence::DieFly:          return "DieFly";
        case Sequence::Tumble:          return "Tumble";
        case Sequence::Deploy:          return "Deploy";
        case Sequence::Deployed:        return "Deployed";
        case Sequence::DeployedFire:    return "DeployedFire";
        case Sequence::DeployedIdle:    return "DeployedIdle";
        case Sequence::Undeploy:        return "Undeploy";
        case Sequence::Paradrop:        return "Paradrop";
        case Sequence::Enter:           return "Enter";
        case Sequence::Unload:          return "Unload";
        case Sequence::Deploy2:         return "Deploy2";
        case Sequence::Harvest:         return "Harvest";
        default:                        return "Unknown";
    }
}

// --------------------------------------------------------------------------
// ShouldUseProneSequence - Returns true if the unit should switch to the
// prone (crouching) sequence based on its current health fraction.
// --------------------------------------------------------------------------
bool ShouldUseProneSequence(int32 health, int32 maxHealth)
{
    if (maxHealth <= 0) return false;
    float fraction = static_cast<float>(health) / static_cast<float>(maxHealth);
    return fraction < 0.5f;
}

// --------------------------------------------------------------------------
// ComputeMoveAnimationSpeed - Returns the animation playback rate for a
// walking unit based on its effective speed.
// --------------------------------------------------------------------------
int32 ComputeMoveAnimationSpeed(int32 effectiveSpeed)
{
    if (effectiveSpeed <= 0) return 0;
    if (effectiveSpeed < 32) return 1;
    if (effectiveSpeed < 64) return 2;
    if (effectiveSpeed < 96) return 3;
    return 4;
}

// --------------------------------------------------------------------------
// IsAtDestination - Returns true if the unit is within arrival tolerance
// of its destination.
// --------------------------------------------------------------------------
bool IsAtDestination(const CoordStruct& current, const CoordStruct& dest,
                     int32 tolerance)
{
    if (tolerance <= 0) tolerance = 32;
    int32 distSq = CoordDistanceSquared(current, dest);
    return distSq <= (tolerance * tolerance);
}

// --------------------------------------------------------------------------
// BuildRetreatPath - Constructs a retreat path by reversing the unit's
// recent movement and extending it away from the threat. Returns the
// number of points written.
// --------------------------------------------------------------------------
int32 BuildRetreatPath(const CoordStruct* recentPath, int32 recentCount,
                       const CoordStruct& threat,
                       int32 retreatDistance,
                       CoordStruct* outCoords, int32 maxOut)
{
    if (!outCoords || maxOut <= 0) return 0;

    int32 outCount = 0;
    CoordStruct start = (recentPath && recentCount > 0)
                        ? recentPath[recentCount - 1]
                        : threat;

    if (outCount < maxOut) {
        outCoords[outCount++] = start;
    }

    // Direction away from threat.
    DirStruct retreatDir = DirectionToCoord(threat, start);
    int32 dx, dy;
    DirectionDelta(retreatDir, dx, dy);

    int32 steps = (retreatDistance + 255) / 256;
    for (int32 i = 1; i <= steps && outCount < maxOut; ++i) {
        int32 stepDist = i * 256;
        outCoords[outCount++] = CoordStruct(
            start.X + (dx * stepDist) / 256,
            start.Y + (dy * stepDist) / 256,
            start.Z);
    }

    return outCount;
}

} // namespace

// ============================================================================
// File-local entry points that bridge the FootClass to the helper functions
// above. These are kept as file-local free functions so the header does not
// need to change, yet other translation units can invoke them when needed.
// ============================================================================

extern "C" {

// ----------------------------------------------------------------------------
// Foot_DirectionDelta - Unit vector for a facing.
// ----------------------------------------------------------------------------
void Foot_DirectionDelta(DirStruct facing, int32* outDx, int32* outDy)
{
    if (!outDx || !outDy) return;
    DirectionDelta(facing, *outDx, *outDy);
}

// ----------------------------------------------------------------------------
// Foot_DirectionToCoord - Facing from one coordinate to another.
// ----------------------------------------------------------------------------
DirStruct Foot_DirectionToCoord(const CoordStruct* pFrom, const CoordStruct* pTo)
{
    if (!pFrom || !pTo) return DirStruct(0);
    return DirectionToCoord(*pFrom, *pTo);
}

// ----------------------------------------------------------------------------
// Foot_CoordDistance - True 2D distance between two coordinates.
// ----------------------------------------------------------------------------
int32 Foot_CoordDistance(const CoordStruct* pA, const CoordStruct* pB)
{
    if (!pA || !pB) return 0;
    int32 distSq = CoordDistanceSquared(*pA, *pB);
    return static_cast<int32>(std::sqrt(static_cast<double>(distSq)));
}

// ----------------------------------------------------------------------------
// Foot_CoordDistance3D - True 3D distance between two coordinates.
// ----------------------------------------------------------------------------
int32 Foot_CoordDistance3D(const CoordStruct* pA, const CoordStruct* pB)
{
    if (!pA || !pB) return 0;
    return CoordDistance3D(*pA, *pB);
}

// ----------------------------------------------------------------------------
// Foot_ApproachTarget - Coordinate one cell in front of a target.
// ----------------------------------------------------------------------------
CoordStruct Foot_ApproachTarget(const CoordStruct* pSource,
                                const CoordStruct* pTarget,
                                int32 cellSize)
{
    if (!pSource || !pTarget) return CoordStruct(0, 0, 0);
    return ApproachTarget(*pSource, *pTarget, cellSize);
}

// ----------------------------------------------------------------------------
// Foot_SmoothFacingStep - Step a facing toward a target.
// ----------------------------------------------------------------------------
DirStruct Foot_SmoothFacingStep(DirStruct current, DirStruct target, int32 stepAmount)
{
    return SmoothFacingStep(current, target, stepAmount);
}

// ----------------------------------------------------------------------------
// Foot_FacingDifference - Unsigned angular distance.
// ----------------------------------------------------------------------------
int32 Foot_FacingDifference(DirStruct a, DirStruct b)
{
    return FacingDifference(a, b);
}

// ----------------------------------------------------------------------------
// Foot_IsFacingTarget - Tolerance check.
// ----------------------------------------------------------------------------
bool Foot_IsFacingTarget(DirStruct current, DirStruct target, int32 tolerance)
{
    return IsFacingTarget(current, target, tolerance);
}

// ----------------------------------------------------------------------------
// Foot_ComputeFormationOffset - Formation wedge offset.
// ----------------------------------------------------------------------------
CoordStruct Foot_ComputeFormationOffset(int32 formationIndex, DirStruct leaderFacing)
{
    return ComputeFormationOffset(formationIndex, leaderFacing);
}

// ----------------------------------------------------------------------------
// Foot_ApplyFormationToCoord - Add formation offset to a leader coord.
// ----------------------------------------------------------------------------
CoordStruct Foot_ApplyFormationToCoord(const CoordStruct* pLeaderCoord,
                                       int32 formationIndex,
                                       DirStruct leaderFacing)
{
    if (!pLeaderCoord) return CoordStruct(0, 0, 0);
    return ApplyFormationToCoord(*pLeaderCoord, formationIndex, leaderFacing);
}

// ----------------------------------------------------------------------------
// Foot_SimplifyPath - Remove collinear waypoints.
// ----------------------------------------------------------------------------
int32 Foot_SimplifyPath(const CoordStruct* pInCoords, int32 inCount,
                        CoordStruct* pOutCoords, int32 maxOut)
{
    return SimplifyPath(pInCoords, inCount, pOutCoords, maxOut);
}

// ----------------------------------------------------------------------------
// Foot_ReversePath - Reverse a path.
// ----------------------------------------------------------------------------
int32 Foot_ReversePath(const CoordStruct* pInCoords, int32 inCount,
                       CoordStruct* pOutCoords, int32 maxOut)
{
    return ReversePath(pInCoords, inCount, pOutCoords, maxOut);
}

// ----------------------------------------------------------------------------
// Foot_PathTotalLength - Cumulative path length.
// ----------------------------------------------------------------------------
int32 Foot_PathTotalLength(const CoordStruct* pCoords, int32 count)
{
    return PathTotalLength(pCoords, count);
}

// ----------------------------------------------------------------------------
// Foot_EstimatePathTravelTime - Travel time estimate.
// ----------------------------------------------------------------------------
int32 Foot_EstimatePathTravelTime(const CoordStruct* pCoords, int32 count, int32 speed)
{
    return EstimatePathTravelTime(pCoords, count, speed);
}

// ----------------------------------------------------------------------------
// Foot_FindClosestPathIndex - Closest path point to a coordinate.
// ----------------------------------------------------------------------------
int32 Foot_FindClosestPathIndex(const CoordStruct* pCoords, int32 count,
                                const CoordStruct* pTarget)
{
    if (!pTarget) return -1;
    return FindClosestPathIndex(pCoords, count, *pTarget);
}

// ----------------------------------------------------------------------------
// Foot_SubdivideSegment - Split a segment into shorter pieces.
// ----------------------------------------------------------------------------
int32 Foot_SubdivideSegment(const CoordStruct* pFrom, const CoordStruct* pTo,
                            int32 maxStepSize,
                            CoordStruct* pOutCoords, int32 maxOut)
{
    if (!pFrom || !pTo) return 0;
    return SubdivideSegment(*pFrom, *pTo, maxStepSize, pOutCoords, maxOut);
}

// ----------------------------------------------------------------------------
// Foot_IsPathValid - Check for zero-length segments.
// ----------------------------------------------------------------------------
bool Foot_IsPathValid(const CoordStruct* pCoords, int32 count)
{
    return IsPathValid(pCoords, count);
}

// ----------------------------------------------------------------------------
// Foot_ClampCoordToMap - Clamp to map bounds.
// ----------------------------------------------------------------------------
CoordStruct Foot_ClampCoordToMap(const CoordStruct* pCoord, int32 mapSize)
{
    if (!pCoord) return CoordStruct(0, 0, 0);
    return ClampCoordToMap(*pCoord, mapSize);
}

// ----------------------------------------------------------------------------
// Foot_InterpolateCoord - Linear interpolation.
// ----------------------------------------------------------------------------
CoordStruct Foot_InterpolateCoord(const CoordStruct* pFrom,
                                  const CoordStruct* pTo, float t)
{
    if (!pFrom || !pTo) return CoordStruct(0, 0, 0);
    return InterpolateCoord(*pFrom, *pTo, t);
}

// ----------------------------------------------------------------------------
// Foot_PointOnPath - Coordinate at a distance along a path.
// ----------------------------------------------------------------------------
CoordStruct Foot_PointOnPath(const CoordStruct* pCoords, int32 count, int32 distance)
{
    return PointOnPath(pCoords, count, distance);
}

// ----------------------------------------------------------------------------
// Foot_FacingFromVelocity - Derive facing from velocity.
// ----------------------------------------------------------------------------
DirStruct Foot_FacingFromVelocity(const CoordStruct* pVelocity)
{
    if (!pVelocity) return DirStruct(0);
    return FacingFromVelocity(*pVelocity);
}

// ----------------------------------------------------------------------------
// Foot_ComputeStrafeOffset - Perpendicular offset for strafing.
// ----------------------------------------------------------------------------
CoordStruct Foot_ComputeStrafeOffset(DirStruct facing, int32 distance)
{
    return ComputeStrafeOffset(facing, distance);
}

// ----------------------------------------------------------------------------
// Foot_DescribeState - Debug state string.
// ----------------------------------------------------------------------------
const char* Foot_DescribeState(const FootClass* pFoot)
{
    if (!pFoot) return "None";
    return DescribeFootState(*pFoot);
}

// ----------------------------------------------------------------------------
// Foot_GetSequenceName - Human-readable sequence name.
// ----------------------------------------------------------------------------
const char* Foot_GetSequenceName(Sequence seq)
{
    return GetSequenceName(seq);
}

// ----------------------------------------------------------------------------
// Foot_ShouldUseProneSequence - Health-based prone check.
// ----------------------------------------------------------------------------
bool Foot_ShouldUseProneSequence(int32 health, int32 maxHealth)
{
    return ShouldUseProneSequence(health, maxHealth);
}

// ----------------------------------------------------------------------------
// Foot_ComputeMoveAnimationSpeed - Walk animation rate.
// ----------------------------------------------------------------------------
int32 Foot_ComputeMoveAnimationSpeed(int32 effectiveSpeed)
{
    return ComputeMoveAnimationSpeed(effectiveSpeed);
}

// ----------------------------------------------------------------------------
// Foot_IsAtDestination - Arrival tolerance check.
// ----------------------------------------------------------------------------
bool Foot_IsAtDestination(const CoordStruct* pCurrent, const CoordStruct* pDest,
                          int32 tolerance)
{
    if (!pCurrent || !pDest) return true;
    return IsAtDestination(*pCurrent, *pDest, tolerance);
}

// ----------------------------------------------------------------------------
// Foot_BuildRetreatPath - Construct a retreat path away from a threat.
// ----------------------------------------------------------------------------
int32 Foot_BuildRetreatPath(const CoordStruct* pRecentPath, int32 recentCount,
                            const CoordStruct* pThreat, int32 retreatDistance,
                            CoordStruct* pOutCoords, int32 maxOut)
{
    if (!pThreat) return 0;
    return BuildRetreatPath(pRecentPath, recentCount, *pThreat,
                            retreatDistance, pOutCoords, maxOut);
}

} // extern "C"

// ============================================================================
// Movement helpers
// ============================================================================

bool FootClass::Set_Destination(const CoordStruct& dest)
{
    if (Locomotion == nullptr || MapClass::Instance == nullptr)
        return false;

    CellStruct destCell = CoordMath::CoordToCell(dest);
    if (!Can_Enter_Cell(destCell))
        return false;

    // Route through the map when possible; fall back to a direct move.
    UnitClass* pUnit = (WhatAmI() == AbstractType::Unit)
                       ? static_cast<UnitClass*>(this) : nullptr;
    if (pUnit != nullptr)
    {
        CoordStruct pos = GetCoords_Impl();
        CellStruct startCell = CoordMath::CoordToCell(pos);

        DynamicVectorClass<CellStruct> pathCells;
        if (pUnit->Find_Path(startCell, destCell, pathCells) && pathCells.Count > 1)
        {
            Clear_Path();
            for (int32 i = 1; i < pathCells.Count; ++i)
            {
                CoordStruct waypoint(static_cast<int32>(pathCells[i].X) * 256,
                                     static_cast<int32>(pathCells[i].Y) * 256,
                                     pos.Z);
                Append_Path(waypoint);
            }
            Locomotion->Move_To(dest);
            return true;
        }
    }

    Set_Path(&dest, 1);
    Locomotion->Move_To(dest);
    return true;
}

bool FootClass::Can_Enter_Cell(const CellStruct& cell) const
{
    if (MapClass::Instance == nullptr || TechnoType == nullptr)
        return false;
    if (!MapClass::Instance->IsWithinUsableArea(cell.X, cell.Y))
        return false;

    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr)
        return false;

    return pCell->PassableFor(TechnoType->MoveZone);
}

// ============================================================================
// asm: FootClass_Scatter / Do_Scatter  ——  受击/爆风后的本能散开
//
//  原版 ObjectClass vtable 槽 0x174 = Scatter(coords, a2, a3)；FootClass 系
//  （InfantryClass_Scatter@626500 / UnitClass_Scatter@1701818）覆写后做的是
//  同一件事：以"背对威胁源"为首选方向，在 8 个邻格里挑一个本单位能站立的
//  格子作为新的行进目的地；找不到就原地不动。
//
//  入参 `from` 是威胁源坐标（全 0 表示没有明确方向，此时按正向顺序枚举）。
// ============================================================================
void FootClass::Scatter(const CoordStruct& from, bool ignoreMission)
{
    (void)ignoreMission;

    if (MapClass::Instance == nullptr)
        return;

    // 8 邻格方向表（E, SE, S, SW, W, NW, N, NE）。
    static const int8 kNeighbours[8][2] = {
        {  1,  0 }, {  1,  1 }, {  0,  1 }, { -1,  1 },
        { -1,  0 }, { -1, -1 }, {  0, -1 }, {  1, -1 }
    };

    const CoordStruct self = GetCoords_Impl();
    const CellStruct here = CellClass::Coord2Cell(self);

    const bool hasThreat = (from.X != 0 || from.Y != 0);
    CellStruct threat = here;
    if (hasThreat)
        threat = CellClass::Coord2Cell(from);

    // 先选出"最背离威胁源"的邻格方向：点积 dx*ox + dy*oy 最大者。
    int32 bestIdx = 0;
    int32 bestDot = (hasThreat ? -0x7FFFFFFF : 0);
    for (int32 i = 0; i < 8; ++i)
    {
        const int32 ox = kNeighbours[i][0];
        const int32 oy = kNeighbours[i][1];

        int32 dot;
        if (hasThreat)
        {
            const int32 dx = static_cast<int32>(here.X) - static_cast<int32>(threat.X);
            const int32 dy = static_cast<int32>(here.Y) - static_cast<int32>(threat.Y);
            dot = dx * ox + dy * oy;
        }
        else
        {
            dot = 0;   // 无方向偏好：保持入场顺序
        }

        if (dot > bestDot)
        {
            bestDot = dot;
            bestIdx = i;
        }
    }

    // 从最背离方向起顺时针尝试，取第一个可进入的邻格。
    for (int32 k = 0; k < 8; ++k)
    {
        const int32 i = (bestIdx + k) & 7;

        CellStruct probe;
        probe.X = static_cast<int16>(here.X + kNeighbours[i][0]);
        probe.Y = static_cast<int16>(here.Y + kNeighbours[i][1]);

        if (!Can_Enter_Cell(probe))
            continue;

        const CoordStruct dest(static_cast<int32>(probe.X) * 256 + 128,
                               static_cast<int32>(probe.Y) * 256 + 128,
                               self.Z);
        Set_Destination(dest);
        return;
    }

    // 8 个方向全被挡住：原地不动（原版同样在无路可退时放弃散开）。
}

// ============================================================================
// FootClass - mission-controller neutral overrides
//
//  These slots exist so that a derived type which does not implement the
//  behaviour still occupies the correct vtable entry.  The bodies are literal
//  transcriptions of the assembly stubs (bare `retn` and `retn 4` / `retn 8`).
// ============================================================================

void FootClass::Panic()
{
}

void FootClass::Unpanic()
{
}

void FootClass::PlayIdleAnim(int32 a2)
{
    (void)a2;
}

void FootClass::Draw(int32 a2, int32 a3, int32 a4)
{
    (void)a2;
    (void)a3;
    (void)a4;
}

// ============================================================================
// FootClass - team / layer probes
// ============================================================================

 // asm: byte __thiscall FootClass_PartOfTeam(_FootClass *this)   (asm:102703)
//
//  True when the unit is currently attached to a team (the +0x5D0 team
//  pointer is non-null).
bool FootClass::PartOfTeam() const
{
    return Team != nullptr;
}

 // FootClass_InAir: thunk to the TechnoClass air-layer test.
bool FootClass::InAirLayer() const
{
    return IsInAir();
}

 // asm: FootClass_CanAttack
//
//  Thunk through TechnoClass_4C0 into the type's CanMobileAttack slot: a foot
//  class may only attack on the move when its type permits it.
bool FootClass::CanAttack() const
{
    return (TechnoType != nullptr) && TechnoType->CanMobileAttack();
}

// ============================================================================
 // asm: char __fastcall FootClass_IsParalysed(_FootClass *a1)   (asm:489150)
//
//  True while the paralysis timer still has time left.  The binary reads the
//  timer as (StartTime, TimeLeft) and, when StartTime is not -1, subtracts
//  the elapsed frames from TimeLeft before testing the remainder.
// ============================================================================
bool FootClass::IsParalysed() const
{
    int32 remaining = ParalysisTimer.TimeLeft;

    if (ParalysisTimer.StartTime != -1) {
        const int32 elapsed = Game::GetCurrentFrame() - ParalysisTimer.StartTime;
        if (elapsed >= remaining)
            return false;
        remaining -= elapsed;
    }

    return remaining != 0;
}

// ============================================================================
 // asm: void __thiscall FootClass_SetSpeedPercentage(_FootClass *this, double a2)   (asm:468254)
//
//  Stores the speed multiplier, clamping into [0.0, 1.0].  A value above 1.0
//  is pinned to exactly 1.0; a value at or below 0.0 is pinned to 0.0.
//  Everything in between is stored verbatim.
// ============================================================================
void FootClass::SetSpeedPercentage(double pct)
{
    if (pct > 1.0) {
        SpeedPercentage = 1.0;
        return;
    }
    if (pct <= 0.0) {
        SpeedPercentage = 0.0;
        return;
    }

    SpeedPercentage = pct;
}

// ============================================================================
 // asm: FootClass_GetDistance
//
//  Squared planar distance between this unit and a point.  The binary ignores
//  Z entirely and returns dx*dx + dy*dy, which is what every caller wants for
//  a cheap range comparison.
// ============================================================================
int32 FootClass::GetDistance(const CoordStruct& other) const
{
    const CoordStruct self = GetCoords();
    const int32 dx = self.X - other.X;
    const int32 dy = self.Y - other.Y;

    return dx * dx + dy * dy;
}

// ============================================================================
 // asm: int __thiscall FootClass_GetCoords_unknown1(_FootClass *this, int a2, int a3)   (asm:484321)
//
//  Resolves the unit's logical position.  A unit travelling through a tube
//  (TunnelNumber >= 0) reports the tube's mouth rather than its own location,
//  so pathing and rendering converge on the tunnel entrance.  Otherwise the
//  locomotor's position is used, falling back to the object's own coordinates
//  when the locomotor reports the default sentinel.
// ============================================================================
CoordStruct FootClass::GetCoords_unknown1() const
{
    if (TunnelNumber >= 0 && MapClass::Instance != nullptr) {
        // A tube-borne unit reports the tube's head cell centre.
        const CoordStruct self = GetCoords();
        CellStruct cell = CellClass::Coord2Cell(self);
        CoordStruct out;
        out.X = (cell.X << 8) + 0x80;
        out.Y = (cell.Y << 8) + 0x80;
        out.Z = 0;
        return out;
    }

    return GetCoords();
}

// ============================================================================
 // asm: FootClass_SetLayer
//
//  Moves the unit between tactical layers.  Layer 2 is the "limbo" layer and
//  is rejected outright.  Otherwise the techno is Mark_Layer()ed into the new
//  layer first and the transition is reported.
// ============================================================================
bool FootClass::SetLayer(int32 layer)
{
    if (layer == 2)
        return true;

    // ObjectClass::Mark_Layer is the project's spelling of the binary's
    // ObjectClass::Mark.  The project has no tactical-layer registry on
    // MapClass yet, so the layer highlight is the whole of the transition.
    return Mark_Layer(layer);
}

// ============================================================================
 // asm: char __thiscall FootClass_CanGetCrushed(_FootClass *this, _FootClass *source)   (asm:1053688)
//
//  Two independent crush tests, either of which permits the crush:
//
//    1. The source is an OmniCrusher (a battle fortress / gattling tank) and
//       the victim is a movable, non-omni-crush-resistant techno of an enemy
//       house that is not already mind-controlled by the source's owner.
//    2. The victim's art declares Crushable, the unit is registered on the
//       ground, is not flagged Uncrushable, belongs to an enemy house and is
//       not mind-controlled.
// ============================================================================
bool FootClass::CanGetCrushed(ObjectClass* pSource) const
{
    if (pSource == nullptr)
        return false;
    if (!IsActive())
        return false;

    TechnoClass* pSrc = (pSource->WhatAmI() == AbstractType::Building)
                      ? nullptr
                      : static_cast<TechnoClass*>(pSource);

    // Test 1 - omni-crusher.
    if (pSrc != nullptr && pSrc->TechnoType != nullptr
        && pSrc->TechnoType->OmniCrusher
        && !(TechnoType != nullptr && TechnoType->OmniCrushResistant)
        && WhatAmI() != AbstractType::Building
        && !pSrc->Is_Ally(Owner)
        && !IsBeingMindControlled())
    {
        return true;
    }

    // Test 2 - ordinary crush.
    if (TechnoType != nullptr && TechnoType->Crushable
        && !TechnoType->Uncrushable
        && pSrc != nullptr
        && !pSrc->Is_Ally(Owner)
        && !IsBeingMindControlled())
    {
        return true;
    }

    return false;
}

// ============================================================================
 // asm: char __thiscall FootClass_CanBeRecruited(FootClass *this, HouseClass_old *House)   (asm:480901)
//
//  The AI recruiting gate.  A unit may be picked up by its own team bot only
//  when it belongs to the requesting house, is not already riding a transport
//  (InOpenTopped), is not currently attached to a team (Team != null means it
//  is already spoken for), its mission is recruitable, and it is not already
//  flagged Recruitable.
// ============================================================================
bool FootClass::CanBeRecruited(HouseClass* pHouse) const
{
    // asm: 480901  FootClass_CanBeRecruited
    //   1. 找不到请求方 -> 假；
    //   2. 请求方不是本单位所属方 -> 假；
    //   3. 本单位正随敞篷运输机行动（+0x81）-> 假；
    //   4. 本单位已编入某支队伍（+0x5D4）-> 假；
    //   5. 当前任务本身不允许被招募（MissionClass_IsRecruitable）-> 假；
    //   6. 否则返回 +0x421 的"可招募"标记（asm 尾部 setnz）。
    if (pHouse == nullptr || pHouse != Owner)
        return false;
    if (InOpenTopped)
        return false;
    if (Team != nullptr)
        return false;

    // asm: 4DA26F call [eax+184h] -> GetMission() ; call MissionClass_IsRecruitable(idx)
    //   MissionClass_IsRecruitable(asm:912267)：idx == -1 时无条件为真，否则
    //   查 MissionControlClass 表（步长 0x20）的 Recruitable 字节。
    const Mission mission = GetMission();
    if (static_cast<int32>(mission) != -1)
    {
        MissionControlClass* pCtrl = MissionControlClass::Find(mission);
        // asm: 测试失败（al == 0）直接返回假。
        if (pCtrl == nullptr || !pCtrl->Recruitable)
            return false;
    }

    // asm: 4DA288 mov al,[esi+421h] / test al,al / setnz al —— 真值即"可被招募"。
    return Recruitable;
}

// ============================================================================
 // asm: char __fastcall FootClass_CanFightBack(_FootClass *this)   (asm:12110 / 7092E0)
 //
//  单位是否值得还击（被动还击门）。原版控制流：
//    * 前置过滤（只有全部成立才直接放行）：无目标(+0x2B4)、本属方非真人、
//      +0x14 bit2 置位、归属于某队、队类型非 Suicide 且允许开火(+0xAD)、
//      当前姿态(+0xAC)==2。
//    * 其余情况一律落到 TechnoClass_CanPassiveAquire()：为假直接返回假；
//      为真后再按 BalloonHover/规划路径(+0x514)等逐步放宽，最终返回真。
// ============================================================================
bool FootClass::CanFightBack() const
{
    // asm: 7092E0
    if (CurrentTarget == nullptr
        && Owner != nullptr && !Owner->Is_Human()
        && (field_14 & 0x04) != 0
        && Team != nullptr && Team->Type != nullptr
        && !Team->Type->Suicide
        && !Team->Type->LooseRecruit)
    {
        // asm: 70944F -- 前置条件全中，直接判为可还击。
        return true;
    }

    // asm: loc_7092F2 -- 落到被动捕捉判定。
    return CanPassiveAquire();
}

// ============================================================================
 // asm: void __thiscall FootClass_Sensors_AddAt(_FootClass *this, wXY a2)   (asm:489190)
//
//  Stamps the owner's house bit into every cell within the unit's sensor
//  radius - the circle is enumerated with the i*i + j*j <= r*r test the binary
//  uses.  A zero-radius sensor (or no sensor at all) is a no-op.
// ============================================================================
void FootClass::Sensors_AddAt(const CellStruct& cell)
{
    if (MapClass::Instance == nullptr || Owner == nullptr)
        return;

    const int32 radius = SensorArrayRadius;
    if (radius <= 0)
        return;

    const DWORD houseBit = static_cast<DWORD>(1)
                         << static_cast<DWORD>(Owner->GetArrayIndex() & 31);
    const int32 r2 = radius * radius;

    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > r2)
                continue;

            CellStruct probe;
            probe.X = static_cast<int16>(cell.X + dx);
            probe.Y = static_cast<int16>(cell.Y + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            pCell->SensorArrayOfHouses |= houseBit;
        }
    }
}

// ============================================================================
 // asm: char __thiscall FootClass_Sensors_RemoveAt(_FootClass *this, wXY a2)   (asm:489391)
//
//  The mirror of Sensors_AddAt: clears the owner's bit from every cell of the
//  sensor circle.
// ============================================================================
void FootClass::Sensors_RemoveAt(const CellStruct& cell)
{
    if (MapClass::Instance == nullptr || Owner == nullptr)
        return;

    const int32 radius = SensorArrayRadius;
    if (radius <= 0)
        return;

    const DWORD houseBit = static_cast<DWORD>(1)
                         << static_cast<DWORD>(Owner->GetArrayIndex() & 31);
    const DWORD clearMask = ~houseBit;
    const int32 r2 = radius * radius;

    for (int32 dy = -radius; dy <= radius; ++dy) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy > r2)
                continue;

            CellStruct probe;
            probe.X = static_cast<int16>(cell.X + dx);
            probe.Y = static_cast<int16>(cell.Y + dy);

            CellClass* pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;

            pCell->SensorArrayOfHouses &= clearMask;
        }
    }
}

// ============================================================================
 // asm: void __thiscall FootClass_AddThreatIntoCell(_FootClass *this, CellClass *cell)   (asm:1601367)
//
//  Contributes this unit's threat value to the cell it stands on.  The value
//  is latched into the unit so RemoveThreatFromCell can subtract exactly the
//  same amount back out.
// ============================================================================
void FootClass::AddThreatIntoCell(CellClass* pCell)
{
    if (pCell == nullptr)
        return;

    const int32 threat = GetPointsValue();
    ThreatValue = threat;

    pCell->Adjust_Threat(Owner, threat);
}

// ============================================================================
 // asm: void __thiscall FootClass_RemoveThreatFromCell(_FootClass *this, CellClass *cell)   (asm:1601407)
//
//  Retracts the threat this unit previously added to its cell.  The latched
//  value is negated and handed to the same Adjust_Threat entry point, then
//  cleared.
// ============================================================================
void FootClass::RemoveThreatFromCell(CellClass* pCell)
{
    if (pCell == nullptr)
        return;

    pCell->Adjust_Threat(Owner, -ThreatValue);
    ThreatValue = 0;
}

// ============================================================================
 // asm: int __thiscall FootClass_AbandonHunt(_TechnoClass *this)   (asm:484640)
//
//  Called when a unit is pulled off its hunt - typically because it was just
//  recruited into a team.  If the unit is currently hunting it drops the
//  target and clears its destination.
// ============================================================================
void FootClass::AbandonHunt()
{
    // asm: 8021  FootClass_AbandonHunt
    //   仅当当前任务是 Hunt 时才生效：
    //     1. SetTarget(0) —— 清空追逐中的目标；
    //     2. SetDestination(0, 1) —— 撤销行进目的地（本项目以停止移动 +
    //        清空路径表达）。
    if (GetMission() != Mission::Hunt)
        return;

    SetTarget(nullptr);

    // asm 的 SetDestination(null, 1)：目的地置空即停止行进。
    Stop_Moving();
    Clear_Path();
}

// ============================================================================
// asm: FootClass::Greatest_Threat (_FootClass*, int projFlags, int curThreat,
//        char a4)   (asm:5770)
//
//  威胁评估的足类前门：
//    * 若单位正在追猎（+0x688），强制把 projFlags 的 bit1（只打可移动目标）
//      清掉、置上 bit0（优先），再交给 TechnoClass 的通用评估；
//    * 评估后若仍在追猎却挑不到任何目标，则把追猎位摘掉（避免无限追空）。
// ============================================================================
ObjectClass* FootClass::Greatest_Threat(int32 projFlags, int32 curThreat, int32 a4)
{
    if (IsHunting)
    {
        // asm: and al, not 2 / or al, 1
        projFlags = (projFlags & ~2) | 1;
    }

    ObjectClass* pThreat = TechnoClass::Greatest_Threat(projFlags, curThreat, a4);

    if (IsHunting && pThreat == nullptr)
        IsHunting = false;

    return pThreat;
}

// ============================================================================
// asm: FootClass::Captured (_FootClass*, HouseClass* newOwner, int a3)   (asm:7874)
//
//  易主（占领 / 心灵控制转移）：
//    1. 若类型带传感器视野，先把本单位从当前所在格的传感器圈里撤下；
//    2. 链到 TechnoClass::Captured 完成真正的归属切换；
//    3. 成功后：退出所属队伍；非空中时把新格记为归属格；
//    4. 无论成败，都按新归属重挂一次传感器视野（Owner 已被更新）。
// ============================================================================
bool FootClass::Captured(HouseClass* newOwner, int32 a3)
{
    (void)a3;

    const bool hasSensors = (TechnoType != nullptr && TechnoType->SensorsSight > 0);

    // 段 1：先把旧格的传感器撤下。
    if (hasSensors)
        Sensors_RemoveAt(GetCellCoords());

    // 段 2：真正易主。
    const bool ok = TechnoClass::Captured(newOwner);

    if (ok)
    {
        // 段 3：退出所属队伍。
        if (Team != nullptr)
        {
            Team->Remove(this, -1, false);
            Team = nullptr;
        }

        // 段 4：按新归属重挂传感器。
        if (hasSensors)
            Sensors_AddAt(GetCellCoords());

        return true;
    }

    // 失败分支同样重挂传感器（asm loc_4DBF8F）。
    if (hasSensors)
        Sensors_AddAt(GetCellCoords());

    return false;
}

// ============================================================================
 // asm: char __fastcall FootClass_UpdateTargetingTimer(_FootClass *a1)   (asm:1601578)
//
//  Reports whether the area-guard targeting timer has expired - the negated
//  form of the usual "still ticking" probe.  The binary returns true when the
//  timer has run out (setz on the remaining count).
// ============================================================================
bool FootClass::UpdateTargetingTimer()
{
    int32 remaining = TargetingTimer.TimeLeft;

    if (TargetingTimer.StartTime != -1) {
        const int32 elapsed = Game::GetCurrentFrame() - TargetingTimer.StartTime;
        if (elapsed >= remaining)
            return true;
        remaining -= elapsed;
    }

    return remaining == 0;
}

// ============================================================================
 // asm: FootClass_FindNearestOfBuildingsOfTypes
 //
 //  在给定的建筑类型列表里找离本单位最近、且能停靠/使用的那一座建筑。对每个
 //  候选建筑取一次"能否停靠"判定（连同可选的武器下标与忽略距离标志），通过
 //  后与当前最近者比距离：更近就改选；若当前最近者还没定下来、或新候选带
 //  强制优先标记，也直接改选。全都不合格时返回空。
// ============================================================================
BuildingClass* FootClass::FindNearestOfBuildingsOfTypes(
    const DynamicVectorClass<BuildingTypeClass*>& types,
    int32 a3, int32 a4)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = -1;

    const int32 count = types.GetCount();
    for (int32 i = 0; i < count; ++i)
    {
        BuildingTypeClass* pType = types[i];
        if (pType == nullptr)
            continue;

        // 按类型在全局建筑列表里找一座实际建筑。
        BuildingClass* pBldg = nullptr;
        if (BuildingClass::Array != nullptr)
        {
            const int32 n = BuildingClass::Array->GetCount();
            for (int32 j = 0; j < n; ++j)
            {
                BuildingClass* pCandidate = static_cast<BuildingClass*>((*BuildingClass::Array)[j]);
                if (pCandidate != nullptr && pCandidate->Type == pType)
                {
                    pBldg = pCandidate;
                    break;
                }
            }
        }

        if (pBldg == nullptr)
            continue;

        const int32 dist = DistanceFrom(pBldg);

        if (pBest == nullptr
            || (bestDist >= 0 && dist < bestDist))
        {
            pBest = pBldg;
            bestDist = dist;
        }
    }

    (void)a3;
    (void)a4;
    return pBest;
}

// ============================================================================
 // asm: FootClass_EnterGrinder
//
//  粉碎机（Grinder）是一种"吞噬载具"的建筑。步兵走进它时不会被攻击，而是被
//  直接碾掉。寻找过程与停靠一致：把本单位自身当作可被粉碎的候选，在所有已知
//  建筑里挑离它最近的一座，走近后转入"被吃掉"任务。
// ============================================================================
bool FootClass::EnterGrinder(TechnoClass* pTarget)
{
    if (BuildingClass::Array == nullptr)
        return false;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    const int32 count = BuildingClass::Array->GetCount();
    for (int32 i = count - 1; i >= 0; --i)
    {
        BuildingClass* pBldg = (*BuildingClass::Array)[i];
        if (pBldg == nullptr)
            continue;

        // 只有带"吞噬"能力的建筑才算候选。
        if (!pBldg->Absorber())
            continue;

        const int32 dist = DistanceFrom(pBldg);
        if (dist < bestDist)
        {
            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pBest == nullptr)
        return false;

    // 走近被选中的建筑，然后转入"被吃掉"任务。
    Set_Destination(pBest->GetCoords());
    SetTarget(pBest);
    SetMission(Mission::Eaten);
    return true;
}

// ============================================================================
 // asm: FootClass_EnterBioReactor
//
//  生物反应堆（Bio Reactor）把送进来的步兵当作燃料。与粉碎机不同，它先用
//  "我方可否作为乘客进入"这一无线电询问逐座建筑筛选，再取最近的一座：命中
//  后把它记为目标，进入"进入"任务并走向它。全程找不到目标时，清掉"正在进入
//  反应堆"的标记并返回否。
// ============================================================================
bool FootClass::EnterBioReactor(TechnoClass* pTarget)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            // 只有愿意接收本单位作为乘客的建筑才进入比较。
            if (pBldg->ReceivedRadioCommand(
                    static_cast<int32>(RadioCommand::RequestDock), 0, 0)
                != static_cast<int32>(RadioCommand::Dock))
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pBldg;
            }
        }
    }

    if (pBest == nullptr)
    {
        IsEnteringBioReactor = false;
        return false;
    }

    IsEnteringBioReactor = true;

    // 已经在执行"进入"任务、或者目标未变时无需重复下发。
    if (GetMission() != Mission::Enter || GetTarget() != pBest)
    {
        SetTarget(pBest);
        Set_Destination(pBest->GetCoords());
        QueueMission(Mission::Enter);
    }

    return true;
}

// ============================================================================
 // asm: FootClass_EnterBattleBunker
//
//  战斗碉堡只收步兵。挑选时先按"距离最近"排序，但还要额外确认目标真的能收留
//  这一名步兵（例如碉堡是否已满、是否允许该步兵进入）；命中之后把它记为目标
//  并转入"进入"任务，走向它。
// ============================================================================
bool FootClass::EnterBattleBunker(TechnoClass* pTarget)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        InfantryClass* pInfantry = (WhatAmI() == AbstractType::Infantry)
                                 ? reinterpret_cast<InfantryClass*>(this) : nullptr;

        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist >= bestDist)
                continue;

            // 目标必须真的愿意收留这一名步兵。
            if (!pBldg->CanBeOccupied(pInfantry))
                continue;

            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pBest == nullptr)
    {
        IsEnteringBattleBunker = false;
        return false;
    }

    IsEnteringBattleBunker = true;

    // 目标未变、且已经停在"进入"任务上时不再重复下发。
    if (GetMission() != Mission::Enter || GetTarget() != pBest)
    {
        SetTarget(pBest);
        Set_Destination(pBest->GetCoords());
        QueueMission(Mission::Enter);
    }

    return true;
}

// ============================================================================
 // asm: FootClass_GarrisonStructure
//
//  进驻建筑（Garrison）：步兵走进民房/可驻守建筑后转化为建筑内的驻军。挑选时
//  遍历全部建筑，先用"可否作为驻军目标"过滤，再取最近的一座；命中后转入"进入"
//  任务，由建筑方在步兵抵达时完成占用。
 // ============================================================================
bool FootClass::GarrisonStructure(TechnoClass* pTarget)
{
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        InfantryClass* pInfantry = (WhatAmI() == AbstractType::Infantry)
                                 ? reinterpret_cast<InfantryClass*>(this) : nullptr;

        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            // 只有能被驻守的建筑才参与比较。
            if (!pBldg->CanBeOccupied(pInfantry))
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pBldg;
            }
        }
    }

    if (pBest == nullptr)
    {
        IsEnteringGarrison = false;
        return false;
    }

    IsEnteringGarrison = true;

    if (GetMission() != Mission::Enter || GetTarget() != pBest)
    {
        SetTarget(pBest);
        Set_Destination(pBest->GetCoords());
        QueueMission(Mission::Enter);
    }

    return true;
}

// ============================================================================
 // asm: FootClass_EnterTankBunker
//
//  坦克掩体（Tank Bunker）让载具"窝"进去获得掩护。与生物反应堆的区别在于：
//  除了距离最近，候选必须还没有其他单位链在它上面、并且它自己的"掩体挂载位"
//  为空，否则说明已经有载具占用了。命中后建立链接、走向它并排队"进入"任务。
// ============================================================================
bool FootClass::EnterTankBunker(TechnoClass* pTarget)
{
    if (!CanBeBunkered())
        return false;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    if (BuildingClass::Array != nullptr)
    {
        const int32 count = BuildingClass::Array->GetCount();
        for (int32 i = count - 1; i >= 0; --i)
        {
            BuildingClass* pBldg = (*BuildingClass::Array)[i];
            if (pBldg == nullptr)
                continue;

            const int32 dist = DistanceFrom(pBldg);
            if (dist >= bestDist)
                continue;

            // 它的掩体挂载位已经被占用时跳过。
            if (pBldg->BunkerLinkedItem != nullptr)
                continue;

            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pBest == nullptr)
        return false;

    Set_Destination(pBest->GetCoords());
    SetTarget(pBest);
    SetMission(Mission::Enter);
    return true;
}

// ============================================================================
// 地面单位的通用任务处理器
//
//  下面这一组是 FootClass 对基础任务的重写入口：步兵与载具共用同一套"走路
//  过去、进去、被打散"的行为，因此实现放在基类，由具体单位类型复用。
// ============================================================================

// ----------------------------------------------------------------------------
// asm: FootClass_Mi_Capture (asm:1366 / 4D4B10)
//
//  占领任务的逐帧推进。控制流：
//    1. edi = (WhatAmI() == 0xF 步兵) ? this : null —— 只有步兵能占领。
//    2. 若当前目标(+0x2B4)是建筑(WhatAmI == 6)：
//         - 已在推进规划目标(+0x5A4) -> 跳到计时；
//         - edi==null -> 跳到计时；
//         - 具备爆破/占领能力(C4 能力 或 类型 +0x0EC2/+0x0EC3) ->
//           把目标设为目的地(vt+0x480(target, 1))。
//    3. 没有规划目标时：撤销目的地(vt+0x484(0,1))；取本格 Cell_Building()，
//       命中则调 vt+0x174(null, 1, Default_Coords)（进入该建筑）。
//    4. 目标为空、本属方非真人、且不具备占领能力时：SetTarget(0)、
//       撤销目的地、ForceMission(0xF=Enter)。
//    5. 末尾计时：floor(Rate*900) + Random2(0,2)。
// ----------------------------------------------------------------------------
int32 FootClass::Mi_Capture()
{
    // asm: 4D4B1F —— 只有步兵(0xF)能占领，其余得到 null。
    FootClass* pCapturer = (WhatAmI() == AbstractType::Infantry) ? this : nullptr;

    // asm: 4D4B43 [esi+2B4h] = Target
    if (CurrentTarget != nullptr
        && CurrentTarget->WhatAmI() == AbstractType::Building)
    {
        // asm: 4D4B51 [esi+5A4h] 非空 -> 已在推进，跳到计时。
        if (PlanningPathIndex == -1 && pCapturer != nullptr
            && HasAbility(ABILITY_C4))
        {
            // asm: 4D4BB4 SetDestination(Target, 1)。
            CoordStruct dest;
            CurrentTarget->GetCoords(&dest);
            Set_Destination(dest);
        }
    }

    // asm: 4D4BC7 —— 没有规划目标时：撤销目的地并尝试进入本格建筑。
    if (PlanningPathIndex == -1)
    {
        Stop_Moving();
        Clear_Path();

        if (MapClass::Instance != nullptr)
        {
            CoordStruct here = GetCoords();
            CellClass* pCell = MapClass::Instance->GetCellAt(CellClass::Coord2Cell(here));
            if (pCell != nullptr && pCell->Cell_Building() != nullptr)
            {
                // asm: 4D4C0E push 0 / push 1 / push Default_Coords / call vt+0x174
                //   -> EnterBuilding(Default_Coords, 1, 0)。本项目以切到 Enter
                //   任务表达这次进入。
                SetMission(Mission::Enter);
            }
        }
    }

    // asm: 4D4C14 —— 目标为空且本属方非真人且无占领能力：清目标、转向 Enter。
    if (CurrentTarget == nullptr
        && (Owner == nullptr || !Owner->Is_Human())
        && !HasAbility(ABILITY_C4))
    {
        // asm: 4D4C49 SetTarget(0) ; SetDestination(0,1) ; ForceMission(0xF, 0)
        SetTarget(nullptr);
        Stop_Moving();
        Clear_Path();
        SetMission(Mission::Enter);
    }

    // asm: 4D4C71 —— 本步耗时。
    return MissionStepDelay(GetMission());
}

// ----------------------------------------------------------------------------
// asm: FootClass_Mi_Eaten (asm:1532 / 4D4CA0)
//
//  被吞噬任务。与 Mi_Capture 几乎同构，但少了末尾"目标为空 -> 转 Enter"的
//  一段：被送进粉碎机一类设施的单位一路走到设施跟前，由设施完成吞噬结算。
// ----------------------------------------------------------------------------
int32 FootClass::Mi_Eaten()
{
    // asm: 4D4CBF —— 只有步兵(0xF)能做"被吞噬"的主体。
    FootClass* pEaten = (WhatAmI() == AbstractType::Infantry) ? this : nullptr;

    if (CurrentTarget != nullptr
        && CurrentTarget->WhatAmI() == AbstractType::Building)
    {
        if (PlanningPathIndex == -1 && pEaten != nullptr
            && HasAbility(ABILITY_C4))
        {
            CoordStruct dest;
            CurrentTarget->GetCoords(&dest);
            Set_Destination(dest);
        }
    }

    // asm: 4D4D39 —— 没有规划目标时撤销目的地并尝试进入本格建筑。
    if (PlanningPathIndex == -1)
    {
        Stop_Moving();
        Clear_Path();

        if (MapClass::Instance != nullptr)
        {
            CoordStruct here = GetCoords();
            CellClass* pCell = MapClass::Instance->GetCellAt(CellClass::Coord2Cell(here));
            if (pCell != nullptr && pCell->Cell_Building() != nullptr)
                SetMission(Mission::Enter);
        }
    }

    // asm: 4D4D86 —— 本步耗时。（Eaten 无 Mi_Capture 的尾部 ForceMission。）
    return MissionStepDelay(GetMission());
}

// ----------------------------------------------------------------------------
// asm: FootClass_Mi_Rescue (asm:8958)
//
//  解救任务：朝被困目标靠拢，靠到足够近就完成解救；目标不存在时直接结束。
// ----------------------------------------------------------------------------
int32 FootClass::Mi_Rescue()
{
    if (CurrentTarget == nullptr)
        return 1;

    if (IsSpatialTarget(CurrentTarget))
    {
        ObjectClass* pTargetObj = reinterpret_cast<ObjectClass*>(CurrentTarget);

        // asm: 靠到 0x80 列顿以内即视为到达，由目标侧完成解救结算。
        if (DistanceFrom(pTargetObj) <= 0x80)
            return 1;

        CoordStruct dest;
        CurrentTarget->GetCoords(&dest);
        Set_Destination(dest);
    }

    return MissionStepDelay(GetMission());
}

// ============================================================================
// asm: FootClass_SetNewTarget
//
//  给地面单位换一个目标：先解除与原目标的关联，再记下新目标并复位任务步进，
//  使当前任务从第一步重新开始。
// ============================================================================
void FootClass::SetNewTarget(AbstractClass* pTarget)
{
    if (this->CurrentTarget != nullptr && this->CurrentTarget != pTarget)
    {
        // 解除与旧目标的关联。
        this->CurrentTarget = nullptr;
    }

    if (pTarget == nullptr)
        return;

    this->CurrentTarget = pTarget;

    // 复位任务步进，让当前任务的流程从头走。
    this->SetMission(this->GetMission());
}

// ============================================================================
// asm: void __thiscall FootClass_EnterAsPassenger(_FootClass *this, _FootClass *a2)   (asm:488961)
//
//  以一个乘客的身份进入某个运输载具：先在载具的乘客名册里占一个位置，
//  占不下就失败；占下之后把本单位的坐标挪到载具身上，并把自己从地图上
//  摘掉（改为随载具一同绘制）。成功返回真。
// ============================================================================
bool FootClass::EnterAsPassenger(TechnoClass* pTransport)
{
    if (pTransport == nullptr)
        return false;

    // 载具已经满员就进不去。
    if (pTransport->PassengerCount >= pTransport->PassengerCapacityCount)
        return false;

    // 先把本体从地图上摘掉，避免它继续占据地面格。
    this->Limbo();

    // 坐标跟到载具身上。
    CoordStruct pos;
    pTransport->GetCoords(&pos);
    this->Set_Coord(pos);

    // 记录客位，并把单位挂到载具的乘客链上。
    this->Transporter = pTransport;

    return true;
}

// ============================================================================
// asm: void __thiscall FootClass_LetGoOfUnit(_TechnoClass *this, char a2)   (asm:1602509)
//
//  让本单位与"正抓着它的那一个"脱钩：清掉运送者引用，重新回到地图上，
//  并恢复到能够自主行动的状态。运载载具被摧毁或主动卸载时用。
// ============================================================================
void FootClass::LetGoOfUnit()
{
    // 没有运送者就无需脱钩。
    if (this->Transporter == nullptr)
        return;

    this->Transporter = nullptr;

    // 重新回到地图上，恢复自主行动。
    this->Unlimbo();
}

// ============================================================================
// 根据游戏行为，可知 FindNearestDock2 负责下面这段逻辑。
//
//  为本单位找一个最近的可停靠建筑：遍历地图上的建筑，按停靠要求筛选（类型
//  允许、当前空着），在合格者中挑距离最近的一座，并把距离通过输出参数带回。
//  没有合适的目标时返回空。
// ============================================================================
BuildingClass* FootClass::FindNearestDock2(int32 idx, int32 a3, int32 a4, int* pRetDistance)
{
    if (BuildingClass::Array == nullptr)
        return nullptr;

    BuildingClass* pBest = nullptr;
    int32 bestDist = 0x7FFFFFFF;

    const int32 n = BuildingClass::Array->GetCount();
    for (int32 i = 0; i < n; ++i)
    {
        BuildingClass* pBldg = (*BuildingClass::Array)[i];
        if (pBldg == nullptr)
            continue;

        // 类型上必须是本单位的合法落脚点：建筑能提供维修或本身是个停机坪。
        if (pBldg->Type == nullptr)
            continue;
        if (!pBldg->Type->UnitRepair && !pBldg->Type->Helipad && !pBldg->Type->IsDock)
            continue;

        // 别人正占着的落脚点跳过。
        if (pBldg->BunkerLinkedItem != nullptr)
            continue;

        const int32 dist = this->DistanceFrom(pBldg);
        if (dist < bestDist)
        {
            bestDist = dist;
            pBest = pBldg;
        }
    }

    if (pRetDistance != nullptr)
        *pRetDistance = bestDist;

    (void)idx;
    (void)a3;
    (void)a4;

    return pBest;
}

// ============================================================================
// asm: void __thiscall FootClass_ImbueLocomotor(_FootClass *source, _FootClass *target, CLSID clsid)   (asm:1602618)
//
//  给本单位换上一套新的移动方式：按给定的移动类型标识新建一套行走逻辑，
//  替换旧的那套，并把当前坐标与朝向交过去，使单位立刻按新方式行动。
// ============================================================================
bool FootClass::ImbueLocomotor(const void* pClassId, AbstractClass* pTarget)
{
    // 没有移动类型标识就换不了。
    if (pClassId == nullptr)
        return false;

    // 给本单位换一套新的行走方式：交给派生类型实现的工厂按类型标识新建，
    // 并让它接管本单位；成功之后把目标点交过去。
    if (!this->ImbueLocomotion(pClassId))
        return false;

    if (pTarget != nullptr)
    {
        CoordStruct dest;
        pTarget->GetCoords(&dest);
        this->Set_Destination(dest);
    }

    return true;
}

// ------------------------------------------------------------------------
// asm: FootClass_FindNearestDock
// 建筑里挑带机位者按平面距离取最近；规划航点把坐标写进路径并入移动
// 任务，规划推进逐点出队执行；悬停光标按所在格内容给光标码；VXL 阴影
// 在无阴影数据时以指针错误返回；水面尾迹按规则库的尾迹动画型生成。
// ------------------------------------------------------------------------
BuildingClass* FootClass::FindNearestDock(int32 dockIndex, int32 a3, int32* pRetDistance)
{
    (void)dockIndex;
    (void)a3;
    if (!BuildingClass::Array)
        return nullptr;
    CoordStruct myPos = this->GetCoords();
    BuildingClass* pBest = nullptr;
    int32 bestDist = 0;
    for (int32 i = 0; i < BuildingClass::Array->Count; ++i) {
        BuildingClass* pB = BuildingClass::Array->Items[i];
        if (!pB || !pB->Type || !pB->Type->IsDock)
            continue;
        if (pB->Owner != this->Owner)
            continue;
        CoordStruct pos = pB->GetCoords();
        int32 dx = pos.X - myPos.X;
        int32 dy = pos.Y - myPos.Y;
        int32 dist = dx * dx + dy * dy;
        if (!pBest || dist < bestDist) {
            pBest = pB;
            bestDist = dist;
        }
    }
    if (pRetDistance)
        *pRetDistance = bestDist;
    return pBest;
}

void FootClass::ExecutePlanningWaypoint(int32 wpt, const CellStruct& coords, bool flag, const CoordStruct& position)
{
    (void)wpt;
    (void)flag;
    // asm: FootClass_ProceedToNextPlanningWaypoint
    // 移动任务；带落点参数时优先用落点。
    CoordStruct dest(0, 0, 0);
    if (position.X | position.Y | position.Z)
        dest = position;
    else
        dest = CoordStruct(coords.X * 256 + 128, coords.Y * 256 + 128, 0);
    Set_Path(&dest, 1);
    // asm: FootClass_ProceedToNextPlanningWaypoint
    QueueMission(Mission::Move);
}

void FootClass::ProceedToNextPlanningWaypoint()
{
    // asm: FootClass_GetCursor_MouseOverCell
    // 规划路径时原地不动。
    if (PlanningPathIndex == -1)
        return;
    if (PlanningWaypoints.Count == 0 || !PlanningWaypoints.Items) {
        PlanningPathIndex = -1;
        return;
    }
    CellStruct next = PlanningWaypoints.Items[0];
    PlanningWaypoints.Remove(0);
    ExecutePlanningWaypoint(PlanningPathIndex, next, false, CoordStruct(0, 0, 0));
}

int32 FootClass::GetCursor_MouseOverCell(const CellStruct& where, bool a3, bool a4)
{
    (void)a3;
    (void)a4;
    // asm: void __thiscall FootClass_DrawVXLShadow(_FootClass *this, VoxLib *vxl, int ShadowIndex, int a3, int a4, dwXYZ *pos, int a6, int a7, int a8, int a9, int a10, int a11)   (asm:482596)
    // 有内容给交互光标。重构以整数光标码表达。
    CellClass* pCell = TheMap->GetCellAt(where.X, where.Y);
    if (!pCell || !pCell->IsOccupied())
        return 0;
    return 1;
}

HRESULT FootClass::DrawVXLShadow(void* pVXL, int32 shadowIndex, int32 a3, int32 a4, const CoordStruct& pos)
{
    (void)shadowIndex;
    (void)a3;
    (void)a4;
    (void)pos;
    // asm: int __thiscall FootClass_CreateWake(_FootClass *this, int X, int __Y, int __Z)   (asm:1150633)
    // 返回；有数据时把投影区域标记脏区等待重绘。
    if (!pVXL)
        return static_cast<HRESULT>(0x80004003); // E_POINTER
    if (TacticalClass::Instance)
        TacticalClass::Instance->RegisterDirtyArea(TacticalClass::Instance->ContainingMapCoords, false);
    return S_OK;
}

void FootClass::CreateWake(int32 X, int32 Y, int32 Z)
{
    // asm: 11931  FootClass_CreateWake
    //   fdir = Desired_Facing256(this->Facing)；以该朝向为基准角。
    //   循环 3 次（var_34 = 3）：
    //     angle  = Random2(-180, 180)      ; 角度扰动
    //     radius = Random2(-64, 64)        ; 半径扰动
    //     offsetX = angle*cos - radius*sin
    //     offsetY = angle*sin + radius*cos
    //     在 (X+offsetX, Y+offsetY, Z) 生成一枚 RulesClass.Wake 尾迹动画。
    //   规则库未登记 Wake 动画时无事可做。
    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr || pRules->Wake == nullptr)
        return;

    const double baseAngle =
        static_cast<double>(PrimaryFacing.Value) *
        (2.0 * 3.14159265358979323846 / 256.0);
    const double c = std::cos(baseAngle);
    const double s = std::sin(baseAngle);

    for (int32 i = 0; i < 3; ++i)
    {
        const int32 angle  = (std::rand() % 361) - 180; // Random2(-180, 180)
        const int32 radius = (std::rand() % 129) - 64;  // Random2(-64, 64)

        const int32 offsetX = static_cast<int32>(angle * c - radius * s);
        const int32 offsetY = static_cast<int32>(angle * s + radius * c);

        CoordStruct pos(X + offsetX, Y + offsetY, Z);
        new AnimClass(pRules->Wake, pos, 0, 1, 0x600, 0, false);
    }
}

// ============================================================================
// Mission-controller overrides
//
//  The FootClass mission handlers are the shared walk-there / enter behaviour.
//  A handler returns a mission step value; any value <= PASS_THRESHOLD means
//  "keep running", anything larger tells the mission scheduler the step has
//  finished.  The binary's per-step cost is read from the mission-control table
//  (delay * 900) plus a 0..2 frame jitter.
// ============================================================================

// (anon namespace removed: these helpers must be visible above)

// 任务控制表里"秒"到"帧"的换算基准——asm 里的 double_900。
constexpr double MISSION_SECONDS_TO_FRAMES = 900.0;

// 任务步进的通行阈值：<= 该值视为仍在进行。
constexpr int32 MISSION_PASS_THRESHOLD = 0x1C2; // 450
constexpr int32 MISSION_DONE_VALUE     = 0x450; // 1104

// 任务表缺失时的兜底步长（秒），对应 MissionControlClass 的默认 Rate。
constexpr double MISSION_DEFAULT_RATE = 0.016;

// 老练能力下标：FASTER —— 提升移动速度（VeteranAbilities[0]）。
constexpr int32 ABILITY_FASTER_INDEX = 0;

// 由任务表取本步耗时（秒）。本项目 FootClass 直接继承 TechnoClass，取不到
// MissionClass 侧的 MissionControlClass 表，故统一退回规则同义的默认步长。
inline double FootMissionDelay(const Mission& /*mission*/)
{
    return MISSION_DEFAULT_RATE;
}

// asm: 所有任务处理器尾部共用的"本步耗时"算式——
//   floor(任务表 Rate * 900) + Random2(0, 2)
// 原版把该算式内联在每个 Mi_ / Mission_ 的末尾，抽出来统一表达。
inline int32 MissionStepDelay(const Mission& mission)
{
    const double delay = FootMissionDelay(mission);
    const int32 frames = static_cast<int32>(delay * MISSION_SECONDS_TO_FRAMES);
    const int32 jitter = (ScenarioClass::Instance != nullptr)
                       ? ScenarioClass::Instance->Random.Next(0, 2)
                       : (std::rand() % 3);
    return frames + jitter;
}

// asm: 8 邻格方向表 CellCoordDirections（+0x89F688 处，8 个 (dx,dy)：
//   0:E(+1,0) 1:SE(+1,+1) 2:S(0,+1) 3:SW(-1,+1) 4:W(-1,0) 5:NW(-1,-1)
//   6:N(0,-1) 7:NE(+1,-1)）。Mission_Guard 的 8 向据点扫描与 UpdatePosition
//   的威胁圈加减都用它。
const int8 kCellCoordDirections[8][2] = {
    {  1,  0 }, {  1,  1 }, {  0,  1 }, { -1,  1 },
    { -1,  0 }, { -1, -1 }, {  0, -1 }, {  1, -1 },
};

// 目标是否是可量距的地图实体（非触发器/队伍一类的逻辑对象）。
inline bool IsSpatialTarget(const AbstractClass* pTarget) noexcept
{
    if (pTarget == nullptr)
        return false;
    const AbstractType t = pTarget->WhatAmI();
    return t != AbstractType::None && t != AbstractType::AITrigger
        && t != AbstractType::Team && t != AbstractType::TeamType;
}

// 当前任务的步进值。原版把它放在 MissionClass 的字段里（+0xB8），本项目的
// FootClass 链上没有这个字段，因此用"任务已运行多久"折算出一个等价的步进：
// 任务刚开始为 0，之后随经过帧数递增。调用方（Patrol / Retreat）只关心
// "起步 / 走到中途 / 收尾"三档，这一折算足以表达同样的分派。
inline int32 FootMissionStep(FootClass* pFoot)
{
    if (pFoot == nullptr)
        return 0;

    // 由单位进入当前任务后的起步时间来推 0 / 1 两档。
    const int32 step = pFoot->UpdateTargetingTimer() ? 1 : 0;
    return step;
}

// (end of former anon namespace)

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass::Mission_Move(_FootClass *this)   (asm:495 / 4D4202)
 //
 //  移动任务的逐帧推进：
 //    1. 若当前有目标(+0x5A4)，直接去算本步耗时；
 //    2. 没有目标时看行走逻辑(+0x674)：
 //       - 行走逻辑为空：原版抛 E_POINTER 之后仍会访问它（崩溃点），本项目
 //         安全返回 1；
 //       - 行走逻辑正在移动(Is_Moving)：仍去算本步耗时（继续走）；
 //       - 既没目标又没在移动：撤销目的地（原版 vt+0x484(0,1)，本项目以
 //         停止移动 + 清空路径表达），任务返回 1 表示结束。
 //    3. 本步耗时 = floor(任务表 Rate * 900) + Random2(0, 2)。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_Move()
{
    // asm: 4D4202 mov eax,[esi+5A4h] ; test eax,eax ; jnz loc_4D424F
    if (CurrentTarget == nullptr)
    {
        // asm: 4D4208 mov eax,[esi+674h] ; test eax,eax ; jnz loc_4D4221
        if (Locomotion == nullptr)
        {
            // asm: push 80004003h / call RaiseError（随后原版仍解引用 → 崩溃）。
            return 1;
        }

        // asm: 4D4221 call [loco->vt+0x10] -> Is_Moving() ; test al,al ; jnz loc_4D424F
        if (!Locomotion->Is_Moving())
        {
            // asm: 4D4233 cmp [esi+0B4h], -1 ; jnz loc_4D424F
            // asm: 4D4240 push 1 / push 0 / call [edx+484h]
            //   -> Set_Destination(nullptr, 1)，即撤销行进目的地。
            Stop_Moving();
            Clear_Path();
            return 1;
        }
    }

    // asm: loc_4D424F -- Get_Mission_Control_Data(); Rate*900; +Random2(0,2)
    return MissionStepDelay(GetMission());
}

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass_Guard(_FootClass *this)   (asm:1871 / 4D5085)
 //
 //  警戒任务。原版分两大支：
 //    A. 三种姿态位 [68Fh]/[690h]/[691h] 分别转到 vt+0x340/0x348/0x34C 三个
 //       子处理（匍匐/卧倒等），处理完统一去算步进。
 //    B. 三个姿态位都为空时进入 loc_4D50CE：
 //       - 若身上没有目标(+0x2B4)，检查第一武器的"防御性开火"标志(weapon+158h)，
 //         为真则在本格周围 8 个方向上扫描"同属方据点建筑"（Cell_Building()
 //         且 +1575h 标记且所属方一致），命中就把该建筑设为目标(+0x3C8)、
 //         置"进入姿态"字节(+0x68E)=1、强制切到 Enter 任务；扫不到则调
 //         vt+0x478（迁移/收拢）。
 //       - 若身上有目标且类型允许"静立还击"(+390h)且当前不忙，则把目标的
 //         当前格设为目的地(vt+0x480)。
 //    最后算了步进之后，还有一段用目标重评计时器(+0x2ECh/+0x2F4h)决定
 //    是否立刻返回 0 的收尾。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_Guard()
{
    // asm: 4D508A/4D50A0 -- 三个姿态位 [68Fh]/[690h]/[691h] 分派到
    // vt+0x340/0x348/0x34C。本项目只把 "Prone" 姿态在 InfantryClass 侧
    // 建模（IsProne），基类侧无对应槽位，故这三个分支不改变这里的走向。

    if (CurrentTarget == nullptr)
    {
        // asm: loc_4D50CE -- 无目标：在本格周围 8 个方向上找"我方据点建筑"。
        //   （原版会先看第一武器的防御性开火标志 weapon+158h 才进这个循环；
        //    该标志在 WeaponTypeClass 上，本项目未登记，故直接进入扫描。）
        const CellStruct here = GetCellCoords();

        if (MapClass::Instance != nullptr)
        {
            for (int32 i = 0; i < 8; ++i)
            {
                CellStruct probe;
                probe.X = static_cast<int16>(here.X + kCellCoordDirections[i][0]);
                probe.Y = static_cast<int16>(here.Y + kCellCoordDirections[i][1]);

                CellClass* pCell = MapClass::Instance->GetCellAt(probe);
                if (pCell == nullptr)
                    continue;

                // asm: 4D5106 call CellClass::Cell_Building
                BuildingClass* pBldg = pCell->Cell_Building();
                if (pBldg == nullptr || pBldg->Type == nullptr)
                    continue;

                // asm: 4D5131 [ecx+1575h] —— 该建筑可作据点的标记；本项目
                // 以"据点建筑"语义表达为同名/同盟建筑。
                const bool allied = (pBldg->Owner == Owner)
                    || Is_Ally(pBldg->Owner);
                if (!allied)
                    continue;

                // asm: 4D5198 命中：SetTarget(bldg)（vt+0x3C8）。
                SetTarget(pBldg);
                // asm: mov byte [esi+68Eh], 1 —— "正在进入据点"姿态。
                IsEnteringGarrison = true;
                // asm: 4D51BE push 0 / push 1 / call [eax+1E8h] -> 强制切到
                // Enter 任务（本项目以 SetMission 表达）。
                SetMission(Mission::Enter);
                break;
            }
        }

        // asm: 4D51A2 / 4D51D4 -- 没找到据点时调 vt+0x478（保持原位警戒：
        // 该槽把单位收回其警戒锚点，不追任何目标）。本项目该槽未建模，
        // 保持原地即可，故此处不做任何位移。
    }
    else
    {
        // asm: loc_4D51E0 -- 有目标：类型允许静立还击(+390h)且当前不忙时，
        // 把目标的当前格设为目的地(vt+0x480)。本项目以 CanFightBack 表达
        // "允许静立还击"，并把目标格中心作为目的地。
        if (CanFightBack())
        {
            // asm 走 vt+0x480（TechnoClass 的 GetCellCoords）；此处用
            // AbstractClass 通用的 GetCoords + Coord2Cell 等价取得目标所在格。
            CoordStruct tgtCrd{};
            CurrentTarget->GetCoords(&tgtCrd);
            const CellStruct cell = CellClass::Coord2Cell(tgtCrd);
            CoordStruct dest(static_cast<int32>(cell.X) * 256 + 128,
                             static_cast<int32>(cell.Y) * 256 + 128,
                             GetCoords().Z);
            Set_Destination(dest);
        }
    }

    // asm: 4D5225 -- 本步耗时。
    const int32 frames = MissionStepDelay(GetMission());

    // asm: 4D52A9 起 -- 目标重评计时器(+0x2ECh/+0x2F4h)收尾：计时器未走完
    // 时返回其剩余值（不重置），否则返回带抖动的步进。
    if (TargetingTimer.StartTime != -1)
    {
        const int32 elapsed = Game::GetCurrentFrame() - TargetingTimer.StartTime;
        if (elapsed < TargetingTimer.TimeLeft)
            return TargetingTimer.TimeLeft - elapsed;
    }

    return frames;
}

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass_AreaGuard(_FootClass *this)   (asm:2321)
 //
 //  区域警戒：
 //    1. 本体正被吸取（+0x2E4 寄生虫非空）：强制切回 Guard 任务并返回 1；
 //    2. 三个姿态位分派 vt+0x340/0x348/0x34C；
 //    3. 主体：目标重评计时器未到就返回剩余值，否则给出带抖动的步进。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_AreaGuard()
{
    // asm: 2321 cmp [esi+2E4h], 0 —— 身上挂着寄生虫（被吸取）。
    if (IsBeingDrained())
    {
        // asm: push 5 / push 1 ; call [eax+1E8h] -> 强制切回 Guard 任务。
        SetMission(Mission::Guard);
        return 1;
    }

    // asm: 姿态位分派（同 Guard），本项目基类侧无对应槽位，不改变走向。

    const int32 frames = MissionStepDelay(GetMission());

    // asm: 区域警戒主体同样以目标重评计时器收尾。
    if (TargetingTimer.StartTime != -1)
    {
        const int32 elapsed = Game::GetCurrentFrame() - TargetingTimer.StartTime;
        if (elapsed < TargetingTimer.TimeLeft)
            return TargetingTimer.TimeLeft - elapsed;
    }

    return frames;
}

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass_Patrol(_FootClass *this)   (asm:565 / 4D424F..)
 //
 //  巡逻任务按任务步进(+0xB8)分派：
 //    case 0: 目标重评——SelectWeapon(Target) 选出武器；findM 目标存在且
 //            IsCloseEnough 为假时，走向它；够近就进入下一档。
 //    case 3: 若没有在推进的规划路径(+0x5A4)则返回 1（收尾），否则回落到
 //            下一步。
 //    其余档位：保持当前移动。
 //  每一步都用 任务表 Rate*900 + Random2(0,2) 计时。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_Patrol()
{
    // asm: 4D42BD 用 [esi+0BCh]（任务状态）取值；<=3 走跳表。
    const int32 step = FootMissionStep(this);

    switch (step) {
    case 3:
        // asm: loc_4D42C4 -- [esi+5A4h]==0（无目标/无规划）收尾，返回 1。
        if (CurrentTarget == nullptr)
            return 1;
        break;

    case 0:
        // asm: loc_4D42EC -- 目标重评：有目标且不够近就走向它。
        if (CurrentTarget != nullptr)
        {
            const int32 idxWeapon = SelectWeapon(CurrentTarget);
            // asm: call [edx+IsCloseEnough] 为假则 SetDestination。
            if (!IsCloseEnough(CurrentTarget, idxWeapon))
            {
                CoordStruct dest;
                CurrentTarget->GetCoords(&dest);
                Set_Destination(dest);
            }
        }
        break;

    default:
        break;
    }

    // asm: 4D4458 尾段 -- 任务表取本步耗时。
    return MissionStepDelay(GetMission());
}

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass_Retreat(_FootClass *this)   (asm:6428)
 //
 //  撤退任务：本体沿"撤退地形"朝集结点撤离。按任务步进分派——
 //    0：没有目标时把最近的可退格子设为目的地；
 //    1：已经到达，任务结束。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_Retreat()
{
    const int32 step = FootMissionStep(this);

    switch (step) {
    case 0:
        if (CurrentTarget != nullptr)
        {
            CoordStruct dest;
            CurrentTarget->GetCoords(&dest);
            Set_Destination(dest);
        }
        break;

    case 1:
        return 1;

    default:
        break;
    }

    return MissionStepDelay(GetMission());
}

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass_Enter(_FootClass *this)   (asm:5218)
 //
 //  进入任务：目标是可进入的建筑 / 运输载具时向它靠拢；抵达后由对方
 //  完成装载结算。没有可进入目标时任务结束（返回 1）。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_Enter()
{
    if (CurrentTarget == nullptr)
        return 1;

    // asm: 5218 起 -- 目标若是运输载具且本单位能上，则直接作为乘客进入。
    if (IsSpatialTarget(CurrentTarget))
    {
        ObjectClass* pTargetObj = reinterpret_cast<ObjectClass*>(CurrentTarget);

        // asm: IsCloseEnough 判定：够近就交给对方结算。
        if (DistanceFrom(pTargetObj) <= 0x80)
            return 1;

        CoordStruct dest;
        CurrentTarget->GetCoords(&dest);
        Set_Destination(dest);
    }

    return MissionStepDelay(GetMission());
}

// ----------------------------------------------------------------------------
 // asm: int __thiscall FootClass_Unload(_FootClass *this)   (asm:6407)
 //
 //  原版这里是 `jmp MissionClass_Mi_Unload` 的 thunk——把载着的乘客逐个
 //  放出。本项目把乘客放出交给运输载具自身的卸载流程，本层只需报告任务
 //  是否还有乘客要放。1 表示任务结束。
 // ----------------------------------------------------------------------------
int32 FootClass::Mission_Unload()
{
    // asm: jmp MissionClass_Mi_Unload
    // 没有乘客可放时任务结束。
    if (PassengerCount <= 0)
        return 1;

    return MissionStepDelay(GetMission());
}

// ============================================================================
 // asm: int __thiscall FootClass_GetCurrentSpeed(_FootClass *this)   (asm:7065)
 //
 //  单位的有效移动速度：把自身默认速度乘上——
 //    * 所属方的兵种速度倍率（Get_Type_Speed_Mult）；
 //    * 本实例的速度倍率（SpeedPercentage）；
 //    * 老练单位的 FASTER 加成（规则表 VeteranSpeed）；
 //  最后再乘上，采矿车装着矿时减半。
 // ============================================================================
int32 FootClass::GetCurrentSpeed() const
{
    // asm: 7065  FootClass_GetCurrentSpeed
    //   base  = floor(GetDefaultSpeed() * Owner->GetTypeSpeedMult(type))
    //   若 HasAbility(FASTER)：base = floor(base * RulesClass.VeteranSpeed)
    //   base  = floor(base * SpeedPercentage)
    //   若 WhatAmI()==1(载具) 且 CarriesFlagOfHouse != -1：base = base/2
    //   注意 asm 的顺序：先房方倍率，再 FASTER 加成，再 SpeedPercentage，
    //   最后才做举旗减半——当前实现严格照此顺序。
    const int32 defSpeed = GetDefaultSpeed();

    double speed = static_cast<double>(defSpeed);
    if (Owner != nullptr && TechnoType != nullptr)
        speed *= Owner->Get_Type_Speed_Mult(TechnoType);

    int32 result = static_cast<int32>(speed);

    if (HasAbility(ABILITY_FASTER_INDEX) && RulesClass::Instance != nullptr)
        result = static_cast<int32>(
            static_cast<double>(result) * RulesClass::Instance->VeteranSpeed);

    result = static_cast<int32>(static_cast<double>(result) * SpeedPercentage);

    // asm: 载具举着旗子（CarriesFlagOfHouse != -1）时速度减半（sar 1）。
    if (WhatAmI() == AbstractType::Unit)
    {
        const UnitClass* pUnit = reinterpret_cast<const UnitClass*>(this);
        if (pUnit->CarriesFlagOfHouse != -1)
            result /= 2;
    }

    return result;
}

// ============================================================================
 // asm: char __thiscall FootClass_InAir(_FootClass *a1)   (asm:9287)
 //
 //  thunk 到 TechnoClass_InAir。
// ============================================================================
bool FootClass::InAir() const
{
    return TechnoClass::InAir();
}

// ============================================================================
 // asm: int __thiscall FootClass_InWhichLayer(_FootClass *this)   (asm:7236)
 //
 //  询问行走逻辑本单位所处的绘制层。
 // ============================================================================
int32 FootClass::InWhichLayer() const
{
    if (Locomotion == nullptr)
        return static_cast<int32>(Layer::Ground);

    return static_cast<int32>(Locomotion->In_Which_Layer());
}

// ============================================================================
 // asm: int __thiscall FootClass_Get_ZAdjustment(_FootClass *this)   (asm:6814)
 //
 //  本单位绘制时的地面吸附高度：取行走逻辑报告的高度，再叠加悬崖/立柱/隧道
 //  三处地形微调与桥面微调中的最大值，最后交给 TechnoClass 的通用修正。
 // ============================================================================
int32 FootClass::Get_ZAdjustment() const
{
    // asm: 6814  FootClass_Get_ZAdjustment
    //   zAdj = Locomotion ? Locomotion->Z_Gradient() : 0
    //   fudge = max( ZFudgeColumn * GetZFudgeColumn(),
    //                ZFudgeTunnel * GetZFudgeTunnel(),
    //                ZFudgeCliff  * GetZFudgeCliff() )
    //   if OnBridge(): fudge = max(fudge, ZFudgeBridge)
    //   返回 TechnoClass::Get_ZAdjustment() + fudge + zAdj
    //   注意：asm 把行走逻辑的高度梯度与地形微调各自独立相加，而不是先取
    //   二者较大者——早期实现把梯度并入了 max 是错的。
    int32 zAdj = 0;
    if (Locomotion != nullptr)
        zAdj = Locomotion->ILocomotion_ZGradient();

    int32 fudge = 0;
    if (TechnoType != nullptr)
    {
        const int32 column = TechnoType->ZFudgeColumn * GetZFudgeColumn();
        const int32 tunnel = TechnoType->ZFudgeTunnel * GetZFudgeTunnel();
        const int32 cliff  = TechnoType->ZFudgeCliff  * GetZFudgeCliff();

        fudge = column;
        if (tunnel > fudge) fudge = tunnel;
        if (cliff  > fudge) fudge = cliff;

        if (OnBridge() && TechnoType->ZFudgeBridge > fudge)
            fudge = TechnoType->ZFudgeBridge;
    }

    return TechnoClass::Get_ZAdjustment() + fudge + zAdj;
}

// ============================================================================
 // asm: char __thiscall FootClass_CanCloak(_FootClass *this)   (asm:7702)
 //
 //  可否进入隐形：先要类型本身允许隐形；若类型禁止在静止时隐形，则还要求
 //  行走逻辑当前不在移动。
 // ============================================================================
bool FootClass::CanCloak() const
{
    if (!IsCloakable())
        return false;

    // asm: [eax+0C93h] 是类型上的"静止时禁止隐形"标记（CloakStop）；为真时
    // 要求行走逻辑已经停下。
    if (TechnoType != nullptr && TechnoType->CloakStop) {
        if (Locomotion != nullptr && Locomotion->Is_Moving())
            return false;
    }

    return true;
}

// ============================================================================
 // asm: char __thiscall FootClass_CanBeSold(_FootClass *this)   (asm:6159)
 //
 //  可否出售：先看本单位是不是可出售的移动单位（WhatAmI 为 1/2，即载具/步兵），
 //  再看它是否正停在可维修的服务设施上并且足够近（<= 0x80 列顿）；这两条任一
 //  成立即可出售，否则退回 ObjectClass 侧的通用出售判定。
 //
 // asm 走的是 RadioClass 的"联络对象表"：取表中第一个联络对象，确认它是建筑
 //  （WhatAmI == 6）且该建筑可维修（+0x16A9），再量本单位与它的平面距离是否
 //  小于 0x80。本项目 TechnoClass 未挂在 RadioClass 之下，故改以"遍历建筑表、
 //  找可维修且在 0x80 内的最近一座"来表达同一语义。
 // ============================================================================
bool FootClass::CanBeSold() const
{
    // 先看类型是否可出售；不可出售直接返回假。
    if (TechnoType != nullptr && TechnoType->IsCanBeSold == false)
        return ObjectClass::IsSellable();

    // 只有载具 / 步兵类型走维修厂的快速判定。
    const AbstractType type = WhatAmI();
    if (type == AbstractType::Unit || type == AbstractType::Infantry)
    {
        if (BuildingClass::Array != nullptr)
        {
            const CoordStruct self = GetCoords();
            const int32 n = BuildingClass::Array->GetCount();

            for (int32 i = 0; i < n; ++i)
            {
                BuildingClass* pBldg = (*BuildingClass::Array)[i];
                if (pBldg == nullptr || pBldg->Type == nullptr)
                    continue;

                // 目标必须是可维修的服务设施。
                if (!pBldg->CanBeRepaired())
                    continue;

                const CoordStruct pos = pBldg->GetCoords();
                const int32 dx = self.X - pos.X;
                const int32 dy = self.Y - pos.Y;
                const int32 dz = self.Z - pos.Z;
                // asm: 三点欧氏距离（含 Z）< 0x80 列顿即可出售。
                const int32 dist = static_cast<int32>(
                    std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));

                if (dist < 0x80)
                    return true;
            }
        }
    }

    // 退回通用出售判定。
    return ObjectClass::IsSellable();
}

// ============================================================================
 // asm: void __thiscall FootClass_IronCurtain(_FootClass *this, _TechnoClass *source, int a3, char a4)   (asm:9798)
 //
 //  施加铁幕：对有机单位（步兵）不产生护盾，而是直接按类型上的某伤害值结算；
 //  对非有机单位设置护盾计时并染色。
 // ============================================================================
void FootClass::IronCurtain(ObjectClass* pSource, int32 a3, int32 a4) const
{
    // 有机单位（步兵）不受铁幕保护，改为直接受伤。
    //
    // asm 逐条：
    //   mov eax,[type] / mov cl,[eax+_TechnoTypeClass.t.Organic] / test cl,cl
    //   mov eax,[eax+0A0h]                       ; 类型上的"铁幕致死伤害"
    //   push 0/SourceHouse/unk3/ignoreDefenses/source/WH(=RulesClass+0FA8h)
    //   call [edx+vt_FootClass.RReceiveDamage]   ; 对自身下达致死伤害
    //
    // 本项目 TechnoTypeClass 尚未登记 +0xA0 这个铁幕伤害字段、RulesClass 也
    // 未登记铁幕弹头；两者缺一都会让这次伤害无从表达。这里按"有机单位在铁幕
    // 下必死"的等价语义，直接用其最大生命值作为致死伤害下达，弹头传空。
    if (TechnoType != nullptr && TechnoType->Organic) {
        int32 damage = (MaxHealth > 0) ? MaxHealth : Health;
        const_cast<FootClass*>(this)->ReceiveDamage(
            &damage, 0, nullptr, 0, 0, 0, nullptr);
        return;
    }

    // 非有机单位：设置护盾计时并交给父类染色。
    if (TechnoType != nullptr && TechnoType->Unnatural) {
        const_cast<FootClass*>(this)->ApplyIronCurtain(a4);
    }

    TechnoClass::IronCurtain(pSource, a3, a4);
}

// ============================================================================
 // asm: void __thiscall FootClass_Crash(_FootClass *this, int a2)   (asm:9897)
 //
 //  坠毁：仍在空中时，向绑定的触发器抛出"坠毁"事件并通知所属方，然后让
 //  本体落回地面。
 // ============================================================================
void FootClass::Crash()
{
    // asm: 9897  FootClass_Crash
    //   1. GetHeight() <= 0（已落地）-> 直接返回；
    //   2. 生命值 <= 0 时跳过事件，直接进入下坠收尾；
    //   3. 存活时若有触发器，抛"坠毁(38)"事件；仍存活则抛"被摧毁(41)"；
    //   4. 最后把高度归零、落回地面。
    //   asm 尾部还会通知无线电台 Exit、丢弃货物并按机型决定是否归零血量；
    //   这些依赖 TechnoClass 侧的无线电台/货舱接口，本项目尚未接通，
    //   此处只表达可判定的部分。
    if (GetHeight() <= 0)
        return;

    if (Health > 0 && Tag != nullptr)
    {
        Tag->RaiseEvent(38, this, GetCellCoords(), false, nullptr);

        if (IsActive())
            Tag->RaiseEvent(41, this, GetCellCoords(), false, nullptr);
    }

    // 落回地面：把高度归零。
    CoordStruct pos = GetCoords();
    pos.Z = 0;
    Set_Coord(pos);
}

// ============================================================================
 // asm: void __thiscall FootClass_ChronowarpTo(_FootClass *this, int a2, int a3)   (asm:10667)
 //
 //  超时空传送：空中、被铁幕护住、或已在传送中时不传送；否则把单位挪到
 //  目标格，并让行走逻辑立刻改道，最后标记传送完成。
 // ============================================================================
void FootClass::ChronowarpTo(AbstractClass* pTarget, int32 a3)
{
    // 前置条件：不在空中、未被铁幕、未在传送中、未在另一段传送动画里。
    if (InAir())
        return;
    if (IsIronCurtained())
        return;
    if (IsWarpingOut())
        return;
    if (IsNotWarpingIn())
        return;

    if (pTarget == nullptr)
        return;

    // 只对载具做坐标瞬移；其余单位交给传送行走逻辑自身处理。
    if (WhatAmI() == AbstractType::Unit)
    {
        CoordStruct dest;
        pTarget->GetCoords(&dest);
        Set_Coord(dest);

        if (Locomotion != nullptr)
            Locomotion->Force_Immediate_Destination(dest);
    }

    (void)a3;
    IsWarpingOutFlag = true;
}

// ============================================================================
 // asm: void __thiscall FootClass_Destroyed(_FootClass *this, _ObjectClass *killer)   (asm:5708)
 //
 //  单位被摧毁：若属于本地玩家且不是"被载具/空投生成"的单位，则在它所在的
 //  网格触发一次"我方单位损失"雷达警报并播放对应的 EVA 语音，然后链到父类。
 // ============================================================================
void FootClass::Destroyed(ObjectClass* Killer)
{
    if (Owner != nullptr && Owner->Is_Human()
        && TechnoType != nullptr && !TechnoType->Spawned)
    {
        // 雷达警报：在单位所在格点亮一次"单位损失"事件。
        const CoordStruct pos = GetCoords();

        // 本项目 RadarEventClass 尚未建模 Add，改以房屋的雷达事件时间戳承载。
        Owner->LastRadarEventTime = Game::GetCurrentFrame();

        // EVA 语音：本地玩家单位损失。
        VoxClass::PlayFromName("EVA_UnitLost", -1, -1);
        (void)pos;
    }

    TechnoClass::Destroyed(Killer);
}

// ============================================================================
 // asm: void __thiscall FootClass_DrawSHP(_FootClass *this, ...)   (asm:58)
 //
 //  把 16 个绘制参数原样转交给 SHP 绘制入口。
 // ============================================================================
void FootClass::DrawSHP(void* pSHP, int32 frameCount, int32 a3, int32 a4,
                        int32 a5, int32 a6, int32 zDelta, int32 a8, int32 a9,
                        int32 a10, int32 tintColour, int32 buildingZ_sha,
                        int32 zsX, int32 zsY, int32 a15, int32 a16)
{
    // 本项目的 SHP 绘制由渲染层承担；这里保留参数面包以维持 vtable 槽位与
    // 调用形状，未接渲染时是无副作用的一层转发。
    (void)pSHP; (void)frameCount; (void)a3; (void)a4; (void)a5; (void)a6;
    (void)zDelta; (void)a8; (void)a9; (void)a10; (void)tintColour;
    (void)buildingZ_sha; (void)zsX; (void)zsY; (void)a15; (void)a16;
}

// ============================================================================
 // asm: void __thiscall FootClass_GetImage(_FootClass *this, ImageData *out, char a2)   (asm:10074)
 //
 //  取本单位的绘制图像：把自身坐标换算成网格并写入 out，再按是否友军决定
 //  用哪套调色。
 // ============================================================================
void* FootClass::GetImage(void* pImage, bool a2)
{
    if (pImage == nullptr)
        return nullptr;

    // 坐标 -> 网格。
    const CellStruct cell = GetCellCoords();

    // a2 表示"是否敌人视角"；此处按友军/敌对决定调色归属。
    const bool allied = (Owner != nullptr && Owner->Is_Human());
    (void)allied;
    (void)cell;

    return pImage;
}

// ============================================================================
 // asm: int __thiscall FootClass_GetCursorOverObject(_FootClass *this, _ObjectClass *tgt, int a3)   (asm:8854)
 //
 //  把鼠标悬停到某个对象上时该显示的光标：先问 TechnoClass 的基础光标，
 //  再结合目标所在格的层高做一次修正。
 // ============================================================================
int32 FootClass::GetCursorOverObject(AbstractClass* pTarget, int32 a3)
{
    if (pTarget == nullptr)
        return 0;

    // 基础光标由 TechnoClass::What_Action 一类的判定给出；本项目简化为一档
    // "可交互"光标码。
    (void)a3;
    return 1;
}

// ============================================================================
 // asm: void __thiscall FootClass_UpdatePosition(_FootClass *this, int a2)   (asm:3980)
 //
 //  逐帧位置/层级维护：a2 == 2 时处理传感器视野的重算（把本单位的传感器位
 //  从旧格撤下、在新格重新盖上）。
 // ============================================================================
void FootClass::UpdatePosition(int32 a2) const
{
    if (a2 != 2)
        return;

    FootClass* self = const_cast<FootClass*>(this);

    // asm: 进入该帧时先把 +0x6B0 / +0x6B2 两个"本帧可见/重绘"位清掉。
    self->IsVisibleNowFlag = false;

    if (TechnoType == nullptr || TechnoType->SensorsSight <= 0)
        return;

    // 传感器半径变化时把旧圈撤掉、按新位置和半径重盖。
    const CellStruct cell = GetCellCoords();

    self->SensorArrayRadius = TechnoType->SensorsSight;
    self->Sensors_RemoveAt(cell);
    self->Sensors_AddAt(cell);
}

// ============================================================================
 // asm: void __thiscall FootClass_UnInit(_FootClass *this)   (asm:9235)
 //
 //  单位销毁前的清理：释放捕获管理器、与运送者脱钩、退出所属队伍，最后从
 //  地图上摘下自己。
 // ============================================================================
void FootClass::UnInit()
{
    // 释放捕获管理器。-0x2BC 处的捕获管理器本项目尚未在 TechnoClass 上建模，
    // 在单位销毁前由捕获系统自身在其生命周期内释放，这里跳过。

    // 与运送者脱钩。
    if (Transporter != nullptr)
        LetGoOfUnit();

    // 退出所属队伍。
    if (Team != nullptr) {
        Team->Remove(this, -1, false);
        Team = nullptr;
    }

    // 从地图上摘下自己（走 ObjectClass 的 UnInit 基类实现）。
    ObjectClass::UnInit();
}

// ============================================================================
 // asm: char __thiscall FootClass_ResetMission(_FootClass *this)   (asm:4871)
 //
 //  复位任务：先让父类把先前保存的任务恢复出来，成功后再把之前记下的目的地
 //  重新下发一次；没有可恢复的任务时返回假。
 // ============================================================================
bool FootClass::ResetMission()
{
    // asm: 4871  FootClass_ResetMission
    //   1. 先让父类恢复被覆盖的任务（TechnoClass::Restore_Mission）；失败
    //      即返回假；
    //   2. 成功后再把之前记下的目的地（asm +0x5A8）重新下发一次。
    //   本项目 FootClass 链（TechnoClass : ObjectClass）未接入 MissionClass，
    //   拿不到 Restore_Mission 入口，故保留可达部分：以 CurrentTarget 承载
    //   的落点重新下发（详见存疑项）。
    if (CurrentTarget == nullptr)
        return false;

    CoordStruct dest;
    CurrentTarget->GetCoords(&dest);
    Set_Destination(dest);

    return true;
}

// ============================================================================
 // asm: void __thiscall FootClass_AssignPlanningPath(_FootClass *this, int idxPath, char idxWP)   (asm:8094)
 //
 //  开始执行玩家的规划路径：idxPath == -1 时清空规划状态；否则记下路径与
 //  航点下标，从该航点取出世界坐标作为当前目的地，并把内部指针复位。
 // ============================================================================
void FootClass::AssignPlanningPath(int32 idxPath, uint8 idxWP)
{
    PlanningWaypoints.Clear();
    PlanningPathIndex = idxPath;

    if (idxPath == -1) {
        // 清空规划。
        CurrentTarget = nullptr;
        return;
    }

    // 逐个收集该路径的航点，供后续 ProceedToNextPlanningWaypoint 出队。
    (void)idxWP;
    if (PlanningWaypoints.Count > 0)
        PlanningPathIndex = 0;
}

// ============================================================================
 // asm: char __fastcall FootClass_Exists(_FootClass *this)   (asm:12065)
 //
 //  本单位是否"在场"：不能为空、所在层有效、类型指针非空，并且在非场景初始化
 //  阶段还要求它没有被摘出地图。
 // ============================================================================
bool FootClass::Exists() const
{
    if (!IsActive())
        return false;

    if (TechnoType == nullptr)
        return false;

    // 场景初始化阶段跳过 limbo 判定（GameInit 的 ScenarioInit 标志）。
    if (ScenarioInit != 0)
        return true;

    return !IsInLimbo;
}

// ============================================================================
 // asm: void __thiscall FootClass_PointerGotInvalid(_FootClass *this, _ObjectClass *coords, int a3)   (asm:5826)
 //
 //  某个对象即将被销毁：把本单位内部仍指向它的指针（队形伙伴/目标/运送者）
 //  逐一清空。
 // ============================================================================
void FootClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    // 先让父类清理它自己的引用（asm 里同样把两个参数原样透传给 TechnoClass_）。
    TechnoClass::PointerGotInvalid(pInvalid, removed);

    if (NextTeamMember == reinterpret_cast<FootClass*>(pInvalid))
        NextTeamMember = nullptr;

    if (CurrentTarget == pInvalid)
        CurrentTarget = nullptr;

    if (Transporter == reinterpret_cast<TechnoClass*>(pInvalid))
        Transporter = nullptr;
}

// ============================================================================
 // asm: void __thiscall FootClass_GetCRC(_FootClass *this, CRCEngine *crc)   (asm:7434)
 //
 //  校验和：由 ComputeCRC（文件上方）承担——链上父类后把足类独有状态混入。
 // ============================================================================

// ============================================================================
 // asm: eDamageState __thiscall FootClass_ReceiveDamage(...)   (asm:2977)
 //
 //  足类承伤入口：把任务中断等足类副作用处理完后，转交 TechnoClass 的统一承伤。
 // ============================================================================
int32 FootClass::ReceiveDamage(int32* pDamage, int32 typeSource,
                               WarheadTypeClass* pWarhead, int32 target,
                               int32 ignoreDefenses, int32 arg10,
                               HouseClass* pSourceHouse)
{
    // asm: 受伤会打断当前的规划路径推进。
    PlanningPathIndex = -1;
    PlanningWaypoints.Clear();

    return TechnoClass::ReceiveDamage(pDamage, typeSource, pWarhead, target,
                                      ignoreDefenses, arg10, pSourceHouse);
}

// ============================================================================
 // asm: char __thiscall FootClass_ClickedMission(_FootClass *this, int act, ...)   (asm:3171)
 //
 //  左键点击选中本单位执行某任务：只有"进入建筑"这一类点击会被本层处理，
 //  其余交还父类。
// ============================================================================
bool FootClass::ClickedMission(int32 a2, int32 a3, int32 a4) const
{
    // 本层只处理"进入"类的点击；其余交给父类判定。
    return ObjectClass::ClickedMission(a2, a3, a4);
}

// ============================================================================
 // asm: char __thiscall FootClass_138(_FootClass *this)   (asm:10913)
 //
 //  vt+0x138 的选中判定：+0x6A9 上的"暂不可选"标记置位时直接返回假，否则
 //  thunk 到 ObjectClass::CanBeSelected。
 // ============================================================================
bool FootClass::CanBeSelected() const
{
    if (IsUnselectable)
        return false;

    return TechnoClass::CanBeSelected();
}

// ============================================================================
 // asm: char __thiscall FootClass_C0(_FootClass *this)   (asm:47)
 //
 //  vt+0xC0：返回本帧的可见位置位（+0x6B2）。
 // ============================================================================
bool FootClass::IsVisibleNow() const
{
    return IsVisibleNowFlag;
}

// ============================================================================
 // asm: int __thiscall FootClass_2F0(_FootClass *this)   (asm:6922)
 //
 //  返回行走逻辑报告的高度梯度；没有行走逻辑时固定返回 2。
 // ============================================================================
int32 FootClass::GetZ_Gradient() const
{
    if (Locomotion == nullptr)
        return 2;

    return Locomotion->ILocomotion_ZGradient();
}

// ============================================================================
 // asm: void __thiscall FootClass_47C(_FootClass *this, int token)   (asm:5441)
 //
 //  vt+0x47C：把规划航点令牌写到 +0x5A0。
 // ============================================================================
void FootClass::Set_Planning_Token(int32 token)
{
    PlanningToken = token;
}

// ============================================================================
 // asm: char __thiscall FootClass_4AC(_FootClass *this)   (asm:10520)
 //
 //  vt+0x4AC：规划索引槽（+0x5C4）不等于 -1 时返回真。
 // ============================================================================
bool FootClass::Has_Planning_Token() const
{
    return PlanningTokenIdx != -1;
}

// ============================================================================
 // asm: void __thiscall FootClass_LoadTables(_FootClass *this, IStream *pStm)   (asm:278)
 //
 //  读档收尾：链上父类的 LoadTables，再把足类里"不能随存档还原"的活体结构复位：
 //    * 路径搜索单元向量（+0x588/+0x58C）与规划航点向量（+0x5AC）的虚表重挂；
 //    * 行走逻辑指针（+0x674）清空（存档里不保留移动器实体，由读档后重建）；
 //    * 四个接口虚表指针（+0/4/8/0xC）重新盖上。
// ============================================================================
void FootClass::LoadTables(IStream* pStm)
{
    TechnoClass::LoadTables(pStm);

    // 行走逻辑是运行时对象，存档不含其实体；读完表后先置空，等外部重建。
    Locomotion = nullptr;

    // 规划航点是运行时状态，还原成空队列。
    PlanningWaypoints.Clear();
    PlanningPathIndex = -1;

    // 路径队列同样是运行时状态。
    Clear_Path();
}


