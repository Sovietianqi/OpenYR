#include "ParticleTypeClass.h"
#include "../IO/CCFileClass.h"
#include "../INI/INIClass.h"

#include <cmath>
#include <cstring>
#include <cstdlib>

// ============================================================
// ParticleTypeClass
// ============================================================

ParticleTypeClass::ParticleTypeClass()
    : Name{}, MaxLifetime(60), EmissionRate(10), MaxParticles(100)
    , InitialVelocity(0.0f, 0.0f, 0.0f), VelocityVariation(0.0f, 0.0f, 0.0f)
    , Acceleration(0.0f, 0.0f, -0.1f), Gravity(0.0f), Drag(0.0f)
    , InitialSize(1.0f), FinalSize(1.0f), SizeVariation(0.0f)
    , InitialRotation(0.0f), RotationSpeed(0.0f), RotationVariation(0.0f)
    , InitialColor(255, 255, 255, 255), FinalColor(255, 255, 255, 0)
    , ColorVariation(0, 0, 0, 0), UseColorGradient(false)
    , EmitShape(EmitShapeType::Point), EmitRadius(0.0f), EmitAngle(0.0f)
    , EmitAngleVariation(0.0f), EmitWidth(0.0f), EmitHeight(0.0f)
    , UseWind(false), WindInfluence(0.0f), UseCollision(false)
    , CollisionBounce(0.5f), CollisionFriction(0.3f), CollisionKill(false)
    , UseBillboard(true), BillboardType_(BillboardType::ScreenAligned)
    , UseAlphaBlend(true), BlendMode(AlphaBlendMode::Normal)
    , TextureIndex(-1), TextureWidth(0), TextureHeight(0)
    , TextureFrameCount(1), TextureFrameRate(4), UseTextureAnimation(false)
    , ParticleType(ParticleRenderType::Sprite), UseTrail(false)
    , TrailLength(0), TrailWidth(1.0f), TrailColor(255, 255, 255, 128)
    , UseLight(false), LightRadius(0), LightColor(255, 255, 255)
    , LightIntensity(128), UseSubEmitter(false), SubEmitterIndex(-1)
    , SubEmitterCount(0), UseNoise(false), NoiseStrength(0.0f)
    , NoiseFrequency(0.0f), NoiseSpeed(0.0f), UseOrbit(false)
    , OrbitRadius(0.0f), OrbitSpeed(0.0f), OrbitAxis(0.0f, 0.0f, 1.0f)
    , InheritVelocity(0.0f), EmissionDelay(0), OneShot(false)
    , OneShotCount(0), Prewarm(0), UseLocalSpace(false)
    , SimulationSpace(SimulationSpaceType::World), SortingOrder(0)
    , UseDepthSort(true), RenderOrder(RenderOrderType::BackToFront)
    , IsEnabled(true), IniIndex(-1), ColorGradientKeys(nullptr)
    , ColorGradientKeyCount(0), SizeGradientKeys(nullptr), SizeGradientKeyCount(0)
    , AlphaGradientKeys(nullptr), AlphaGradientKeyCount(0) {
}

ParticleTypeClass::~ParticleTypeClass() {
    if (ColorGradientKeys) {
        delete[] ColorGradientKeys;
        ColorGradientKeys = nullptr;
    }
    if (SizeGradientKeys) {
        delete[] SizeGradientKeys;
        SizeGradientKeys = nullptr;
    }
    if (AlphaGradientKeys) {
        delete[] AlphaGradientKeys;
        AlphaGradientKeys = nullptr;
    }
    ColorGradientKeyCount = 0;
    SizeGradientKeyCount = 0;
    AlphaGradientKeyCount = 0;
}

