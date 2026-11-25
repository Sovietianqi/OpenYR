#include "DriveLocomotionClass.h"
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

#include <cmath>
#include <cstdlib>

#include "../Map/MapClass.h"
#include "../AI/TagClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Abstract/InfantryClass.h"

// ============================================================================
// DriveLocomotionClass - Vehicle driving locomotion.
// Track-based movement, speed per terrain type, turret/barrel rotation,
// body rotation, crush logic for infantry, building collision, slope
// handling, and bridge traversal.
// ============================================================================

// ============================================================================
// 载具格子占用登记（CellClass::Occupier 槽 + 地面 FirstObject 链）
//
//   步兵侧（WalkLocomotionClass）在跨格时就地调用 Add_Occupier/Remove_Occupier，
//   载具侧此前只走 MapClass::MarkCellOccupied —— 那个函数把"本格当前 Occupier"
//   再登记一遍，而载具从来没登记过 Occupier，于是整条调用恒为空转：格子链上
//   看不到坦克，FootClass::Can_Enter_Cell 就放行步兵走进坦克所在的格。
//
//   这里按"上次登记的格"做差量同步：格号没变就什么都不做，变了才
//   Remove 旧格 + Add 新格。不改任何公共类的成员布局 —— 上次登记的格存在
//   本文件内的旁表里（与 AI/TeamClass.cpp 的 TeamBirthFrames() 同一手法）。
//
//   为什么停车/到达不注销：格子占用表达的是"这一格里站着东西"。停着的坦克
//   仍然占格，注销它等于把坦克重新变透明，步兵又会开进去 —— 那正是本次要修
//   的毛病。所以到达与 Stop_Moving 只把登记"迁到当前格"，绝不留旧格残链；
//   只有载体离场（Limbo / 销毁 / 移动器换绑）才真正摘除。
// ============================================================================

namespace {

struct VehicleCellRecord
{
    ObjectClass* Owner;   // 登记时的载体（载体先于移动器消失时用来对账）
    CellStruct   Cell;    // 上一次登记到哪一格
};

std::unordered_map<const DriveLocomotionClass*, VehicleCellRecord>& VehicleCells()
{
    static std::unordered_map<const DriveLocomotionClass*, VehicleCellRecord> table;
    return table;
}

// 对象是否已挂在本格的地面链上。
bool OnGroundChain(CellClass* pCell, ObjectClass* pObj)
{
    if (pCell == nullptr || pObj == nullptr)
        return false;
    for (ObjectClass* p = pCell->First_Object(false); p != nullptr; p = p->NextObject)
    {
        if (p == pObj)
            return true;
    }
    return false;
}

} // namespace

DriveLocomotionClass::DriveLocomotionClass()
    : LocomotionClass()
    , PreviousRamp(0)
    , CurrentRamp(0)
    , Destination(0, 0, 0)
    , HeadToCoord(0, 0, 0)
    , SpeedAccum(0)
    , MovementSpeed(1.0)
    , TrackNumber(0)
    , TrackIndex(0)
    , IsOnShortTrack(false)
    , IsTurretLockedDown(0)
    , IsRotating(false)
    , IsDriving(false)
    , IsRocking(false)
    , IsLocked(false)
    , Piggybackee(nullptr)
    , TrackType(0)
    , Crusher(false)
    , Destroyer(false)
    , IsHovering(false)
    , IsAmphibious(false)
    , CurrentFacing(0)
    , TargetFacing(0)
    , RotationSpeed(8)
{
    Speed = 128;
}

DriveLocomotionClass::~DriveLocomotionClass()
{
    // 移动器没了就没人能再维护这格登记：先把载体从它最后登记的格子上摘掉，
    //   否则格子链上会留一个指向"再也走不动"的载体的条目，步兵绕路绕到死。
    ReleaseCellOccupation();

    if (Piggybackee) {
        Piggybackee = nullptr;
    }
}

HRESULT DriveLocomotionClass::GetClassID(CLSID* pClassID)
{
    // asm: 判空失败返回 80004003h (E_POINTER)，否则把 16 字节 GUID 原样拷出。
    if (pClassID == nullptr)
        return E_POINTER;

    *pClassID = CLSIDs::Get(LocoID);
    return S_OK;
}

int32 DriveLocomotionClass::Size()
{
    return sizeof(DriveLocomotionClass);
}

