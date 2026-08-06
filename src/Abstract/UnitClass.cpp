// =============================================================================
// UnitClass.cpp - Vehicle unit implementation
//
// Implements all vehicle unit behavior including harvesting, turret rotation,
// deploy/undeploy transitions, carryall transport, iron curtain immunity,
// and combat-related state management.
// =============================================================================

#include <Abstract/UnitClass.h>
#include <Abstract/UnitTypeClass.h>
#include <Combat/WeaponTypeClass.h>
#include <Combat/WarheadTypeClass.h>
#include <Houses/HouseClass.h>
#include <Rules/RulesClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Game/Game.h>
#include <Math/CoordStruct.h>
#include <Math/Facing.h>

#include <cmath>
#include <cstdlib>

// =============================================================================
// Constants
// =============================================================================
static const int32 HARVEST_CAPACITY_DEFAULT = 500;
static const int32 HARVEST_RATE_DEFAULT = 25;
static const int32 TURRET_ROT_SPEED_DEFAULT = 10;
static const int32 IRON_CURTAIN_DURATION = 750;
static const int32 FORCE_SHIELD_DURATION = 500;
static const int32 DEPLOY_ANIMATION_FRAMES = 30;
static const int32 CARRYALL_PICKUP_HEIGHT = 1000;
static const int32 CARRYALL_CRUISE_HEIGHT = 2000;

// =============================================================================
// Static member definitions
// =============================================================================
DynamicVectorClass<UnitClass*>* UnitClass::Array = nullptr;

// =============================================================================
// Constructor
// =============================================================================
UnitClass::UnitClass(HouseClass* pOwner) noexcept
    : FootClass()
    , Type(nullptr)
    , TurretDir()
    , BarrelDir()
    , TurretRotation(0)
    , BarrelRotation(0)
    , IsHarvestingTiberium(false)
    , IsDumpingTiberium(false)
    , IsShowingCrate(false)
    , IsMCV(false)
    , IsHarvester(false)
    , IsAPC(false)
    , IsDeployer(false)
    , IsChrono(false)
    , IsBombing(false)
    , IsUnderground(false)
    , IsSubterranean(false)
    , IsButtMissile(false)
    , IsSpace(false)
    , IsTank(false)
    , IsWalker(false)
    , IsAntiAir(false)
    , IsOpenTopped(false)
    , IsCarryall(false)
    , IsCarryAllFly(false)
    , IsJumpJet(false)
    , HarvestAmount(0)
    , HarvestRate(HARVEST_RATE_DEFAULT)
    , TotalTiberiumValue(0)
    , FlagHouseIndex(-1)
    , Deployed(false)
    , Deploying(false)
    , Undeploying(false)
    , Unloading(false)
    , DeathFrameCounter(0)
    , NonPassengerCount(0)
    , HasFollowerCar(false)
    , FollowerCar(nullptr)
    , unknown_7E0(0), unknown_7E4(0), unknown_7E8(0), unknown_7EC(0)
    , unknown_7F0(0), unknown_7F4(0), unknown_7F8(0), unknown_7FC(0)
    , unknown_800(0), unknown_804(0), unknown_808(0), unknown_80C(0)
    , unknown_810(0), unknown_814(0), unknown_818(0), unknown_81C(0)
    , unknown_820(0), unknown_824(0), unknown_828(0), unknown_82C(0)
    , unknown_830(0), unknown_834(0), unknown_838(0), unknown_83C(0)
    , unknown_840(0), unknown_844(0), unknown_848(0), unknown_84C(0)
    , unknown_850(0), unknown_854(0), unknown_858(0), unknown_85C(0)
    , unknown_860(0), unknown_864(0), unknown_868(0), unknown_86C(0)
    , unknown_870(0), unknown_874(0), unknown_878(0), unknown_87C(0)
    , unknown_880(0), unknown_884(0), unknown_888(0), unknown_88C(0)
    , unknown_890(0), unknown_894(0), unknown_898(0), unknown_89C(0)
    , unknown_8A0(0), unknown_8A4(0), unknown_8A8(0), unknown_8AC(0)
    , unknown_8B0(0), unknown_8B4(0), unknown_8B8(0), unknown_8BC(0)
    , unknown_8C0(0), unknown_8C4(0), unknown_8C8(0), unknown_8CC(0)
    , unknown_8D0(0), unknown_8D4(0), unknown_8D8(0), unknown_8DC(0)
    , unknown_8E0(0), unknown_8E4(0), unknown_8E8(0), unknown_8EC(0)
{
    Owner = pOwner;

    // Apply type-derived flags if a type is assigned before construction.
    if (Type) {
        IsHarvester = Type->Harvester;
        IsMCV = Type->IsSimpleDeployer;
        IsAPC = (Type->Passengers > 0);
        IsDeployer = Type->HasDeployer;
        IsChrono = Type->IsChrono;
        IsCarryall = Type->IsCarryall;
        IsTank = !Type->IsBalloonHover;
        IsJumpJet = Type->IsBalloonHover;
        IsSubterranean = Type->IsTeleporter;
    }

    if (Array) {
        Array->Add(this);
    }
}

// =============================================================================
// Destructor
// =============================================================================
UnitClass::~UnitClass()
{
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

// =============================================================================
// IPersistStream
// =============================================================================
HRESULT __stdcall UnitClass::Load(IStream* pStm)
{
    if (!pStm) return E_POINTER;
    return FootClass::Load(pStm);
}

HRESULT __stdcall UnitClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (!pStm) return E_POINTER;
    return FootClass::Save(pStm, fClearDirty);
}

// =============================================================================
// TechnoClass overrides
// =============================================================================
bool UnitClass::IsVoxel() const
{
    if (!Type) return false;
    return Type->IsVoxel;
}

void UnitClass::Destroyed(ObjectClass* Killer)
{
    IsHarvestingTiberium = false;
    IsDumpingTiberium = false;

    // Clear turret and barrel state.
    TurretRotation = 0;
    BarrelRotation = 0;

    // Remove iron curtain / force shield effects.
    IronCurtainTimer = 0;
    ForceShieldTimer = 0;

    // If this is a carryall with passengers, release them.
    if (IsCarryall) {
        IsCarryAllFly = false;
    }
}

bool UnitClass::CanScatter() const
{
    // Harvesters and MCVs cannot scatter while performing their tasks.
    if (IsHarvester && IsHarvestingTiberium) return false;
    if (IsMCV && IsDeployer) return false;
    return true;
}

