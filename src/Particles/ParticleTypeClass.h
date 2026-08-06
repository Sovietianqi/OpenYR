#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"
#include "../Special/AlphaShapeClass.h"

class INIClass;

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

class ParticleTypeClass {
public:
    static DynamicVectorClass<ParticleTypeClass*>* Array;

    ParticleTypeClass();
    ~ParticleTypeClass();

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

    char Name[32];
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
};