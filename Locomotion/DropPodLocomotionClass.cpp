#include "DropPodLocomotionClass.h"

#include <cstdlib>
#include <cmath>

#include "../Map/MapClass.h"
#include "../Rules/RulesClass.h"
#include "../Combat/WarheadTypeClass.h"
#include "../Combat/DamageArea.h"
#include "../Animations/AnimClass.h"
#include "../Abstract/InfantryClass.h"

// ============================================================================
// DropPodLocomotionClass - Drop pod locomotion.
// Parachute descent, landing impact, smoke trail, dust cloud on landing,
// unit deployment after landing, and impact damage.
// ============================================================================

DropPodLocomotionClass::DropPodLocomotionClass()
    : LocomotionClass()
    , DropHeight(5000)
    , IsDropping(false)
    , ImpactPoint(0, 0, 0)
    , DescentSpeed(80)
    , CurrentHeight(0)
    , HasImpacted(false)
    , ImpactDelay(15)
{
    Speed = 80;
}

DropPodLocomotionClass::~DropPodLocomotionClass()
{
    // No additional cleanup required. DropPodLocomotionClass owns no
    // dynamically allocated resources: its state consists of scalar
    // fields (DropHeight, DescentSpeed, CurrentHeight, etc.) and a
    // CDTimerClass (ImpactTimer) which is a plain-old-data countdown
    // timer with no heap allocations. The base LocomotionClass
    // destructor handles cleanup of the shared owner/coordinate state.
}

HRESULT DropPodLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 DropPodLocomotionClass::Size()
{
    return sizeof(DropPodLocomotionClass);
}

// ============================================================================
// Is_Moving - Drop pod is moving while dropping.
// ============================================================================

bool DropPodLocomotionClass::Is_Moving()
{
    return IsDropping;
}

// ============================================================================
// Destination - Returns the impact point.
// ============================================================================

CoordStruct DropPodLocomotionClass::Destination()
{
    return ImpactPoint;
}

// ============================================================================
// Move_To - Initiates the drop sequence at the target coordinate.
// ============================================================================

void DropPodLocomotionClass::Move_To(CoordStruct to)
{
    DropTo(to);
}

// ============================================================================
// Stop_Moving - Cancels the drop sequence.
// ============================================================================

void DropPodLocomotionClass::Stop_Moving()
{
    IsDropping = false;
    IsMoving = false;
    HasImpacted = false;
}

// ============================================================================
// Do_Turn - Drop pod turning (rarely used, orientation is fixed).
// ============================================================================

void DropPodLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        Owner->SetFacing(coord);
    }
}

// ============================================================================
// Process - Main drop pod movement loop. Handles descent and impact.
// ============================================================================

bool DropPodLocomotionClass::Process()
{
    if (!IsDropping) {
        return false;
    }

    if (HasImpacted) {
        return ProcessImpact();
    }

    return ProcessDescent();
}

// ============================================================================
// ProcessDescent - Handles the drop pod descent from high altitude.
// ============================================================================

bool DropPodLocomotionClass::ProcessDescent()
{
    if (!IsDropping || HasImpacted) {
        return false;
    }

    int32 effectiveSpeed = static_cast<int32>(static_cast<float>(DescentSpeed) * SpeedPercentage);

    CurrentHeight -= effectiveSpeed;

    CurrentCoord = ImpactPoint;
    CurrentCoord.Z = CurrentHeight;

    if (CurrentHeight > DropHeight / 2) {
        GenerateSmokeTrail();
    }

    if (CurrentHeight <= DropHeight / 4) {
        DeployParachute();
    }

    int32 groundZ = MapClass::Instance->GetGroundHeight(ImpactPoint);
    if (CurrentHeight <= groundZ) {
        CurrentHeight = groundZ;
        CurrentCoord.Z = groundZ;
        HasImpacted = true;
        ImpactTimer.Start(ImpactDelay);

        OnImpact();
        return true;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Paradrop);
    }

    return true;
}

// ============================================================================
// ProcessImpact - Handles the post-impact delay and deployment.
// ============================================================================

