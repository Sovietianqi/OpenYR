#include "TiberiumClass.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Houses/HouseClass.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/TechnoClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================
// TiberiumClass
// ============================================================

TiberiumClass::TiberiumClass()
    : Type(TiberiumType::Green), Value(50), GrowthRate(2), SpreadRate(1)
    , MaxGrowth(24), MinGrowth(1), CellLevel(0), MaxCellLevel(12)
    , DamageToInfantry(1), DamageInterval(60), IsHarvestable(true)
    , IsChainReactable(true), ChainReactionRadius(3), ChainReactionDamage(200)
    , ExplosionChance(0.0f), ExplosionDamage(100), FlowRate(1)
    , FlowDirection(0), IsFlowing(false), GrowthTimer(0), SpreadTimer(0)
    , CellCount(0), SubType(0), ImageIndex(0), TintColor(0, 255, 0)
    , Power(0), WeaponIndex(0), RevolutionIndex(0), ValueMultiplier(1.0f)
    , SpreadChance(0.01f), RegrowsAfterHarvest(true), RegrowthRate(1)
    , RegrowthTimer(0), DamageToVehicles(0), Optimized(false)
    , IsSpecial(false), IsWeaponTiberium(false), RadarColor(0, 255, 0) {
}

TiberiumClass::~TiberiumClass() {
}

void TiberiumClass::Initialize(TiberiumType type, int32 startingLevel) {
    Type = type;
    CellLevel = startingLevel;
    if (CellLevel < MinGrowth) CellLevel = MinGrowth;
    if (CellLevel > MaxGrowth) CellLevel = MaxGrowth;
    GrowthTimer = 0;
    SpreadTimer = 0;
    RegrowthTimer = 0;

    switch (type) {
        case TiberiumType::Green:
            Value = 50;
            GrowthRate = 2;
            SpreadRate = 1;
            MaxCellLevel = 12;
            DamageToInfantry = 1;
            ChainReactionRadius = 3;
            TintColor = ColorStruct(0, 255, 0);
            RadarColor = ColorStruct(0, 255, 0);
            break;
        case TiberiumType::Blue:
            Value = 100;
            GrowthRate = 1;
            SpreadRate = 1;
            MaxCellLevel = 12;
            DamageToInfantry = 2;
            ChainReactionRadius = 4;
            ChainReactionDamage = 300;
            TintColor = ColorStruct(0, 128, 255);
            RadarColor = ColorStruct(0, 128, 255);
            break;
        case TiberiumType::Riparius:
            Value = 25;
            GrowthRate = 3;
            SpreadRate = 2;
            MaxCellLevel = 12;
            DamageToInfantry = 1;
            IsSpecial = false;
            TintColor = ColorStruct(0, 200, 0);
            RadarColor = ColorStruct(0, 200, 0);
            break;
        case TiberiumType::Cruentus:
            Value = 75;
            GrowthRate = 2;
            SpreadRate = 1;
            MaxCellLevel = 12;
            DamageToInfantry = 3;
            DamageToVehicles = 1;
            TintColor = ColorStruct(200, 0, 0);
            RadarColor = ColorStruct(200, 0, 0);
            break;
        case TiberiumType::Vinifera:
            Value = 150;
            GrowthRate = 1;
            SpreadRate = 1;
            MaxCellLevel = 12;
            DamageToInfantry = 5;
            DamageToVehicles = 2;
            ChainReactionRadius = 5;
            ChainReactionDamage = 500;
            TintColor = ColorStruct(128, 0, 255);
            RadarColor = ColorStruct(128, 0, 255);
            break;
        case TiberiumType::Aboreus:
            Value = 200;
            GrowthRate = 1;
            SpreadRate = 1;
            MaxCellLevel = 12;
            DamageToInfantry = 1;
            IsSpecial = true;
            TintColor = ColorStruct(255, 255, 0);
            RadarColor = ColorStruct(255, 255, 0);
            break;
        case TiberiumType::Arboreus:
            Value = 200;
            GrowthRate = 1;
            SpreadRate = 1;
            MaxCellLevel = 12;
            DamageToInfantry = 1;
            IsSpecial = true;
            TintColor = ColorStruct(255, 200, 0);
            RadarColor = ColorStruct(255, 200, 0);
            break;
        default:
            break;
    }
}

