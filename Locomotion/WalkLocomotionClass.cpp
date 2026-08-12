#include "WalkLocomotionClass.h"
#include "../Map/MapClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================================
// WalkLocomotionClass - Infantry walking locomotion.
// Per-cell movement processing, crawling/prone states, terrain-aware speed,
// sound effects, and direction-based animation sequencing.
// ============================================================================

WalkLocomotionClass::WalkLocomotionClass()
    : LocomotionClass()
    , CurrentCell(0, 0)
    , NextCell(0, 0)
    , PathIndex(0)
    , PathLength(0)
    , IsFalling(false)
    , IsJumping(false)
    , IsCrawling(false)
    , MoveStepTimer(0)
    , StepSize(8)
{
    Speed = 64;
}

WalkLocomotionClass::~WalkLocomotionClass()
{
    Path.Clear();
}

HRESULT WalkLocomotionClass::GetClassID(CLSID* pClassID)
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

int32 WalkLocomotionClass::Size()
{
    return sizeof(WalkLocomotionClass);
}

// ============================================================================
// Move_To - Sets infantry destination. Generates a cell-by-cell path
// from the current position to the target coordinate using a simple
// Bresenham-like line algorithm.
// ============================================================================

void WalkLocomotionClass::Move_To(CoordStruct to)
{
    Dest = to;
    IsMoving = true;
    IsFalling = false;

    Path.Clear();
    PathIndex = 0;
    PathLength = 0;

    CellStruct startCell = CoordMath::CoordToCell(CurrentCoord);
    CellStruct destCell = CoordMath::CoordToCell(to);

    CurrentCell = startCell;

    int32 dx = static_cast<int32>(destCell.X) - static_cast<int32>(startCell.X);
    int32 dy = static_cast<int32>(destCell.Y) - static_cast<int32>(startCell.Y);

    int32 adx = std::abs(dx);
    int32 ady = std::abs(dy);
    int32 steps = adx > ady ? adx : ady;
    if (steps == 0) {
        IsMoving = false;
        return;
    }

    float stepX = static_cast<float>(dx) / static_cast<float>(steps);
    float stepY = static_cast<float>(dy) / static_cast<float>(steps);

    int32 lastCellX = static_cast<int32>(startCell.X);
    int32 lastCellY = static_cast<int32>(startCell.Y);

    for (int32 i = 0; i <= steps; ++i) {
        int32 cx = static_cast<int32>(startCell.X) + static_cast<int32>(stepX * static_cast<float>(i));
        int32 cy = static_cast<int32>(startCell.Y) + static_cast<int32>(stepY * static_cast<float>(i));

        if (cx == lastCellX && cy == lastCellY && i > 0) {
            continue;
        }

        CellStruct cs;
        cs.X = static_cast<int16>(cx);
        cs.Y = static_cast<int16>(cy);
        Path.Add(cs);

        lastCellX = cx;
        lastCellY = cy;
    }

    PathLength = Path.GetCount();
    if (PathLength > 0) {
        NextCell = Path[0];
    }
}

// ============================================================================
// Stop_Moving - Halts infantry movement and clears the path.
// ============================================================================

void WalkLocomotionClass::Stop_Moving()
{
    IsMoving = false;
    IsFalling = false;
    IsJumping = false;
    Path.Clear();
    PathIndex = 0;
    PathLength = 0;
    MoveStepTimer = 0;
}

// ============================================================================
// Process - Main infantry movement loop. Each frame, accumulates speed
// and makes step-by-step progress along the cell path.
// ============================================================================

bool WalkLocomotionClass::Process()
{
    if (!IsMoving) {
        return false;
    }

    if (IsFalling) {
        return ProcessFall();
    }

    if (IsJumping) {
        return ProcessJump();
    }

    if (!Powered) {
        IsMoving = false;
        return false;
    }

    int32 effectiveSpeed = static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
    if (effectiveSpeed <= 0) {
        effectiveSpeed = 1;
    }

    MoveStepTimer += effectiveSpeed;
    int32 stepThreshold = 256;

    while (MoveStepTimer >= stepThreshold) {
        MoveStepTimer -= stepThreshold;

        if (!MoveOneStep()) {
            IsMoving = false;
            Stop_Movement_Animation();
            return false;
        }

        UpdatePosition();
    }

    if (IsMoving && Owner) {
        Owner->SetSequence(Sequence::Walk);
    }

    return IsMoving;
}