int32 UnitClass::GetDefaultSpeed() const
{
    if (!Type) return 0;
    return Type->Speed;
}

bool UnitClass::HasTurret() const
{
    if (!Type) return false;
    return Type->HasTurret;
}

bool UnitClass::CanDeploySlashUnload() const
{
    if (!Type) return false;
    return Type->HasDeployer || Type->HasUndeployer;
}

bool UnitClass::IsUnitFactory() const
{
    if (!Type) return false;
    return Type->Factory != AbstractType::None;
}

// =============================================================================
// Harvesting subsystem
// =============================================================================
bool UnitClass::CanHarvest() const
{
    if (!Type) return false;
    return Type->Harvester || Type->ResourceGatherer;
}

bool UnitClass::IsHarvesting() const
{
    return IsHarvestingTiberium;
}

int32 UnitClass::GetTiberiumLoad() const
{
    return HarvestAmount;
}

float UnitClass::GetTiberiumValue() const
{
    return static_cast<float>(TotalTiberiumValue);
}

bool UnitClass::IsHarvestingTooMuch() const
{
    // The harvester is full when it reaches its capacity.
    int32 capacity = HARVEST_CAPACITY_DEFAULT;
    if (Type && Type->Passengers > 0) {
        capacity = Type->Passengers * 100;
    }
    return HarvestAmount >= capacity;
}

void UnitClass::StartHarvesting()
{
    if (!CanHarvest()) return;
    if (IsHarvestingTooMuch()) return;

    IsHarvestingTiberium = true;
    IsDumpingTiberium = false;
}

void UnitClass::StopHarvesting()
{
    IsHarvestingTiberium = false;
    IsDumpingTiberium = false;
}

void UnitClass::HarvestTiberium()
{
    if (!IsHarvestingTiberium) return;
    if (!CanHarvest()) return;

    // Check capacity before harvesting more.
    if (IsHarvestingTooMuch()) {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
        return;
    }

    // Extract tiberium from the current cell.
    CoordStruct currentPos = GetCoords();
    CellStruct cellPos = Math::CoordToCell(currentPos);

    // The harvest rate determines how much tiberium is gathered per cycle.
    int32 amount = HarvestRate;
    if (amount <= 0) amount = HARVEST_RATE_DEFAULT;

    HarvestAmount += amount;
    TotalTiberiumValue += amount;

    // When full, stop harvesting and start heading to a refinery.
    if (IsHarvestingTooMuch()) {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
    }
}

void UnitClass::EnterTiberiumField()
{
    if (!CanHarvest()) return;

    // Only start harvesting if we have capacity remaining.
    if (!IsHarvestingTooMuch()) {
        StartHarvesting();
    }
}

// =============================================================================
// Drawing subsystem
// =============================================================================
void UnitClass::DrawAsVXL(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint)
{
    if (!Type || !IsVoxel()) return;

    // Voxel rendering uses the type's voxel model data with current facing.
    DirStruct facing = PrimaryFacing;
    int32 frame = static_cast<int32>(TurretRotation);

    // Adjust turret direction for rendering based on body facing.
    if (HasTurret()) {
        TurretDir = facing;
    }

    (void)Coords; (void)BoundingRect; (void)Brightness; (void)Tint; (void)frame;
}

void UnitClass::DrawAsSHP(Point2D Coords, RectangleStruct BoundingRect, int32 Brightness, int32 Tint)
{
    if (!Type) return;
    if (IsVoxel()) return; // voxel units do not use SHP rendering

    // SHP rendering uses the current sequence frame.
    Sequence seq = GetSequence();
    int32 frameIndex = static_cast<int32>(seq);

    // Apply brightness and tint for night/fog rendering.
    int32 adjustedBrightness = Brightness;
    if (IsCloaked()) {
        adjustedBrightness = adjustedBrightness / 2;
    }

    (void)Coords; (void)BoundingRect; (void)Tint; (void)frameIndex; (void)adjustedBrightness;
}

void UnitClass::DrawObject(Surface* pSurface, Point2D Coords, RectangleStruct CacheRect, int32 Brightness, int32 Tint)
{
    if (!pSurface || !Type) return;

    // Delegate to VXL or SHP based on the type's rendering mode.
    if (IsVoxel()) {
        DrawAsVXL(Coords, CacheRect, Brightness, Tint);
    } else {
        DrawAsSHP(Coords, CacheRect, Brightness, Tint);
    }
}

void UnitClass::Draw(Point2D& point, RectangleStruct& rect)
{
    if (!Type) return;

    // Draw the unit at the given screen position within the bounding rect.
    CoordStruct worldPos = GetCoords();
    if (Deployed) {
        // Deployed units use a stationary render sequence.
        SetSequence(Sequence::DeployedIdle);
    } else if (IsMoving()) {
        SetSequence(Sequence::Walk);
    } else {
        SetSequence(Sequence::Ready);
    }

    (void)point; (void)rect; (void)worldPos;
}

void UnitClass::DrawShadow(Point2D& point)
{
    if (!Type) return;

    // Draw a shadow ellipse beneath the unit at the given screen position.
    // Air units (carryall in flight, jumpjet) cast a smaller, offset shadow.
    if (IsCarryAllFly || IsJumpJet) {
        point.Y += 15; // offset shadow downward for elevated units
    }

    (void)point;
}

int32 UnitClass::GetZBias() const
{
    // Ground units have a Z-bias of 0; air units are elevated.
    if (IsCarryAllFly || IsJumpJet) return 1000;
    if (IsUnderground) return -500;
    return 0;
}

// =============================================================================
// State / deactivation
// =============================================================================
bool UnitClass::IsDeactivated() const
{
    if (IsInLimbo) return true;
    if (Health <= 0) return true;
    if (unknown_7FC > 0) return true; // DisableCount
    return false;
}

// =============================================================================
// Update subsystems
// =============================================================================
void UnitClass::UpdateTube()
{
    if (!IsSubterranean) return;

    // Subterranean units update their underground tunnel position.
    if (IsUnderground) {
        DeathFrameCounter++; // reuse as underground frame timer
        if (DeathFrameCounter > 60) {
            // Tunnel collapse or surface after timeout.
            IsUnderground = false;
            DeathFrameCounter = 0;
        }
    }
}

