#include "RadSiteClass.h"
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
// RadSiteClass
// ============================================================

RadSiteClass::RadSiteClass()
    : Position(0, 0, 0), RadLevel(0), MaxRadLevel(500), RadRadius(0), MaxRadius(256)
    , Duration(0), CurrentDuration(0), SpreadRate(8), DecayRate(2)
    , IsActive(false), IsNuclear(false), IsSpreading(true)
    , OwnerHouse(-1), DamagePerTick(5), DamageInterval(15)
    , DamageTimer(0), VisualTimer(0), GlowIntensity(0)
    , GlowColor(0, 255, 0), Desaturation(0.0f)
    , AffectedCells(nullptr), AffectedCellCount(0)
    , RadValue(0), RadIntensity(0) {
}

RadSiteClass::~RadSiteClass() {
    Release();
}

void RadSiteClass::Initialize(const CoordStruct& pos, int32 radius, int32 level, int32 duration) {
    Position = pos;
    RadRadius = 0;
    MaxRadius = radius;
    RadLevel = level;
    MaxRadLevel = level;
    Duration = duration;
    CurrentDuration = 0;
    IsActive = true;
    IsSpreading = true;
    IsNuclear = false;
    DamageTimer = 0;
    VisualTimer = 0;
    GlowIntensity = 0;
    RadValue = level;
    RadIntensity = level;
    ReleaseAffectedCells();
}

void RadSiteClass::InitializeNuclear(const CoordStruct& pos, int32 radius, int32 level) {
    Initialize(pos, radius, level, 0);
    IsNuclear = true;
    SpreadRate = 16;
    DecayRate = 1;
    MaxRadius = radius * 2;
    GlowColor = ColorStruct(255, 255, 128);
    DamagePerTick = 20;
    RadValue = level * 10;
    RadIntensity = level * 10;
}

void RadSiteClass::Release() {
    IsActive = false;
    ReleaseAffectedCells();
}

void RadSiteClass::ReleaseAffectedCells() {
    if (AffectedCells) {
        delete[] AffectedCells;
        AffectedCells = nullptr;
    }
    AffectedCellCount = 0;
}

void RadSiteClass::Update() {
    if (!IsActive) return;

    ++CurrentDuration;
    if (Duration > 0 && CurrentDuration >= Duration) {
        IsActive = false;
        return;
    }

    if (IsSpreading && RadRadius < MaxRadius) {
        RadRadius += SpreadRate;
        if (RadRadius > MaxRadius) {
            RadRadius = MaxRadius;
        }
    }

    // Decay radiation level
    int32 decayAmount = DecayRate;
    RadLevel -= decayAmount;
    if (RadLevel < 0) {
        RadLevel = 0;
        IsActive = false;
    }

    RadIntensity = RadLevel;
    RadValue = RadLevel;

    ++DamageTimer;
    if (DamageTimer >= DamageInterval) {
        DamageTimer = 0;
        ApplyRadiationDamage();
    }

    ++VisualTimer;
    UpdateVisualEffects();
}

void RadSiteClass::ApplyRadiationDamage() {
    if (RadLevel <= 0) return;

    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (RadRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (MapClass::Instance) {
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX >= MapClass::Instance->MapWidth) maxX = MapClass::Instance->MapWidth - 1;
        if (maxY >= MapClass::Instance->MapHeight) maxY = MapClass::Instance->MapHeight - 1;
    }

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            CoordStruct cellCenter = CellClass::Cell2Coord(cell);
            cellCenter.X += LeptonsPerCell / 2;
            cellCenter.Y += LeptonsPerCell / 2;

            int32 distX = cellCenter.X - Position.X;
            int32 distY = cellCenter.Y - Position.Y;
            int32 distance = static_cast<int32>(std::sqrt(static_cast<float>(distX * distX + distY * distY)));

            if (distance <= RadRadius) {
                float radLevel = CalculateRadLevelAtDistance(distance);
                ApplyRadDamageToCell(cell, static_cast<int32>(radLevel));
            }
        }
    }
}

float RadSiteClass::CalculateRadLevelAtDistance(int32 distance) const {
    if (RadRadius <= 0) return 0.0f;
    if (distance >= RadRadius) return 0.0f;

    float t = static_cast<float>(distance) / static_cast<float>(RadRadius);
    float falloff = 1.0f - t * t;

    return static_cast<float>(RadLevel) * falloff;
}