// ============================================================================
// Begin_Piggyback / End_Piggyback - Piggyback system for vehicle
// carrying (e.g., transport loading).
// ============================================================================

HRESULT DriveLocomotionClass::Begin_Piggyback(ILocomotion* pointer)
{
    Piggybackee = pointer;
    return S_OK;
}

HRESULT DriveLocomotionClass::End_Piggyback(ILocomotion** pointer)
{
    if (pointer) {
        *pointer = Piggybackee;
    }
    Piggybackee = nullptr;
    return S_OK;
}

// ============================================================================
// Move_To - Sets vehicle destination. Begins driving toward target.
// ============================================================================

void DriveLocomotionClass::Move_To(CoordStruct to)
{
    // 已经在目的地上了：不要再"重新出发"。
    //   任务层每一步都会重新 SetDestination(目标建筑坐标)，而建筑坐标是个
    //   固定点；单位开到点后若不短路，每帧都会被重新点亮 IsDriving，于是
    //   Is_Moving() 永远为真 —— 上层"到了就转任务"的判据（Mission_Harvest
    //   的卸矿分支、AI 的运输线）全部失效，矿车就顶着卸矿站在厂里却永远
    //   不卸货。
    if (CurrentCoord.DistanceFrom(to) == 0)
    {
        Destination = CoordStruct(0, 0, 0);
        Dest        = to;
        IsDriving   = false;
        IsMoving    = false;
        IsRotating  = false;
        SpeedAccum  = 0;
        return;
    }

    Destination = to;
    Dest = to;
    IsDriving = true;
    IsMoving = true;
    IsRotating = false;

    TargetFacing = CoordMath::DirectionTo(CurrentCoord, to);
    HeadToCoord = to;
}

// ============================================================================
// Stop_Moving - Halts vehicle movement.
// ============================================================================

void DriveLocomotionClass::Stop_Moving()
{
    IsDriving = false;
    IsMoving = false;
    IsRotating = false;
    IsRocking = false;
    SpeedAccum = 0;

    // ★ 目的地必须一起清。
    //   ILocomotion::Is_Moving 的判据是 `Destination != (0,0,0)`（asm:372），
    //   只清 IsDriving 会让"已经停下"的车永远报告"在移动"：AI 的
    //   "行进途中不重算路径"判据因此永久成立，单位停在原地一帧也推不动
    //   （实测 AI 坦克在 (5252,4890) 从 f=600 卡到 f=1800）。到达分支
    //   （StepTowardsDestination）本来就是这么清的，停止分支漏了。
    Destination = CoordStruct(0, 0, 0);
    HeadToCoord = CoordStruct(0, 0, 0);
    Dest        = CoordStruct(0, 0, 0);

    // 停车保留占用：位置不变就把登记迁到当前格（原地停车时是同格 no-op）。
    SyncCellOccupation();
}

// ============================================================================
// SyncCellOccupation - 把本载体的格子占用登记同步到当前坐标所在格。
//
//   与步兵侧 WalkLocomotionClass 的跨格处理对称：旧格 Remove_Occupier、
//   新格 Add_Occupier，一格一辆（原版载具占 1 格，多格是建筑的事）。
//   只在格号真正变化时动链，避免每帧重复 Add/Remove。
// ============================================================================