bool ParticleTypeClass::ReadFromINI(INIClass* ini, const char* section) {
    if (!ini || !section) return false;

    // Lifetime
    MaxLifetime = ini->ReadInteger(section, "Lifetime", 60);
    if (MaxLifetime < 1) MaxLifetime = 1;

    // Emission
    EmissionRate = ini->ReadInteger(section, "EmissionRate", 10);
    MaxParticles = ini->ReadInteger(section, "MaxParticles", 100);
    EmissionDelay = ini->ReadInteger(section, "EmissionDelay", 0);
    OneShot = ini->ReadBool(section, "OneShot", false);
    OneShotCount = ini->ReadInteger(section, "OneShotCount", 0);
    Prewarm = ini->ReadInteger(section, "Prewarm", 0);

    // Velocity
    InitialVelocity.X = ini->ReadFloat(section, "VelocityX", 0.0f);
    InitialVelocity.Y = ini->ReadFloat(section, "VelocityY", 0.0f);
    InitialVelocity.Z = ini->ReadFloat(section, "VelocityZ", 0.0f);
    VelocityVariation.X = ini->ReadFloat(section, "VelocityVariationX", 0.0f);
    VelocityVariation.Y = ini->ReadFloat(section, "VelocityVariationY", 0.0f);
    VelocityVariation.Z = ini->ReadFloat(section, "VelocityVariationZ", 0.0f);

    // Acceleration
    Acceleration.X = ini->ReadFloat(section, "AccelerationX", 0.0f);
    Acceleration.Y = ini->ReadFloat(section, "AccelerationY", 0.0f);
    Acceleration.Z = ini->ReadFloat(section, "AccelerationZ", -0.1f);
    Gravity = ini->ReadFloat(section, "Gravity", 0.0f);
    Drag = ini->ReadFloat(section, "Drag", 0.0f);

    // Size
    InitialSize = ini->ReadFloat(section, "InitialSize", 1.0f);
    FinalSize = ini->ReadFloat(section, "FinalSize", 1.0f);
    SizeVariation = ini->ReadFloat(section, "SizeVariation", 0.0f);

    // Rotation
    InitialRotation = ini->ReadFloat(section, "InitialRotation", 0.0f);
    RotationSpeed = ini->ReadFloat(section, "RotationSpeed", 0.0f);
    RotationVariation = ini->ReadFloat(section, "RotationVariation", 0.0f);

    // Color
    InitialColor.R = static_cast<uint8>(ini->ReadInteger(section, "InitialColorR", 255));
    InitialColor.G = static_cast<uint8>(ini->ReadInteger(section, "InitialColorG", 255));
    InitialColor.B = static_cast<uint8>(ini->ReadInteger(section, "InitialColorB", 255));
    InitialColor.A = static_cast<uint8>(ini->ReadInteger(section, "InitialColorA", 255));
    FinalColor.R = static_cast<uint8>(ini->ReadInteger(section, "FinalColorR", 255));
    FinalColor.G = static_cast<uint8>(ini->ReadInteger(section, "FinalColorG", 255));
    FinalColor.B = static_cast<uint8>(ini->ReadInteger(section, "FinalColorB", 255));
    FinalColor.A = static_cast<uint8>(ini->ReadInteger(section, "FinalColorA", 0));
    UseColorGradient = ini->ReadBool(section, "UseColorGradient", false);

    // Emit shape
    EmitShape = static_cast<EmitShapeType>(ini->ReadInteger(section, "EmitShape", 0));
    EmitRadius = ini->ReadFloat(section, "EmitRadius", 0.0f);
    EmitAngle = ini->ReadFloat(section, "EmitAngle", 0.0f);
    EmitAngleVariation = ini->ReadFloat(section, "EmitAngleVariation", 0.0f);
    EmitWidth = ini->ReadFloat(section, "EmitWidth", 0.0f);
    EmitHeight = ini->ReadFloat(section, "EmitHeight", 0.0f);

    // Behavior flags
    UseWind = ini->ReadBool(section, "UseWind", false);
    WindInfluence = ini->ReadFloat(section, "WindInfluence", 0.0f);
    UseCollision = ini->ReadBool(section, "UseCollision", false);
    CollisionBounce = ini->ReadFloat(section, "CollisionBounce", 0.5f);
    CollisionFriction = ini->ReadFloat(section, "CollisionFriction", 0.3f);
    CollisionKill = ini->ReadBool(section, "CollisionKill", false);

    // Billboard
    UseBillboard = ini->ReadBool(section, "UseBillboard", true);
    BillboardType_ = static_cast<BillboardType>(ini->ReadInteger(section, "BillboardType", 0));
    UseAlphaBlend = ini->ReadBool(section, "UseAlphaBlend", true);
    BlendMode = static_cast<AlphaBlendMode>(ini->ReadInteger(section, "BlendMode", 0));

    // Texture
    TextureIndex = ini->ReadInteger(section, "TextureIndex", -1);
    TextureWidth = ini->ReadInteger(section, "TextureWidth", 0);
    TextureHeight = ini->ReadInteger(section, "TextureHeight", 0);
    TextureFrameCount = ini->ReadInteger(section, "TextureFrameCount", 1);
    TextureFrameRate = ini->ReadInteger(section, "TextureFrameRate", 4);
    UseTextureAnimation = ini->ReadBool(section, "UseTextureAnimation", false);

    // Particle type
    ParticleType = static_cast<ParticleRenderType>(ini->ReadInteger(section, "ParticleType", 0));

    // Trail
    UseTrail = ini->ReadBool(section, "UseTrail", false);
    TrailLength = ini->ReadInteger(section, "TrailLength", 0);
    TrailWidth = ini->ReadFloat(section, "TrailWidth", 1.0f);
    TrailColor.R = static_cast<uint8>(ini->ReadInteger(section, "TrailColorR", 255));
    TrailColor.G = static_cast<uint8>(ini->ReadInteger(section, "TrailColorG", 255));
    TrailColor.B = static_cast<uint8>(ini->ReadInteger(section, "TrailColorB", 255));
    TrailColor.A = static_cast<uint8>(ini->ReadInteger(section, "TrailColorA", 128));

    // Light
    UseLight = ini->ReadBool(section, "UseLight", false);
    LightRadius = ini->ReadInteger(section, "LightRadius", 0);
    LightColor.R = static_cast<uint8>(ini->ReadInteger(section, "LightColorR", 255));
    LightColor.G = static_cast<uint8>(ini->ReadInteger(section, "LightColorG", 255));
    LightColor.B = static_cast<uint8>(ini->ReadInteger(section, "LightColorB", 255));
    LightIntensity = ini->ReadInteger(section, "LightIntensity", 128);

    // Sub-emitter
    UseSubEmitter = ini->ReadBool(section, "UseSubEmitter", false);
    SubEmitterIndex = ini->ReadInteger(section, "SubEmitterIndex", -1);
    SubEmitterCount = ini->ReadInteger(section, "SubEmitterCount", 0);

    // Noise
    UseNoise = ini->ReadBool(section, "UseNoise", false);
    NoiseStrength = ini->ReadFloat(section, "NoiseStrength", 0.0f);
    NoiseFrequency = ini->ReadFloat(section, "NoiseFrequency", 0.0f);
    NoiseSpeed = ini->ReadFloat(section, "NoiseSpeed", 0.0f);

    // Orbit
    UseOrbit = ini->ReadBool(section, "UseOrbit", false);
    OrbitRadius = ini->ReadFloat(section, "OrbitRadius", 0.0f);
    OrbitSpeed = ini->ReadFloat(section, "OrbitSpeed", 0.0f);
    OrbitAxis.X = ini->ReadFloat(section, "OrbitAxisX", 0.0f);
    OrbitAxis.Y = ini->ReadFloat(section, "OrbitAxisY", 0.0f);
    OrbitAxis.Z = ini->ReadFloat(section, "OrbitAxisZ", 1.0f);

    // Misc
    InheritVelocity = ini->ReadFloat(section, "InheritVelocity", 0.0f);
    UseLocalSpace = ini->ReadBool(section, "UseLocalSpace", false);
    SimulationSpace = static_cast<SimulationSpaceType>(ini->ReadInteger(section, "SimulationSpace", 0));
    SortingOrder = ini->ReadInteger(section, "SortingOrder", 0);
    UseDepthSort = ini->ReadBool(section, "UseDepthSort", true);
    RenderOrder = static_cast<RenderOrderType>(ini->ReadInteger(section, "RenderOrder", 0));
    IsEnabled = ini->ReadBool(section, "Enabled", true);

    return true;
}