void UnitClass::UpdateRotation()
{
    if (!HasTurret()) return;

    // Rotate the turret toward the primary facing direction.
    DirStruct currentFacing = TurretDir;
    DirStruct targetFacing = PrimaryFacing;

    int32 current = static_cast<int32>(currentFacing.Value);
    int32 target = static_cast<int32>(targetFacing.Value);
    int32 diff = target - current;

    // Normalize to [-128, 127] for shortest rotation.
    if (diff > 128) diff -= 256;
    if (diff < -128) diff += 256;

    int32 rotSpeed = TURRET_ROT_SPEED_DEFAULT;
    if (Type && Type->ROT > 0) {
        rotSpeed = Type->ROT;
    }

    if (diff > 0) {
        if (diff > rotSpeed) diff = rotSpeed;
        TurretRotation += diff;
    } else if (diff < 0) {
        if (diff < -rotSpeed) diff = -rotSpeed;
        TurretRotation += diff;
    }

    TurretDir = targetFacing;

    // Also update barrel elevation to follow turret.
    int32 targetPitch = Pitch;
    int32 barrelDiff = targetPitch - BarrelRotation;
    int32 barrelSpeed = TURRET_ROT_SPEED_DEFAULT / 2;
    if (barrelDiff > barrelSpeed) barrelDiff = barrelSpeed;
    if (barrelDiff < -barrelSpeed) barrelDiff = -barrelSpeed;
    BarrelRotation += barrelDiff;
    BarrelDir = TurretDir;
}

void UnitClass::UpdateEdgeOfWorld()
{
    // Check if the unit has wandered off the map edge.
    CoordStruct pos = GetCoords();
    if (pos.X < 0 || pos.Y < 0) {
        // Unit is outside the map; turn it around.
        Stop();
        return;
    }

    // Check map bounds through the map instance.
    if (MapClass::Instance) {
        if (!MapClass::Instance->IsWithinUsableArea(pos)) {
            Stop();
        }
    }
}

void UnitClass::UpdateFiring()
{
    // Decrement fire recharge timer each frame.
    if (FireRechargeTimer > 0) {
        FireRechargeTimer--;
    }

    // Update spark/fire effects if active.
    if (FireDamageTimer > 0) {
        FireDamageTimer--;
    }
    if (SparkyCounter > 0) {
        SparkyCounter--;
    }

    // Update muzzle flash state.
    if (unknown_810 > 0) {
        unknown_810--;
    }
}

void UnitClass::UpdateVisceroid()
{
    // Visceroids are special units that spawn from tiberium.
    // This updates their movement and splitting behavior.
    if (!IsButtMissile) return;

    // Visceroids wander randomly and split when damaged enough.
    if (Health < MaxHealth / 2) {
        // Mark for potential splitting on next AI cycle.
        unknown_814 = 1;
    }
}

void UnitClass::UpdateDisguise()
{
    if (!IsDisguised()) return;

    // Update disguise state: if the unit fires or takes damage, reveal.
    if (FireRechargeTimer > 0 || FireDamageTimer > 0) {
        UnDisguise();
    }
}

void UnitClass::Explode()
{
    // Trigger the death explosion for this unit.
    DeathFrameCounter = 0;

    // Set the death sequence based on unit type.
    if (IsTank || IsWalker) {
        SetSequence(Sequence::Die1);
    } else if (IsCarryall || IsJumpJet) {
        SetSequence(Sequence::DieFly);
    } else {
        SetSequence(Sequence::Die2);
    }

    // If the unit has a death weapon, arm it.
    if (Type && Type->DeathWeaponIndex >= 0) {
        unknown_818 = static_cast<DWORD>(Type->DeathWeaponIndex);
    }
}