void DriveLocomotionClass::SyncCellOccupation()
{
    auto& table = VehicleCells();
    auto it = table.find(this);

    if (Owner == nullptr || MapClass::Instance == nullptr)
    {
        // 载体已经没了/地图不在：不能再解引用，只清旁表（格子侧由
        //   ObjectClass 的析构走 RemoveContent 收尾）。
        if (it != table.end())
            table.erase(it);
        return;
    }

    const CellStruct cell = CoordMath::CoordToCell(CurrentCoord);

    if (it == table.end() || it->second.Owner != Owner)
    {
        // 首次登记，或移动器被换绑到另一个载体。
        //   换绑时旧载体已不属于本移动器，按旧指针去注销会误伤，只补新登记。
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell == nullptr)
            return;

        if (!OnGroundChain(pCell, Owner) && Owner->NextObject != nullptr)
        {
            // 载体的链指针指着别的格：Unlimbo 只在"落位那一格"挂过链，
            //   Limbo/Unlimbo 换格时不会迁移，于是 NextObject 非空但本格链
            //   里没有它，Add_Occupier 的挂链条件（NextObject 必须为空）就
            //   永远不成立。就近三格内找一遍残链并摘掉，只做一次。
            for (int32 dx = -1; dx <= 1; ++dx)
            {
                for (int32 dy = -1; dy <= 1; ++dy)
                {
                    const CellStruct probe(static_cast<int16>(static_cast<int32>(cell.X) + dx),
                                           static_cast<int16>(static_cast<int32>(cell.Y) + dy));
                    CellClass* pProbe = MapClass::Instance->GetCellAt(probe);
                    if (pProbe != nullptr && pProbe != pCell && OnGroundChain(pProbe, Owner))
                        pProbe->Remove_Occupier(Owner);
                }
            }
        }

        pCell->Add_Occupier(Owner);
        table[this] = VehicleCellRecord{Owner, cell};
        return;
    }

    if (it->second.Cell == cell)
    {
        // 同格的常规情形：什么都不做。但要防一类"外力抹除"——
        //   UnitClass::SetCoords 在每次写坐标前都会 UnmarkAllOccupationBits
        //   旧格，而它的重新标记走 MarkCellOccupied，只能把"本格当前
        //   Occupier"再登记一遍：坦克正是本格 Occupier 时，Unmark 把自己
        //   摘掉之后，Mark 已经读不到任何 Occupier 了，登记就被抹成空的。
        //   每帧补一次，保证"我这一格里确实登记着我"这个不变式。
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell != nullptr
            && pCell->Get_Occupier() != Owner
            && !OnGroundChain(pCell, Owner))
        {
            pCell->Add_Occupier(Owner);
        }
        return;
    }

    // 跨格：旧格注销 + 新格登记。
    CellClass* pOldCell = MapClass::Instance->GetCellAt(it->second.Cell);
    if (pOldCell != nullptr)
        pOldCell->Remove_Occupier(Owner);

    CellClass* pNewCell = MapClass::Instance->GetCellAt(cell);
    if (pNewCell != nullptr)
        pNewCell->Add_Occupier(Owner);

    it->second.Cell = cell;

    // 跨格即"进入新格"：若该格挂有 [CellTags]，向标签上抛 EnteredBy。
    TagClass::SpringCellTag(cell, Owner);
}

// ============================================================================
// ReleaseCellOccupation - 摘掉本载体的占用登记（离场用）。
// ============================================================================

void DriveLocomotionClass::ReleaseCellOccupation()
{
    auto& table = VehicleCells();
    auto it = table.find(this);
    if (it == table.end())
        return;

    // 只有"当前 Owner 就是登记时的那个指针"才敢解引用；载体的析构会把
    //   Owner 置空/换人，这时按旧指针调 Remove_Occupier 就是悬空访问。
    if (Owner != nullptr && it->second.Owner == Owner && MapClass::Instance != nullptr)
    {
        CellClass* pCell = MapClass::Instance->GetCellAt(it->second.Cell);
        if (pCell != nullptr)
            pCell->Remove_Occupier(Owner);
    }

    table.erase(it);
}

// ============================================================================
// Do_Turn - Initiates vehicle body rotation toward a facing direction.
// ============================================================================

void DriveLocomotionClass::Do_Turn(DirStruct coord)
{
    // asm: 载具不做瞬转——把目标朝向登记到 FacingClass，由每帧的
    //   RotateTowards 按 RotationSpeed 逐帧逼近；到位前 Is_Moving_Now
    //   恒为真，其它系统据此知道"车还在转"。
    TargetFacing = coord;
    if (CurrentFacing.Value == coord.Value) {
        IsRotating = false;
        return;
    }
    IsRotating = true;
}

// ============================================================================
// Do_Turret_Turn - Vehicle turret rotation independent of body.
// ============================================================================

void DriveLocomotionClass::Do_Turret_Turn(DirStruct coord)
{
    if (!Owner || !Owner->HasTurret()) {
        return;
    }

    DirStruct turretFacing = Owner->GetTurretFacing();
    int32 diff = static_cast<int32>(coord.Value) - static_cast<int32>(turretFacing.Value);

    if (diff == 0) {
        return;
    }

    int32 turretSpeed = 5;
    int32 step = 0;

    if (diff < -128 || (diff > 0 && diff <= 128)) {
        step = (diff > 0) ? turretSpeed : -turretSpeed;
    } else {
        step = (diff > 0) ? -turretSpeed : turretSpeed;
    }

    int32 newVal = static_cast<int32>(turretFacing.Value) + step;
    if (newVal < 0) {
        newVal += 256;
    }
    if (newVal >= 256) {
        newVal -= 256;
    }

    if (std::abs(newVal - static_cast<int32>(coord.Value)) <= turretSpeed) {
        Owner->SetTurretFacing(coord);
    } else {
        Owner->SetTurretFacing(DirStruct(static_cast<uint8>(newVal)));
    }
}

