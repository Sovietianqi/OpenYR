#include "TunnelLocomotionClass.h"
#include "../Map/MapClass.h"
#include "../Houses/HouseClass.h"
#include "../Combat/WarheadTypeClass.h"

#include <cstdlib>
#include <cmath>

// ============================================================================
// TunnelLocomotionClass - Subterranean/tunnel locomotion.
// Underground movement, surface emergence, tunnel entrance/exit finding,
// attack-from-underground timing, and dig state management.
// ============================================================================

TunnelLocomotionClass::TunnelLocomotionClass()
    : LocomotionClass()
    , IsUnderground(false)
    , TunnelEntrance(0, 0, 0)
    , TunnelExit(0, 0, 0)
    , IsEnteringTunnel(false)
    , IsExitingTunnel(false)
    , EnterTimer(0)
    , ExitTimer(0)
    , SubterraneanSpeed(150)
    , DiggingTime(30)
    , EnterDirection(0)
{
    Speed = 150;
}

TunnelLocomotionClass::~TunnelLocomotionClass()
{
    // No additional cleanup required. TunnelLocomotionClass owns no
    // dynamically allocated resources: its state consists of scalar
    // fields (IsUnderground, EnterTimer, ExitTimer, SubterraneanSpeed,
    // DiggingTime, etc.) and coordinate/direction values, none of which
    // require manual release. Occupation bits are cleared during normal
    // Limbo()/movement transitions, not at destruction. The base
    // LocomotionClass destructor handles cleanup of the shared
    // owner/coordinate state.
}

HRESULT TunnelLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 TunnelLocomotionClass::Size()
{
    return sizeof(TunnelLocomotionClass);
}

// ============================================================================
// Is_Moving - Tunnel unit is moving during any underground phase.
// ============================================================================

bool TunnelLocomotionClass::Is_Moving()
{
    return IsEnteringTunnel || IsExitingTunnel || IsUnderground;
}

// ============================================================================
// Destination - Returns the tunnel exit or entrance based on state.
// ============================================================================

CoordStruct TunnelLocomotionClass::Destination()
{
    return IsUnderground ? TunnelExit : TunnelEntrance;
}

// ============================================================================
// Move_To - Initiates tunnel movement. Enters tunnel if above ground,
// exits tunnel if underground.
// ============================================================================

void TunnelLocomotionClass::Move_To(CoordStruct to)
{
    if (IsUnderground) {
        ExitTunnel(to);
    } else {
        EnterTunnel(to);
    }
}

// ============================================================================
// Stop_Moving - Cancels tunnel movement.
// ============================================================================

void TunnelLocomotionClass::Stop_Moving()
{
    if (IsEnteringTunnel) {
        IsEnteringTunnel = false;
        IsMoving = false;
    }
    if (IsExitingTunnel) {
        IsExitingTunnel = false;
        IsMoving = false;
    }
}

// ============================================================================
// Do_Turn - Tunnel unit turning.
// ============================================================================

void TunnelLocomotionClass::Do_Turn(DirStruct coord)
{
    if (Owner) {
        Owner->SetFacing(coord);
    }
}

// ============================================================================
// Process - Main tunnel movement loop. Handles entering, underground
// movement, and exiting phases.
// ============================================================================

bool TunnelLocomotionClass::Process()
{
    if (IsEnteringTunnel) {
        return ProcessEntering();
    }

    if (IsUnderground) {
        return ProcessUnderground();
    }

    if (IsExitingTunnel) {
        return ProcessExiting();
    }

    return false;
}

// ============================================================================
// ProcessEntering - Handles the dig-in animation and transition to
// underground state.
// ============================================================================

