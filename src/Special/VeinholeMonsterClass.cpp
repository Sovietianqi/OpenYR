#include "VeinholeMonsterClass.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Houses/HouseClass.h"
#include "../Rendering/Blitter.h"
#include "../Rendering/Surface.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/TechnoClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================
// VeinholeMonsterClass
// ============================================================

VeinholeMonsterClass::VeinholeMonsterClass()
    : Position(0, 0, 0), MaxGrowthRadius(512), CurrentGrowthRadius(0)
    , GrowthRate(2), VeinCount(0), TactileCount(4), MaxTactileCount(8)
    , TactileBaseLength(128), TactileLength(0), TactileMaxLength(256)
    , TactileAngleOffset(0.0f), TactileAnimationPhase(0.0f)
    , TactileAnimationSpeed(0.02f), IsActive(false), IsSpawning(false)
    , SpawnTimer(0), SpawnInterval(300), SpawnCount(0), MaxSpawnCount(10)
    , DamagePerTick(25), DamageInterval(30), DamageTimer(0)
    , AttackRange(64), BuildingDamage(50), UnitDamage(25)
    , LifeTimer(0), MaxLifetime(0), IsDying(false), DeathTimer(0), DeathDuration(120)
    , VeinCells(nullptr), VeinCellCount(0), TactileAngles(nullptr)
    , TactileExtensions(nullptr), TargetCell_X(0), TargetCell_Y(0)
    , GrowthDirection(0), TargetBuildingID(-1) {
    for (int32 i = 0; i < 8; ++i) {
        VeinDirections[i].X = 0;
        VeinDirections[i].Y = 0;
        VeinGrowth[i] = 0.0f;
    }
}

VeinholeMonsterClass::~VeinholeMonsterClass() {
    Release();
}

void VeinholeMonsterClass::Initialize(const CoordStruct& pos, int32 radius, int32 lifetime) {
    Position = pos;
    MaxGrowthRadius = radius;
    MaxLifetime = lifetime;
    CurrentGrowthRadius = 0;
    LifeTimer = 0;
    VeinCount = 0;
    IsActive = true;
    IsSpawning = false;
    IsDying = false;
    SpawnTimer = 0;
    SpawnCount = 0;
    DamageTimer = 0;
    DeathTimer = 0;
    TactileAnimationPhase = 0.0f;
    TactileAngleOffset = 0.0f;

    TactileCount = 4;
    TactileBaseLength = 128;
    TactileMaxLength = 256;
    TactileLength = TactileBaseLength;

    if (TactileAngles) {
        delete[] TactileAngles;
        TactileAngles = nullptr;
    }
    TactileAngles = new float[TactileCount];
    for (int32 i = 0; i < TactileCount; ++i) {
        TactileAngles[i] = (360.0f / TactileCount) * static_cast<float>(i);
    }

    if (TactileExtensions) {
        delete[] TactileExtensions;
        TactileExtensions = nullptr;
    }
    TactileExtensions = new float[TactileCount];
    for (int32 i = 0; i < TactileCount; ++i) {
        TactileExtensions[i] = 1.0f;
    }

    ReleaseVeinCells();

    // Initialize vein growth directions
    for (int32 i = 0; i < 8; ++i) {
        float angle = 45.0f * static_cast<float>(i) * 3.14159f / 180.0f;
        VeinDirections[i].X = static_cast<int16>(std::cos(angle) * 127.0f);
        VeinDirections[i].Y = static_cast<int16>(std::sin(angle) * 127.0f);
        VeinGrowth[i] = 0.0f;
    }
}

void VeinholeMonsterClass::Release() {
    ReleaseVeinCells();
    if (TactileAngles) {
        delete[] TactileAngles;
        TactileAngles = nullptr;
    }
    if (TactileExtensions) {
        delete[] TactileExtensions;
        TactileExtensions = nullptr;
    }
    IsActive = false;
}

void VeinholeMonsterClass::ReleaseVeinCells() {
    if (VeinCells) {
        delete[] VeinCells;
        VeinCells = nullptr;
    }
    VeinCellCount = 0;
}

void VeinholeMonsterClass::Update() {
    if (!IsActive) return;

    if (IsDying) {
        UpdateDeath();
        return;
    }

    ++LifeTimer;
    if (MaxLifetime > 0 && LifeTimer >= MaxLifetime) {
        StartDeath();
        return;
    }

    if (CurrentGrowthRadius < MaxGrowthRadius) {
        GrowVeins();
    }

    UpdateTactiles();

    ++DamageTimer;
    if (DamageTimer >= DamageInterval) {
        DamageTimer = 0;
        ApplyDamage();
    }

    if (IsSpawning) {
        ++SpawnTimer;
        if (SpawnTimer >= SpawnInterval && SpawnCount < MaxSpawnCount) {
            SpawnTimer = 0;
            SpawnUnit();
        }
    }
}