// ============================================================================
// Is_Moving - 目的地/朝往点任一非零即为在动。
// asm: DriveLocomotionClass_ILocomotion_IsMoving   (asm:372)
//   先看 Destination(+0x34)：非零 → true；否则看 HeadTo(+0x40)：非零且与载体
//   的缓存绘制点（+0x9C）不相等 → true；两者都归零 → false。
// ============================================================================

bool DriveLocomotionClass::Is_Moving()
{
    if (!(Destination == CoordStruct(0, 0, 0))) {
        return true;
    }

    if (HeadToCoord == CoordStruct(0, 0, 0)) {
        return false;
    }

    if (Owner) {
        const CoordStruct cached = Owner->CachedLocation;
        if (cached == HeadToCoord) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Is_Moving_Now - 转身未完成也算在动（原版先看朝向是否到位）。
// asm: DriveLocomotionClass_ILocomotion_IsMovingNow   (asm:467)
//   1) 载体 FacingStruct 尚未到位（IsReady 为假）→ true；
//   2) 否则 Is_Moving() && HeadTo != 0 && 载体当前速度 > 0 → true；
//   3) 其它 → false。
// ============================================================================

bool DriveLocomotionClass::Is_Moving_Now()
{
    if (Owner && IsRotating) {
        return true;
    }

    if (!Is_Moving()) {
        return false;
    }
    if (HeadToCoord == CoordStruct(0, 0, 0)) {
        return false;
    }
    if (Owner && Owner->GetCurrentSpeed() <= 0) {
        return false;
    }
    return true;
}

// ============================================================================
// Process - 载具每帧驱动入口。
// asm: 由 sub_4B0F20 承载（Drive 的每帧仿真）。控制流：
//   1) 被锁（Lock）或不在动 → 直接返回；
//   2) 若在摇车（过断桥坠落）→ 走摇车分支；
//   3) 若在转身 → 朝目标朝向逐帧旋转，转到位后清旗标；
//   4) 若在驾驶 → 地形速度修正、加减速曲线、逐格推进、碾压判定、
//      桥梁检查、坡道俯仰；
//   5) 回传是否仍在活动。
// ============================================================================

bool DriveLocomotionClass::Process()
{
    // 每帧先把占用登记对齐到当前坐标所在格：
    //   * 停着的车也要保住自己那一格（步兵不能开进来）；
    //   * 被外力挪格（卸载落位 / 传送 / Force_Track）后，下一帧就能自愈，
    //     不会在旧格留一条永远不会走开的残链。
    SyncCellOccupation();

    UpdateDrive();
    return IsDriving || IsRotating || IsRocking;
}

// ============================================================================
// UpdateDrive - 载具每帧驱动主体。
// asm: sub_4B0F20   —— 控制流：
//   1) 被锁（Lock）或不在动 → 直接返回；
//   2) 若在摇车（过断桥坠落）→ 走摇车分支；
//   3) 若在转身 → 朝目标朝向逐帧旋转，转到位后清旗标；
//   4) 若在驾驶 → 地形速度、坡道收敛、到达减速、逐格推进、碾压判定、
//      桥梁检查、坡道俯仰。
// ============================================================================

void DriveLocomotionClass::UpdateDrive()
{
    if (IsLocked) {
        return;
    }

    if (!IsDriving && !IsRotating && !IsRocking) {
        return;
    }

    if (IsRocking) {
        ProcessRocking();
        return;
    }

    // --- 转身（原地或行进中） ---
    if (IsRotating) {
        RotateTowards(TargetFacing);
        if (CurrentFacing.Value == TargetFacing.Value) {
            IsRotating = false;
        }
        if (!IsDriving) {
            return;
        }
    }

    if (!IsDriving) {
        return;
    }

    // --- 地形速度修正 ---
    Movement_AI();

    // --- 坡道速度收敛（爬坡降速、下坡恢复） ---
    UpdateSlopeSpeed();

    // --- 到达减速曲线 ---
    float targetSpeed = static_cast<float>(Speed) * SpeedPercentage
        * static_cast<float>(MovementSpeed);

    int32 distToDest = CurrentCoord.DistanceFrom(Destination);
    const int32 decelDist = (Speed > 0) ? Speed * 4 : 256;
    if (distToDest < decelDist) {
        float factor = static_cast<float>(distToDest) / static_cast<float>(decelDist);
        targetSpeed *= (0.4f + 0.6f * factor);
    }

    int32 effectiveSpeed = static_cast<int32>(targetSpeed);
    if (effectiveSpeed <= 0) {
        effectiveSpeed = 1;
    }

    // --- 坡差：上坡减速、下坡加速 ---
    if (MapClass::Instance != nullptr) {
        CellStruct curCell = CoordMath::CoordToCell(CurrentCoord);
        CellStruct dstCell = CoordMath::CoordToCell(Destination);
        CellClass* pCur = MapClass::Instance->GetCellAt(curCell);
        CellClass* pDst = MapClass::Instance->GetCellAt(dstCell);
        if (pCur != nullptr && pDst != nullptr) {
            int32 heightDelta = pDst->Get_Ground_Height() - pCur->Get_Ground_Height();
            if (heightDelta > 0) {
                effectiveSpeed = (effectiveSpeed * 3) / 4;   // 上坡
            } else if (heightDelta < 0) {
                effectiveSpeed = (effectiveSpeed * 5) / 4;   // 下坡
            }
        }
    }

    SpeedAccum += effectiveSpeed;

    // --- 逐格推进 ---
    StepTowardsDestination(effectiveSpeed);

    if (IsDriving) {
        CheckCrush();
        CheckBridge();

        if (IsOnBridge() && CheckBridge()) {
            SlopeTimer.Update(effectiveSpeed);
        }

        Tilt_Pitch_AI();
    }
}

// ============================================================================
// UpdateSlopeSpeed - 由 CurrentRamp 推目标速度系数并逐帧逼近。
// asm: sub_4B0F20 中 MovementSpeed 由坡度与 SlopeTimer 收敛的段。
// ============================================================================

void DriveLocomotionClass::UpdateSlopeSpeed()
{
    float rampTarget = 1.0f;
    if (CurrentRamp > 0) {
        rampTarget = 1.0f - static_cast<float>(CurrentRamp) * 0.08f;
        if (rampTarget < 0.3f) {
            rampTarget = 0.3f;
        }
    }

    double delta = static_cast<double>(rampTarget) - MovementSpeed;
    MovementSpeed += delta * 0.25;

    if (MovementSpeed < 0.3) {
        MovementSpeed = 0.3;
    }
    if (MovementSpeed > 1.0) {
        MovementSpeed = 1.0;
    }
}

// ============================================================================
// StepTowardsDestination - 按速度累积逐格推进并做到达判定。
// asm: sub_4B0F20 的 SpeedAccum >= 256 步进段（含 UpdatePosition 与履带计数）。
// ============================================================================

void DriveLocomotionClass::StepTowardsDestination(int32 effectiveSpeed)
{
    const int32 stepThreshold = 256;

    while (SpeedAccum >= stepThreshold && IsDriving) {
        SpeedAccum -= stepThreshold;

        UpdatePosition();

        // 每推进一格履带动画前进一帧。
        ++TrackNumber;

        int32 dist = CurrentCoord.DistanceFrom(Destination);
        if (dist <= effectiveSpeed) {
            CurrentCoord = Destination;
            if (Owner) {
                Owner->SetCoords(CurrentCoord);
                Owner->CachedLocation = CurrentCoord;
            }
            IsDriving = false;
            IsMoving  = false;

            // 到达即把占用登记迁到落点格：Destination 清掉之后单位就停在
            //   这里，登记必须跟着走，否则旧格留残链、新格没有它。
            SyncCellOccupation();

            // 到达即清目的地/朝往点。原版 ILocomotion::Is_Moving 的判据是
            //   `Destination != (0,0,0)`（asm:372），不清就会永远报"在动"：
            //   单位明明到了，AI 与动画层却始终把它当行进中。
            Destination = CoordStruct(0, 0, 0);
            HeadToCoord = CoordStruct(0, 0, 0);
            SpeedAccum  = 0;
            break;
        }
    }
}

// ============================================================================
// ApplyTerrainSpeed - 按地表类型设置 SpeedPercentage（水面非两栖即停车）。
// asm: sub_4B0F20 的地表速度段。
// ============================================================================

void DriveLocomotionClass::ApplyTerrainSpeed()
{
    if (!IsDriving) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(currentCell);

    float baseMod = 1.0f;
    switch (land) {
        case LandType::Road:
            baseMod = 1.1f;
            break;
        case LandType::Rough:
            baseMod = 0.7f;
            break;
        case LandType::Ice:
            baseMod = 0.75f;
            break;
        case LandType::Weeds:
            baseMod = 0.8f;
            break;
        case LandType::Tiberium:
            baseMod = 0.65f;
            break;
        case LandType::Beach:
            baseMod = 0.9f;
            break;
        case LandType::Water:
            if (IsAmphibious) {
                baseMod = 0.6f;
            } else {
                IsDriving = false;
                IsMoving = false;
                return;
            }
            break;
        default:
            baseMod = 1.0f;
            break;
    }

    SpeedPercentage = baseMod;
}


// ============================================================================
// UpdatePosition - Moves the vehicle toward its destination.
// ============================================================================

void DriveLocomotionClass::UpdatePosition()
{
    if (IsDriving && !IsRotating) {
        int32 effectiveSpeed = static_cast<int32>(
            static_cast<float>(Speed) * SpeedPercentage * MovementSpeed);

        CurrentCoord = VectorMath::MoveTowards(CurrentCoord, Destination, effectiveSpeed);

        // Ground the vehicle on the terrain height below.  Hovering units
        // keep altitude but never sink beneath the terrain.
        if (MapClass::Instance != nullptr) {
            int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);
            if (!IsHovering) {
                CurrentCoord.Z = groundZ;
            } else if (CurrentCoord.Z < groundZ) {
                CurrentCoord.Z = groundZ;
            }
        }

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
        }

        // 逐格推进后立刻同步：本帧可能连推多格（SpeedAccum 累积），
        //   每跨一格都要把登记从旧格搬到新格。
        SyncCellOccupation();
    }
}

