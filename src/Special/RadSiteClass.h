#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class BlitterClass;
class DSurface;

static constexpr int32 MAX_RAD_SITES = 32;

class RadSiteClass {
public:
    static DynamicVectorClass<RadSiteClass*>* Array;

    RadSiteClass();
    ~RadSiteClass();

    void Initialize(const CoordStruct& pos, int32 radius, int32 level, int32 duration);
    void InitializeNuclear(const CoordStruct& pos, int32 radius, int32 level);
    void Release();
    void ReleaseAffectedCells();
    void Update();
    void ApplyRadiationDamage();
    float CalculateRadLevelAtDistance(int32 distance) const;
    void ApplyRadDamageToCell(const CellStruct& cell, int32 radLevel);
    void UpdateVisualEffects();
    void RenderGlowEffect(BlitterClass* blitter, DSurface* surface);

    void SetRadLevel(int32 level);
    void SetRadRadius(int32 radius);
    void SetDuration(int32 duration);
    void SetSpreadRate(int32 rate);
    void SetDecayRate(int32 rate);
    void SetDamagePerTick(int32 damage);
    void SetDamageInterval(int32 interval);
    void SetNuclear(bool nuclear);
    void SetOwnerHouse(int32 house);
    void SetSpreading(bool spreading);
    void SetGlowColor(const ColorStruct& color);
    void SetPosition(const CoordStruct& pos);

    int32 GetRadLevel() const;
    int32 GetRadRadius() const;
    int32 GetDuration() const;
    int32 GetGlowIntensity() const;
    float GetDesaturation() const;
    bool IsActiveSite() const;

    CoordStruct Position;
    int32 RadLevel;
    int32 MaxRadLevel;
    int32 RadRadius;
    int32 MaxRadius;
    int32 Duration;
    int32 CurrentDuration;
    int32 SpreadRate;
    int32 DecayRate;
    bool IsActive;
    bool IsNuclear;
    bool IsSpreading;
    int32 OwnerHouse;
    int32 DamagePerTick;
    int32 DamageInterval;
    int32 DamageTimer;
    int32 VisualTimer;
    int32 GlowIntensity;
    ColorStruct GlowColor;
    float Desaturation;
    CellStruct* AffectedCells;
    int32 AffectedCellCount;
    int32 RadValue;
    int32 RadIntensity;
};

class RadSiteManagerClass {
public:
    RadSiteManagerClass();
    ~RadSiteManagerClass();

    static RadSiteManagerClass* GetInstance();

    int32 CreateRadSite(const CoordStruct& pos, int32 radius, int32 level, int32 duration);
    int32 CreateNuclearSite(const CoordStruct& pos, int32 radius, int32 level);
    bool RemoveRadSite(int32 index);
    void RemoveAllSites();
    void UpdateAllSites();
    int32 CalculateRadLevelAt(const CoordStruct& pos) const;
    int32 GetActiveCount() const;
    RadSiteClass* GetRadSite(int32 index) const;
    int32 GetGlobalRadLevel() const;
    void RenderAllGlowEffects(BlitterClass* blitter, DSurface* surface);

    RadSiteClass* Sites[MAX_RAD_SITES];
    int32 ActiveCount;
    int32 GlobalRadLevel;
};