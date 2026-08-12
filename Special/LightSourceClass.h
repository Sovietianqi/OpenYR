#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"

enum class LightFalloffType : int32 {
    Linear = 0,
    Quadratic = 1,
    Exponential = 2,
    InverseSquared = 3,
    SmoothStep = 4
};

class LightSourceClass {
public:
    LightSourceClass();
    ~LightSourceClass();

    void Initialize(const CoordStruct& pos, int32 radius, int32 intensity, const ColorStruct& color);
    void Release();
    void Update();
    void UpdateFlicker();
    void UpdatePulse();
    void CalculateIntensity(int32& red, int32& green, int32& blue) const;
    float CalculateFalloff(int32 distance) const;
    float CalculateSpotlightFactor(const CoordStruct& targetPos) const;
    void UpdateCellLighting();
    void ApplyLightToCell(const CellStruct& cell, int32 r, int32 g, int32 b);

    void SetColor(int32 r, int32 g, int32 b);
    void SetIntensity(int32 intensity);
    void SetRadius(int32 radius);
    void SetFlicker(bool enabled, float intensity, int32 interval);
    void SetPulse(bool enabled, float speed);
    void SetFalloffType(LightFalloffType type, float exponent);
    void SetSpotlight(bool enabled, float angle, float exponent, const CoordStruct& direction);
    void SetPosition(const CoordStruct& pos);
    void SetZRange(int32 range, int32 top);
    void SetOverlayAlpha(int32 alpha);
    void SetPriority(int32 priority);
    void ToggleActive();
    void SetActive(bool active);

    CoordStruct Position;
    int32 Radius;
    int32 Intensity;
    int32 MaxIntensity;
    ColorStruct Color;
    uint8 RedComponent;
    uint8 GreenComponent;
    uint8 BlueComponent;
    LightFalloffType FalloffType;
    float FalloffExponent;
    bool IsActive;
    bool IsFlickering;
    float FlickerIntensity;
    int32 FlickerInterval;
    int32 FlickerTimer;
    float FlickerOffset;
    float PulsePhase;
    float PulseSpeed;
    bool Pulsing;
    uint8* CellBuffer;
    int32 CellBufferSize;
    int32 LastUpdateFrame;
    int32 PoolIndex;
    bool IsPooled;
    int32 Priority;
    int32 LightID;
    int32 ZRange;
    int32 ZRangeTop;
    int32 OverlayAlpha;
    int32 InnerRadius;
    int32 OuterRadius;
    bool UseSpotlight;
    float SpotAngle;
    CoordStruct SpotDirection;
    float SpotExponent;
};

static constexpr int32 MAX_POOLED_LIGHTS = 64;

class LightSourcePoolClass {
public:
    LightSourcePoolClass();
    ~LightSourcePoolClass();

    static LightSourcePoolClass* GetInstance();

    LightSourceClass* AllocateLight();
    void ReleaseLight(LightSourceClass* light);
    void UpdateAllLights();
    void ReleaseAllLights();
    int32 GetActiveCount() const;
    LightSourceClass* FindLightAtPosition(const CoordStruct& pos, int32 threshold);

    LightSourceClass LightSources[MAX_POOLED_LIGHTS];
    int32 ActiveCount;
    int32 PoolSize;
    int32 NextLightID;
};