// ============================================================================
// RotateTowards - Smoothly rotates the vehicle body toward the target facing.
// ============================================================================

void DriveLocomotionClass::RotateTowards(DirStruct targetDir)
{
    int32 diff = static_cast<int32>(targetDir.Value) - static_cast<int32>(CurrentFacing.Value);
    if (diff == 0) {
        return;
    }

    if (diff < -128 || (diff > 0 && diff <= 128)) {
        int32 step = (diff > 0) ? RotationSpeed : -RotationSpeed;
        int32 newVal = static_cast<int32>(CurrentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(targetDir.Value)) <= RotationSpeed) {
            CurrentFacing = targetDir;
        } else {
            CurrentFacing = DirStruct(static_cast<uint8>(newVal));
        }
    } else {
        int32 step = (diff > 0) ? -RotationSpeed : RotationSpeed;
        int32 newVal = static_cast<int32>(CurrentFacing.Value) + step;
        if (newVal < 0) {
            newVal += 256;
        }
        if (newVal >= 256) {
            newVal -= 256;
        }

        if (std::abs(newVal - static_cast<int32>(targetDir.Value)) <= RotationSpeed) {
            CurrentFacing = targetDir;
        } else {
            CurrentFacing = DirStruct(static_cast<uint8>(newVal));
        }
    }

    if (Owner) {
        Owner->SetFacing(CurrentFacing);
    }
}