ColorStruct ParticleTypeClass::CalculateColor(float lifetimeT) const {
    if (UseColorGradient && ColorGradientKeyCount > 0) {
        return CalculateGradientColor(lifetimeT);
    }

    ColorStruct result;
    float t = lifetimeT;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    result.R = static_cast<uint8>(InitialColor.R + (FinalColor.R - InitialColor.R) * t);
    result.G = static_cast<uint8>(InitialColor.G + (FinalColor.G - InitialColor.G) * t);
    result.B = static_cast<uint8>(InitialColor.B + (FinalColor.B - InitialColor.B) * t);
    result.A = static_cast<uint8>(InitialColor.A + (FinalColor.A - InitialColor.A) * t);
    return result;
}

float ParticleTypeClass::CalculateSize(float lifetimeT) const {
    if (SizeGradientKeyCount > 0) {
        return CalculateGradientSize(lifetimeT);
    }

    float t = lifetimeT;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return InitialSize + (FinalSize - InitialSize) * t;
}

float ParticleTypeClass::CalculateAlpha(float lifetimeT) const {
    if (AlphaGradientKeyCount > 0) {
        return CalculateGradientAlpha(lifetimeT);
    }

    float t = lifetimeT;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (InitialColor.A + (FinalColor.A - InitialColor.A) * t) / 255.0f;
}