// =============================================================================
// Deploy / harvest / flag
// =============================================================================
bool UnitClass::GotoClearSpot()
{
    if (!Type) return false;

    // Find a nearby cell that is unoccupied for deployment or unloading.
    CoordStruct currentPos = GetCoords();
    CellStruct currentCell = Math::CoordToCell(currentPos);

    // Search in a spiral around the current position.
    for (int32 radius = 1; radius <= 5; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (abs(dx) != radius && abs(dy) != radius) continue;
                CellStruct testCell(static_cast<int32>(currentCell.X + dx), static_cast<int32>(currentCell.Y + dy));
                if (MapClass::Instance && MapClass::Instance->IsWithinUsableArea(testCell.X, testCell.Y)) {
                    if (!MapClass::Instance->IsCellOccupied(testCell)) {
                        CoordStruct dest = Math::CellToCoord(testCell);
                        SetDestination(dest);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool UnitClass::TryToDeploy()
{
    if (!CanDeploy()) return false;
    if (Deploying) return false;
    if (Deployed) return false;

    Deploying = true;
    Undeploying = false;
    unknown_7E0 = 0; // reset deploy animation frame counter
    SetSequence(Sequence::Deploy);
    return true;
}

void UnitClass::Deploy()
{
    Deploying = false;
    Deployed = true;
    Undeploying = false;
    SetSequence(Sequence::DeployedIdle);
    unknown_7E0 = 0;
}

void UnitClass::Undeploy()
{
    if (!Deployed) return;

    Undeploying = true;
    Deploying = false;
    Deployed = false;
    SetSequence(Sequence::Undeploy);
    unknown_7E0 = 0;
}

bool UnitClass::Harvesting()
{
    if (!IsHarvestingTiberium) return false;
    if (!CanHarvest()) return false;

    // Check if at a tiberium field and harvest.
    CoordStruct currentPos = GetCoords();
    CellStruct cellPos = Math::CoordToCell(currentPos);

    // Verify the cell has tiberium.
    if (MapClass::Instance) {
        LandType land = MapClass::Instance->GetLandType(cellPos);
        if (land != LandType::Tiberium && land != LandType::Weeds) {
            // No tiberium here; stop harvesting.
            IsHarvestingTiberium = false;
            return false;
        }
    }

    // Perform the harvest extraction.
    int32 amount = HarvestRate;
    if (amount <= 0) amount = HARVEST_RATE_DEFAULT;

    HarvestAmount += amount;
    TotalTiberiumValue += amount;

    if (IsHarvestingTooMuch()) {
        IsHarvestingTiberium = false;
        IsDumpingTiberium = true;
    }

    return true;
}

bool UnitClass::FlagAttach(int32 nHouseIdx)
{
    if (FlagHouseIndex >= 0) return false;
    if (nHouseIdx < 0) return false;
    FlagHouseIndex = nHouseIdx;
    return true;
}

bool UnitClass::FlagRemove()
{
    if (FlagHouseIndex < 0) return false;
    FlagHouseIndex = -1;
    return true;
}

// =============================================================================
// APC door
// =============================================================================
void UnitClass::APCCloseDoor()
{
    if (!IsAPC) return;
    // Close the APC door for passenger loading/unloading.
    SetSequence(Sequence::Ready);
    Unloading = false;
}

void UnitClass::APCOpenDoor()
{
    if (!IsAPC) return;
    // Open the APC door to allow passengers to exit.
    SetSequence(Sequence::Unload);
    Unloading = true;
}

// =============================================================================
// Combat decisions
// =============================================================================
bool UnitClass::ShouldCrashIt(TechnoClass* pTarget)
{
    if (!pTarget) return false;
    if (!Type) return false;

    // Air units should be shot down by anti-air weapons.
    if (pTarget->IsInAir()) {
        return IsAntiAir;
    }

    // Ground units can always be targeted unless they are allies.
    if (Is_Ally(pTarget)) return false;

    return true;
}

AbstractClass* UnitClass::AssignDestination(AbstractClass* pTarget)
{
    if (!pTarget) return nullptr;

    // Convert the target into a destination coordinate for movement.
    CoordStruct destCoord;
    pTarget->GetCoords(&destCoord);
    SetDestination(destCoord);

    return pTarget;
}

bool UnitClass::AStarAttempt(const CellStruct& cell1, const CellStruct& cell2)
{
    // Attempt A* pathfinding between two cells.
    if (!MapClass::Instance) return false;

    // Check if both cells are valid and passable.
    if (!MapClass::Instance->IsWithinUsableArea(cell1.X, cell1.Y)) return false;
    if (!MapClass::Instance->IsWithinUsableArea(cell2.X, cell2.Y)) return false;

    // Simple heuristic: check if a straight-line path is clear.
    int32 dx = cell2.X - cell1.X;
    int32 dy = cell2.Y - cell1.Y;
    int32 steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);

    for (int32 i = 0; i <= steps; ++i) {
        int16 x = static_cast<int16>(cell1.X + (dx * i) / (steps > 0 ? steps : 1));
        int16 y = static_cast<int16>(cell1.Y + (dy * i) / (steps > 0 ? steps : 1));
        CellStruct stepCell(x, y);
        if (MapClass::Instance->IsCellOccupied(stepCell)) {
            return false; // Path blocked
        }
    }

    return true; // Path is clear
}

// =============================================================================
// Mouse interaction
// =============================================================================
Action UnitClass::MouseOverCell(CellStruct const* pCell, bool checkFog, bool ignoreForce) const
{
    if (!pCell) return Action::None;

    // Determine what action the player can perform on this cell.
    if (!Type) return Action::None;

    // Check if the cell is valid.
    if (MapClass::Instance && !MapClass::Instance->IsWithinUsableArea(pCell->X, pCell->Y)) {
        return Action::NoMove;
    }

    // If checking fog, verify the cell is visible.
    if (checkFog && MapClass::Instance) {
        CellClass* pCellObj = MapClass::Instance->GetCellAt(*pCell);
        if (pCellObj && !pCellObj->Is_Visible()) {
            return Action::None;
        }
    }

    (void)ignoreForce;

    // Default: the unit can move to this cell.
    if (CanDeploy() && Deployed) {
        return Action::NoDeploy; // Already deployed, cannot move
    }

    return Action::Move;
}

Action UnitClass::MouseOverObject(ObjectClass const* pObject, bool ignoreForce) const
{
    if (!pObject) return Action::None;
    if (!Type) return Action::None;

    // Determine action based on the target object type.
    if (pObject == this) {
        return Action::Select;
    }

    // Check if the target is an enemy.
    auto pTechno = const_cast<ObjectClass*>(pObject);
    if (Is_Enemy(pTechno->Get_Owner())) {
        return Action::Attack;
    }

    // Check if the target is a friendly structure the unit can enter.
    if (Is_Ally(pTechno->Get_Owner())) {
        return Action::Enter;
    }

    (void)ignoreForce;

    return Action::None;
}

// =============================================================================
// Occupation bits
// =============================================================================
void UnitClass::MarkAllOccupationBits(const CoordStruct& coords)
{
    // Mark all cells occupied by this unit at the given coordinates.
    CellStruct cell = Math::CoordToCell(coords);
    if (MapClass::Instance) {
        MapClass::Instance->MarkCellOccupied(cell, true);

        // For large units, mark adjacent cells too.
        if (Type && Type->SizeLimit > 1) {
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y)), true);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X), static_cast<int32>(cell.Y + 1)), true);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y + 1)), true);
        }
    }
}

void UnitClass::UnmarkAllOccupationBits(const CoordStruct& coords)
{
    // Unmark all cells previously occupied by this unit.
    CellStruct cell = Math::CoordToCell(coords);
    if (MapClass::Instance) {
        MapClass::Instance->MarkCellOccupied(cell, false);

        if (Type && Type->SizeLimit > 1) {
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y)), false);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X), static_cast<int32>(cell.Y + 1)), false);
            MapClass::Instance->MarkCellOccupied(CellStruct(static_cast<int32>(cell.X + 1), static_cast<int32>(cell.Y + 1)), false);
        }
    }
}

// =============================================================================
// Target management
// =============================================================================
void UnitClass::SetTarget(AbstractClass* pTarget)
{
    // Store the target pointer across two adjacent DWORD fields.
    uintptr_t ptrVal = reinterpret_cast<uintptr_t>(pTarget);
    unknown_7E8 = static_cast<DWORD>(ptrVal);
    unknown_7EC = static_cast<DWORD>(ptrVal >> 32);
}

AbstractClass* UnitClass::GetTarget() const
{
    uintptr_t ptrVal = static_cast<uintptr_t>(unknown_7E8) |
                       (static_cast<uintptr_t>(unknown_7EC) << 32);
    return reinterpret_cast<AbstractClass*>(ptrVal);
}

void UnitClass::ClearTarget()
{
    unknown_7E8 = 0;
    unknown_7EC = 0;
}

bool UnitClass::HasTarget() const
{
    return unknown_7E8 != 0 || unknown_7EC != 0;
}