// ============================================================================
// CheckCrush - Checks if the vehicle is crushing infantry in its path.
// ============================================================================

void DriveLocomotionClass::CheckCrush()
{
    if (!Crusher && !Destroyer) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (pCell == nullptr) {
                continue;
            }

            // 碾压目标必须走地面链来找，不能只查 Occupier 槽：
            //   Occupier 只有一个槽，坦克自己一登记就会把同格的步兵从槽里
            //   挤掉（Add_Occupier 无条件写槽），只查槽等于"坦克压到人身上
            //   时反而看不见人"。链是登记的唯一权威，槽再补查一遍以覆盖
            //   只进了槽、没挂链的外力落位对象。
            ObjectClass* pSlotOccupier = pCell->Get_Occupier();

            auto crushInfantry = [&](ObjectClass* pObj) {
                if (pObj == nullptr || pObj == Owner) {
                    return;
                }
                if (pObj->WhatAmI() != AbstractType::Infantry) {
                    return;
                }
                InfantryClass* infantry = static_cast<InfantryClass*>(pObj);
                if (Destroyer) {
                    infantry->TakeDamage(10000, static_cast<ObjectClass*>(nullptr), WarheadTypeClass::GetDefault());
                } else if (Crusher) {
                    infantry->TakeDamage(250, Owner, WarheadTypeClass::GetDefault());
                }
            };

            crushInfantry(pSlotOccupier);

            for (ObjectClass* pObj = pCell->First_Object(false);
                 pObj != nullptr;
                 pObj = pObj->NextObject)
            {
                if (pObj == pSlotOccupier) {
                    continue;                   // 上面已经处理过
                }
                crushInfantry(pObj);
            }
        }
    }
}