void VeinholeMonsterClass::GrowVeins() {
    CurrentGrowthRadius += GrowthRate;
    if (CurrentGrowthRadius > MaxGrowthRadius) {
        CurrentGrowthRadius = MaxGrowthRadius;
    }

    // Determine which veins to grow
    int32 numVeins = 4 + (CurrentGrowthRadius * 4 / (MaxGrowthRadius > 0 ? MaxGrowthRadius : 1));
    if (numVeins > 8) numVeins = 8;
    VeinCount = numVeins;

    for (int32 i = 0; i < numVeins; ++i) {
        VeinGrowth[i] = static_cast<float>(CurrentGrowthRadius) / static_cast<float>(MaxGrowthRadius);
        if (VeinGrowth[i] > 1.0f) VeinGrowth[i] = 1.0f;
    }

    // Spread vein cells on the map
    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (CurrentGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    if (MapClass::Instance) {
        int32 mapW = MapClass::Instance->MapWidth;
        int32 mapH = MapClass::Instance->MapHeight;

        for (int32 i = 0; i < numVeins; ++i) {
            float angle = static_cast<float>(i) * 360.0f / static_cast<float>(numVeins);
            float angleRad = angle * 3.14159f / 180.0f;

            for (int32 d = 0; d < cellRadius; ++d) {
                int32 cx = centerCell.X + static_cast<int32>(std::cos(angleRad) * static_cast<float>(d));
                int32 cy = centerCell.Y + static_cast<int32>(std::sin(angleRad) * static_cast<float>(d));

                if (cx >= 0 && cx < mapW && cy >= 0 && cy < mapH) {
                    CellStruct veinCell(static_cast<int16>(cx), static_cast<int16>(cy));
                    PlaceVeinCell(veinCell);
                }
            }
        }
    }
}

void VeinholeMonsterClass::PlaceVeinCell(const CellStruct& cell) {
    if (MapClass::Instance) {
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell) {
            pCell->SetFlag(CellFlags::Veinhole, true);
        }
    }
}

void VeinholeMonsterClass::UpdateTactiles() {
    TactileAnimationPhase += TactileAnimationSpeed;
    if (TactileAnimationPhase > 6.28318f) {
        TactileAnimationPhase -= 6.28318f;
    }

    TactileAngleOffset += 0.005f;
    if (TactileAngleOffset > 360.0f) {
        TactileAngleOffset -= 360.0f;
    }

    for (int32 i = 0; i < TactileCount; ++i) {
        float wave = std::sin(TactileAnimationPhase + static_cast<float>(i) * 0.785398f);
        TactileExtensions[i] = 0.5f + 0.5f * wave;
        TactileAngles[i] += 0.01f;
    }

    TactileLength = TactileBaseLength + static_cast<int32>(
        static_cast<float>(TactileMaxLength - TactileBaseLength) *
        static_cast<float>(CurrentGrowthRadius) / static_cast<float>(MaxGrowthRadius));
}

void VeinholeMonsterClass::ApplyDamage() {
    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (CurrentGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (MapClass::Instance) {
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        int32 mapW = MapClass::Instance->MapWidth;
        int32 mapH = MapClass::Instance->MapHeight;
        if (maxX >= mapW) maxX = mapW - 1;
        if (maxY >= mapH) maxY = mapH - 1;
    }

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            if (MapClass::Instance) {
                CellClass* pCell = MapClass::Instance->GetCellAt(cell);
                if (pCell && pCell->HasFlag(CellFlags::Veinhole)) {
                    ApplyVeinDamageToCell(cell);
                }
            }
        }
    }
}

void VeinholeMonsterClass::ApplyVeinDamageToCell(const CellStruct& cell) {
    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(cell) : nullptr;
    if (!pCell) return;

    ObjectClass* pObj = pCell->Occupier;
    if (!pObj) return;

    int32 damage = 0;
    if (pObj->WhatAmI() == AbstractType::Building) {
        damage = BuildingDamage;
    } else if (pObj->WhatAmI() == AbstractType::Infantry) {
        damage = UnitDamage;
    }

    if (damage > 0) {
        static_cast<TechnoClass*>(pObj)->TakeDamage(damage, nullptr, nullptr);
    }
}