// =============================================================================
// Mission management
// =============================================================================
void UnitClass::SetMission(Mission mission)
{
    unknown_7F0 = static_cast<DWORD>(mission);

    // Apply mission-specific sequence changes.
    switch (mission) {
        case Mission::Sleep:
            SetSequence(Sequence::Ready);
            break;
        case Mission::Move:
            SetSequence(Sequence::Walk);
            break;
        case Mission::Guard:
        case Mission::AreaGuard:
            SetSequence(Sequence::Guard);
            break;
        case Mission::Harvest:
            SetSequence(Sequence::Harvest);
            break;
        case Mission::Unload:
            SetSequence(Sequence::Unload);
            break;
        default:
            break;
    }
}

Mission UnitClass::GetMission() const
{
    return static_cast<Mission>(unknown_7F0);
}

void UnitClass::QueueMission(Mission mission)
{
    unknown_7F4 = static_cast<DWORD>(mission);
}

// =============================================================================
// Mission implementations
// =============================================================================
void UnitClass::MissionAttack()
{
    SetMission(Mission::Attack);
    if (HasTarget()) {
        AbstractClass* pTarget = GetTarget();
        if (pTarget) {
            CoordStruct targetPos;
            pTarget->GetCoords(&targetPos);
            SetDestination(targetPos);
        }
    }
}

void UnitClass::MissionMove()
{
    SetMission(Mission::Move);
    SetSequence(Sequence::Walk);
}

void UnitClass::MissionGuard()
{
    SetMission(Mission::Guard);
    SetSequence(Sequence::Guard);
}

void UnitClass::MissionSleep()
{
    SetMission(Mission::Sleep);
    SetSequence(Sequence::Ready);
}

void UnitClass::MissionHunt()
{
    SetMission(Mission::Hunt);
    // Hunting units actively seek out enemies.
    SetSequence(Sequence::Walk);
}

void UnitClass::MissionReturn()
{
    SetMission(Mission::Return);
    // Return to a refinery (harvester) or base.
    if (IsHarvester && IsDumpingTiberium) {
        SetSequence(Sequence::Walk);
    }
}

void UnitClass::MissionStop()
{
    SetMission(Mission::Stop);
    Stop();
    SetSequence(Sequence::Ready);
}

void UnitClass::MissionHarvest()
{
    SetMission(Mission::Harvest);
    if (CanHarvest() && !IsHarvestingTooMuch()) {
        StartHarvesting();
        SetSequence(Sequence::Harvest);
    }
}

void UnitClass::MissionUnload()
{
    SetMission(Mission::Unload);
    if (IsAPC && NonPassengerCount > 0) {
        APCOpenDoor();
        SetSequence(Sequence::Unload);
    }
    if (IsHarvester && HarvestAmount > 0) {
        IsDumpingTiberium = true;
        HarvestAmount = 0;
        TotalTiberiumValue = 0;
    }
}

void UnitClass::MissionEnter()
{
    SetMission(Mission::Enter);
    SetSequence(Sequence::Enter);
}

void UnitClass::MissionPatrol()
{
    SetMission(Mission::Patrol);
    SetSequence(Sequence::Walk);
}

void UnitClass::MissionAreaGuard()
{
    SetMission(Mission::AreaGuard);
    SetSequence(Sequence::Guard);
}

void UnitClass::UpdateMission()
{
    Mission currentMission = GetMission();

    // Process the queued mission if current is complete.
    if (unknown_7F4 != 0) {
        Mission queued = static_cast<Mission>(unknown_7F4);
        if (currentMission != queued) {
            SetMission(queued);
            unknown_7F4 = 0;
        }
    }

    // Update deploy animation timer.
    if (Deploying) {
        if (unknown_7E0 < static_cast<DWORD>(DEPLOY_ANIMATION_FRAMES)) {
            unknown_7E0++;
        } else {
            Deploy();
        }
    } else if (Undeploying) {
        if (unknown_7E0 < static_cast<DWORD>(DEPLOY_ANIMATION_FRAMES)) {
            unknown_7E0++;
        } else {
            Undeploying = false;
            SetSequence(Sequence::Ready);
            unknown_7E0 = 0;
        }
    }
}

// =============================================================================
// AI updates
// =============================================================================
void UnitClass::AI_Update()
{
    if (IsInLimbo) return;
    if (Health <= 0) return;

    // Update all subsystems.
    UpdateRotation();
    UpdateFiring();
    UpdateMission();
    UpdateDisguise();

    // Update iron curtain / force shield timers.
    if (IronCurtainTimer > 0) {
        IronCurtainTimer--;
    }
    if (ForceShieldTimer > 0) {
        ForceShieldTimer--;
    }

    // Update EMP timer.
    if (unknown_7E4 > 0) {
        unknown_7E4--;
    }

    // Update temporal timer.
    if (TemporalTimer > 0) {
        TemporalTimer--;
    }

    // Update carryall flight.
    if (IsCarryall) {
        if (IsCarryAllFly) {
            CoordStruct currentPos = GetCoords();
            if (currentPos.Z < CARRYALL_PICKUP_HEIGHT) {
                currentPos.Z += 20;
                SetCoords(currentPos);
            }
        } else {
            CoordStruct currentPos = GetCoords();
            if (currentPos.Z < CARRYALL_CRUISE_HEIGHT) {
                currentPos.Z += 15;
                SetCoords(currentPos);
            } else if (currentPos.Z > CARRYALL_CRUISE_HEIGHT) {
                currentPos.Z -= 10;
                SetCoords(currentPos);
            }
        }
    }
}

void UnitClass::Combat_AI()
{
    if (IsInLimbo) return;
    if (Health <= 0) return;
    if (unknown_7E4 > 0) return; // EMPed, cannot fight

    // Check if we have a target and can fire.
    if (!HasTarget()) return;

    AbstractClass* pTarget = GetTarget();
    if (!pTarget) return;

    // Check weapon range and fire.
    for (int32 i = 0; i < GetWeaponCount(); ++i) {
        if (Can_Fire_At(nullptr, i)) {
            Fire_At(nullptr, i);
            break;
        }
    }
}