bool TunnelLocomotionClass::ProcessEntering()
{
    --EnterTimer;

    if (Owner) {
        float progress = 1.0f - static_cast<float>(EnterTimer) / static_cast<float>(DiggingTime);
        Do_Turn(EnterDirection);

        if (progress < 0.5f) {
            Owner->SetSequence(Sequence::Deploy);
        } else {
            Owner->SetSequence(Sequence::Enter);
        }
    }

    if (EnterTimer <= 0) {
        IsEnteringTunnel = false;
        IsUnderground = true;
        IsMoving = true;

        Mark_All_Occupation_Bits(MarkType::Down);

        CurrentCoord = TunnelEntrance;
        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetAlpha(0);
        }
    }

    return true;
}

// ============================================================================
// ProcessUnderground - Handles movement while underground.
// Unit moves toward tunnel exit at subterranean speed.
// ============================================================================

bool TunnelLocomotionClass::ProcessUnderground()
{
    if (!IsUnderground) {
        return false;
    }

    int32 moveSpeed = static_cast<int32>(static_cast<float>(SubterraneanSpeed) * SpeedPercentage);

    CurrentCoord = VectorMath::MoveTowards(CurrentCoord, TunnelExit, moveSpeed);

    if (Owner) {
        DirStruct facing = CoordMath::DirectionTo(CurrentCoord, TunnelExit);
        Do_Turn(facing);
    }

    int32 dist = CurrentCoord.DistanceFrom(TunnelExit);
    if (dist <= moveSpeed) {
        CurrentCoord = TunnelExit;
        IsUnderground = false;
        IsExitingTunnel = true;
        ExitTimer = DiggingTime;

        CheckAttackFromUnderground();
    }

    return true;
}

// ============================================================================
// ProcessExiting - Handles the dig-out animation and transition back
// to surface state.
// ============================================================================

bool TunnelLocomotionClass::ProcessExiting()
{
    --ExitTimer;

    if (Owner) {
        float progress = 1.0f - static_cast<float>(ExitTimer) / static_cast<float>(DiggingTime);

        Owner->SetAlpha(static_cast<uint8>(255.0f * progress));

        if (progress < 0.5f) {
            Owner->SetSequence(Sequence::Undeploy);
        } else {
            Owner->SetSequence(Sequence::Enter);
        }
    }

    if (ExitTimer <= 0) {
        IsExitingTunnel = false;
        IsMoving = false;

        if (Owner) {
            Owner->SetAlpha(255);
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Ready);
        }

        Mark_All_Occupation_Bits(MarkType::Up);
    }

    return true;
}

// ============================================================================
// CheckAttackFromUnderground - At the moment the unit emerges from
// underground, check for nearby enemies to attack.
// ============================================================================

void TunnelLocomotionClass::CheckAttackFromUnderground()
{
    if (!Owner) {
        return;
    }

    HouseClass* ownerHouse = Owner->GetOwningHouse();
    if (!ownerHouse) {
        return;
    }

    CellStruct exitCell = CoordMath::CoordToCell(TunnelExit);

    for (int32 dx = -3; dx <= 3; ++dx) {
        for (int32 dy = -3; dy <= 3; ++dy) {
            CellStruct cell;
            cell.X = static_cast<int16>(static_cast<int32>(exitCell.X) + dx);
            cell.Y = static_cast<int16>(static_cast<int32>(exitCell.Y) + dy);

            if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
                continue;
            }

            ObjectClass* occupant = MapClass::Instance->GetCellOccupier(cell);
            if (!occupant) {
                continue;
            }

            if (occupant->WhatAmI() == AbstractType::Infantry ||
                occupant->WhatAmI() == AbstractType::Unit) {
                TechnoClass* enemy = static_cast<TechnoClass*>(occupant);
                HouseClass* enemyHouse = enemy->GetOwningHouse();

                if (enemyHouse && !enemyHouse->IsAlliedWith(ownerHouse)) {
                    int32 damage = 200;
                    enemy->TakeDamage(damage, Owner, WarheadTypeClass::GetDefault());
                }
            }
        }
    }
}

// ============================================================================
// EnterTunnel - Begins tunneling into the ground at the entrance point.
// ============================================================================

