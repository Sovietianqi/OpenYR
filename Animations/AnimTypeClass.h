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
    static AnimTypeClass* FindOrAllocate(const char* pID);
    static AnimTypeClass* FindByIndex(int32 index);

    // AnimClass_FindIndex - the ordinal of the anim type named pID in the
    // registry, or -1.  RulesClass_Addition_Powerups stores the ordinal in
    // its anim table rather than the pointer.
    static int32 FindIndex(const char* pID);
    static int32 GetCount();

    AnimTypeClass(const char* pID) noexcept;
    virtual ~AnimTypeClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    // asm: AnimTypeClass_GetAbstractDerivationID (mov eax, 5 ; retn).
    virtual AbstractType GetAbstractDerivationID() const;

    // asm: AnimTypeClass_GetArrayIndex (mov eax, [ecx+294h] ; retn).
    virtual int32 GetArrayIndex() const override;

    // asm: AnimTypeClass_GetCRC - chains the base CRC then folds in the
    // behaviour-relevant anim fields in the original order.
    virtual int32 GetCRC() const;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // asm: AnimTypeClass_PointerGotInvalid (retn 8).  Clears the cached
    // Next anim (asm +0x2C8) when it was the expired pointer.
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed);

    // asm: AnimTypeClass_GetImage - demand-loads the SHP when the type is
    // flagged DemandLoad and no image is resident yet, then returns it.
    virtual SHPStruct* GetImage() const override;

    virtual bool SpawnAtMapCoords(CellStruct* pMapCoords, HouseClass* pOwner);

    // Deprecated/alternate spawn entry point retained for vtable completeness.
    // The original returns false unconditionally.  Mirrors AnimTypeClass_SpawnAnimAtMapCoords.
    bool SpawnAnimAtMapCoords(CellStruct* pMapCoords, HouseClass* pOwner);
    virtual ObjectClass* CreateObject(HouseClass* pOwner);

    // Releases the demand-loaded SHP image.  The original only frees the
    // image when both image-loaded flag bytes (0x35E/0x35F) are set and the
    // image pointer (0xA4) is non-null; afterwards the pointer and the
    // companion flag byte (0xA8) are cleared.  Mirrors AnimTypeClass_FreeImage.
    void FreeImage();

    virtual SHPStruct* LoadImage();
    virtual void Load2DArt(int32 idxTheater);

    bool LoadFromINI(CCINIClass* pINI) override;

    // Shared resolver for the anim-reference keys (BounceAnim / ExpireAnim /
    // TrailerAnim): absent key keeps the current value, "<none>"/"none"
    // clears it, otherwise it resolves through the registry.
    static AnimTypeClass* ResolveAnimRef(CCINIClass* pINI, const char* section,
                                         const char* key, AnimTypeClass* current);

    // asm: AnimTypeClass_CreateFromINIList - refreshes the demand-loaded SHP
    // of every registered anim type for the current theater.
    static void CreateFromINIList(int32 idxTheater);

    // asm: AnimTypeClass_LoadTheaterSpecificArt (retn 4) - reloads this type's
    // SHP for idxTheater when it carries theater-specific art.
    virtual void LoadTheaterSpecificArt(int32 idxTheater) override;

    // asm: AnimTypeClass_FindOrAllocateFromList(char*, VectorPtrAnimTypeClass*)
    static AnimTypeClass* FindOrAllocateFromList(
        const char* pID, DynamicVectorClass<AnimTypeClass*>* pList);

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
    // mutable: GetImage() is const yet performs the lazy demand-load, matching
    // the original's write to the image slot from a const-visible accessor.
    mutable SHPStruct* AnimShape;
    mutable SHPStruct* ShadowShape;
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    bool         Theater;
    bool         NewTheater;
    Layer        LayerValue;
    int32 StartSound;
};