void VeinholeMonsterClass::SpawnUnit() {
    if (SpawnCount >= MaxSpawnCount) return;

    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 offsetX = (std::rand() % 5) - 2;
    int32 offsetY = (std::rand() % 5) - 2;
    CellStruct spawnCell(static_cast<int16>(centerCell.X + offsetX),
                         static_cast<int16>(centerCell.Y + offsetY));

    ++SpawnCount;
}

void VeinholeMonsterClass::StartDeath() {
    IsDying = true;
    DeathTimer = DeathDuration;
}

void VeinholeMonsterClass::UpdateDeath() {
    --DeathTimer;
    if (DeathTimer <= 0) {
        IsActive = false;
        IsDying = false;
        RemoveAllVeinCells();
    }
}

void VeinholeMonsterClass::RemoveAllVeinCells() {
    if (!MapClass::Instance) return;

    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (MaxGrowthRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    int32 mapW = MapClass::Instance->MapWidth;
    int32 mapH = MapClass::Instance->MapHeight;

    for (int32 y = centerCell.Y - cellRadius; y <= centerCell.Y + cellRadius; ++y) {
        for (int32 x = centerCell.X - cellRadius; x <= centerCell.X + cellRadius; ++x) {
            if (x >= 0 && x < mapW && y >= 0 && y < mapH) {
                CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
                CellClass* pCell = MapClass::Instance->GetCellAt(cell);
                if (pCell && pCell->HasFlag(CellFlags::Veinhole)) {
                    pCell->SetFlag(CellFlags::Veinhole, false);
                }
            }
        }
    }
}

void VeinholeMonsterClass::RenderTactiles(BlitterClass* blitter, DSurface* surface) {
    if (!IsActive || !blitter || !surface) return;
    if (IsDying) return;

    uint8 tactR = 128;
    uint8 tactG = 0;
    uint8 tactB = 128;

    if (IsDying) {
        float deathPct = static_cast<float>(DeathTimer) / static_cast<float>(DeathDuration);
        uint8 alpha = static_cast<uint8>(255.0f * deathPct);
        tactR = static_cast<uint8>(tactR * deathPct);
        tactG = static_cast<uint8>(tactG * deathPct);
        tactB = static_cast<uint8>(tactB * deathPct);
    }

    for (int32 i = 0; i < TactileCount; ++i) {
        float angle = TactileAngles[i] + TactileAngleOffset;
        float angleRad = angle * 3.14159f / 180.0f;
        float ext = TactileExtensions[i];

        float startX = static_cast<float>(Position.X);
        float startY = static_cast<float>(Position.Y);
        float endX = startX + std::cos(angleRad) * TactileLength * ext;
        float endY = startY + std::sin(angleRad) * TactileLength * ext;

        float dirX = endX - startX;
        float dirY = endY - startY;
        float segLen = std::sqrt(dirX * dirX + dirY * dirY);
        if (segLen > 0) {
            dirX /= segLen;
            dirY /= segLen;
        }

        int32 width = 4 + static_cast<int32>(4.0f * ext);
        float perpX = -dirY * static_cast<float>(width);
        float perpY = dirX * static_cast<float>(width);

        int32 steps = static_cast<int32>(segLen / 4.0f);
        if (steps < 1) steps = 1;

        float curX = startX;
        float curY = startY;

        for (int32 s = 0; s < steps; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(steps);
            float offsetX = static_cast<float>(std::sin(t * 3.14159f * 3.0f + TactileAnimationPhase) * 8.0f);
            float offsetY = offsetX * 0.5f;

            int32 px = static_cast<int32>(curX + offsetX);
            int32 py = static_cast<int32>(curY + offsetY);

            for (int32 w = -width; w <= width; ++w) {
                surface->SetPixelAlpha(px + static_cast<int32>(perpX * w / width),
                                       py + static_cast<int32>(perpY * w / width),
                                       tactR, tactG, tactB, 200);
            }

            curX += dirX * 4.0f;
            curY += dirY * 4.0f;
        }
    }
}

void VeinholeMonsterClass::SetGrowthRate(int32 rate) {
    GrowthRate = rate;
    if (GrowthRate < 1) GrowthRate = 1;
    if (GrowthRate > 16) GrowthRate = 16;
}

void VeinholeMonsterClass::SetMaxRadius(int32 radius) {
    MaxGrowthRadius = radius;
    if (MaxGrowthRadius < 0) MaxGrowthRadius = 0;
    if (MaxGrowthRadius > 2048) MaxGrowthRadius = 2048;
}

void VeinholeMonsterClass::SetLifetime(int32 lifetime) {
    MaxLifetime = lifetime;
}

void VeinholeMonsterClass::SetSpawnInterval(int32 interval) {
    SpawnInterval = interval;
    if (SpawnInterval < 1) SpawnInterval = 1;
}

void VeinholeMonsterClass::SetMaxSpawnCount(int32 count) {
    MaxSpawnCount = count;
    if (MaxSpawnCount < 0) MaxSpawnCount = 0;
}

void VeinholeMonsterClass::SetBuildingDamage(int32 damage) {
    BuildingDamage = damage;
    if (BuildingDamage < 0) BuildingDamage = 0;
}

void VeinholeMonsterClass::SetUnitDamage(int32 damage) {
    UnitDamage = damage;
    if (UnitDamage < 0) UnitDamage = 0;
}

void VeinholeMonsterClass::SetTactileCount(int32 count) {
    if (TactileAngles) delete[] TactileAngles;
    if (TactileExtensions) delete[] TactileExtensions;

    TactileCount = count;
    if (TactileCount < 1) TactileCount = 1;
    if (TactileCount > 8) TactileCount = 8;

    TactileAngles = new float[TactileCount];
    TactileExtensions = new float[TactileCount];
    for (int32 i = 0; i < TactileCount; ++i) {
        TactileAngles[i] = (360.0f / TactileCount) * static_cast<float>(i);
        TactileExtensions[i] = 1.0f;
    }
}

void VeinholeMonsterClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

void VeinholeMonsterClass::SetSpawning(bool spawning) {
    IsSpawning = spawning;
}

void VeinholeMonsterClass::SetTargetBuilding(int32 buildingID) {
    TargetBuildingID = buildingID;
}

int32 VeinholeMonsterClass::GetCurrentGrowthRadius() const {
    return CurrentGrowthRadius;
}

int32 VeinholeMonsterClass::GetLifeTimer() const {
    return LifeTimer;
}

bool VeinholeMonsterClass::IsActiveMonster() const {
    return IsActive;
}

bool VeinholeMonsterClass::IsMonsterDying() const {
    return IsDying;
}

// ============================================================
// VeinholeMonsterManagerClass
// ============================================================

static VeinholeMonsterManagerClass* g_VeinholeMonsterManagerInstance = nullptr;

VeinholeMonsterManagerClass::VeinholeMonsterManagerClass()
    : ActiveCount(0) {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        Monsters[i] = nullptr;
    }
}

