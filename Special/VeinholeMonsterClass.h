#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class BlitterClass;
class DSurface;

static constexpr int32 MAX_VEINHOLES = 16;

class VeinholeMonsterClass {
public:
    VeinholeMonsterClass();
    ~VeinholeMonsterClass();

    void Initialize(const CoordStruct& pos, int32 radius, int32 lifetime);
    void Release();
    void ReleaseVeinCells();
    void Update();
    void GrowVeins();
    void PlaceVeinCell(const CellStruct& cell);
    void UpdateTactiles();
    void ApplyDamage();
    void ApplyVeinDamageToCell(const CellStruct& cell);
    void SpawnUnit();
    void StartDeath();
    void UpdateDeath();
    void RemoveAllVeinCells();
    void RenderTactiles(BlitterClass* blitter, DSurface* surface);

    void SetGrowthRate(int32 rate);
    void SetMaxRadius(int32 radius);
    void SetLifetime(int32 lifetime);
    void SetSpawnInterval(int32 interval);
    void SetMaxSpawnCount(int32 count);
    void SetBuildingDamage(int32 damage);
    void SetUnitDamage(int32 damage);
    void SetTactileCount(int32 count);
    void SetPosition(const CoordStruct& pos);
    void SetSpawning(bool spawning);
    void SetTargetBuilding(int32 buildingID);

    int32 GetCurrentGrowthRadius() const;
    int32 GetLifeTimer() const;
    bool IsActiveMonster() const;
    bool IsMonsterDying() const;

    CoordStruct Position;
    int32 MaxGrowthRadius;
    int32 CurrentGrowthRadius;
    int32 GrowthRate;
    int32 VeinCount;
    int32 TactileCount;
    int32 MaxTactileCount;
    int32 TactileBaseLength;
    int32 TactileLength;
    int32 TactileMaxLength;
    float TactileAngleOffset;
    float TactileAnimationPhase;
    float TactileAnimationSpeed;
    bool IsActive;
    bool IsSpawning;
    int32 SpawnTimer;
    int32 SpawnInterval;
    int32 SpawnCount;
    int32 MaxSpawnCount;
    int32 DamagePerTick;
    int32 DamageInterval;
    int32 DamageTimer;
    int32 AttackRange;
    int32 BuildingDamage;
    int32 UnitDamage;
    int32 LifeTimer;
    int32 MaxLifetime;
    bool IsDying;
    int32 DeathTimer;
    int32 DeathDuration;
    CellStruct* VeinCells;
    int32 VeinCellCount;
    float* TactileAngles;
    float* TactileExtensions;
    int16 TargetCell_X;
    int16 TargetCell_Y;
    int32 GrowthDirection;
    int32 TargetBuildingID;
    CellStruct VeinDirections[8];
    float VeinGrowth[8];
};

class VeinholeMonsterManagerClass {
public:
    VeinholeMonsterManagerClass();
    ~VeinholeMonsterManagerClass();

    static VeinholeMonsterManagerClass* GetInstance();

    int32 CreateMonster(const CoordStruct& pos, int32 radius, int32 lifetime);
    bool RemoveMonster(int32 index);
    void RemoveAllMonsters();
    void UpdateAllMonsters();
    void RenderAllTactiles(BlitterClass* blitter, DSurface* surface);
    int32 GetActiveCount() const;
    VeinholeMonsterClass* GetMonster(int32 index) const;

    VeinholeMonsterClass* Monsters[MAX_VEINHOLES];
    int32 ActiveCount;
};