// ============================================================================
// UpdatePosition - Smoothly interpolates the current coordinate toward
// the current cell's center coordinate.
// ============================================================================

void WalkLocomotionClass::UpdatePosition()
{
    CoordStruct targetCoord = CoordMath::CellToCoord(CurrentCell);
    if (CurrentCoord != targetCoord) {
        int32 effectiveSpeed = static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
        CurrentCoord = VectorMath::MoveTowards(CurrentCoord, targetCoord, effectiveSpeed);

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
        }
    }
}

// ============================================================================
// MoveOneStep - Advances the infantry to the next cell in the path.
// Returns false if the path is exhausted or blocked.
// ============================================================================

bool WalkLocomotionClass::MoveOneStep()
{
    if (PathIndex >= PathLength) {
        IsMoving = false;
        return false;
    }

    CellStruct nextCell = Path[PathIndex];
    if (!CanMoveToCell(nextCell)) {
        CellStruct altCell = FindAlternativeCell(nextCell);
        if (altCell.X == 0 && altCell.Y == 0) {
            IsMoving = false;
            return false;
        }
        nextCell = altCell;
    }

    DirStruct moveDir = CoordMath::DirectionTo(
        CoordMath::CellToCoord(CurrentCell),
        CoordMath::CellToCoord(nextCell)
    );

    if (Owner) {
        Do_Turn(moveDir);
    }

    CurrentCell = nextCell;
    CurrentCoord = CoordMath::CellToCoord(nextCell);

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    ++PathIndex;

    if (PathIndex < PathLength) {
        NextCell = Path[PathIndex];
    }

    return true;
}

// ============================================================================
// MoveToNextCell - Advances to the next cell in the path.
// ============================================================================

bool WalkLocomotionClass::MoveToNextCell()
{
    return MoveOneStep();
}

// ============================================================================
// CanMoveToCell - Validates whether an infantry unit can enter a cell.
// Checks map bounds, terrain, and occupancy.
// ============================================================================

bool WalkLocomotionClass::CanMoveToCell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return false;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return false;
    }

    if (MapClass::Instance->IsCellOccupied(cell) && !IsCrawling) {
        return false;
    }

    return true;
}

// ============================================================================
// FindAlternativeCell - When the target cell is blocked, searches for
// a nearby passable cell to navigate around obstacles.
// ============================================================================

CellStruct WalkLocomotionClass::FindAlternativeCell(CellStruct blocked)
{
    for (int32 radius = 1; radius <= 4; ++radius) {
        for (int32 dx = -radius; dx <= radius; ++dx) {
            for (int32 dy = -radius; dy <= radius; ++dy) {
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                CellStruct cs;
                cs.X = static_cast<int16>(static_cast<int32>(blocked.X) + dx);
                cs.Y = static_cast<int16>(static_cast<int32>(blocked.Y) + dy);
                if (CanMoveToCell(cs)) {
                    return cs;
                }
            }
        }
    }
    return CellStruct(0, 0);
}

// ============================================================================
// ProcessFall - Handles falling state (e.g., from a cliff or bridge).
// ============================================================================

bool WalkLocomotionClass::ProcessFall()
{
    CurrentCoord.Z -= 8;
    if (CurrentCoord.Z < 0) {
        CurrentCoord.Z = 0;
    }

    int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);
    if (CurrentCoord.Z <= groundZ) {
        CurrentCoord.Z = groundZ;
        IsFalling = false;
        IsMoving = false;

        if (Owner) {
            Owner->SetCoords(CurrentCoord);
            Owner->SetSequence(Sequence::Prone);
        }
        return false;
    }

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
        Owner->SetSequence(Sequence::Tumble);
    }

    return true;
}

// ============================================================================
// ProcessJump - Handles jump animation state (e.g., jumping over obstacles).
// ============================================================================