VeinholeMonsterManagerClass::~VeinholeMonsterManagerClass() {
    RemoveAllMonsters();
}

VeinholeMonsterManagerClass* VeinholeMonsterManagerClass::GetInstance() {
    if (!g_VeinholeMonsterManagerInstance) {
        g_VeinholeMonsterManagerInstance = new VeinholeMonsterManagerClass();
    }
    return g_VeinholeMonsterManagerInstance;
}

int32 VeinholeMonsterManagerClass::CreateMonster(const CoordStruct& pos, int32 radius, int32 lifetime) {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i] == nullptr) {
            Monsters[i] = new VeinholeMonsterClass();
            Monsters[i]->Initialize(pos, radius, lifetime);
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

bool VeinholeMonsterManagerClass::RemoveMonster(int32 index) {
    if (index < 0 || index >= MAX_VEINHOLES) return false;
    if (Monsters[index] == nullptr) return false;

    delete Monsters[index];
    Monsters[index] = nullptr;
    --ActiveCount;
    return true;
}

void VeinholeMonsterManagerClass::RemoveAllMonsters() {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i]) {
            delete Monsters[i];
            Monsters[i] = nullptr;
        }
    }
    ActiveCount = 0;
}

void VeinholeMonsterManagerClass::UpdateAllMonsters() {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i] && Monsters[i]->IsActive) {
            Monsters[i]->Update();
        }
    }
}

void VeinholeMonsterManagerClass::RenderAllTactiles(BlitterClass* blitter, DSurface* surface) {
    for (int32 i = 0; i < MAX_VEINHOLES; ++i) {
        if (Monsters[i] && Monsters[i]->IsActive) {
            Monsters[i]->RenderTactiles(blitter, surface);
        }
    }
}

int32 VeinholeMonsterManagerClass::GetActiveCount() const {
    return ActiveCount;
}

VeinholeMonsterClass* VeinholeMonsterManagerClass::GetMonster(int32 index) const {
    if (index < 0 || index >= MAX_VEINHOLES) return nullptr;
    return Monsters[index];
}