void TiberiumClass::UpdateGrowth(const CellStruct& cell) {
    if (CellLevel <= 0) return;

    ++GrowthTimer;
    if (GrowthTimer >= (60 * 60 / GrowthRate)) {
        GrowthTimer = 0;
        if (CellLevel < MaxCellLevel) {
            ++CellLevel;
        }
    }

    ++SpreadTimer;
    if (SpreadTimer >= (60 * 60 * 10 / SpreadRate)) {
        SpreadTimer = 0;
        SpreadToNeighbors(cell);
    }
}

void TiberiumClass::SpreadToNeighbors(const CellStruct& cell) {
    if (!MapClass::Instance) return;

    static const CellStruct neighbors[8] = {
        CellStruct(-1, -1), CellStruct(0, -1), CellStruct(1, -1),
        CellStruct(-1, 0),  CellStruct(1, 0),
        CellStruct(-1, 1),  CellStruct(0, 1),  CellStruct(1, 1)
    };

    int32 mapW = MapClass::Instance->MapWidth;
    int32 mapH = MapClass::Instance->MapHeight;

    for (int32 i = 0; i < 8; ++i) {
        int32 nx = cell.X + neighbors[i].X;
        int32 ny = cell.Y + neighbors[i].Y;

        if (nx >= 0 && nx < mapW && ny >= 0 && ny < mapH) {
            CellStruct neighborCell(static_cast<int16>(nx), static_cast<int16>(ny));
            CellClass* pCell = MapClass::Instance->GetCellAt(neighborCell);
            if (pCell && !pCell->IsTiberium()) {
                float chance = SpreadChance * CellLevel / MaxCellLevel;
                if (static_cast<float>(std::rand() % 1000) / 1000.0f < chance) {
                    pCell->Land = LandType::Tiberium;
                    pCell->Overlay = static_cast<int32>(Type);
                    pCell->TiberiumValue = 1;
                }
            }
        }
    }
}

int32 TiberiumClass::CalculateHarvestValue() const {
    if (!IsHarvestable) return 0;
    int32 baseValue = Value * CellLevel;
    return static_cast<int32>(baseValue * ValueMultiplier);
}

int32 TiberiumClass::Harvest(int32 amount) {
    int32 harvested = amount;
    if (harvested > CellLevel) harvested = CellLevel;

    CellLevel -= harvested;
    if (CellLevel < 0) CellLevel = 0;

    int32 harvestValue = Value * harvested;
    return static_cast<int32>(harvestValue * ValueMultiplier);
}

void TiberiumClass::ApplyDamageToInfantry(ObjectClass* pObj) {
    if (!pObj || DamageToInfantry <= 0) return;
    if (pObj->WhatAmI() != AbstractType::Infantry) return;

    int32 damage = DamageToInfantry * CellLevel / MaxCellLevel;
    if (damage < 1) damage = 1;
    static_cast<TechnoClass*>(pObj)->TakeDamage(damage, nullptr, nullptr);
}

void TiberiumClass::ApplyDamageToVehicles(ObjectClass* pObj) {
    if (!pObj || DamageToVehicles <= 0) return;
    if (!static_cast<TechnoClass*>(pObj)->IsVehicle()) return;

    int32 damage = DamageToVehicles * CellLevel / MaxCellLevel;
    if (damage < 1) damage = 1;
    static_cast<TechnoClass*>(pObj)->TakeDamage(damage, nullptr, nullptr);
}

bool TiberiumClass::CheckChainReaction(const CellStruct& cell) {
    if (!IsChainReactable) return false;
    if (CellLevel < MaxCellLevel / 2) return false;

    float chance = ExplosionChance;
    if (chance <= 0.0f) return false;

    if (static_cast<float>(std::rand() % 1000) / 1000.0f < chance) {
        TriggerChainReaction(cell);
        return true;
    }
    return false;
}