bool WalkLocomotionClass::ProcessJump()
{
    static int32 jumpHeight = 0;
    static int32 jumpPhase = 0;
    static int32 jumpMax = 128;

    int32 groundZ = MapClass::Instance->GetGroundHeight(CurrentCoord);

    if (jumpPhase == 0) {
        jumpHeight += 16;
        if (jumpHeight >= jumpMax) {
            jumpHeight = jumpMax;
            jumpPhase = 1;
        }
    } else {
        jumpHeight -= 16;
        if (jumpHeight <= 0) {
            jumpHeight = 0;
            jumpPhase = 0;
            IsJumping = false;
            CurrentCoord.Z = groundZ;
        }
    }

    CurrentCoord.Z = groundZ + jumpHeight;

    if (Owner) {
        Owner->SetCoords(CurrentCoord);
    }

    return IsJumping;
}

// ============================================================================
// BeginCrawl - Puts the infantry into crawl/prone state.
// ============================================================================

void WalkLocomotionClass::BeginCrawl()
{
    IsCrawling = true;
    SpeedPercentage *= 0.5f;
    StepSize = 4;

    if (Owner) {
        Owner->SetSequence(Sequence::Crawl);
    }
}

// ============================================================================
// EndCrawl - Returns the infantry to normal walking state.
// ============================================================================

void WalkLocomotionClass::EndCrawl()
{
    IsCrawling = false;
    SpeedPercentage = 1.0f;
    StepSize = 8;

    if (Owner) {
        Owner->SetSequence(Sequence::Ready);
    }
}

// ============================================================================
// Mark_All_Occupation_Bits - Infantry-specific occupation bit marking.
// Infantry only occupies a single cell.
// ============================================================================

void WalkLocomotionClass::Mark_All_Occupation_Bits(MarkType mark)
{
    if (mark == MarkType::Up) {
        MapClass::Instance->MarkCellOccupied(CurrentCell, true);
    } else {
        MapClass::Instance->MarkCellOccupied(CurrentCell, false);
    }
}

// ============================================================================
// Limbo - Infantry limbo state.
// ============================================================================

void WalkLocomotionClass::Limbo()
{
    Mark_All_Occupation_Bits(MarkType::Down);
    IsMoving = false;
    IsFalling = false;
    IsJumping = false;
    IsCrawling = false;
    MoveStepTimer = 0;
    Path.Clear();
    PathIndex = 0;
    PathLength = 0;
}

// ============================================================================
// Do_Turn - Infantry rotation with smooth directional change.
// Infantry turns instantly toward the target direction.
// ============================================================================

void WalkLocomotionClass::Do_Turn(DirStruct coord)
{
    if (!Owner) {
        return;
    }

    DirStruct currentDir = Owner->GetFacing();
    if (currentDir.Value == coord.Value) {
        return;
    }

    Owner->SetFacing(coord);
}

// ============================================================================
// Can_Enter_Cell - Infantry-specific cell traversal check.
// ============================================================================

Move WalkLocomotionClass::Can_Enter_Cell(CellStruct cell)
{
    if (cell.X < 0 || cell.X >= 512 || cell.Y < 0 || cell.Y >= 512) {
        return Move::No;
    }

    LandType land = MapClass::Instance->GetLandType(cell);
    if (land == LandType::Water || land == LandType::Wall) {
        return Move::No;
    }

    if (land == LandType::Rock) {
        return Move::Temp;
    }

    return Move::OK;
}

// ============================================================================
// GetStepSound - Returns the appropriate footstep sound based on terrain.
// ============================================================================

int32 WalkLocomotionClass::GetStepSound() const
{
    CellStruct cell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(cell);

    switch (land) {
        case LandType::Road:
            return 1;
        case LandType::Ice:
            return 2;
        case LandType::Weeds:
            return 3;
        case LandType::Tiberium:
            return 4;
        case LandType::Rough:
            return 5;
        case LandType::Clear:
        default:
            return 0;
    }
}

// ============================================================================
// PlayStepSound - Plays the footstep sound effect for the current terrain.
// ============================================================================