bool DropPodLocomotionClass::ProcessImpact()
{
    ImpactTimer.Update();

    float progress = 1.0f - static_cast<float>(ImpactTimer.GetTimeLeft())
        / static_cast<float>(ImpactDelay);

    if (progress < 0.5f) {
        GenerateDustCloud();
    }

    if (ImpactTimer.Expired()) {
        IsDropping = false;
        IsMoving = false;
        HasImpacted = false;

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Deploy);
        }

        DeployUnits();
        Mark_All_Occupation_Bits(MarkType::Up);

        return false;
    }

    return true;
}

// ============================================================================
// OnImpact - Handles the immediate impact effects.
// ============================================================================

void DropPodLocomotionClass::OnImpact()
{
    CurrentCoord = ImpactPoint;
    CurrentCoord.Z = 0;

    GenerateImpactCrater();

    int32 impactDamage = RulesClass::Instance->GetDropPodImpactDamage();
    int32 impactRadius = RulesClass::Instance->GetDropPodImpactRadius();

    if (impactDamage > 0 && impactRadius > 0) {
        DamageArea damageArea;
        damageArea.X = ImpactPoint.X;
        damageArea.Y = ImpactPoint.Y;
        damageArea.Z = 0;
        damageArea.Damage = impactDamage;
        damageArea.Range = impactRadius;
        damageArea.Warhead = WarheadTypeClass::GetDefault();

        MapClass::Instance->ApplyDamageArea(damageArea);
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Deploy);
    }
}

// ============================================================================
// GenerateSmokeTrail - Creates smoke particles trailing the drop pod.
// ============================================================================

void DropPodLocomotionClass::GenerateSmokeTrail()
{
    if (!Owner) {
        return;
    }

    AnimClass* smokeAnim = GameCreate<AnimClass>();
    if (smokeAnim) {
        CoordStruct smokePos = CurrentCoord;
        smokePos.Z = CurrentHeight + 50;
        smokeAnim->SetCoords(smokePos);
        smokeAnim->SetOwner(Owner->GetOwningHouse());
    }
}

// ============================================================================
// GenerateDustCloud - Creates a dust cloud at the impact point.
// ============================================================================

void DropPodLocomotionClass::GenerateDustCloud()
{
    if (!Owner) {
        return;
    }

    for (int32 i = 0; i < 3; ++i) {
        AnimClass* dustAnim = GameCreate<AnimClass>();
        if (dustAnim) {
            CoordStruct dustPos = ImpactPoint;
            dustPos.X += (rand() % 200) - 100;
            dustPos.Y += (rand() % 200) - 100;
            dustPos.Z = 0;

            dustAnim->SetCoords(dustPos);
            dustAnim->SetOwner(Owner->GetOwningHouse());
        }
    }
}

// ============================================================================
// GenerateImpactCrater - Creates a crater at the impact point.
// ============================================================================

void DropPodLocomotionClass::GenerateImpactCrater()
{
    CellStruct cell = CoordMath::CoordToCell(ImpactPoint);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return;
    }

    MapClass::Instance->CreateCrater(cell, 3);
}

// ============================================================================
// DeployParachute - Activates parachute effect during descent.
// ============================================================================

void DropPodLocomotionClass::DeployParachute()
{
    if (DescentSpeed > 30) {
        DescentSpeed = 30;
        SpeedPercentage = 0.5f;
    }
}

// ============================================================================
// DeployUnits - Deploys the contained units after landing.
// ============================================================================

