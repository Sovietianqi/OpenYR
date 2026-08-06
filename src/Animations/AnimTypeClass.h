#pragma once

#include "../Abstract/ObjectTypeClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Particles/ParticleTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================

class OverlayTypeClass;
class WarheadTypeClass;
class HouseClass;
class SHPStruct;

// BlitterFlags is defined in Rendering/Surface.h

// ============================================================================
// AnimTypeClass
// ============================================================================

class NOVTABLE AnimTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::AnimType;

    static DynamicVectorClass<AnimTypeClass*>* Array;

    static AnimTypeClass* Find(const char* pID);
    static AnimTypeClass* FindByIndex(int32 index);
    static int32 GetCount();

    AnimTypeClass(const char* pID) noexcept;
    virtual ~AnimTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual bool SpawnAtMapCoords(CellStruct* pMapCoords, HouseClass* pOwner);
    virtual ObjectClass* CreateObject(HouseClass* pOwner);

    virtual SHPStruct* LoadImage();
    virtual void Load2DArt();

    bool LoadFromINI(CCINIClass* pINI);
    ObjectClass* CreateAnim();

    // Helper methods
    bool HasDamage() const;
    bool HasSound() const;
    bool HasLight() const;
    bool IsGroundLayer() const;
    bool IsAirLayer() const;
    bool IsTopLayer() const;
    int32 GetTotalFrames() const;
    double GetAnimationDuration() const;
    int32 GetLoopStartFrame() const;
    int32 GetLoopEndFrame() const;

    static void RegisterAll();

protected:
    explicit AnimTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}

public:
    int32 MiddleFrameIndex;
    int32 MiddleFrameWidth;
    int32 MiddleFrameHeight;
    BYTE unknown_2A4;
    double Damage;
    int32 Rate;
    int32 Start;
    int32 LoopStart;
    int32 LoopEnd;
    int32 End;
    int32 LoopCount;
    AnimTypeClass* Next;
    int32 SpawnsParticle;
    int32 NumParticles;
    int32 DetailLevel;
    int32 TranslucencyDetailLevel;
    RandomStruct RandomLoopDelay;
    RandomStruct RandomRate;
    int32 Translucency;
    AnimTypeClass* Spawns;
    int32 SpawnCount;
    int32 Report;
    int32 StopSound;
    AnimTypeClass* BounceAnim;
    AnimTypeClass* ExpireAnim;
    AnimTypeClass* TrailerAnim;
    int32 TrailerSeperation;
    double Elasticity;
    double MinZVel;
    double unknown_double_320;
    double MaxXYVel;
    WarheadTypeClass* Warhead;
    int32 DamageRadius;
    OverlayTypeClass* TiberiumSpawnType;
    int32 TiberiumSpreadRadius;
    int32 YSortAdjust;
    int32 YDrawOffset;
    int32 ZAdjust;
    int32 MakeInfantry;
    int32 RunningFrames;
    bool IsFlamingGuy;
    bool IsVeins;
    bool IsMeteor;
    bool TiberiumChainReaction;
    bool IsTiberium;
    bool HideIfNoOre;
    bool Bouncer;
    bool Tiled;
    bool ShouldUseCellDrawer;
    bool UseNormalLight;
    bool IsNuke;
    bool IsIonCannon;
    bool DemandLoad;
    bool FreeLoad;
    bool IsAnimatedTiberium;
    bool AltPalette;
    bool Normalized;
    Layer Layer_;
    bool DoubleThick;
    bool Flat;
    bool Translucent;
    bool Scorch;
    bool Flamer;
    bool Crater;
    bool ForceBigCraters;
    bool Sticky;
    bool PingPong;
    bool Reverse;
    bool Shadow;
    bool PsiWarning;
    bool ShouldFogRemove;

    // Additional fields
    char AnimationName[0x20];
    int32 NumFrames;
    int32 FrameRate;
    bool IsLooping;
    bool IsInvisible;
    bool IsFlameThrower;
    bool IsNormalized;
    bool IsBigGrey;
    bool IsParticle;
    bool IsRailgun;
    int32 MakeInfantryOwner;
    int32 LightSize;
    double LightIntensity;
    int32 LightVisibility;
    double LightRedTint;
    double LightGreenTint;
    double LightBlueTint;
    int32 LightFlashFrames;
    SHPStruct* AnimShape;
    SHPStruct* ShadowShape;
};