void WalkLocomotionClass::PlayStepSound()
{
    int32 soundId = GetStepSound();
    if (Owner) {
        Owner->PlaySoundEffect(soundId);
    }
}

// ============================================================================
// Is_Really_Moving_Now - Infantry is moving if not falling and path active.
// ============================================================================

bool WalkLocomotionClass::Is_Really_Moving_Now() const
{
    return IsMoving && !IsFalling && !IsJumping && PathIndex < PathLength;
}

// ============================================================================
// Get_Status - Infantry-specific status code.
// ============================================================================

int32 WalkLocomotionClass::Get_Status() const
{
    if (IsFalling) {
        return static_cast<int32>(Sequence::Tumble);
    }
    if (IsCrawling) {
        return static_cast<int32>(Sequence::Crawl);
    }
    if (IsJumping) {
        return static_cast<int32>(Sequence::Walk);
    }
    if (IsMoving) {
        return static_cast<int32>(Sequence::Walk);
    }
    return static_cast<int32>(Sequence::Ready);
}

// ============================================================================
// Movement_AI - Infantry movement AI with terrain-aware speed adjustment.
// ============================================================================

void WalkLocomotionClass::Movement_AI()
{
    if (!IsMoving || !Powered) {
        return;
    }

    CellStruct currentCell = CoordMath::CoordToCell(CurrentCoord);
    LandType land = MapClass::Instance->GetLandType(currentCell);

    float baseSpeedMod = 1.0f;
    switch (land) {
        case LandType::Road:
            baseSpeedMod = 1.1f;
            break;
        case LandType::Rough:
            baseSpeedMod = 0.75f;
            break;
        case LandType::Ice:
            baseSpeedMod = 0.8f;
            break;
        case LandType::Weeds:
            baseSpeedMod = 0.85f;
            break;
        case LandType::Tiberium:
            baseSpeedMod = 0.6f;
            break;
        case LandType::Beach:
            baseSpeedMod = 0.9f;
            break;
        default:
            baseSpeedMod = 1.0f;
            break;
    }

    if (IsCrawling) {
        baseSpeedMod *= 0.5f;
    }

    SpeedPercentage = baseSpeedMod;

    CellStruct destCell = CoordMath::CoordToCell(Dest);
    int32 dist = CoordMath::CellDistance(currentCell, destCell);
    if (dist <= 1) {
        SpeedPercentage *= 0.8f;
    }
}

// ============================================================================
// Can_Fire - Infantry can fire unless falling or jumping.
// ============================================================================

FireError WalkLocomotionClass::Can_Fire() const
{
    if (IsFalling) {
        return FireError::Movement;
    }
    if (IsJumping) {
        return FireError::Movement;
    }
    return FireError::OK;
}

// ============================================================================
// Is_To_Have_Moving_Anim - Infantry shows walking animation when moving.
// ============================================================================

bool WalkLocomotionClass::Is_To_Have_Moving_Anim() const
{
    return IsMoving && !IsFalling && !IsJumping;
}

// ============================================================================
// Apparent_Speed - Infantry apparent speed considering crawl state.
// ============================================================================

int32 WalkLocomotionClass::Apparent_Speed() const
{
    if (IsCrawling) {
        return static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage * 0.5f);
    }
    return static_cast<int32>(static_cast<float>(Speed) * SpeedPercentage);
}

// ============================================================================
// Move_To overload for AbstractClass target.
// ============================================================================

void WalkLocomotionClass::Move_To(AbstractClass* target)
{
    if (!target) {
        return;
    }
    CoordStruct targetPos = target->GetCoords();
    Move_To(targetPos);
}

// ============================================================================
// Force_New_Slope - Infantry slope handling.
// ============================================================================

void WalkLocomotionClass::Force_New_Slope(int32 ramp)
{
    if (ramp > 2) {
        IsFalling = true;
        CurrentCoord.Z = 0;
    } else {
        SpeedPercentage = 1.0f - static_cast<float>(ramp) * 0.1f;
        if (SpeedPercentage < 0.5f) {
            SpeedPercentage = 0.5f;
        }
    }
}