void UnitClass::Movement_AI()
{
    if (IsInLimbo) return;
    if (Health <= 0) return;
    if (unknown_7E4 > 0) return; // EMPed, cannot move

    // Update pathfinding and movement.
    if (Is_Moving()) {
        if (Has_Path()) {
            CoordStruct nextStep = Peek_Next_Path();
            SetDestination(nextStep);
        }
    }

    // Update subterranean tunnel state.
    if (IsSubterranean) {
        UpdateTube();
    }

    // Check map edges.
    UpdateEdgeOfWorld();
}

// =============================================================================
// Combat - firing and weapons
// =============================================================================
void UnitClass::Fire_At(TargetClass* pTarget, int32 weaponIndex)
{
    if (!Type) return;
    if (weaponIndex < 0 || weaponIndex >= GetWeaponCount()) return;

    // Check fire recharge timer.
    if (FireRechargeTimer > 0) return;

    // Trigger muzzle flash.
    MuzzleFlash(weaponIndex);

    // Set the last fire frame.
    LastFireFrame = Game::CurrentFrame;

    // Set recharge timer based on weapon ROF.
    int32 rof = GetROF();
    if (rof > 0) {
        FireRechargeTimer = rof;
    }

    (void)pTarget;

    OnFired(weaponIndex);
}

bool UnitClass::Can_Fire_At(TechnoClass* pTarget, int32 weaponIndex) const
{
    if (!Type) return false;
    if (weaponIndex < 0 || weaponIndex >= GetWeaponCount()) return false;
    if (FireRechargeTimer > 0) return false;
    if (unknown_7E4 > 0) return false; // EMPed

    // Check if the unit is in a state that allows firing.
    if (Deploying || Undeploying) return false;

    // Check target range if a target is provided.
    if (pTarget) {
        CoordStruct myPos = GetCoords();
        CoordStruct targetPos;
        pTarget->GetCoords(&targetPos);
        int32 distance = myPos.DistanceFrom(targetPos);
        int32 range = GetWeaponRange(weaponIndex);
        if (distance > range) return false;
    }

    return true;
}

int32 UnitClass::GetWeaponRange(int32 weaponIndex) const
{
    if (!Type) return 0;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return 0;

    // Return the range of the specified weapon.
    WeaponStruct& ws = Type->Weapons[weaponIndex];
    if (!ws.WeaponType) return 0;

    // Range is stored in leptons; use the weapon type's range.
    return static_cast<int32>(Type->GuardRange);
}

int32 UnitClass::GetWeaponDamage(int32 weaponIndex) const
{
    if (!Type) return 0;
    if (weaponIndex < 0 || weaponIndex >= Type->WeaponCount) return 0;

    WeaponStruct& ws = Type->Weapons[weaponIndex];
    if (!ws.WeaponType) return 0;

    // Return the weapon's damage value.
    return ws.dummy;
}

void UnitClass::MuzzleFlash(int32 weaponIndex)
{
    if (!Type) return;
    if (weaponIndex < 0 || weaponIndex >= GetWeaponCount()) return;

    // Activate the muzzle flash animation for the specified weapon.
    unknown_810 = 3; // muzzle flash lasts 3 frames

    // Set the firing sequence.
    if (HasTurret()) {
        SetSequence(Sequence::FireUp);
    }
}

void UnitClass::OnFired(int32 weaponIndex)
{
    // Post-fire processing.
    (void)weaponIndex;

    // If the unit is disguised, reveal the disguise after firing.
    if (IsDisguised()) {
        UnDisguise();
    }
}

int32 UnitClass::GetWeaponCount() const
{
    if (!Type) return 0;
    return Type->WeaponCount;
}

// =============================================================================
// Damage and death
// =============================================================================
void UnitClass::TakeDamage(int32 damage, TechnoClass* pSource, WarheadTypeClass* pWarhead)
{
    if (damage <= 0) return;

    // Iron curtained / force shielded units take no damage.
    if (IronCurtainTimer > 0 || ForceShieldTimer > 0) return;

    // Apply armor reduction.
    int32 armor = GetArmor();
    int32 actualDamage = damage;
    if (armor > 0) {
        actualDamage = damage * 100 / (100 + armor);
    }

    Health -= actualDamage;
    if (Health < 0) Health = 0;

    // If the unit is on fire, take additional damage.
    if (FireDamageTimer > 0 && pWarhead) {
        FireDamageTimer += actualDamage / 10;
    }

    // If health reaches zero, destroy the unit.
    if (Health <= 0) {
        OnDestroyed();
    }

    (void)pSource; (void)pWarhead;
}

void UnitClass::OnDestroyed()
{
    Explode();

    // Drop a crate if the unit type allows it.
    if (CanCrate()) {
        CreateCrate();
    }

    // Remove from the unit array.
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            if ((*Array)[i] == this) {
                Array->Remove(i);
                break;
            }
        }
    }
}

void UnitClass::OnCaptured(HouseClass* pNewOwner)
{
    if (!pNewOwner) return;

    // Change ownership of the unit.
    Owner = pNewOwner;

    // Reset combat state.
    ClearTarget();
    Stop();

    // If the unit was harvesting, stop.
    StopHarvesting();

    // If the unit was deployed, undeploy.
    if (Deployed) {
        Undeploy();
    }
}

void UnitClass::OnVeterancyUp()
{
    VeterancyLevel++;
    if (VeterancyLevel > 2) VeterancyLevel = 2;

    // Heal the unit partially on promotion.
    int32 healAmount = MaxHealth / 4;
    Repair(healAmount);
}

// =============================================================================
// Visibility and sight
// =============================================================================
bool UnitClass::IsVisibleTo(HouseClass* pHouse) const
{
    if (!pHouse) return false;
    if (Is_Ally(pHouse)) return true;

    // Check if the unit is within sight range of any of the house's units.
    // Cloaked units are not visible to enemies.
    if (IsCloaked()) {
        return false;
    }

    // Check map visibility at the unit's position.
    CoordStruct pos = GetCoords();
    if (MapClass::Instance) {
        CellStruct cell = Math::CoordToCell(pos);
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell && pCell->Is_Visible()) {
            return true;
        }
    }

    return false;
}

void UnitClass::RevealTo(HouseClass* pHouse)
{
    if (!pHouse) return;
    // Reveal the area around this unit to the specified house.
    // This is used when a unit exits fog of war.
    (void)pHouse;
}

int32 UnitClass::GetSightRange() const
{
    if (!Type) return 0;
    return Type->SightRange;
}

