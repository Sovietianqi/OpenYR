#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/ObjectTypeClass.h"
#include "../Special/AlphaShapeClass.h"

class INIClass;
class CCINIClass;
class CRCEngine;

enum class EmitShapeType : int32 {
    Point = 0,
    Circle = 1,
    Sphere = 2,
    Box = 3,
    Cone = 4
};

enum class BillboardType : int32 {
    ScreenAligned = 0,
    WorldAligned = 1,
    AxisAligned = 2
};

enum class ParticleRenderType : int32 {
    Sprite = 0,
    Trail = 1,
    Mesh = 2,
    Ribbon = 3
};

enum class SimulationSpaceType : int32 {
    World = 0,
    Local = 1
};

enum class RenderOrderType : int32 {
    BackToFront = 0,
    FrontToBack = 1,
    None = 2
};

struct GradientKey {
    float Time;
    ColorStruct Color;
};

struct FloatKey {
    float Time;
    float Value;
};

// ============================================================================
// ParticleTypeClass - single particle *type* definition (fire/smoke/spark...)
//
//  asm: ParticleTypeClass_CTOR begins with `call ObjectTypeClass_CTOR` and the
//  object carries the ParticleTypeClass / IRTTITypeInfo / INoticeSink /
//  INoticeSource vtables; it is therefore an ObjectTypeClass derivative (as
//  opposed to the standalone design an earlier revision assumed).  Its identity
//  string lives in the inherited AbstractTypeClass::ID slot (asm uses
//  `[this+0x24]` as the INI section), and GetAbstractDerivationID returns 0x17
//  (AbstractType::ParticleType).
// ============================================================================
class NOVTABLE ParticleTypeClass : public ObjectTypeClass {
public:
    static const AbstractType AbsID = AbstractType::ParticleType;

    static DynamicVectorClass<ParticleTypeClass*>* Array;

    static ParticleTypeClass* Find(const char* pID);
    static ParticleTypeClass* FindOrAllocate(const char* pID);
    static int32 FindIndexOrAllocate(const char* pID);

    // "BehavesLike=" is matched against the five entry behaviour table the
    // rules parser walks; an unrecognised token stores -1.
    static int32 BehavesLikeFromName(const char* pName);
    static const char* BehavesLikeToName(int32 nIndex);

    ParticleTypeClass() noexcept;
    explicit ParticleTypeClass(const char* pID) noexcept;
    explicit ParticleTypeClass(noinit_t) noexcept : ObjectTypeClass(noinit) {}
    virtual ~ParticleTypeClass();

    // ------------------------------------------------------------------------
    // IPersistStream / vtable contract (asm vftable slots).
    // ------------------------------------------------------------------------
    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void ComputeCRC(CRCEngine& crc) const override;
    int32 GetCRC() const;

    // asm: ParticleTypeClass_GetAbstractDerivationID: mov eax,17h.
    // AbstractClass has no such virtual, so (as with ParticleClass/BulletClass)
    // it is supplied here as a fresh virtual whose value matches the asm.
    virtual AbstractType GetAbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: ParticleTypeClass_PointerGotInvalid (retn 8): clears Warhead if the
    // invalidated pointer is it.  Unified project signature (see briefing §3).
    // ------------------------------------------------------------------------
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // ------------------------------------------------------------------------
    // [Particles] INI entry parser.  asm takes a single CCINIClass* and derives
    // the section from the object's ID (set by the constructor).
    // ------------------------------------------------------------------------
    virtual bool LoadFromINI(CCINIClass* pINI) override;

    // Legacy two-argument entry kept for callers that pass the section name
    // explicitly; forwards to the asm-shaped overload semantics.
    bool ReadFromINI(INIClass* ini, const char* section);

    ColorStruct CalculateColor(float lifetimeT) const;
    float CalculateSize(float lifetimeT) const;
    float CalculateAlpha(float lifetimeT) const;
    ColorStruct CalculateGradientColor(float t) const;
    float CalculateGradientSize(float t) const;
    float CalculateGradientAlpha(float t) const;

    void AddColorGradientKey(float time, const ColorStruct& color);
    void AddSizeGradientKey(float time, float value);
    void AddAlphaGradientKey(float time, float value);

