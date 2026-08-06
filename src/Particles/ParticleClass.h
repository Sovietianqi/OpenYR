#pragma once

#include "../Abstract/ObjectClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/CoordStruct.h"
#include "ParticleTypeClass.h"

// ============================================================================
// Forward declarations
// ============================================================================

class ParticleSystemClass;

// ============================================================================
// ParticleClass - individual particle instance
// ============================================================================

class NOVTABLE ParticleClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Particle;

    static DynamicVectorClass<ParticleClass*>* Array;

    ParticleClass(
        ParticleTypeClass* pParticleType,
        CoordStruct* pCrd1,
        CoordStruct* pCrd2,
        ParticleSystemClass* pParticleSystem) noexcept;
    virtual ~ParticleClass();

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual void Update() override;
    virtual CoordStruct* GetCoords(CoordStruct* pCrd) const override;

    // Particle-specific
    void Fire();
    void Draw();
    void FadeOut();
    bool IsDead() const { return Dead; }
    int32 GetAge() const { return Age; }
    int32 GetMaxAge() const { return MaxAge; }

    void SetVelocity(const CoordStruct& vel) { Velocity = vel; }
    CoordStruct GetVelocity() const { return Velocity; }
    void SetAcceleration(const CoordStruct& accel) { Acceleration = accel; }
    void SetColor(const ColorStruct& col) { Color = col; }
    void SetAlpha(uint8 alpha) { Alpha = alpha; }
    void SetSize(float size) { Size_ = size; }

    // Particle sub-type factory methods
    static ParticleClass* CreateSparkParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateFireParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateSmokeParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateGasParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateRadiationParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateElectricParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateLaserParticle(const CoordStruct& pos, const CoordStruct& vel);
    static ParticleClass* CreateBubbleParticle(const CoordStruct& pos, const CoordStruct& vel);

protected:
    explicit ParticleClass(noinit_t) noexcept : ObjectClass(noinit) {}

    void UpdateMotion();
    void UpdateLifetime();
    void UpdateAlpha();
    void UpdateSize();

public:
    ParticleTypeClass* Type;
    BYTE unknown_B0;
    BYTE unknown_B1;
    BYTE unknown_B2;
    uint32 unknown_B4;
    uint32 unknown_B8;
    uint32 unknown_BC;
    CoordStruct Velocity;
    uint32 unknown_CC;
    double unknown_double_D0;
    uint32 unknown_D8;
    uint32 unknown_DC;
    uint32 unknown_E0;
    float Speed;
    CoordStruct DestCoords;
    CoordStruct SourceCoords;
    CoordStruct unknown_coords_100;
    float unknown_vector3d_10C_x;
    float unknown_vector3d_10C_y;
    float unknown_vector3d_10C_z;
    float unknown_vector3d_118_x;
    float unknown_vector3d_118_y;
    float unknown_vector3d_118_z;
    ParticleSystemClass* ParticleSystem;
    WORD RemainingEC;
    WORD RemainingDC;
    BYTE StateAIAdvance;
    BYTE unknown_12D;
    BYTE StartStateAI;
    BYTE Translucency;
    BYTE unknown_130;
    BYTE unknown_131;
    uint32 unused_134;

    // Additional runtime data
    int32 Age;
    int32 MaxAge;
    CoordStruct Acceleration;
    ColorStruct Color;
    uint8 Alpha;
    float Size_;
    bool Dead;
    bool FadingOut;
    int32 ParticleType_;
};