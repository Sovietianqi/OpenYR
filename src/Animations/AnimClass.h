#pragma once

#include "../Abstract/ObjectClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/CoordStruct.h"
#include "../Rendering/Surface.h"

// ============================================================================
// AnimStruct
// ============================================================================

struct AnimStruct {
    int32 Rate;
    int32 Start;
    int32 LoopStart;
    int32 LoopEnd;
    int32 End;
    int32 LoopCount;
    int32 Value;
    int32 CurrentLoop;
    bool IsActive;
    bool IsLooping;
    bool HasStarted;
    char PaletteName[16];

    AnimStruct() : Rate(1), Start(0), LoopStart(-1), LoopEnd(-1), End(0),
        LoopCount(0), Value(0), CurrentLoop(0),
        IsActive(false), IsLooping(false), HasStarted(false) {
        for (int32 i = 0; i < 16; ++i) PaletteName[i] = 0;
    }

    bool IsAtEnd() const { return Value >= End; }
    void Reset() { Value = Start; CurrentLoop = 0; }
    void Advance() { ++Value; }
    bool HasLooped() const { return Value > End; }
};

// ============================================================================
// Forward declarations
// ============================================================================

class AnimTypeClass;
class ObjectClass;
class HouseClass;
class BulletClass;
class SHPStruct;

// ============================================================================
// AnimClass
// ============================================================================

class NOVTABLE AnimClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Anim;

    static DynamicVectorClass<AnimClass*>* Array;

    AnimClass(AnimTypeClass* pAnimType, const CoordStruct& loc,
        int32 loopDelay = 0, int32 loopCount = 1,
        uint32 flags = 0x600, int32 forceZAdjust = 0,
        bool reverse = false) noexcept;
    AnimClass() noexcept;
    virtual ~AnimClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual void Update() override;
    virtual void PointerExpired(AbstractClass* pAbstract, bool removed) override;

    // Animation extras
    int32 AnimExtras();
    int32 GetEnd() const;
    void SetOwnerObject(ObjectClass* pOwner);
    void SetCoords(const CoordStruct& coord) { Location = coord; }
    void SetOwner(HouseClass* pOwner) { Owner = pOwner; }

    void Pause();
    void Unpause();

    // Animation control
    void Play();
    void Stop();
    void SetFrame(int32 frame);
    void NextFrame();

    // Special effects
    void MakeInfantry();
    void CreateExplosion();
    void CreateFire();
    void CreateSmoke();
    void CreateSpark();
    void KillAnim();
    void FireNuke();

    // Static factory methods
    static AnimClass* CreateNukeAnim(const CoordStruct& pos);
    static AnimClass* CreateIonCannonAnim(const CoordStruct& pos);
    static AnimClass* CreateConYardAnim(const CoordStruct& pos);
    static AnimClass* CreateWarFactoryAnim(const CoordStruct& pos);
    static AnimClass* CreateExplosionAnim(const CoordStruct& pos, int32 damageLevel = 1);
    static AnimClass* CreateFireAnim(const CoordStruct& pos);
    static AnimClass* CreateSmokeAnim(const CoordStruct& pos);

    // Rendering
    void Render(bool isometric = true);
    Point2D ConvertCoordToDrawPos() const;
    int32 GetDrawLayer() const;
    int32 GetYSort() const;
    int32 GetZAdjust() const;
    bool IsVisible() const;
    bool ShouldRemoveInFog() const;

    // Static utility methods
    static AnimClass* FindByID(const char* id);
    static int32 GetActiveCount();
    static void RemoveAll();
    static void UpdateAll();
    static void RenderAll();

protected:
    explicit AnimClass(noinit_t) noexcept : ObjectClass(noinit) {}

    // Internal update methods
    void UpdateAnimation();
    void UpdateBounce();
    void UpdateExtras();
    void UpdateSound();
    void UpdateLight();
    void UpdateTrailer();
    void ApplyDamage();
    void CheckFlamingGuy();
    void ProcessTiberium();
    void ProcessVeins();
    void ProcessSpawns();
    void ProcessScorch();
    void ProcessCrater();
    void OnAnimationEnd();
    void OnLoop();

public:
    AnimTypeClass* Type;
    AnimStruct Animation;
    ObjectClass* OwnerObject;
    int32 unknown_D0;
    ObjectClass* LightConvert;
    int32 LightConvertIndex;
    uint32 TintColor;
    int32 ZAdjust;
    int32 YSortAdjust;
    CoordStruct FlamingGuyCoords;
    int32 FlamingGuyRetries;
    bool IsBuildingAnim;
    bool UnderTemporal;
    bool Paused;
    bool Unpaused;
    int32 PausedAnimFrame;
    bool Reverse;
    uint32 unknown_124;
    BYTE TranslucencyLevel;
    bool TimeToDie;
    BulletClass* AttachedBullet;
    HouseClass* Owner;
    int32 LoopDelay;
    double Accum;
    BlitterFlags AnimFlags;
    bool HasExtras;
    int32 RemainingIterations;
    BYTE unknown_196;
    BYTE unknown_197;
    bool IsPlaying_;
    bool IsFogged;
    bool FlamingGuyExpire;
    bool UnableToContinue;
    bool SkipProcessOnce;
    bool Invisible;
    bool PowerOff;
    bool unused_19F;
    bool IsFlaming;
    bool IsNuke;
    bool IsIonCannon;
    bool IsLooping_;

    // Bounce physics
    struct BounceData {
        CoordStruct Velocity;
        float Elasticity;
        int32 Bounces;
        int32 MaxBounces;
        bool IsActive;
        bool padding[3];

        BounceData() : Velocity(0, 0, 0), Elasticity(0.5f), Bounces(0),
            MaxBounces(3), IsActive(false) {
            std::memset(padding, 0, sizeof(padding));
        }

        void Update() {
            // Apply gravity
            if (IsActive) {
                Velocity.Z -= 1;
            }
        }
    };
    BounceData Bounce;

    // State tracking
    int32 TrailerCount;
    int32 TrailerTimer;
    int32 LightTimer;
    int32 SoundTimer;
    bool ScorchCreated;
    bool CraterCreated;
    bool InfantryCreated;
    bool TiberiumCreated;
    bool ChainAnimCreated;

    char PaletteName[0x20];
};