    void SetName(const char* name);
    void SetMaxLifetime(int32 lifetime);
    void SetMaxParticles(int32 count);
    void SetEmissionRate(int32 rate);
    void SetEmitShape(EmitShapeType shape);
    void SetEmitRadius(float radius);
    void SetTextureIndex(int32 index);
    void SetUseWind(bool use);
    void SetUseCollision(bool use);
    void SetUseBillboard(bool use);
    void SetUseAlphaBlend(bool use);
    void SetBlendMode(AlphaBlendMode mode);
    void SetEnabled(bool enabled);

    const char* GetName() const;
    int32 GetMaxLifetime() const;
    int32 GetMaxParticles() const;
    int32 GetEmissionRate() const;
    bool IsEnabledType() const;

    // Identity string: inherited AbstractTypeClass::ID (asm [this+0x24]).
    int32 MaxLifetime;
    int32 MaxSystemLifetime;
    int32 EmissionRate;
    int32 MaxParticles;
    CoordStruct InitialVelocity;
    CoordStruct VelocityVariation;
    CoordStruct Acceleration;
    float Gravity;
    float Drag;
    float InitialSize;
    float FinalSize;
    float SizeVariation;
    float InitialRotation;
    float RotationSpeed;
    float RotationVariation;
    ColorStruct InitialColor;
    ColorStruct FinalColor;
    ColorStruct ColorVariation;
    bool UseColorGradient;
    EmitShapeType EmitShape;
    float EmitRadius;
    float EmitAngle;
    float EmitAngleVariation;
    float EmitWidth;
    float EmitHeight;
    bool UseWind;
    float WindInfluence;
    bool UseCollision;
    float CollisionBounce;
    float CollisionFriction;
    bool CollisionKill;
    bool UseBillboard;
    BillboardType BillboardType_;
    bool UseAlphaBlend;
    AlphaBlendMode BlendMode;
    int32 TextureIndex;
    int32 TextureWidth;
    int32 TextureHeight;
    int32 TextureFrameCount;
    int32 TextureFrameRate;
    bool UseTextureAnimation;
    ParticleRenderType ParticleType;
    bool UseTrail;
    int32 TrailLength;
    float TrailWidth;
    ColorStruct TrailColor;
    bool UseLight;
    int32 LightRadius;
    ColorStruct LightColor;
    int32 LightIntensity;
    bool UseSubEmitter;
    int32 SubEmitterIndex;
    int32 SubEmitterCount;
    bool UseNoise;
    float NoiseStrength;
    float NoiseFrequency;
    float NoiseSpeed;
    bool UseOrbit;
    float OrbitRadius;
    float OrbitSpeed;
    CoordStruct OrbitAxis;
    float InheritVelocity;
    int32 EmissionDelay;
    bool OneShot;
    bool OneShotEmitted;
    int32 OneShotCount;
    int32 Prewarm;
    bool UseLocalSpace;
    SimulationSpaceType SimulationSpace;
    int32 SortingOrder;
    bool UseDepthSort;
    RenderOrderType RenderOrder;
    bool IsEnabled;
    int32 IniIndex;
    GradientKey* ColorGradientKeys;
    int32 ColorGradientKeyCount;
    FloatKey* SizeGradientKeys;
    int32 SizeGradientKeyCount;
    FloatKey* AlphaGradientKeys;
    int32 AlphaGradientKeyCount;
    // ------------------------------------------------------------------
    // Rules / Art INI fields
    // ------------------------------------------------------------------
    DynamicVectorClass<ColorStruct> ColorList;
    int32        MaxDC;
    int32        MaxEC;
    int32        Damage;
    WarheadTypeClass* Warhead;
    int32        StartFrame;
    int32        NumLoopFrames;
    int32        Translucency;
    int32        WindEffect;
    double       Velocity;
    double       Deacc;
    int32        Radius;
    bool         DeleteOnStateLimit;
    int32        EndStateAI;
    int32        StartStateAI;
    int32        StateAIAdvance;
    int32        Translucent50State;
    int32        Translucent25State;
    bool         Normalized;
    double       ColorSpeed;
    int32        XVelocity;
    int32        YVelocity;
    int32        MinZVelocity;
    int32        ZVelocityRange;
    int32        NextParticleOffset[3];
    uint8        StartColor1[3];
    uint8        StartColor2[3];
    int32        FinalDamageState;
    char        NextParticle[0x20];
    int32        NextParticleIndex;
    int32 BehavesLike;
};