// =============================================================================
// Health and armor
// =============================================================================
int32 UnitClass::GetArmor() const
{
    if (!Type) return 0;
    return static_cast<int32>(Type->ArmorType);
}

int32 UnitClass::GetMaxHealth() const
{
    if (!Type) return 0;
    return Type->Strength;
}

int32 UnitClass::GetHealth() const
{
    return Health;
}

void UnitClass::SetHealth(int32 hp)
{
    Health = hp;
    if (Health < 0) Health = 0;
    int32 maxHp = GetMaxHealth();
    if (maxHp > 0 && Health > maxHp) Health = maxHp;
}

bool UnitClass::IsAlive() const
{
    return Health > 0;
}

bool UnitClass::IsDead() const
{
    return Health <= 0;
}

bool UnitClass::IsDamaged() const
{
    return Health < GetMaxHealth();
}

bool UnitClass::IsGreenHP() const
{
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return false;
    return GetHealthRatio() > 0.66f;
}

bool UnitClass::IsYellowHP() const
{
    float ratio = GetHealthRatio();
    return ratio > 0.33f && ratio <= 0.66f;
}

bool UnitClass::IsRedHP() const
{
    return GetHealthRatio() <= 0.33f;
}

float UnitClass::GetHealthRatio() const
{
    int32 maxHp = GetMaxHealth();
    if (maxHp <= 0) return 0.0f;
    return static_cast<float>(Health) / static_cast<float>(maxHp);
}

void UnitClass::Repair(int32 amount)
{
    if (amount <= 0) return;
    if (Health <= 0) return;

    Health += amount;
    int32 maxHp = GetMaxHealth();
    if (Health > maxHp) Health = maxHp;

    RepairActive = true;
}

void UnitClass::Kill()
{
    Health = 0;
    OnDestroyed();
}

// =============================================================================
// Capabilities
// =============================================================================
bool UnitClass::CanDeploy() const
{
    if (!Type) return false;
    if (!Type->HasDeployer) return false;
    if (Deploying || Deployed) return false;
    return true;
}

bool UnitClass::CanEnter() const
{
    if (!Type) return false;
    // Units can enter structures if they are not deployed.
    if (Deployed) return false;
    return true;
}

bool UnitClass::CanBeEntered() const
{
    // Check if passengers can enter this unit (APC, transport).
    if (!Type) return false;
    if (!IsAPC) return false;
    if (NonPassengerCount >= Type->Passengers) return false;
    return true;
}

bool UnitClass::CanCrate() const
{
    if (!Type) return false;
    return Type->IsCanBeDestroyed;
}

void UnitClass::CreateCrate()
{
    if (!CanCrate()) return;
    IsShowingCrate = true;
    unknown_80C = static_cast<DWORD>(GetValue());
}

void UnitClass::PickUpCrate()
{
    if (!IsShowingCrate) return;
    IsShowingCrate = false;
    unknown_80C = 0;
}

// =============================================================================
// Value and economy
// =============================================================================
int32 UnitClass::GetValue() const
{
    if (!Type) return 0;
    return Type->ThreatPosedValue > 0 ? static_cast<int32>(Type->ThreatPosedValue) : 1;
}

int32 UnitClass::GetCost() const
{
    if (!Type) return 0;
    return Type->BuildCost;
}

int32 UnitClass::GetBuildTime() const
{
    if (!Type) return 0;
    return Type->BuildTime;
}

int32 UnitClass::GetSpeed() const
{
    if (!Type) return 0;
    return Type->Speed;
}

int32 UnitClass::GetROF() const
{
    if (!Type) return 0;

    // Rate of fire is derived from the primary weapon's ROF value.
    WeaponStruct* pWeapons = Type->Weapons;
    if (pWeapons && pWeapons[0].WeaponType) {
        int32 rof = pWeapons[0].WeaponType->ROF;
        if (rof > 0) return rof;
    }

    // Fall back to the weapon charge timer if set.
    if (Type->WeaponCharge > 0) return Type->WeaponCharge;

    return 15; // default ROF in frames
}

// =============================================================================
// Direction
// =============================================================================
DirStruct UnitClass::GetDirection() const
{
    return PrimaryFacing;
}

void UnitClass::SetDirection(DirStruct dir)
{
    PrimaryFacing = dir;
    SetFacing(dir);
}

DirStruct UnitClass::GetTurretDir() const
{
    return TurretDir;
}

void UnitClass::SetTurretDir(DirStruct dir)
{
    TurretDir = dir;
    SetTurretFacing(dir);
}

// =============================================================================
// Coordinates
// =============================================================================
CoordStruct UnitClass::GetCoords() const
{
    return Location;
}

void UnitClass::SetCoords(CoordStruct coords)
{
    // Unmark old occupation, move, then mark new.
    UnmarkAllOccupationBits(Location);
    Location = coords;
    MarkAllOccupationBits(coords);
}

CoordStruct UnitClass::GetDestination() const
{
    return Get_Destination();
}

void UnitClass::SetDestination(CoordStruct dest)
{
    Move_To(dest);
}

// =============================================================================
// Movement control
// =============================================================================
void UnitClass::Stop()
{
    Stop_Moving();
    Clear_Path();
    ClearTarget();
    SetSequence(Sequence::Ready);
}

void UnitClass::Scatter()
{
    if (!CanScatter()) return;
    // Move to a nearby random position to avoid danger.
    CoordStruct currentPos = GetCoords();
    int32 offsetX = (rand() % 200) - 100;
    int32 offsetY = (rand() % 200) - 100;
    CoordStruct scatterPos(currentPos.X + offsetX, currentPos.Y + offsetY, currentPos.Z);
    SetDestination(scatterPos);
}

void UnitClass::Hold()
{
    Stop_Moving();
    SetSequence(Sequence::Guard);
}

bool UnitClass::IsMoving() const
{
    return Is_Moving();
}

bool UnitClass::IsFiring() const
{
    return unknown_810 > 0 || FireRechargeTimer > 0;
}

bool UnitClass::IsIdle() const
{
    if (IsMoving()) return false;
    if (IsFiring()) return false;
    if (Deploying || Undeploying) return false;
    if (IsHarvestingTiberium) return false;
    return GetSequence() == Sequence::Ready || GetSequence() == Sequence::Guard;
}

void UnitClass::SetIdle()
{
    Stop_Moving();
    SetSequence(Sequence::Ready);
}