// ============================================================================
// CheckBuildingCollision - Checks for and handles building collisions.
// ============================================================================

bool DriveLocomotionClass::CheckBuildingCollision()
{
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(currentCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(currentCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            if (MapClass::Instance->IsCellOccupied(cell)) {
                ObjectClass* occupant = MapClass::Instance->GetCellOccupier(cell);
                if (occupant && occupant->WhatAmI() == AbstractType::Building) {
                    if (Destroyer) {
                        return true;
                    }
                    return !Crusher;
                }
            }
        }
    }
    return false;
}

// ============================================================================
// IsOnBridge - Checks if the vehicle is currently on a bridge.
// ============================================================================

bool DriveLocomotionClass::IsOnBridge() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }
    return MapClass::Instance->IsBridgeCell(cell);
}

// ============================================================================
// CheckBridge - Validates bridge integrity and handles destruction.
// ============================================================================

bool DriveLocomotionClass::CheckBridge()
{
    if (!IsOnBridge()) {
        return false;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    if (MapClass::Instance->IsBridgeDestroyed(cell)) {
        IsDriving = false;
        IsMoving = false;
        IsRocking = true;
        return true;
    }

    return false;
}

// ============================================================================
// ProcessRocking - Handles vehicle rocking animation after bridge collapse.
// ============================================================================

bool DriveLocomotionClass::ProcessRocking()
{
    int32 rockTimer = 30;
    static int32 rockPhase = 0;

    if (rockPhase < rockTimer) {
        ++rockPhase;
        if (Owner) {
            Owner->SetSequence(Sequence::Tumble);
        }
        return true;
    }

    rockPhase = 0;
    IsRocking = false;
    return false;
}

// ============================================================================
// Can_Enter_Cell - Vehicle-specific cell traversal check.
// ============================================================================

Move DriveLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    LandType land = MapClass::Instance->GetLandType(cell);

    if (IsAmphibious) {
        if (land == LandType::Wall) {
            return Move::No;
        }
        return Move::OK;
    }

    if (IsHovering) {
        if (land == LandType::Wall || land == LandType::Rock) {
            return Move::No;
        }
        return Move::OK;
    }

    if (land == LandType::Water || land == LandType::Wall) {
        return Move::No;
    }

    if (land == LandType::Rock && !Destroyer) {
        return Move::No;
    }

    return Move::OK;
}

// ============================================================================
// Mark_All_Occupation_Bits - 载具的格子占用标记（单格）。
//
// asm: FootClass_ImbueLocomotor+15C → call [vt+ILocomotion::Mark_All_Occupation_Bits]
//   原版由移动器在落位/离场时把"自己占住的格子"标上/抹掉。本树此前转给
//   MapClass::MarkCellOccupied(cell, occupied)，而该函数只能操作"本格当前的
//   Occupier"——载具从没登记过 Occupier，于是 Up/Down 都是空转。这里直接走
//   权威的 Add_Occupier / Remove_Occupier（与步兵同一套 API）。
//
//   载具占 1 格（原版坦克占 1 格；3x3 是建筑地基的事），所以不再按 3x3
//   邻格标记——那一圈标记本来也只是把邻格现有 Occupier 再登记一遍。
// ============================================================================

void DriveLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (mark == MarkType::Up)
        SyncCellOccupation();
    else
        ReleaseCellOccupation();
}

// ============================================================================
// Limbo - Vehicle limbo state.
// ============================================================================

