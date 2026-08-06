#pragma once

#include "../Abstract/ObjectTypeClass.h"

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

    virtual bool LoadFromINI(CCINIClass* pINI) override;
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
};