void TunnelLocomotionClass::EnterTunnel(CoordStruct entrance)
{
    if (IsUnderground || IsEnteringTunnel) {
        return;
    }

    if (!IsValidEntrance(entrance)) {
        return;
    }

    TunnelEntrance = entrance;
    EnterDirection = CoordMath::DirectionTo(CurrentCoord, entrance);
    IsEnteringTunnel = true;
    IsMoving = true;
    EnterTimer = DiggingTime;
}

// ============================================================================
// ExitTunnel - Sets the exit point for underground travel.
// Initiates surface emergence if underground.
// ============================================================================

void TunnelLocomotionClass::ExitTunnel(CoordStruct exit)
{
    TunnelExit = exit;

    if (IsUnderground) {
        IsMoving = true;
    }

    if (!IsUnderground && !IsEnteringTunnel) {
        IsMoving = true;
    }
}

// ============================================================================
// IsValidEntrance - Validates that the entrance point is suitable for digging.
// ============================================================================

bool TunnelLocomotionClass::IsValidEntrance(CoordStruct entrance) const
{
    CellStruct cell = CoordMath::CoordToCell(entrance);

    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall || land == LandType::Rock) {
        return false;
    }

    return true;
}

// ============================================================================
// IsValidExit - Validates that the exit point is suitable for emergence.
// ============================================================================

bool TunnelLocomotionClass::IsValidExit(CoordStruct exit) const
{
    CellStruct cell = CoordMath::CoordToCell(exit);

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
// FindNearestTunnelExit - Searches for the nearest valid exit point.
// ============================================================================

CoordStruct TunnelLocomotionClass::FindNearestTunnelExit(CoordStruct from) const
{
    CellStruct fromCell = CoordMath::CoordToCell(from);

    for (int32 radius = 1; radius <= 32; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(fromCell.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(fromCell.Y) + dy);

                CoordStruct cellCoord = CoordMath::CellToCoord(cs);
                if (IsValidExit(cellCoord)) {
                    return cellCoord;
                }
            }
        }
    }

    return CoordStruct(0, 0, 0);
}

// ============================================================================
// Mark_All_Occupation_Bits - Tunnel unit occupation marking.
// Underground units don't occupy cells.
// ============================================================================

void TunnelLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (IsUnderground || IsEnteringTunnel) {
        return;
    }

    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    bool occupy = (mark == MarkType::Up);
    MapClass::Instance->MarkCellOccupied(cell, occupy);
}

// ============================================================================
// Limbo - Tunnel limbo state.
// ============================================================================

void TunnelLocomotionClass::Limbo()
{
    IsEnteringTunnel = false;
    IsExitingTunnel = false;
    IsUnderground = false;
    IsMoving = false;
    Mark_All_Occupation_Bits(MarkType::Down);
}

// ============================================================================
// Get_Status - Tunnel movement status code.
// ============================================================================

int32 TunnelLocomotionClass::Get_Status() const
{
    if (IsEnteringTunnel) {
        return static_cast<int32>(Sequence::Enter);
    }
    if (IsExitingTunnel) {
        return static_cast<int32>(Sequence::Unload);
    }
    if (IsUnderground) {
        return static_cast<int32>(Sequence::Ready);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Can_Fire - Cannot fire while entering/exiting tunnel.
// ============================================================================

FireError TunnelLocomotionClass::Can_Fire() const
{
    if (IsEnteringTunnel || IsExitingTunnel) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Is_Really_Moving_Now - Tunnel unit is moving underground.
// ============================================================================

bool TunnelLocomotionClass::Is_Really_Moving_Now() const
{
    return IsUnderground || IsEnteringTunnel || IsExitingTunnel;
}

// ============================================================================
// Can_Enter_Cell - Tunnel unit can enter any non-water cell.
// ============================================================================

Move TunnelLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (IsUnderground) {
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