void DriveLocomotionClass::Limbo()
{
    Mark_All_Occupation_Bits(MarkType::Down);
    IsDriving = false;
    IsMoving = false;
    IsRotating = false;
    IsRocking = false;
    SpeedAccum = 0;
}

// ============================================================================
// Force_New_Slope - Vehicle slope handling with speed penalty.
// ============================================================================

void DriveLocomotionClass::Force_New_Slope(int32 ramp)
{
    PreviousRamp = CurrentRamp;
    CurrentRamp = static_cast<uint32>(ramp);

    if (PreviousRamp != CurrentRamp) {
        if (ramp > 0) {
            SlopeTimer.SetRate(60);
            MovementSpeed = 1.0 - static_cast<double>(ramp) * 0.08;
            if (MovementSpeed < 0.3) {
                MovementSpeed = 0.3;
            }
        } else {
            MovementSpeed = 1.0;
        }
    }
}

// ============================================================================
// Movement_AI - Vehicle movement AI with terrain and slope awareness.
// ============================================================================

void DriveLocomotionClass::Movement_AI()
{
    if (!IsDriving) {
        return;
    }

    // 地表速度（水面非两栖会直接停车）。
    ApplyTerrainSpeed();
    if (!IsDriving) {
        return;
    }

    // 坡道降速。
    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    int32 slope = MapClass::Instance->GetCellSlope(currentCell);
    if (slope > 0) {
        SpeedPercentage *= 1.0f - static_cast<float>(slope) * 0.05f;
        if (SpeedPercentage < 0.2f) {
            SpeedPercentage = 0.2f;
        }
    }

    // 断桥处停车（后续转摇车分支）。
    if (Is_Moving_On_Bridge() && Is_Bridge_Destroyed()) {
        IsDriving = false;
        IsMoving = false;
    }
}

// ============================================================================
// Get_Status - Vehicle movement status code.
// ============================================================================

int32 DriveLocomotionClass::Get_Status() const
{
    if (IsRocking) {
        return static_cast<int32>(Sequence::Tumble);
    }
    if (IsRotating) {
        return static_cast<int32>(Sequence::Ready);
    }
    if (IsDriving) {
        return static_cast<int32>(Sequence::Walk);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Is_Really_Moving_Now - Vehicle is moving if actually driving.
// ============================================================================

bool DriveLocomotionClass::Is_Really_Moving_Now() const
{
    return IsDriving && !IsRotating && !IsRocking;
}

// ============================================================================
// Can_Fire - Vehicle can fire unless rotating (turret lock).
// ============================================================================

FireError DriveLocomotionClass::Can_Fire() const
{
    if (IsTurretLockedDown > 0) {
        return FireError::ROT;
    }
    if (IsRocking) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Force_Track - Forces the vehicle onto a specific track.
// ============================================================================

void DriveLocomotionClass::Force_Track(int32 track, CoordStruct coord)
{
    TrackNumber = static_cast<uint32>(track);
    TrackIndex = track;
    IsOnShortTrack = (track < 0);

    Dest = coord;
    Destination = coord;
    IsDriving = true;
    IsMoving = true;
    SpeedAccum = 0;
}

// ============================================================================
// Get_Track_Number - Returns the current track number.
// ============================================================================

int32 DriveLocomotionClass::Get_Track_Number() const
{
    return static_cast<int32>(TrackNumber);
}

// ============================================================================
// Get_Track_Index - Returns the current track index.
// ============================================================================

int32 DriveLocomotionClass::Get_Track_Index() const
{
    return TrackIndex;
}

// ============================================================================
// Lock / Unlock - Vehicle lock state management.
// ============================================================================

void DriveLocomotionClass::Lock()
{
    IsLocked = true;
    IsDriving = false;
    IsMoving = false;
}

void DriveLocomotionClass::Unlock()
{
    IsLocked = false;
}

// ============================================================================
// Tilt_Pitch_AI - Vehicle pitch adjustment for slopes.
// ============================================================================

void DriveLocomotionClass::Tilt_Pitch_AI()
{
    int32 slope = MapClass::Instance->GetCellSlope(CoordMath::CoordToCell(CurrentCoord));
    if (slope == 0) {
        return;
    }

    if (Owner) {
        int32 pitch = slope * 3;
        if (pitch > 20) {
            pitch = 20;
        }
        if (pitch < -20) {
            pitch = -20;
        }
        Owner->SetPitch(pitch);
    }
}