void RadSiteClass::ApplyRadDamageToCell(const CellStruct& cell, int32 radLevel) {
    if (radLevel <= 0) return;

    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(cell) : nullptr;
    if (!pCell) return;

    ObjectClass* pObj = pCell->Occupier;
    if (!pObj) return;

    TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);

    if (OwnerHouse >= 0) {
        HouseClass* pHouse = pTechno->GetOwningHouse();
        if (pHouse && pHouse->GetOwningHouseIndex() == OwnerHouse && !IsNuclear) {
            return;
        }
    }

    int32 damage = (radLevel * DamagePerTick) / 100;
    if (damage < 1) damage = 1;

    if (pObj->WhatAmI() == AbstractType::Infantry) {
        damage *= 2;
    }

    if (pTechno->IsVehicle()) {
        damage = damage * 3 / 4;
    }

    pTechno->TakeDamage(damage, nullptr, nullptr);
    // Update cell rad level (stored in TiberiumValue as a proxy)
    int32 currentRad = pCell->TiberiumValue;
    currentRad += radLevel;
    if (currentRad > 1000) currentRad = 1000;
    pCell->TiberiumValue = currentRad;
}

void RadSiteClass::UpdateVisualEffects() {
    if (RadRadius <= 0 || RadLevel <= 0) return;

    float normalizedLevel = static_cast<float>(RadLevel) / static_cast<float>(MaxRadLevel);
    GlowIntensity = static_cast<int32>(normalizedLevel * 255.0f);

    float normalizedRadius = static_cast<float>(RadRadius) / static_cast<float>(MaxRadius);
    Desaturation = normalizedLevel * normalizedRadius;
}

void RadSiteClass::RenderGlowEffect(BlitterClass* blitter, DSurface* surface) {
    if (!IsActive || !blitter || !surface) return;
    if (GlowIntensity <= 0) return;

    int32 screenX = Position.X;
    int32 screenY = Position.Y;
    int32 screenRadius = RadRadius;

    uint8 alpha = static_cast<uint8>(GlowIntensity / 2);
    uint8 r = static_cast<uint8>(GlowColor.R);
    uint8 g = static_cast<uint8>(GlowColor.G);
    uint8 b = static_cast<uint8>(GlowColor.B);

    for (int32 y = screenY - screenRadius; y <= screenY + screenRadius; ++y) {
        for (int32 x = screenX - screenRadius; x <= screenX + screenRadius; ++x) {
            int32 dx = x - screenX;
            int32 dy = y - screenY;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist <= screenRadius) {
                float fade = 1.0f - (dist / screenRadius);
                fade = fade * fade;
                uint8 pixelAlpha = static_cast<uint8>(alpha * fade);
                surface->SetPixelAlpha(x, y, r, g, b, pixelAlpha);
            }
        }
    }
}

void RadSiteClass::SetRadLevel(int32 level) {
    RadLevel = level;
    if (RadLevel < 0) RadLevel = 0;
    if (RadLevel > MaxRadLevel) RadLevel = MaxRadLevel;
}

void RadSiteClass::SetRadRadius(int32 radius) {
    RadRadius = radius;
    if (RadRadius < 0) RadRadius = 0;
    if (RadRadius > MaxRadius) RadRadius = MaxRadius;
}

void RadSiteClass::SetDuration(int32 duration) {
    Duration = duration;
}

void RadSiteClass::SetSpreadRate(int32 rate) {
    SpreadRate = rate;
    if (SpreadRate < 1) SpreadRate = 1;
    if (SpreadRate > 32) SpreadRate = 32;
}

void RadSiteClass::SetDecayRate(int32 rate) {
    DecayRate = rate;
    if (DecayRate < 0) DecayRate = 0;
    if (DecayRate > 100) DecayRate = 100;
}

void RadSiteClass::SetDamagePerTick(int32 damage) {
    DamagePerTick = damage;
    if (DamagePerTick < 0) DamagePerTick = 0;
}

void RadSiteClass::SetDamageInterval(int32 interval) {
    DamageInterval = interval;
    if (DamageInterval < 1) DamageInterval = 1;
}

void RadSiteClass::SetNuclear(bool nuclear) {
    IsNuclear = nuclear;
    if (nuclear) {
        DamagePerTick = 20;
        SpreadRate = 16;
        DecayRate = 1;
        GlowColor = ColorStruct(255, 255, 128);
    }
}

void RadSiteClass::SetOwnerHouse(int32 house) {
    OwnerHouse = house;
}

void RadSiteClass::SetSpreading(bool spreading) {
    IsSpreading = spreading;
}

void RadSiteClass::SetGlowColor(const ColorStruct& color) {
    GlowColor = color;
}

void RadSiteClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

int32 RadSiteClass::GetRadLevel() const {
    return RadLevel;
}

int32 RadSiteClass::GetRadRadius() const {
    return RadRadius;
}

int32 RadSiteClass::GetDuration() const {
    return CurrentDuration;
}

int32 RadSiteClass::GetGlowIntensity() const {
    return GlowIntensity;
}

float RadSiteClass::GetDesaturation() const {
    return Desaturation;
}

bool RadSiteClass::IsActiveSite() const {
    return IsActive;
}

// ============================================================
// RadSiteManagerClass
// ============================================================

static RadSiteManagerClass* g_RadSiteManagerInstance = nullptr;

RadSiteManagerClass::RadSiteManagerClass()
    : ActiveCount(0), GlobalRadLevel(0) {
    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        Sites[i] = nullptr;
    }
}

RadSiteManagerClass::~RadSiteManagerClass() {
    RemoveAllSites();
}

RadSiteManagerClass* RadSiteManagerClass::GetInstance() {
    if (!g_RadSiteManagerInstance) {
        g_RadSiteManagerInstance = new RadSiteManagerClass();
    }
    return g_RadSiteManagerInstance;
}

int32 RadSiteManagerClass::CreateRadSite(const CoordStruct& pos, int32 radius, int32 level, int32 duration) {
    if (ActiveCount >= MAX_RAD_SITES) {
        for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
            if (Sites[i] && !Sites[i]->IsActive) {
                Sites[i]->Initialize(pos, radius, level, duration);
                Sites[i]->IsActive = true;
                ++ActiveCount;
                return i;
            }
        }
        return -1;
    }

    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        if (Sites[i] == nullptr) {
            Sites[i] = new RadSiteClass();
            Sites[i]->Initialize(pos, radius, level, duration);
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

int32 RadSiteManagerClass::CreateNuclearSite(const CoordStruct& pos, int32 radius, int32 level) {
    if (ActiveCount >= MAX_RAD_SITES) {
        for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
            if (Sites[i] && !Sites[i]->IsActive) {
                Sites[i]->InitializeNuclear(pos, radius, level);
                Sites[i]->IsActive = true;
                ++ActiveCount;
                return i;
            }
        }
        return -1;
    }

    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        if (Sites[i] == nullptr) {
            Sites[i] = new RadSiteClass();
            Sites[i]->InitializeNuclear(pos, radius, level);
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

bool RadSiteManagerClass::RemoveRadSite(int32 index) {
    if (index < 0 || index >= MAX_RAD_SITES) return false;
    if (Sites[index] == nullptr) return false;

    Sites[index]->Release();
    --ActiveCount;
    return true;
}

void RadSiteManagerClass::RemoveAllSites() {
    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        if (Sites[i]) {
            delete Sites[i];
            Sites[i] = nullptr;
        }
    }
    ActiveCount = 0;
}

void RadSiteManagerClass::UpdateAllSites() {
    GlobalRadLevel = 0;

    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        if (Sites[i] && Sites[i]->IsActive) {
            Sites[i]->Update();
            GlobalRadLevel += Sites[i]->GetRadLevel();
        }
    }
}

int32 RadSiteManagerClass::CalculateRadLevelAt(const CoordStruct& pos) const {
    int32 totalRad = 0;
    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        if (Sites[i] && Sites[i]->IsActive) {
            int32 dx = Sites[i]->Position.X - pos.X;
            int32 dy = Sites[i]->Position.Y - pos.Y;
            int32 distance = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
            int32 siteRad = Sites[i]->GetRadRadius();
            if (distance < siteRad) {
                float factor = 1.0f - static_cast<float>(distance) / static_cast<float>(siteRad);
                totalRad += static_cast<int32>(Sites[i]->GetRadLevel() * factor);
            }
        }
    }
    return totalRad;
}

int32 RadSiteManagerClass::GetActiveCount() const {
    return ActiveCount;
}

RadSiteClass* RadSiteManagerClass::GetRadSite(int32 index) const {
    if (index < 0 || index >= MAX_RAD_SITES) return nullptr;
    return Sites[index];
}

int32 RadSiteManagerClass::GetGlobalRadLevel() const {
    return GlobalRadLevel;
}

void RadSiteManagerClass::RenderAllGlowEffects(BlitterClass* blitter, DSurface* surface) {
    for (int32 i = 0; i < MAX_RAD_SITES; ++i) {
        if (Sites[i] && Sites[i]->IsActive) {
            Sites[i]->RenderGlowEffect(blitter, surface);
        }
    }
}