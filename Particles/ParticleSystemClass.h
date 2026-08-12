#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"
#include "ParticleTypeClass.h"

class BlitterClass;
class DSurface;

struct Particle {
    bool IsAlive;
    int32 Lifetime;
    int32 MaxLifetime;
    CoordStruct Position;
    CoordStruct Velocity;
    float Size;
    float Rotation;
    float RotationSpeed;
    ColorStruct Color;
};

static constexpr int32 MAX_PARTICLE_SYSTEMS = 128;
static constexpr int32 MAX_PARTICLE_TYPES = 256;
static constexpr int32 MAX_SYSTEM_TYPES = 128;

class ParticleSystemClass {
public:
    static DynamicVectorClass<ParticleSystemClass*>* Array;

    ParticleSystemClass();
    ~ParticleSystemClass();

    void Initialize(ParticleTypeClass* type, const CoordStruct& position);
    void Destroy();
    void Update();
    void EmitParticles();
    int32 FindFreeParticleSlot();
    void SpawnParticle(int32 slot);
    CoordStruct CalculateEmitPosition();
    void UpdateParticles();
    void CheckCollision(Particle& p);
    void CleanupParticles();
    void Render(BlitterClass* blitter, DSurface* surface);
    void RenderSprite(BlitterClass* blitter, DSurface* surface, const Particle& p);
    void RenderTrail(BlitterClass* blitter, DSurface* surface, const Particle& p);
    void Prewarm(int32 frames);
    float RandomVariation(float variation);

    void Pause();
    void Resume();
    void Stop();
    void SetPosition(const CoordStruct& pos);
    void SetInheritedVelocity(const CoordStruct& velocity);
    void SetWindVelocity(const CoordStruct& wind);
    void SetLooping(bool looping);
    void SetSortingOrder(int32 order);
    void SetRenderOrder(RenderOrderType order);
    void SetOwner(ObjectClass* owner);

    bool IsActiveSystem() const;
    bool IsPausedSystem() const;
    bool IsCompleted() const;
    int32 GetActiveParticleCount() const;
    int32 GetTotalEmitted() const;
    ParticleTypeClass* GetType() const;
    ObjectClass* GetOwner() const;

    ParticleTypeClass* Type;
    CoordStruct Position;
    bool IsActive;
    bool IsPaused;
    int32 EmissionTimer;
    int32 EmissionCount;
    int32 TotalEmitted;
    int32 ActiveParticleCount;
    Particle* ParticlePool;
    int32 PoolSize;
    int32 SystemLifetime;
    int32 MaxSystemLifetime;
    bool LoopSystem;
    bool SystemCompleted;
    CoordStruct InheritedVelocity;
    CoordStruct WindVelocity;
    int32 SortingOrder;
    RenderOrderType RenderOrder;
    ObjectClass* OwnerObject;
    int32 SystemID;
    bool Prewarmed;
    bool PrewarmComplete;
};

#include "ParticleSystemTypeClass.h"

class ParticleSystemManagerClass {
public:
    ParticleSystemManagerClass();
    ~ParticleSystemManagerClass();

    static ParticleSystemManagerClass* GetInstance();

    int32 CreateSystem(ParticleTypeClass* type, const CoordStruct& position);
    bool RemoveSystem(int32 index);
    void RemoveAllSystems();
    void UpdateAllSystems();
    void UpdateGlobalWind();
    void RenderAllSystems(BlitterClass* blitter, DSurface* surface);

    int32 RegisterParticleType(ParticleTypeClass* type);
    int32 RegisterSystemType(ParticleSystemTypeClass* type);
    ParticleTypeClass* GetParticleType(int32 index) const;
    ParticleSystemTypeClass* GetSystemType(int32 index) const;
    ParticleSystemClass* GetSystem(int32 index) const;

    void SetWind(bool enable, float strength, float direction);
    void SetRenderingEnabled(bool enable);
    int32 GetActiveCount() const;
    int32 GetTotalParticleCount() const;
    int32 GetParticleTypeCount() const;
    int32 GetSystemTypeCount() const;
    int32 CreateSystemFromType(int32 typeIndex, const CoordStruct& position);

    int32 ActiveCount;
    ParticleSystemClass* Systems[MAX_PARTICLE_SYSTEMS];
    CoordStruct GlobalWindVelocity;
    bool EnableWind;
    float WindStrength;
    float WindDirection;
    bool EnableRendering;
    int32 MaxTotalSystems;
    int32 MaxTotalParticles;
    int32 CurrentTotalParticles;
    int32 ParticleTypeCount;
    int32 SystemTypeCount;
    ParticleTypeClass* ParticleTypes[MAX_PARTICLE_TYPES];
    ParticleSystemTypeClass* SystemTypes[MAX_SYSTEM_TYPES];
};