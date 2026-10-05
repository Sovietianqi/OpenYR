#include "TeleportLocomotionClass.h"
#include "../Map/MapClass.h"
#include "../Animations/AnimClass.h"
#include "../Rules/RulesClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/DamageArea.h"

#include <cstdlib>
#include <cmath>

// ============================================================================
// TeleportLocomotionClass - Chrono/teleport locomotion.
// Teleport animation (fade in/out), chrono shift effect, destination
// validation, terrain check, and chrono vortex chance on teleport.
// ============================================================================

TeleportLocomotionClass::TeleportLocomotionClass()
    : LocomotionClass()
    , IsTeleportingNow(false)
    , HasArrived(false)
    , WarpOutAnim(nullptr)
    , WarpInAnim(nullptr)
    , TargetCell(0, 0, 0)
    , WarpPhase(0)
    , WarpDelay(30)
{
    Speed = 0;
}

TeleportLocomotionClass::~TeleportLocomotionClass()
{
    WarpOutAnim = nullptr;
    WarpInAnim = nullptr;
}

HRESULT TeleportLocomotionClass::GetClassID(CLSID* pClassID)
{
    if (pClassID) {
        pClassID->Data1 = static_cast<uint32>(LocoID);
        pClassID->Data2 = 0;
        pClassID->Data3 = 0;
        for (int32 i = 0; i < 8; ++i) {
            pClassID->Data4[i] = 0;
        }
        return S_OK;
    }
    return E_FAIL;
}

int32 TeleportLocomotionClass::Size()
{
    return sizeof(TeleportLocomotionClass);
}

// ============================================================================
// Is_Moving - Teleport is "moving" during the teleportation sequence.
// ============================================================================

bool TeleportLocomotionClass::Is_Moving()
{
    return IsTeleportingNow;
}

// ============================================================================
// Destination - Returns the teleport destination.
// ============================================================================

CoordStruct TeleportLocomotionClass::Destination()
{
    return TargetCell;
}

// ============================================================================
// Move_To - Initiates teleport to the target coordinate.
// ============================================================================

void TeleportLocomotionClass::Move_To(CoordStruct to)
{
    TeleportTo(to);
}

// ============================================================================
// Stop_Moving - Cancels the teleport sequence.
// ============================================================================

void TeleportLocomotionClass::Stop_Moving()
{
    if (IsTeleportingNow && WarpPhase == 0) {
        IsTeleportingNow = false;
        IsMoving = false;
        HasArrived = false;
        WarpPhase = 0;
        TeleportTimer.Stop();
    }
}

// ============================================================================
// Do_Turn - Teleport turning is instant at the destination.
// ============================================================================

void TeleportLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        Owner->SetFacing(coord);
    }
}

// ============================================================================
// Process - Main teleport animation loop with three phases:
// Phase 0: Warp out (unit fades away)
// Phase 1: Warp transition (unit is invisible)
// Phase 2: Warp in (unit reappears at destination)
// ============================================================================

bool TeleportLocomotionClass::Process()
{
    if (!IsTeleportingNow) {
        return false;
    }

    switch (WarpPhase) {
        case 0:
            return ProcessWarpOut();
        case 1:
            return ProcessWarpTransition();
        case 2:
            return ProcessWarpIn();
        default:
            return false;
    }
}

// ============================================================================
// ProcessWarpOut - Phase 0: Unit fades out at origin.
// ============================================================================

bool TeleportLocomotionClass::ProcessWarpOut()
{
    TeleportTimer.Update();

    if (WarpOutAnim) {
        PlayWarpOutAnimation();
    }

    if (Owner) {
        float progress = 1.0f - static_cast<float>(TeleportTimer.GetTimeLeft())
            / static_cast<float>(WarpDelay);
        Owner->SetAlpha(static_cast<uint8>(255.0f * (1.0f - progress)));
    }

    if (TeleportTimer.Expired()) {
        WarpPhase = 1;
        Mark_All_Occupation_Bits(MarkType::Down);

        CurrentCoord = TargetCell;
        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetAlpha(0);
        }

        TeleportTimer.Start(WarpDelay);
    }

    return true;
}

// ============================================================================
// ProcessWarpTransition - Phase 1: Unit is in transit (invisible).
// ============================================================================

bool TeleportLocomotionClass::ProcessWarpTransition()
{
    TeleportTimer.Update();

    CheckChronoVortex();

    if (TeleportTimer.Expired()) {
        WarpPhase = 2;

        if (!IsValidDestination()) {
            RevertTeleport();
            return true;
        }

        TeleportTimer.Start(WarpDelay);
    }

    return true;
}

// ============================================================================
// ProcessWarpIn - Phase 2: Unit reappears at destination.
// ============================================================================

