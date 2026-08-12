#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"
#include "../Containers/DynamicVectorClass.h"

enum class TiberiumType : int32 {
    Green = 0,
    Blue = 1,
    Riparius = 2,
    Cruentus = 3,
    Vinifera = 4,
    Aboreus = 5,
    Arboreus = 6,
    Count = 7
};

static constexpr int32 MAX_TIBERIUM_TYPES = 8;

class TiberiumClass {
public:
    static DynamicVectorClass<TiberiumClass*>* Array;

    TiberiumClass();
    ~TiberiumClass();

    void Initialize(TiberiumType type, int32 startingLevel);
    void UpdateGrowth(const CellStruct& cell);
    void SpreadToNeighbors(const CellStruct& cell);
    int32 CalculateHarvestValue() const;
    int32 Harvest(int32 amount);
    void ApplyDamageToInfantry(ObjectClass* pObj);
    void ApplyDamageToVehicles(ObjectClass* pObj);
    bool CheckChainReaction(const CellStruct& cell);
    void TriggerChainReaction(const CellStruct& cell);
    void UpdateRegrowth();

    void SetType(TiberiumType type);
    void SetCellLevel(int32 level);
    void SetValue(int32 value);
    void SetGrowthRate(int32 rate);
    void SetSpreadRate(int32 rate);
    void SetDamageToInfantry(int32 damage);
    void SetExplosionChance(float chance);
    void SetChainReactionRadius(int32 radius);
    void SetChainReactionDamage(int32 damage);
    void SetHarvestable(bool harvestable);
    void SetChainReactable(bool reactable);
    void SetRegrowsAfterHarvest(bool regrows);
    void SetRegrowthRate(int32 rate);
    void SetValueMultiplier(float multiplier);
    void SetTintColor(const ColorStruct& color);
    void SetRadarColor(const ColorStruct& color);

    TiberiumType GetType() const;
    int32 GetCellLevel() const;
    int32 GetValue() const;
    int32 GetMaxCellLevel() const;
    bool IsHarvestableTiberium() const;
    bool IsChainReactableType() const;
    float GetExplosionChance() const;

    TiberiumType Type;
    int32 Value;
    int32 GrowthRate;
    int32 SpreadRate;
    int32 MaxGrowth;
    int32 MinGrowth;
    int32 CellLevel;
    int32 MaxCellLevel;
    int32 DamageToInfantry;
    int32 DamageInterval;
    bool IsHarvestable;
    bool IsChainReactable;
    int32 ChainReactionRadius;
    int32 ChainReactionDamage;
    float ExplosionChance;
    int32 ExplosionDamage;
    int32 FlowRate;
    int32 FlowDirection;
    bool IsFlowing;
    int32 GrowthTimer;
    int32 SpreadTimer;
    int32 CellCount;
    int32 SubType;
    int32 ImageIndex;
    ColorStruct TintColor;
    int32 Power;
    int32 WeaponIndex;
    int32 RevolutionIndex;
    float ValueMultiplier;
    float SpreadChance;
    bool RegrowsAfterHarvest;
    int32 RegrowthRate;
    int32 RegrowthTimer;
    int32 DamageToVehicles;
    bool Optimized;
    bool IsSpecial;
    bool IsWeaponTiberium;
    ColorStruct RadarColor;
};

class TiberiumManagerClass {
public:
    TiberiumManagerClass();
    ~TiberiumManagerClass();

    static TiberiumManagerClass* GetInstance();

    void Initialize();
    void UpdateAllTiberium();
    void ApplyDamageToInfantryInCell(const CellStruct& cell, ObjectClass* pObj);
    void ApplyDamageToVehiclesInCell(const CellStruct& cell, ObjectClass* pObj);
    int32 HarvestCell(const CellStruct& cell, int32 amount);
    bool CheckChainReactionAtCell(const CellStruct& cell);

    void SetGlobalGrowthRate(int32 rate);
    void SetGlobalSpreadRate(int32 rate);
    void SetGlobalValueMultiplier(float multiplier);
    void SetGrowthEnabled(bool enabled);
    void SetSpreadEnabled(bool enabled);
    void SetChainReactionEnabled(bool enabled);
    void SetRegrowthEnabled(bool enabled);
    void SetGlobalRegrowthRate(int32 rate);

    TiberiumClass* GetTiberiumType(int32 index) const;
    int32 GetTiberiumTypeCount() const;
    int32 GetGlobalGrowthRate() const;
    int32 GetGlobalSpreadRate() const;
    float GetGlobalValueMultiplier() const;

    int32 GlobalGrowthRate;
    int32 GlobalSpreadRate;
    float GlobalDamageMultiplier;
    float GlobalValueMultiplier;
    bool EnableGrowth;
    bool EnableSpread;
    bool EnableChainReaction;
    int32 GlobalRegrowthRate;
    bool EnableRegrowth;
    TiberiumClass* TiberiumTypes[MAX_TIBERIUM_TYPES];
    int32 TiberiumTypesCount;
};