#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class BlitterClass;
class DSurface;

static constexpr int32 MAX_EMPULSES = 16;

class EMPulseClass {
public:
    EMPulseClass();
    ~EMPulseClass();

    void Initialize(const CoordStruct& pos, int32 radius, int32 duration, int32 damage);
    void Update();
    void CreatePulseRing();
    void ApplyEMPDisable();
    void ApplyEMPToCell(const CellStruct& cell, int32 distance);
    bool IsEMPImmune(ObjectClass* pObj) const;
    void RenderEffect(BlitterClass* blitter, DSurface* surface);

    void SetRadius(int32 radius);
    void SetDuration(int32 duration);
    void SetDisableDuration(int32 duration);
    void SetDamageAmount(int32 damage);
    void SetPulseColor(const ColorStruct& color);
    void SetMaxPulses(int32 count);
    void SetPulseInterval(int32 interval);
    void SetOwnerHouse(int32 house);
    void SetPosition(const CoordStruct& pos);

    int32 GetCurrentRadius() const;
    int32 GetCurrentDuration() const;
    bool IsActivePulse() const;
    bool HasExpandedFully() const;

    CoordStruct Position;
    int32 Radius;
    int32 MaxRadius;
    int32 Duration;
    int32 MaxDuration;
    int32 ExpandSpeed;
    int32 CurrentRadius;
    int32 CurrentDuration;
    bool IsActive;
    bool IsExpanding;
    bool HasExpanded;
    ColorStruct PulseColor;
    uint8 PulseAlpha;
    uint8 FlashAlpha;
    int32 RingThickness;
    int32 PulseCount;
    int32 MaxPulses;
    int32 PulseInterval;
    int32 PulseTimer;
    int32 CurrentPulse;
    int32 DamageAmount;
    int32 DisableDuration;
    int32 OwnerHouse;
};

class EMPulseManagerClass {
public:
    EMPulseManagerClass();
    ~EMPulseManagerClass();

    static EMPulseManagerClass* GetInstance();

    int32 CreateEMPulse(const CoordStruct& pos, int32 radius, int32 duration, int32 damage);
    bool RemoveEMPulse(int32 index);
    void RemoveAllPulses();
    void UpdateAllPulses();
    void RenderAllEffects(BlitterClass* blitter, DSurface* surface);
    int32 GetActiveCount() const;
    EMPulseClass* GetEMPulse(int32 index) const;

    EMPulseClass* Pulses[MAX_EMPULSES];
    int32 ActiveCount;
};