ColorStruct ParticleTypeClass::CalculateGradientColor(float t) const {
    if (ColorGradientKeyCount <= 0) return InitialColor;

    if (t <= ColorGradientKeys[0].Time) return ColorGradientKeys[0].Color;
    if (t >= ColorGradientKeys[ColorGradientKeyCount - 1].Time)
        return ColorGradientKeys[ColorGradientKeyCount - 1].Color;

    for (int32 i = 0; i < ColorGradientKeyCount - 1; ++i) {
        if (t >= ColorGradientKeys[i].Time && t <= ColorGradientKeys[i + 1].Time) {
            float localT = (t - ColorGradientKeys[i].Time) /
                          (ColorGradientKeys[i + 1].Time - ColorGradientKeys[i].Time);
            ColorStruct result;
            result.R = static_cast<uint8>(ColorGradientKeys[i].Color.R +
                (ColorGradientKeys[i + 1].Color.R - ColorGradientKeys[i].Color.R) * localT);
            result.G = static_cast<uint8>(ColorGradientKeys[i].Color.G +
                (ColorGradientKeys[i + 1].Color.G - ColorGradientKeys[i].Color.G) * localT);
            result.B = static_cast<uint8>(ColorGradientKeys[i].Color.B +
                (ColorGradientKeys[i + 1].Color.B - ColorGradientKeys[i].Color.B) * localT);
            result.A = static_cast<uint8>(ColorGradientKeys[i].Color.A +
                (ColorGradientKeys[i + 1].Color.A - ColorGradientKeys[i].Color.A) * localT);
            return result;
        }
    }
    return InitialColor;
}

float ParticleTypeClass::CalculateGradientSize(float t) const {
    if (SizeGradientKeyCount <= 0) return InitialSize;

    if (t <= SizeGradientKeys[0].Time) return SizeGradientKeys[0].Value;
    if (t >= SizeGradientKeys[SizeGradientKeyCount - 1].Time)
        return SizeGradientKeys[SizeGradientKeyCount - 1].Value;

    for (int32 i = 0; i < SizeGradientKeyCount - 1; ++i) {
        if (t >= SizeGradientKeys[i].Time && t <= SizeGradientKeys[i + 1].Time) {
            float localT = (t - SizeGradientKeys[i].Time) /
                          (SizeGradientKeys[i + 1].Time - SizeGradientKeys[i].Time);
            return SizeGradientKeys[i].Value +
                   (SizeGradientKeys[i + 1].Value - SizeGradientKeys[i].Value) * localT;
        }
    }
    return InitialSize;
}

float ParticleTypeClass::CalculateGradientAlpha(float t) const {
    if (AlphaGradientKeyCount <= 0) return InitialColor.A / 255.0f;

    if (t <= AlphaGradientKeys[0].Time) return AlphaGradientKeys[0].Value;
    if (t >= AlphaGradientKeys[AlphaGradientKeyCount - 1].Time)
        return AlphaGradientKeys[AlphaGradientKeyCount - 1].Value;

    for (int32 i = 0; i < AlphaGradientKeyCount - 1; ++i) {
        if (t >= AlphaGradientKeys[i].Time && t <= AlphaGradientKeys[i + 1].Time) {
            float localT = (t - AlphaGradientKeys[i].Time) /
                          (AlphaGradientKeys[i + 1].Time - AlphaGradientKeys[i].Time);
            return AlphaGradientKeys[i].Value +
                   (AlphaGradientKeys[i + 1].Value - AlphaGradientKeys[i].Value) * localT;
        }
    }
    return InitialColor.A / 255.0f;
}

