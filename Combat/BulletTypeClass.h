#pragma once

#include "../Abstract/ObjectTypeClass.h"

class BulletClass;
class HouseClass;
struct CoordStruct;

class CCINIClass;
class CRCEngine;

// ============================================================================
// BulletTypeClass - definition of a projectile type
// Defines the flight characteristics of a weapon projectile: rotation rate,
// arc trajectory, target-type flags (AA/AG/ASW/AN), and collision behaviour.
// Parsed from the [ProjectileTypes] INI block.
// ============================================================================
class BulletTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::BulletType;

    static DynamicVectorClass<BulletTypeClass*>* Array;

    static BulletTypeClass* Find(const char* pID);
    static BulletTypeClass* FindOrAllocate(const char* pID);
    static int32 GetCount();

    BulletTypeClass(const char* pID) noexcept;
    explicit BulletTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}

    virtual ~BulletTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual AbstractType GetAbstractDerivationID() const;

    // asm: BulletTypeClass_PointerGotInvalid (0x46C8xx) retn 8 -> 2 args.
    //   Clears the cached Trailer anim pointer (+0x2D8) when it is the victim.
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    virtual bool LoadFromINI(CCINIClass* pINI) override;

    // ------------------------------------------------------------------------
    // asm: BulletType_CreateBullet (0x46B060)
    //
    //   BulletClass *__fastcall BulletType_CreateBullet(
    //       BulletTypeClass *pType, CoordStruct *pPos, HouseClass *pOwner,
    //       int32 a4, int32 a8, int32 warhead, int32 speed,
    //       bool bright, int32 a7)
    //
    //   建一颗子弹实例并完成初始化。asm 逐段：
    //     CoCreateInstance(CLSID_BulletClass, 0, 7, IID_Invalid1, &bullet)
    //     test eax,eax / jge ok / return 0
    //     BulletClass_Initialize(bullet, type, pos, owner, damage, WH,
    //                            speed, bright, a7)
    //     mov eax,esi / retn 14h
    //
    //   原版走 COM 激活；本项目直接堆分配 BulletClass 并由构造函数完成
    //   类型挂接（等价于原版的 Initialize 前半段）。
    // ------------------------------------------------------------------------
    static BulletClass* Create_Bullet(BulletTypeClass* pType,
                                      const CoordStruct* pPos,
                                      HouseClass* pOwner,
                                      int32 a4, int32 a8,
                                      int32 warhead, int32 speed,
                                      bool bright, int32 a7);
    virtual bool SaveToINI(CCINIClass* pINI);
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // Task-required accessor API
    bool Is_Arcing() const;
    bool Is_Homing() const;
    bool Is_AA() const;
    bool Is_AG() const;
    bool Is_ASW() const;
    bool Is_AN() const;
    bool Is_Inviso() const;
    bool Is_Dropping() const;
    bool Is_FlakScatter() const;
    bool Is_SubjectToCliffs() const;
    bool Is_SubjectToElevation() const;
    bool Is_SubjectToWalls() const;
    bool Is_Inaccurate() const;
    bool Is_Level() const;
    bool Is_Proximity() const;
    bool Is_CourseLocked() const;
    bool Has_Shadow() const;
    int32 Get_Speed() const;
    int32 Get_ROT() const;
    int32 Get_Arm() const;

    int32 ROT;
    bool Arcing;
    bool Dropping;
    bool Inviso;
    bool FlakScatter;
    bool SubjectToCliffs;
    bool SubjectToElevation;
    bool SubjectToWalls;
    bool VeryHigh;
    bool High;
    bool Shadow;
    bool AA;
    bool AG;
    bool ASW;
    bool AN;
    bool Inaccurate;
    bool NoRotate;
    bool Level;
    bool Proximity;
    bool Ranged;
    bool Scalable;
    int32 ScaledSpawnDelay;
    bool CourseLocked;
    int32 Arm;
    int32 ProjectileSpeed;
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    int32        CourseLockDuration;
    double       Elasticity;
    int32        Acceleration;
    int32        Color;
    bool         Floater;
    bool         Degenerates;
    bool         Bouncy;
    bool         Airburst;
    int32        Cluster;
    // NOTE: the "Image" key is stored in the inherited ObjectTypeClass::Image
    //   (+0x1F8 in the binary), not a separate field -- the asm writes it there
    //   and the base art loader resolves ImageSHP from it.
    AnimTypeClass* Trailer;
    int32        SpawnDelay;
    // asm: BulletTypeClass_SetSpawnDelay (0x46C8xx) -- writes the *scaled*
    //   spawn delay (+0x2E8) used by BulletClass_Update's trailer timer.
    void SetSpawnDelay(int32 delay) { ScaledSpawnDelay = delay; }
    int32 GetSpawnDelay() const { return SpawnDelay; }
    bool         Rotates;
    bool         Flat;
    WeaponTypeClass* AirburstWeapon;
    WeaponTypeClass* ShrapnelWeapon;
    int32        ShrapnelCount;
    int32        DetonationAltitude;
    bool         Vertical;
    bool         FirersPalette;
    int32        AnimLow;
    int32        AnimHigh;
    int32        AnimRate;
    bool         AnimPalette;
    // asm: BulletTypeClass+0x2DC -- trajectory selector consumed by
    // WeaponTypeClass_CalculateSpeed (non-zero disables auto speed calc).
    int32        TrajectoryKind;
    // asm: BulletTypeClass+0x295 -- requests half-gravity ballistics.
    bool         HalfGravity;
};
