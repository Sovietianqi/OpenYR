#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Containers/DynamicVectorClass.h"

class BulletClass {
public:
    static DynamicVectorClass<BulletClass*>* Array;
    BulletClass(BulletTypeClass* pType) noexcept;
    virtual ~BulletClass();

    void Init(BulletTypeClass* pType, CoordStruct source, CoordStruct target,
              TechnoClass* pOwner, int32 damage, WarheadTypeClass* pWarhead,
              WeaponTypeClass* pWeapon);
    void Update();
    void Detonate();
    void Destroy();
    void SetTarget(CoordStruct target);
    void SetOwner(TechnoClass* pOwner);
    TechnoClass* GetOwner() const;
    CoordStruct GetLocation() const;
    CoordStruct GetTarget() const;
    bool IsActive() const;
    bool IsDetonated() const;
    int32 GetDamage() const;
    bool IsBulletGravity() const;
    bool IsBulletInaccurate() const;
    bool IsTargetingAA() const;
    bool IsTargetingAG() const;
    BulletTypeClass* GetBulletType() const;
    FacingType GetFacing() const;
    void SetFacing(FacingType facing);
    int32 GetSpeed() const;
    void SetSpeed(int32 speed);
    void MoveTo(CoordStruct location);
    void SetRange(int32 range);
    void SetWeaponType(WeaponTypeClass* pWeapon);
    WeaponTypeClass* GetWeaponType() const;

    // Methods mirroring BulletClass_* in the original binary
    static BulletClass* Fire(BulletTypeClass* pType, WeaponTypeClass* pWeapon,
                             CoordStruct source, CoordStruct target,
                             TechnoClass* pOwner, int32 damage,
                             WarheadTypeClass* pWarhead);
    void Shrapnel(int32 damage, WarheadTypeClass* pWarhead);
    void SetMovement(CoordStruct source, CoordStruct target, int32 speed);
    void NukeMaker(bool activate);
    void TargetWentAway();
    void Draw(Point2D* pCoord, RectangleStruct* pRect);
    int32 GetAnimRate() const;
    void Initialize(BulletTypeClass* pType);

private:
    void CheckForCollision();
    bool CanHit(TechnoClass* pTarget) const;
    void Impact(TechnoClass* pTarget);

public:
    BulletTypeClass* Class;
    TechnoClass* Owner;
    TechnoClass* Target;
    WeaponTypeClass* WeaponType;
    bool IsBulletActive;
    bool IsBulletDetonated;
    bool IsInAir;
    bool IsIncoming;
    bool IsFalling;
    bool IsParachuted;
    int32 Health;
    int32 Strength;
    int32 Speed;
    int32 Range;
    int32 DistanceTraveled;
    int32 Timer;
    int32 CreationFrame;
    FacingType Facing;
    bool Bright;
    bool IsGravity;
    bool IsAccurate;
    bool IsInaccurate;
    bool IsAnimating;
    bool IsInvisible;
    bool IsProximityArmed;
    bool IsSplinter;
    bool IsFlakScatter;
    bool IsAA;
    bool IsAG;
    bool IsAS;
    CoordStruct Location;
    CoordStruct StartCoords;
    CoordStruct TargetCoords;
    CoordStruct LastCoords;
    BulletClass* Next;
    BulletClass* Prev;
};