void ParticleTypeClass::AddColorGradientKey(float time, const ColorStruct& color) {
    GradientKey* newKeys = new GradientKey[ColorGradientKeyCount + 1];
    if (ColorGradientKeys) {
        for (int32 i = 0; i < ColorGradientKeyCount; ++i) {
            newKeys[i] = ColorGradientKeys[i];
        }
        delete[] ColorGradientKeys;
    }
    newKeys[ColorGradientKeyCount].Time = time;
    newKeys[ColorGradientKeyCount].Color = color;
    ColorGradientKeys = newKeys;
    ++ColorGradientKeyCount;
}

void ParticleTypeClass::AddSizeGradientKey(float time, float value) {
    FloatKey* newKeys = new FloatKey[SizeGradientKeyCount + 1];
    if (SizeGradientKeys) {
        for (int32 i = 0; i < SizeGradientKeyCount; ++i) {
            newKeys[i] = SizeGradientKeys[i];
        }
        delete[] SizeGradientKeys;
    }
    newKeys[SizeGradientKeyCount].Time = time;
    newKeys[SizeGradientKeyCount].Value = value;
    SizeGradientKeys = newKeys;
    ++SizeGradientKeyCount;
}

void ParticleTypeClass::AddAlphaGradientKey(float time, float value) {
    FloatKey* newKeys = new FloatKey[AlphaGradientKeyCount + 1];
    if (AlphaGradientKeys) {
        for (int32 i = 0; i < AlphaGradientKeyCount; ++i) {
            newKeys[i] = AlphaGradientKeys[i];
        }
        delete[] AlphaGradientKeys;
    }
    newKeys[AlphaGradientKeyCount].Time = time;
    newKeys[AlphaGradientKeyCount].Value = value;
    AlphaGradientKeys = newKeys;
    ++AlphaGradientKeyCount;
}

void ParticleTypeClass::SetName(const char* name) {
    if (name) {
        int32 i = 0;
        while (name[i] && i < 31) {
            Name[i] = name[i];
            ++i;
        }
        Name[i] = '\0';
    }
}

void ParticleTypeClass::SetMaxLifetime(int32 lifetime) {
    MaxLifetime = lifetime;
    if (MaxLifetime < 1) MaxLifetime = 1;
}

void ParticleTypeClass::SetMaxParticles(int32 count) {
    MaxParticles = count;
    if (MaxParticles < 0) MaxParticles = 0;
}

void ParticleTypeClass::SetEmissionRate(int32 rate) {
    EmissionRate = rate;
    if (EmissionRate < 0) EmissionRate = 0;
}

void ParticleTypeClass::SetEmitShape(EmitShapeType shape) {
    EmitShape = shape;
}

void ParticleTypeClass::SetEmitRadius(float radius) {
    EmitRadius = radius;
    if (EmitRadius < 0.0f) EmitRadius = 0.0f;
}

void ParticleTypeClass::SetTextureIndex(int32 index) {
    TextureIndex = index;
}

void ParticleTypeClass::SetUseWind(bool use) {
    UseWind = use;
}

void ParticleTypeClass::SetUseCollision(bool use) {
    UseCollision = use;
}

void ParticleTypeClass::SetUseBillboard(bool use) {
    UseBillboard = use;
}

void ParticleTypeClass::SetUseAlphaBlend(bool use) {
    UseAlphaBlend = use;
}

void ParticleTypeClass::SetBlendMode(AlphaBlendMode mode) {
    BlendMode = mode;
}

void ParticleTypeClass::SetEnabled(bool enabled) {
    IsEnabled = enabled;
}

const char* ParticleTypeClass::GetName() const {
    return Name;
}

int32 ParticleTypeClass::GetMaxLifetime() const {
    return MaxLifetime;
}

int32 ParticleTypeClass::GetMaxParticles() const {
    return MaxParticles;
}

int32 ParticleTypeClass::GetEmissionRate() const {
    return EmissionRate;
}

bool ParticleTypeClass::IsEnabledType() const {
    return IsEnabled;
}