bool TeleportLocomotionClass::ProcessWarpIn()
{
    TeleportTimer.Update();

    if (WarpInAnim) {
        PlayWarpInAnimation();
    }

    if (Owner) {
        float progress = 1.0f - static_cast<float>(TeleportTimer.GetTimeLeft())
            / static_cast<float>(WarpDelay);
        Owner->SetAlpha(static_cast<uint8>(255.0f * progress));
    }

    if (TeleportTimer.Expired()) {
        HasArrived = true;
        IsTeleportingNow = false;
        IsMoving = false;
        WarpPhase = 0;

        if (Owner) {
            Owner->SetAlpha(255);
            Owner->SetCoords(TargetCell);
        }

        Mark_All_Occupation_Bits(MarkType::Up);
    }

    return true;
}

// ============================================================================
// IsValidDestination - Validates the teleport destination terrain.
// ============================================================================

bool TeleportLocomotionClass::IsValidDestination() const
{
    CellStruct cell = CoordMath::CoordToCell(TargetCell);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return false;
    }

    if (MapClass::Instance->IsCellOccupied(cell)) {
        return false;
    }

    return true;
}

// ============================================================================
// RevertTeleport - Reverts a failed teleport back to origin.
// ============================================================================

void TeleportLocomotionClass::RevertTeleport()
{
    IsTeleportingNow = false;
    IsMoving = false;
    HasArrived = false;
    WarpPhase = 0;

    if (Owner) {
        Owner->SetAlpha(255);
        CoordStruct originalPos = GetOriginalPosition();
        CurrentCoord = originalPos;
        Owner->SetCoords(originalPos);
    }
}

// ============================================================================
// GetOriginalPosition - Returns the position before teleport.
// ============================================================================

CoordStruct TeleportLocomotionClass::GetOriginalPosition() const
{
    return CurrentCoord;
}

// ============================================================================
// PlayWarpOutAnimation - Plays the chrono fade-out animation.
// ============================================================================

void TeleportLocomotionClass::PlayWarpOutAnimation()
{
    if (WarpOutAnim && Owner) {
        AnimClass* anim = GameCreate<AnimClass>();
        if (anim) {
            anim->Type = WarpOutAnim;
            anim->SetCoords(CurrentCoord);
            anim->SetOwner(Owner->GetOwningHouse());
        }
    }
}

// ============================================================================
// PlayWarpInAnimation - Plays the chrono fade-in animation.
// ============================================================================

void TeleportLocomotionClass::PlayWarpInAnimation()
{
    if (WarpInAnim && Owner) {
        AnimClass* anim = GameCreate<AnimClass>();
        if (anim) {
            anim->Type = WarpInAnim;
            anim->SetCoords(TargetCell);
            anim->SetOwner(Owner->GetOwningHouse());
        }
    }
}

// ============================================================================
// CheckChronoVortex - Random chance of creating a chrono vortex during
// teleport, which can destroy the unit or nearby objects.
// ============================================================================

void TeleportLocomotionClass::CheckChronoVortex()
{
    if (!Owner) {
        return;
    }

    int32 vortexChance = RulesClass::Instance->GetChronoVortexChance();
    if (vortexChance <= 0) {
        return;
    }

    int32 roll = rand() % 100;
    if (roll < vortexChance) {
        SpawnChronoVortex();
    }
}

// ============================================================================
// SpawnChronoVortex - Creates a chrono vortex at the destination.
// ============================================================================

void TeleportLocomotionClass::SpawnChronoVortex()
{
    if (!Owner) {
        return;
    }

    CoordStruct vortexPos = TargetCell;

    int32 damage = RulesClass::Instance->GetChronoVortexDamage();
    int32 radius = RulesClass::Instance->GetChronoVortexRadius();

    DamageArea damageArea;
    damageArea.X = vortexPos.X;
    damageArea.Y = vortexPos.Y;
    damageArea.Z = vortexPos.Z;
    damageArea.Damage = damage;
    damageArea.Range = radius;
    damageArea.Warhead = WarheadTypeClass::GetDefault();

    MapClass::Instance->ApplyDamageArea(damageArea);

    if (Owner) {
        Owner->TakeDamage(damage, nullptr, WarheadTypeClass::GetDefault());
    }

    IsTeleportingNow = false;
    IsMoving = false;
    HasArrived = false;
    WarpPhase = 0;
}

// ============================================================================
// Mark_All_Occupation_Bits - Teleport occupation marking.
// ============================================================================

void TeleportLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (IsTeleportingNow) {
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    bool occupy = (mark == MarkType::Up);
    MapClass::Instance->MarkCellOccupied(cell, occupy);
}

// ============================================================================
// Limbo - Teleport limbo state.
// ============================================================================

void TeleportLocomotionClass::Limbo()
{
    IsTeleportingNow = false;
    IsMoving = false;
    HasArrived = false;
    WarpPhase = 0;
    TeleportTimer.Stop();
    Mark_All_Occupation_Bits(MarkType::Down);
}

// ============================================================================
// TeleportTo - Initiates a teleport to a target coordinate.
// ============================================================================

void TeleportLocomotionClass::TeleportTo(CoordStruct coord)
{
    if (IsTeleportingNow) {
        return;
    }

    if (!IsValidDestinationForCoord(coord)) {
        CoordStruct nearest = GetClosestOkCell(coord);
        if (nearest == CoordStruct(0, 0, 0)) {
            return;
        }
        coord = nearest;
    }

    TargetCell = coord;
    IsTeleportingNow = true;
    IsMoving = true;
    HasArrived = false;
    WarpPhase = 0;
    TeleportTimer.Start(WarpDelay);
}