void DropPodLocomotionClass::DeployUnits()
{
    if (!Owner) {
        return;
    }

    HouseClass* ownerHouse = Owner->GetOwningHouse();
    if (!ownerHouse) {
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(ImpactPoint);

    for (int32 dx = -1; dx <= 1; ++dx) {
        for (int32 dy = -1; dy <= 1; ++dy) {
            CellStruct deployCell;
            deployCell.X = static_cast<int16>(static_cast<int32>(cell.X) + dx);
            deployCell.Y = static_cast<int16>(static_cast<int32>(cell.Y) + dy);

            if (deployCell.X < 0 || deployCell.X >= 512 || deployCell.Y < 0 || deployCell.Y >= 512) {
                continue;
            }

            CoordStruct deployPos = CoordMath::CellToCoord(deployCell);
            Owner->SetCoords(deployPos);
            break;
        }
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - Drop pod occupation marking.
// ============================================================================

void DropPodLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (IsDropping && !HasImpacted) {
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    bool occupy = (mark == MarkType::Up);
    MapClass::Instance->MarkCellOccupied(cell, occupy);
}

// ============================================================================
// Limbo - Drop pod limbo state.
// ============================================================================

void DropPodLocomotionClass::Limbo()
{
    IsDropping = false;
    IsMoving = false;
    HasImpacted = false;
    CurrentHeight = DropHeight;
    DescentSpeed = 80;
    SpeedPercentage = 1.0f;
}

// ============================================================================
// DropTo - Initiates the drop pod sequence at a target coordinate.
// ============================================================================

void DropPodLocomotionClass::DropTo(CoordStruct coord)
{
    if (IsDropping) {
        return;
    }

    if (!IsValidDropPoint(coord)) {
        CoordStruct nearest = FindNearestDropPoint(coord);
        if (nearest == CoordStruct(0, 0, 0)) {
            return;
        }
        coord = nearest;
    }

    ImpactPoint = coord;
    ImpactPoint.Z = 0;
    CurrentCoord = coord;
    CurrentCoord.Z = DropHeight;
    CurrentHeight = DropHeight;
    IsDropping = true;
    IsMoving = true;
    HasImpacted = false;
    DescentSpeed = 80;
    SpeedPercentage = 1.0f;
}

// ============================================================================
// Impact - Triggers the impact sequence.
// ============================================================================

void DropPodLocomotionClass::Impact()
{
    ImpactPoint.Z = 0;
    CurrentCoord = ImpactPoint;
    HasImpacted = true;
}

// ============================================================================
// IsValidDropPoint - Validates that a drop point is suitable.
// ============================================================================

bool DropPodLocomotionClass::IsValidDropPoint(CoordStruct coord) const
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
// FindNearestDropPoint - Searches for the nearest valid drop point.
// ============================================================================

CoordStruct DropPodLocomotionClass::FindNearestDropPoint(CoordStruct from) const
{
    CellStruct fromCell = CoordMath::CoordToCell(from);

    for (int32 radius = 1; radius <= 16; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(fromCell.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(fromCell.Y) + dy);

                CoordStruct cellCoord = CoordMath::CellToCoord(cs);
                if (IsValidDropPoint(cellCoord)) {
                    return cellCoord;
                }
            }
        }
    }

    return CoordStruct(0, 0, 0);
}

// ============================================================================
// Get_Status - Drop pod movement status code.
// ============================================================================

int32 DropPodLocomotionClass::Get_Status() const
{
    if (IsDropping && !HasImpacted) {
        return static_cast<int32>(Sequence::Paradrop);
    }
    if (HasImpacted) {
        return static_cast<int32>(Sequence::Deploy);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Can_Fire - Cannot fire while dropping or deploying.
// ============================================================================

FireError DropPodLocomotionClass::Can_Fire() const
{
    if (IsDropping) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Is_Really_Moving_Now - Drop pod is moving while dropping.
// ============================================================================

bool DropPodLocomotionClass::Is_Really_Moving_Now() const
{
    return IsDropping;
}

// ============================================================================
// GetCurrentHeight - Returns the current height during descent.
// ============================================================================

int32 DropPodLocomotionClass::GetCurrentHeight() const
{
    return CurrentHeight;
}

// ============================================================================
// Can_Enter_Cell - Drop pod can land on any non-water cell.
// ============================================================================

Move DropPodLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (IsDropping && !HasImpacted) {
        return Move::OK;
    }

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return Move::No;
    }

    return Move::OK;
}

// ============================================================================
// GetDropProgress - Returns the progress of the drop (0.0 to 1.0).
// ============================================================================

float DropPodLocomotionClass::GetDropProgress() const
{
    if (!IsDropping || DropHeight == 0) {
        return 0.0f;
    }

    return 1.0f - static_cast<float>(CurrentHeight) / static_cast<float>(DropHeight);
}