void TiberiumClass::TriggerChainReaction(const CellStruct& cell) {
    if (!MapClass::Instance) return;

    int32 mapW = MapClass::Instance->MapWidth;
    int32 mapH = MapClass::Instance->MapHeight;

    CellLevel = 0;

    for (int32 y = cell.Y - ChainReactionRadius; y <= cell.Y + ChainReactionRadius; ++y) {
        for (int32 x = cell.X - ChainReactionRadius; x <= cell.X + ChainReactionRadius; ++x) {
            if (x >= 0 && x < mapW && y >= 0 && y < mapH) {
                CellStruct neighborCell(static_cast<int16>(x), static_cast<int16>(y));
                CellClass* pCell = MapClass::Instance->GetCellAt(neighborCell);
                if (pCell) {
                    if (pCell->IsTiberium()) {
                        int32 tibLevel = pCell->TiberiumValue;
                        float chance = 0.5f * static_cast<float>(tibLevel) / MaxCellLevel;
                        if (static_cast<float>(std::rand() % 1000) / 1000.0f < chance) {
                            // Apply damage to objects on this cell
                            if (pCell->Occupier) {
                                static_cast<TechnoClass*>(pCell->Occupier)->TakeDamage(ChainReactionDamage, nullptr, nullptr);
                            }
                            pCell->TiberiumValue = 0;
                        }
                    }
                }
            }
        }
    }
}

void TiberiumClass::UpdateRegrowth() {
    if (!RegrowsAfterHarvest) return;
    if (CellLevel >= MinGrowth) return;

    ++RegrowthTimer;
    if (RegrowthTimer >= (60 * 60 * 10 / RegrowthRate)) {
        RegrowthTimer = 0;
        if (CellLevel < MinGrowth) {
            ++CellLevel;
        }
    }
}

void TiberiumClass::SetType(TiberiumType type) {
    Type = type;
}

void TiberiumClass::SetCellLevel(int32 level) {
    CellLevel = level;
    if (CellLevel < 0) CellLevel = 0;
    if (CellLevel > MaxCellLevel) CellLevel = MaxCellLevel;
}

void TiberiumClass::SetValue(int32 value) {
    Value = value;
    if (Value < 0) Value = 0;
}

void TiberiumClass::SetGrowthRate(int32 rate) {
    GrowthRate = rate;
    if (GrowthRate < 1) GrowthRate = 1;
    if (GrowthRate > 16) GrowthRate = 16;
}

void TiberiumClass::SetSpreadRate(int32 rate) {
    SpreadRate = rate;
    if (SpreadRate < 1) SpreadRate = 1;
    if (SpreadRate > 16) SpreadRate = 16;
}

void TiberiumClass::SetDamageToInfantry(int32 damage) {
    DamageToInfantry = damage;
    if (DamageToInfantry < 0) DamageToInfantry = 0;
}

void TiberiumClass::SetExplosionChance(float chance) {
    ExplosionChance = chance;
    if (ExplosionChance < 0.0f) ExplosionChance = 0.0f;
    if (ExplosionChance > 1.0f) ExplosionChance = 1.0f;
}

void TiberiumClass::SetChainReactionRadius(int32 radius) {
    ChainReactionRadius = radius;
    if (ChainReactionRadius < 0) ChainReactionRadius = 0;
    if (ChainReactionRadius > 10) ChainReactionRadius = 10;
}

void TiberiumClass::SetChainReactionDamage(int32 damage) {
    ChainReactionDamage = damage;
    if (ChainReactionDamage < 0) ChainReactionDamage = 0;
}

void TiberiumClass::SetHarvestable(bool harvestable) {
    IsHarvestable = harvestable;
}

void TiberiumClass::SetChainReactable(bool reactable) {
    IsChainReactable = reactable;
}

void TiberiumClass::SetRegrowsAfterHarvest(bool regrows) {
    RegrowsAfterHarvest = regrows;
}

void TiberiumClass::SetRegrowthRate(int32 rate) {
    RegrowthRate = rate;
    if (RegrowthRate < 1) RegrowthRate = 1;
}