// ============================================================================
// IsValidDestinationForCoord - Validates a specific coordinate for teleport.
// ============================================================================

bool TeleportLocomotionClass::IsValidDestinationForCoord(CoordStruct coord) const
{
    CellStruct cell = CoordMath::CoordToCell(coord);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return false;
    }

    return true;
}

// ============================================================================
// IsTeleporting - Returns true if a teleport is in progress.
// ============================================================================

bool TeleportLocomotionClass::IsTeleporting() const
{
    return IsTeleportingNow;
}

// ============================================================================
// ChronoWarpTo - Chrono warp variant of teleport.
// ============================================================================

void TeleportLocomotionClass::ChronoWarpTo(CoordStruct coord)
{
    TeleportTo(coord);
}

// ============================================================================
// GetWarpPhase - Returns the current warp phase.
// ============================================================================

int32 TeleportLocomotionClass::GetWarpPhase() const
{
    return WarpPhase;
}

// ============================================================================
// Get_Status - Teleport movement status code.
// ============================================================================

int32 TeleportLocomotionClass::Get_Status() const
{
    if (IsTeleportingNow) {
        if (WarpPhase == 0) {
            return static_cast<int32>(Sequence::Deploy);
        }
        if (WarpPhase == 2) {
            return static_cast<int32>(Sequence::Undeploy);
        }
        return static_cast<int32>(Sequence::Ready);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Can_Fire - Cannot fire while teleporting.
// ============================================================================

FireError TeleportLocomotionClass::Can_Fire() const
{
    if (IsTeleportingNow) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Is_Really_Moving_Now - Teleport is "moving" during warp.
// ============================================================================

bool TeleportLocomotionClass::Is_Really_Moving_Now() const
{
    return IsTeleportingNow;
}
// 传送面板的默认登记点（复位槽位写回该值）。
static CoordStruct TeleportLocomotionClass_Default_Pos(0, 0, 0);

bool TeleportLocomotionClass::ILocomotion_718080() const
{
    // 根据游戏行为，可知该槽位判定传送状态机是否处于传送中。
    return IsTeleportingNow;
}

void TeleportLocomotionClass::ILocomotion_7180A0(CoordStruct* pOut) const
{
    // 根据游戏行为，可知取坐标分两路：传送中给登记的目标点，否则给
    // 载体的现行坐标。
    if (!pOut)
        return;
    if (IsTeleportingNow)
        *pOut = TargetCell;
    else if (LinkedTo)
        LinkedTo->GetCoords(pOut);
    else
        *pOut = CoordStruct(0, 0, 0);
}

void TeleportLocomotionClass::ILocomotion_718100(const CoordStruct& to)
{
    // 根据游戏行为，可知传送起步：先对载体做许可检查（滞留虚位、阵亡、
    // 身处封闭载具任一命中即放弃）；目标格被占用时原版会先驱散占位内容，
    // 重构的占用位模型以拒绝进入表达同一约束；随后登记目标点并归零
    // 传送相位。
    if (!LinkedTo)
        return;
    if (LinkedTo->IsInLimbo || LinkedTo->IsDead() || LinkedTo->InOpenTopped)
        return;
    CellClass* pCell = TheMap->GetCellAt(to.X >> 8, to.Y >> 8);
    if (pCell && pCell->IsOccupied())
        return;
    TargetCell = to;
    IsTeleportingNow = true;
    WarpPhase = 0;
}

void TeleportLocomotionClass::ILocomotion_718230()
{
    // 根据游戏行为，可知复位把登记点归回默认值并清两个状态旗标。
    TargetCell = TeleportLocomotionClass_Default_Pos;
    IsTeleportingNow = false;
    HasArrived = false;
}

void TeleportLocomotionClass::ILocomotion_7192C0(DirStruct dir)
{
    // 根据游戏行为，可知转向槽位把期望朝向写进载体的朝向机。
    if (LinkedTo)
        LinkedTo->SetFacing(dir);
}

int32 TeleportLocomotionClass::ILocomotion_719E20() const
{
    // 根据游戏行为，可知该槽位固定返回 2（传送移动的组别值）。
    return 2;
}

void TeleportLocomotionClass::ILocomotion_71A090(CoordStruct* pCoord)
{
    // 根据游戏行为，可知放置槽位只在未带坐标参数时动作：把当前登记点
    // 回写给载体位置；带坐标参数时直接返回。
    if (pCoord)
        return;
    if (LinkedTo)
        LinkedTo->SetLocation(TargetCell);
}

HRESULT TeleportLocomotionClass::ILocomotion_71A160(REFIID iid, void** ppvObject)
{
    // 根据游戏行为，可知 COM 三槽是本体接口的转发形态。
    return QueryInterface(iid, ppvObject);
}

ULONG TeleportLocomotionClass::ILocomotion_71A170()
{
    return AddRef();
}

ULONG TeleportLocomotionClass::ILocomotion_71A180()
{
    return Release();
}