// =============================================================================
// Freeze / limbo
// =============================================================================
void UnitClass::Freeze()
{
    // Stop all activity.
    Stop_Moving();
    StopHarvesting();
    unknown_7F8 = 1; // frozen flag
}

void UnitClass::Unfreeze()
{
    unknown_7F8 = 0; // clear frozen flag
}

bool UnitClass::Limbo()
{
    if (IsInLimbo) return false;
    IsInLimbo = true;
    UnmarkAllOccupationBits(Location);
    Stop_Moving();
    return true;
}

bool UnitClass::Unlimbo()
{
    if (!IsInLimbo) return false;
    IsInLimbo = false;
    MarkAllOccupationBits(Location);
    return true;
}

bool UnitClass::InLimbo() const
{
    return IsInLimbo;
}

// =============================================================================
// Mark / sync
// =============================================================================
void UnitClass::Mark(MarkType mark)
{
    if (mark == MarkType::Down) {
        MarkAllOccupationBits(Location);
    } else {
        UnmarkAllOccupationBits(Location);
    }
}

void UnitClass::Unmark()
{
    UnmarkAllOccupationBits(Location);
}

void UnitClass::Sync()
{
    // Synchronize state for network play.
    unknown_820 = static_cast<DWORD>(Health);
    unknown_824 = static_cast<DWORD>(unknown_7F0); // mission
    unknown_828 = static_cast<DWORD>(PrimaryFacing.Value);
}

void UnitClass::Unsync()
{
    // Restore synchronized state.
    Health = static_cast<int32>(unknown_820);
    unknown_7F0 = unknown_824;
    PrimaryFacing = DirStruct(static_cast<uint8>(unknown_828));
}

// =============================================================================
// Lock / disable
// =============================================================================
void UnitClass::Lock()
{
    unknown_7F8++;
}

void UnitClass::Unlock()
{
    if (unknown_7F8 > 0) {
        unknown_7F8--;
    }
}

bool UnitClass::IsLocked() const
{
    return unknown_7F8 > 0;
}

void UnitClass::Disable()
{
    unknown_7FC++;
    Stop_Moving();
}

void UnitClass::Enable()
{
    if (unknown_7FC > 0) {
        unknown_7FC--;
    }
}

bool UnitClass::IsDisabled() const
{
    return unknown_7FC > 0;
}

void UnitClass::Activate()
{
    if (Health > 0) {
        unknown_7FC = 0;
    }
}

void UnitClass::Deactivate()
{
    unknown_7FC++;
    Stop_Moving();
    StopHarvesting();
}

bool UnitClass::IsActive() const
{
    if (IsInLimbo) return false;
    if (Health <= 0) return false;
    if (unknown_7FC > 0) return false;
    return true;
}

// =============================================================================
// Cloak
// =============================================================================
void UnitClass::Cloak()
{
    if (!Type || !Type->CanCloak) return;
    CloakState = CloakStateEnum::Cloaking;
    CloakTimer = 30; // cloaking animation duration
}

void UnitClass::Decloak()
{
    CloakState = CloakStateEnum::Uncloaking;
    CloakTimer = 30;
}

bool UnitClass::IsCloaked() const
{
    return CloakState == CloakStateEnum::Cloaked;
}

// =============================================================================
// EMP
// =============================================================================
void UnitClass::EMPulse()
{
    if (Type && Type->IsImmuneToEMP) return;
    unknown_7E4 = 150; // EMP duration in frames
    Stop_Moving();
    StopHarvesting();
}

void UnitClass::UnEMP()
{
    unknown_7E4 = 0;
}

bool UnitClass::IsEMPed() const
{
    return unknown_7E4 > 0;
}

// =============================================================================
// Iron Curtain / Force Shield
// =============================================================================
void UnitClass::IronCurtain()
{
    IronCurtainTimer = IRON_CURTAIN_DURATION;
    ForceShieldTimer = 0;
}

void UnitClass::UnIronCurtain()
{
    IronCurtainTimer = 0;
}

bool UnitClass::IsIronCurtained() const
{
    return IronCurtainTimer > 0;
}

void UnitClass::ForceShield()
{
    ForceShieldTimer = FORCE_SHIELD_DURATION;
    IronCurtainTimer = 0;
}

void UnitClass::UnForceShield()
{
    ForceShieldTimer = 0;
}

bool UnitClass::IsForceShielded() const
{
    return ForceShieldTimer > 0;
}

// =============================================================================
// Chrono / temporal
// =============================================================================
void UnitClass::ChronoShift()
{
    if (!IsChrono) return;
    // Chrono shift teleports the unit to a new location.
    // The destination must have been set before calling this.
    CoordStruct dest = GetDestination();
    UnmarkAllOccupationBits(Location);
    Location = dest;
    MarkAllOccupationBits(dest);
}

void UnitClass::TemporalWarp()
{
    TemporalTimer = 300; // freeze for 300 frames
}

void UnitClass::UnTemporal()
{
    TemporalTimer = 0;
}

bool UnitClass::IsTemporalWarped() const
{
    return TemporalTimer > 0;
}

// =============================================================================
// Mind control
// =============================================================================
void UnitClass::MindControl(TechnoClass* pTarget)
{
    if (!pTarget) return;
    if (Type && Type->IsImmuneToPsionics) return;

    // Store the mind control target.
    uintptr_t ptrVal = reinterpret_cast<uintptr_t>(pTarget);
    unknown_804 = static_cast<DWORD>(ptrVal);
    unknown_808 = static_cast<DWORD>(ptrVal >> 32);
}

void UnitClass::UnMindControl()
{
    unknown_804 = 0;
    unknown_808 = 0;
}

bool UnitClass::IsMindControlled() const
{
    return unknown_804 != 0 || unknown_808 != 0;
}

// =============================================================================
// Disguise
// =============================================================================
void UnitClass::Disguise()
{
    if (!Type) return;
    unknown_800 = 1; // disguise active
}

void UnitClass::UnDisguise()
{
    unknown_800 = 0;
}

bool UnitClass::IsDisguised() const
{
    return unknown_800 != 0;
}

// =============================================================================
// Identity
// =============================================================================
bool UnitClass::IsUnit() const
{
    return true;
}

AbstractType UnitClass::WhatAmI() const
{
    return AbstractType::Unit;
}

int32 UnitClass::Size() const
{
    return sizeof(UnitClass);
}