void TiberiumClass::SetValueMultiplier(float multiplier) {
    ValueMultiplier = multiplier;
    if (ValueMultiplier < 0.0f) ValueMultiplier = 0.0f;
}

void TiberiumClass::SetTintColor(const ColorStruct& color) {
    TintColor = color;
}

void TiberiumClass::SetRadarColor(const ColorStruct& color) {
    RadarColor = color;
}

TiberiumType TiberiumClass::GetType() const {
    return Type;
}

int32 TiberiumClass::GetCellLevel() const {
    return CellLevel;
}

int32 TiberiumClass::GetValue() const {
    return Value;
}

int32 TiberiumClass::GetMaxCellLevel() const {
    return MaxCellLevel;
}

bool TiberiumClass::IsHarvestableTiberium() const {
    return IsHarvestable;
}

bool TiberiumClass::IsChainReactableType() const {
    return IsChainReactable;
}

float TiberiumClass::GetExplosionChance() const {
    return ExplosionChance;
}

// ============================================================
// TiberiumManagerClass
// ============================================================

static TiberiumManagerClass* g_TiberiumManagerInstance = nullptr;

TiberiumManagerClass::TiberiumManagerClass()
    : GlobalGrowthRate(2), GlobalSpreadRate(1), GlobalDamageMultiplier(1.0f)
    , GlobalValueMultiplier(1.0f), EnableGrowth(true), EnableSpread(true)
    , EnableChainReaction(true), GlobalRegrowthRate(1), EnableRegrowth(true)
    , TiberiumTypesCount(0) {
    for (int32 i = 0; i < MAX_TIBERIUM_TYPES; ++i) {
        TiberiumTypes[i] = nullptr;
    }
}

TiberiumManagerClass::~TiberiumManagerClass() {
    for (int32 i = 0; i < MAX_TIBERIUM_TYPES; ++i) {
        if (TiberiumTypes[i]) {
            delete TiberiumTypes[i];
            TiberiumTypes[i] = nullptr;
        }
    }
    TiberiumTypesCount = 0;
}

TiberiumManagerClass* TiberiumManagerClass::GetInstance() {
    if (!g_TiberiumManagerInstance) {
        g_TiberiumManagerInstance = new TiberiumManagerClass();
    }
    return g_TiberiumManagerInstance;
}

void TiberiumManagerClass::Initialize() {
    TiberiumTypesCount = 0;
    for (int32 i = 0; i < MAX_TIBERIUM_TYPES; ++i) {
        if (TiberiumTypes[i]) {
            delete TiberiumTypes[i];
            TiberiumTypes[i] = nullptr;
        }
    }

    TiberiumClass* green = new TiberiumClass();
    green->Initialize(TiberiumType::Green, 1);
    TiberiumTypes[TiberiumTypesCount++] = green;

    TiberiumClass* blue = new TiberiumClass();
    blue->Initialize(TiberiumType::Blue, 1);
    TiberiumTypes[TiberiumTypesCount++] = blue;
}

void TiberiumManagerClass::UpdateAllTiberium() {
    if (!EnableGrowth || !MapClass::Instance) return;

    int32 mapW = MapClass::Instance->MapWidth;
    int32 mapH = MapClass::Instance->MapHeight;

    for (int32 y = 0; y < mapH; ++y) {
        for (int32 x = 0; x < mapW; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (pCell && pCell->IsTiberium()) {
                int32 tibType = pCell->Overlay;
                if (tibType >= 0 && tibType < TiberiumTypesCount && TiberiumTypes[tibType]) {
                    TiberiumClass* tib = TiberiumTypes[tibType];
                    tib->CellLevel = pCell->TiberiumValue;
                    tib->UpdateGrowth(cell);
                    pCell->TiberiumValue = tib->CellLevel;
                }
            }
        }
    }
}

void TiberiumManagerClass::ApplyDamageToInfantryInCell(const CellStruct& cell, ObjectClass* pObj) {
    if (!pObj || !MapClass::Instance) return;
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell || !pCell->IsTiberium()) return;

    int32 tibType = pCell->Overlay;
    if (tibType >= 0 && tibType < TiberiumTypesCount && TiberiumTypes[tibType]) {
        TiberiumTypes[tibType]->ApplyDamageToInfantry(pObj);
    }
}

