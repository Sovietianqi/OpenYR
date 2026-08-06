#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

class BlitterClass;
class DSurface;

static constexpr int32 MAX_DISK_LASERS = 16;

class DiskLaserClass {
public:
    DiskLaserClass();
    ~DiskLaserClass();

    void Initialize(const CoordStruct& pos, const CoordStruct& target, int32 damage, int32 lifetime);
    void ReleaseSegments();
    void Update();
    void UpdateRotation();
    void UpdateColorCycle();
    void ApplyBeamDamage();
    void RenderEffect(BlitterClass* blitter, DSurface* surface);

    void SetBeamWidth(int32 width);
    void SetRotationSpeed(float speed);
    void SetScanRange(float range);
    void SetScanning(bool scanning);
    void SetTracking(bool tracking);
    void SetDamagePerTick(int32 damage);
    void SetDamageInterval(int32 interval);
    void SetLifetime(int32 lifetime);
    void SetColors(const ColorStruct& c1, const ColorStruct& c2, const ColorStruct& c3);
    void SetColorSpeed(float speed);
    void SetFadeInTime(int32 time);
    void SetFadeOutTime(int32 time);
    void SetOwnerHouse(int32 house);
    void SetTarget(const CoordStruct& target);
    void SetPosition(const CoordStruct& pos);

    bool IsActiveLaser() const;
    int32 GetCurrentLifetime() const;
    int32 GetBeamWidth() const;
    float GetCurrentAngle() const;

    CoordStruct Position;
    CoordStruct TargetPosition;
    int32 BeamWidth;
    int32 BeamLength;
    float CurrentAngle;
    float RotationSpeed;
    float ScanAngle;
    float ScanRange;
    float ScanDirection;
    bool IsActive;
    bool IsScanning;
    int32 DamagePerTick;
    int32 DamageInterval;
    int32 DamageTimer;
    int32 Lifetime;
    int32 MaxLifetime;
    int32 CurrentLifetime;
    ColorStruct Color1;
    ColorStruct Color2;
    ColorStruct Color3;
    float ColorPhase;
    float ColorSpeed;
    bool IsTracking;
    int32 FadeInTime;
    int32 FadeOutTime;
    uint8 CurrentAlpha;
    int32 OwnerHouse;
    int32 TargetID;
    CoordStruct* BeamSegments;
    int32 SegmentCount;
};

class DiskLaserManagerClass {
public:
    DiskLaserManagerClass();
    ~DiskLaserManagerClass();

    static DiskLaserManagerClass* GetInstance();

    int32 CreateDiskLaser(const CoordStruct& pos, const CoordStruct& target, int32 damage, int32 lifetime);
    bool RemoveDiskLaser(int32 index);
    void RemoveAllLasers();
    void UpdateAllLasers();
    void RenderAllEffects(BlitterClass* blitter, DSurface* surface);
    int32 GetActiveCount() const;
    DiskLaserClass* GetDiskLaser(int32 index) const;

    DiskLaserClass* Lasers[MAX_DISK_LASERS];
    int32 ActiveCount;
};