void TiberiumManagerClass::ApplyDamageToVehiclesInCell(const CellStruct& cell, ObjectClass* pObj) {
    if (!pObj || !MapClass::Instance) return;
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell || !pCell->IsTiberium()) return;

    int32 tibType = pCell->Overlay;
    if (tibType >= 0 && tibType < TiberiumTypesCount && TiberiumTypes[tibType]) {
        TiberiumTypes[tibType]->ApplyDamageToVehicles(pObj);
    }
}

int32 TiberiumManagerClass::HarvestCell(const CellStruct& cell, int32 amount) {
    if (!MapClass::Instance) return 0;
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell || !pCell->IsTiberium()) return 0;

    int32 tibType = pCell->Overlay;
    if (tibType < 0 || tibType >= TiberiumTypesCount || !TiberiumTypes[tibType]) return 0;

    TiberiumClass* tib = TiberiumTypes[tibType];
    tib->CellLevel = pCell->TiberiumValue;
    int32 profit = tib->Harvest(amount);
    pCell->TiberiumValue = tib->CellLevel;

    return static_cast<int32>(profit * GlobalValueMultiplier);
}

bool TiberiumManagerClass::CheckChainReactionAtCell(const CellStruct& cell) {
    if (!EnableChainReaction || !MapClass::Instance) return false;
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell || !pCell->IsTiberium()) return false;

    int32 tibType = pCell->Overlay;
    if (tibType < 0 || tibType >= TiberiumTypesCount || !TiberiumTypes[tibType]) return false;

    TiberiumClass* tib = TiberiumTypes[tibType];
    tib->CellLevel = pCell->TiberiumValue;
    return tib->CheckChainReaction(cell);
}

void TiberiumManagerClass::SetGlobalGrowthRate(int32 rate) {
    GlobalGrowthRate = rate;
    if (GlobalGrowthRate < 1) GlobalGrowthRate = 1;
    if (GlobalGrowthRate > 16) GlobalGrowthRate = 16;
}

void TiberiumManagerClass::SetGlobalSpreadRate(int32 rate) {
    GlobalSpreadRate = rate;
    if (GlobalSpreadRate < 1) GlobalSpreadRate = 1;
    if (GlobalSpreadRate > 16) GlobalSpreadRate = 16;
}

void TiberiumManagerClass::SetGlobalValueMultiplier(float multiplier) {
    GlobalValueMultiplier = multiplier;
    if (GlobalValueMultiplier < 0.0f) GlobalValueMultiplier = 0.0f;
}

void TiberiumManagerClass::SetGrowthEnabled(bool enabled) {
    EnableGrowth = enabled;
}

void TiberiumManagerClass::SetSpreadEnabled(bool enabled) {
    EnableSpread = enabled;
}

void TiberiumManagerClass::SetChainReactionEnabled(bool enabled) {
    EnableChainReaction = enabled;
}

void TiberiumManagerClass::SetRegrowthEnabled(bool enabled) {
    EnableRegrowth = enabled;
}

void TiberiumManagerClass::SetGlobalRegrowthRate(int32 rate) {
    GlobalRegrowthRate = rate;
    if (GlobalRegrowthRate < 1) GlobalRegrowthRate = 1;
}

TiberiumClass* TiberiumManagerClass::GetTiberiumType(int32 index) const {
    if (index < 0 || index >= TiberiumTypesCount) return nullptr;
    return TiberiumTypes[index];
}

int32 TiberiumManagerClass::GetTiberiumTypeCount() const {
    return TiberiumTypesCount;
}

int32 TiberiumManagerClass::GetGlobalGrowthRate() const {
    return GlobalGrowthRate;
}

int32 TiberiumManagerClass::GetGlobalSpreadRate() const {
    return GlobalSpreadRate;
}

float TiberiumManagerClass::GetGlobalValueMultiplier() const {
    return GlobalValueMultiplier;
}