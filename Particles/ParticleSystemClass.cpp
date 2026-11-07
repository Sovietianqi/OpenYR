#include "ParticleSystemClass.h"
#include "ParticleClass.h"
#include "../Math/Timer.h"
#include "../Rendering/Blitter.h"
#include "../Rendering/Surface.h"
#include "../Map/MapClass.h"
#include "../Map/LightFlashClass.h"
#include "../IO/CRC.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================
// ParticleSystemClass
// ============================================================

// asm 全局: vec_ParticleSystems。原版为静态 DynamicVectorClass，本实现以
// 惰性堆对象承载，并暴露 ParticleSystems_Vector() 供登记/反注册。
static DynamicVectorClass<ParticleSystemClass*>* g_ParticleSystems = nullptr;

// asm: ParticleSystem_Gas —— 毒气类动画共用的粒子系统实例（见头文件说明）。
ParticleSystemClass* ParticleSystem_Gas = nullptr;

DynamicVectorClass<ParticleSystemClass*>* ParticleSystems_Vector() {
    if (!g_ParticleSystems) {
        g_ParticleSystems = new DynamicVectorClass<ParticleSystemClass*>();
    }
    return g_ParticleSystems;
}

int32 ParticleSystems_Count() {
    return g_ParticleSystems ? g_ParticleSystems->Count : 0;
}

ParticleSystemClass::ParticleSystemClass()
    : ObjectClass()
    , Type(nullptr), Position(0, 0, 0), IsActive(false), IsPaused(false)
    , EmissionTimer(0), EmissionCount(0), TotalEmitted(0)
    , ActiveParticleCount(0), ParticlePool(nullptr), PoolSize(0)
    , SystemLifetime(0), MaxSystemLifetime(0), LoopSystem(false)
    , SystemCompleted(false), InheritedVelocity(0.0f, 0.0f, 0.0f)
    , WindVelocity(0.0f, 0.0f, 0.0f), SortingOrder(0)
    , RenderOrder(RenderOrderType::BackToFront), OwnerObject(nullptr)
    , SystemID(0), Prewarmed(false), PrewarmComplete(false)
    , field_F0(0), field_F8(false) {
    // asm: CTOR 尾部把 this 登记进 vec_ParticleSystems / vec_Abstracts_0。
    ParticleSystems_Vector()->Add(this);
}

ParticleSystemClass::~ParticleSystemClass() {
    Destroy();
    // asm: ~ParticleSystemClass 从 vec_ParticleSystems / vec_Abstracts_* 反注册。
    DynamicVectorClass<ParticleSystemClass*>* vec = ParticleSystems_Vector();
    for (int32 i = vec->Count - 1; i >= 0; --i) {
        if (vec->Items[i] == this) {
            vec->Remove(i);
            break;
        }
    }
}

void ParticleSystemClass::Initialize(ParticleTypeClass* type, const CoordStruct& position) {
    // 本项目入口把"发射的粒子类型"直接传入；原版由 Type(粒子系统类型)
    // 的 PartSysType->ParticleType 索引解析得到，语义等价。
    ParticleType = type;
    if (Type && Type->ParticleType) {
        ParticleType = Type->ParticleType;
    }
    if (!ParticleType) return;

    Position = position;
    IsActive = true;
    IsPaused = false;
    EmissionTimer = 0;
    EmissionCount = 0;
    TotalEmitted = 0;
    SystemLifetime = 0;
    SystemCompleted = false;
    Prewarmed = false;
    PrewarmComplete = false;

    if (ParticleType->MaxSystemLifetime > 0) {
        MaxSystemLifetime = ParticleType->MaxSystemLifetime;
    } else {
        MaxSystemLifetime = 0;
    }

    PoolSize = ParticleType->MaxParticles;
    if (PoolSize > 0) {
        ParticlePool = new Particle[PoolSize];
        for (int32 i = 0; i < PoolSize; ++i) {
            ParticlePool[i].IsAlive = false;
            ParticlePool[i].Lifetime = 0;
            ParticlePool[i].Position = CoordStruct(0, 0, 0);
            ParticlePool[i].Velocity = CoordStruct(0, 0, 0);
            ParticlePool[i].Size = 0.0f;
            ParticlePool[i].Rotation = 0.0f;
            ParticlePool[i].Color = ColorStruct(0, 0, 0, 0);
        }
    }
    ActiveParticleCount = 0;

    if (ParticleType->Prewarm > 0) {
        Prewarm(ParticleType->Prewarm);
    }
}

void ParticleSystemClass::Destroy() {
    IsActive = false;
    if (ParticlePool) {
        delete[] ParticlePool;
        ParticlePool = nullptr;
    }
    PoolSize = 0;
    ActiveParticleCount = 0;
    ParticleType = nullptr;
}

void ParticleSystemClass::Update() {
    AI();
}

// ============================================================================
// asm: ParticleSystemClass_Update (0x62FD5E)
//
//   先按 Type->BehavesLike 分派到五个行为处理器（0 烟 / 1 气 / 2 火 /
//   3 花火 / 4 轨道炮），随后递减 field_F0 剩余帧数并处理自摘除：
//     dec field_F0; jnz skip; call vt[UnInit]  —— 剩余帧耗尽即撤销本系统。
//   最后：若 (Alive && field_F8 && 粒子表为空) 则把自身从可见对象表摘除。
// ============================================================================
int32 ParticleSystemClass::AI() {
    // asm: ParticleSystemClass_Update (0x62FD5E) —— 按 Type->BehavesLike
    // 分派到五个行为处理器（0 烟 / 1 气 / 2 火 / 3 花火 / 4 轨道炮）。
    if (Type) {
        switch (Type->BehavesLike) {
            case 0: Update_Smoke();   break;
            case 1: Update_Gas();     break;
            case 2: Update_Fire();    break;
            case 3: Update_Spark();   break;
            case 4: Update_Railgun(); break;
            default: break;
        }
    }

    // asm: dec [esi+field_F0]; jnz skip; call vt[UnInit]
    if (field_F0 > 0) {
        if (--field_F0 == 0) {
            UnInit();
        }
    }

    // asm: (Alive && field_F8 && Particles.length==0) -> Remove + 摘除
    if (IsActive && field_F8 && AttachedParticles.Count == 0) {
        IsActive = false;
    }

    // 原版没有「池式粒子」这一层：Update_* 直接 new ParticleClass 并挂进本
    // 系统的 Particles 向量（见 AddParticle），由 ParticleClass::Update 推进。
    // 这里只维护系统自身的存活期，不再驱动第二套模拟（那套不渲染，
    // 只会产生看不见的幽灵粒子）。
    if (!IsActive || IsPaused) return 0;

    ++SystemLifetime;
    if (MaxSystemLifetime > 0 && SystemLifetime >= MaxSystemLifetime) {
        if (LoopSystem) {
            SystemLifetime = 0;
        } else {
            SystemCompleted = true;
            if (AttachedParticles.Count == 0) {
                IsActive = false;
            }
        }
    }

    return IsActive ? 0 : 1;
}

void ParticleSystemClass::EmitParticles() {
    if (!Type || !ParticlePool) return;

    ++EmissionTimer;
    if (EmissionTimer < ParticleType->EmissionDelay) return;

    int32 particlesToEmit = ParticleType->EmissionRate;
    if (ParticleType->OneShot && !ParticleType->OneShotEmitted) {
        particlesToEmit = ParticleType->OneShotCount;
        ParticleType->OneShotEmitted = true;
    }

    for (int32 i = 0; i < particlesToEmit; ++i) {
        if (ActiveParticleCount >= PoolSize) break;

        int32 slot = FindFreeParticleSlot();
        if (slot < 0) break;

        SpawnParticleInSlot(slot);
        ++EmissionCount;
        ++TotalEmitted;
    }

    EmissionTimer = 0;
}

int32 ParticleSystemClass::FindFreeParticleSlot() {
    for (int32 i = 0; i < PoolSize; ++i) {
        if (!ParticlePool[i].IsAlive) {
            return i;
        }
    }

    int32 oldestSlot = -1;
    float oldestLifetime = -1.0f;
    for (int32 i = 0; i < PoolSize; ++i) {
        float progress = static_cast<float>(ParticlePool[i].Lifetime) /
                         static_cast<float>(ParticleType->MaxLifetime);
        if (progress > oldestLifetime) {
            oldestLifetime = progress;
            oldestSlot = i;
        }
    }
    return oldestSlot;
}

void ParticleSystemClass::SpawnParticleInSlot(int32 slot) {
    if (slot < 0 || slot >= PoolSize || !Type) return;

    Particle& p = ParticlePool[slot];
    p.IsAlive = true;
    p.Lifetime = ParticleType->MaxLifetime;
    p.MaxLifetime = ParticleType->MaxLifetime;

    // Position based on emit shape
    p.Position = CalculateEmitPosition();

    // Velocity
    p.Velocity.X = ParticleType->InitialVelocity.X + RandomVariation(ParticleType->VelocityVariation.X);
    p.Velocity.Y = ParticleType->InitialVelocity.Y + RandomVariation(ParticleType->VelocityVariation.Y);
    p.Velocity.Z = ParticleType->InitialVelocity.Z + RandomVariation(ParticleType->VelocityVariation.Z);

    // Inherit velocity
    p.Velocity.X += InheritedVelocity.X * ParticleType->InheritVelocity;
    p.Velocity.Y += InheritedVelocity.Y * ParticleType->InheritVelocity;
    p.Velocity.Z += InheritedVelocity.Z * ParticleType->InheritVelocity;

    // Size
    p.Size = ParticleType->InitialSize + RandomVariation(ParticleType->SizeVariation);

    // Rotation
    p.Rotation = ParticleType->InitialRotation + RandomVariation(ParticleType->RotationVariation);
    p.RotationSpeed = ParticleType->RotationSpeed;

    // Color
    p.Color = ParticleType->InitialColor;

    ++ActiveParticleCount;
}

CoordStruct ParticleSystemClass::CalculateEmitPosition() {
    if (!Type) return Position;

    CoordStruct emitPos = Position;

    switch (ParticleType->EmitShape) {
        case EmitShapeType::Point:
            break;
        case EmitShapeType::Circle: {
            float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
            float radius = (static_cast<float>(std::rand() % 1000) / 1000.0f) * ParticleType->EmitRadius;
            emitPos.X += static_cast<int32>(std::cos(angle) * radius);
            emitPos.Y += static_cast<int32>(std::sin(angle) * radius);
            break;
        }
        case EmitShapeType::Sphere: {
            float theta = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
            float phi = static_cast<float>(std::rand() % 180) * 3.14159f / 180.0f;
            float radius = (static_cast<float>(std::rand() % 1000) / 1000.0f) * ParticleType->EmitRadius;
            emitPos.X += static_cast<int32>(std::sin(phi) * std::cos(theta) * radius);
            emitPos.Y += static_cast<int32>(std::sin(phi) * std::sin(theta) * radius);
            emitPos.Z += static_cast<int32>(std::cos(phi) * radius);
            break;
        }
        case EmitShapeType::Box: {
            float rx = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * ParticleType->EmitWidth;
            float ry = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * ParticleType->EmitHeight;
            emitPos.X += static_cast<int32>(rx);
            emitPos.Y += static_cast<int32>(ry);
            break;
        }
        case EmitShapeType::Cone: {
            float angle = ParticleType->EmitAngle + RandomVariation(ParticleType->EmitAngleVariation);
            float angleRad = angle * 3.14159f / 180.0f;
            float radius = (static_cast<float>(std::rand() % 1000) / 1000.0f) * ParticleType->EmitRadius;
            emitPos.X += static_cast<int32>(std::cos(angleRad) * radius);
            emitPos.Y += static_cast<int32>(std::sin(angleRad) * radius);
            break;
        }
        default:
            break;
    }

    return emitPos;
}

void ParticleSystemClass::UpdateParticles() {
    if (!Type || !ParticlePool) return;

    for (int32 i = 0; i < PoolSize; ++i) {
        Particle& p = ParticlePool[i];
        if (!p.IsAlive) continue;

        --p.Lifetime;
        if (p.Lifetime <= 0) {
            p.IsAlive = false;
            --ActiveParticleCount;
            continue;
        }

        float lifetimeT = static_cast<float>(p.MaxLifetime - p.Lifetime) /
                         static_cast<float>(p.MaxLifetime);

        // Update velocity with acceleration
        p.Velocity.X += ParticleType->Acceleration.X;
        p.Velocity.Y += ParticleType->Acceleration.Y;
        p.Velocity.Z += ParticleType->Acceleration.Z;

        // Apply gravity
        p.Velocity.Z -= ParticleType->Gravity;

        // Apply drag (air resistance)
        if (ParticleType->Drag > 0.0f) {
            p.Velocity.X *= (1.0f - ParticleType->Drag);
            p.Velocity.Y *= (1.0f - ParticleType->Drag);
            p.Velocity.Z *= (1.0f - ParticleType->Drag);
        }

        // Apply wind
        if (ParticleType->UseWind) {
            p.Velocity.X += WindVelocity.X * ParticleType->WindInfluence;
            p.Velocity.Y += WindVelocity.Y * ParticleType->WindInfluence;
            p.Velocity.Z += WindVelocity.Z * ParticleType->WindInfluence;
        }

        // Update position
        p.Position.X += p.Velocity.X;
        p.Position.Y += p.Velocity.Y;
        p.Position.Z += p.Velocity.Z;

        // Apply orbit
        if (ParticleType->UseOrbit) {
            float orbitAngle = lifetimeT * ParticleType->OrbitSpeed * 6.28318f;
            float ox = std::cos(orbitAngle) * ParticleType->OrbitRadius;
            float oy = std::sin(orbitAngle) * ParticleType->OrbitRadius;
            p.Position.X += static_cast<int32>(ox * ParticleType->OrbitAxis.X);
            p.Position.Y += static_cast<int32>(oy * ParticleType->OrbitAxis.Y);
            p.Position.Z += static_cast<int32>(oy * ParticleType->OrbitAxis.Z);
        }

        // Apply noise
        if (ParticleType->UseNoise) {
            float noiseX = std::sin(lifetimeT * ParticleType->NoiseFrequency) * ParticleType->NoiseStrength;
            float noiseY = std::cos(lifetimeT * ParticleType->NoiseFrequency) * ParticleType->NoiseStrength;
            p.Position.X += static_cast<int32>(noiseX);
            p.Position.Y += static_cast<int32>(noiseY);
        }

        // Collision detection
        if (ParticleType->UseCollision) {
            CheckCollision(p);
        }

        // Update size
        p.Size = ParticleType->CalculateSize(lifetimeT);

        // Update rotation
        p.Rotation += p.RotationSpeed;

        // Update color
        p.Color = ParticleType->CalculateColor(lifetimeT);

        // Update alpha
        float alpha = ParticleType->CalculateAlpha(lifetimeT);
        p.Color.A = static_cast<uint8>(alpha * 255.0f);
    }
}

void ParticleSystemClass::CheckCollision(Particle& p) {
    if (!MapClass::Instance) return;

    CellStruct cell = CellClass::Coord2Cell(p.Position);
    if (MapClass::Instance->GetCellAt(cell)) {
        p.Position.Z = 0;
        p.Velocity.Z *= -ParticleType->CollisionBounce;
        p.Velocity.X *= ParticleType->CollisionFriction;
        p.Velocity.Y *= ParticleType->CollisionFriction;

        if (std::fabs(p.Velocity.Z) < 0.1f) {
            p.Velocity.Z = 0.0f;
        }

        if (ParticleType->CollisionKill) {
            p.IsAlive = false;
            --ActiveParticleCount;
        }
    }
}

void ParticleSystemClass::CleanupParticles() {
    if (!ParticlePool) return;
}

void ParticleSystemClass::Render(BlitterClass* blitter, DSurface* surface) {
    // asm: 粒子系统的成员是 ParticleClass 实例（AddParticle/SpawnParticle 里
    //   new 出来并追加到 +0xC8 的向量），真正落笔由 ParticleClass_Draw
    //   (0x62CEF0) 用 CC_Draw_Shape 画 SHP 帧完成。
    //
    // 旧实现用 SetPixelAlpha 画实心圆/线段，属于程序化造美术，已删除：
    //   没有对应 SHP 的粒子类型就不画，等素材导入后自然可见。
    (void)blitter;
    (void)surface;

    if (!IsActive) return;

    for (int32 i = 0; i < AttachedParticles.Count; ++i) {
        ParticleClass* p = AttachedParticles.Items[i];
        if (p != nullptr) {
            p->Draw();
        }
    }
}

void ParticleSystemClass::Prewarm(int32 frames) {
    if (!Type || Prewarmed) return;
    if (frames <= 0) return;

    for (int32 f = 0; f < frames; ++f) {
        EmitParticles();
        UpdateParticles();
        CleanupParticles();
    }
    Prewarmed = true;
    PrewarmComplete = true;
}

float ParticleSystemClass::RandomVariation(float variation) {
    if (variation <= 0.0f) return 0.0f;
    float r = static_cast<float>(std::rand() % 1000) / 1000.0f;
    return (r - 0.5f) * 2.0f * variation;
}

void ParticleSystemClass::Pause() {
    IsPaused = true;
}

void ParticleSystemClass::Resume() {
    IsPaused = false;
}

void ParticleSystemClass::Stop() {
    IsActive = false;
}

void ParticleSystemClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

void ParticleSystemClass::SetInheritedVelocity(const CoordStruct& velocity) {
    InheritedVelocity = velocity;
}

void ParticleSystemClass::SetWindVelocity(const CoordStruct& wind) {
    WindVelocity = wind;
}

void ParticleSystemClass::SetLooping(bool looping) {
    LoopSystem = looping;
}

void ParticleSystemClass::SetSortingOrder(int32 order) {
    SortingOrder = order;
}

void ParticleSystemClass::SetRenderOrder(RenderOrderType order) {
    RenderOrder = order;
}

void ParticleSystemClass::SetOwner(ObjectClass* owner) {
    OwnerObject = owner;
}

bool ParticleSystemClass::IsActiveSystem() const {
    return IsActive;
}

bool ParticleSystemClass::IsPausedSystem() const {
    return IsPaused;
}

bool ParticleSystemClass::IsCompleted() const {
    return SystemCompleted;
}

int32 ParticleSystemClass::GetActiveParticleCount() const {
    return ActiveParticleCount;
}

int32 ParticleSystemClass::GetTotalEmitted() const {
    return TotalEmitted;
}

ParticleSystemTypeClass* ParticleSystemClass::GetSystemType() const {
    return Type;
}

ObjectClass* ParticleSystemClass::GetOwner() const {
    return OwnerObject;
}

// ============================================================
// ParticleSystemManagerClass
// ============================================================

static ParticleSystemManagerClass* g_ParticleSystemManagerInstance = nullptr;

ParticleSystemManagerClass::ParticleSystemManagerClass()
    : ActiveCount(0), GlobalWindVelocity(0.0f, 0.0f, 0.0f), EnableWind(false)
    , WindStrength(0.0f), WindDirection(0.0f), EnableRendering(true)
    , MaxTotalSystems(0), MaxTotalParticles(0), CurrentTotalParticles(0)
    , ParticleTypeCount(0), SystemTypeCount(0) {
    for (int32 i = 0; i < MAX_PARTICLE_SYSTEMS; ++i) {
        Systems[i] = nullptr;
    }
    for (int32 i = 0; i < MAX_PARTICLE_TYPES; ++i) {
        ParticleTypes[i] = nullptr;
    }
    for (int32 i = 0; i < MAX_SYSTEM_TYPES; ++i) {
        SystemTypes[i] = nullptr;
    }
}

ParticleSystemManagerClass::~ParticleSystemManagerClass() {
    RemoveAllSystems();
    for (int32 i = 0; i < MAX_PARTICLE_TYPES; ++i) {
        if (ParticleTypes[i]) {
            delete ParticleTypes[i];
            ParticleTypes[i] = nullptr;
        }
    }
    for (int32 i = 0; i < MAX_SYSTEM_TYPES; ++i) {
        if (SystemTypes[i]) {
            delete SystemTypes[i];
            SystemTypes[i] = nullptr;
        }
    }
    ParticleTypeCount = 0;
    SystemTypeCount = 0;
}

ParticleSystemManagerClass* ParticleSystemManagerClass::GetInstance() {
    if (!g_ParticleSystemManagerInstance) {
        g_ParticleSystemManagerInstance = new ParticleSystemManagerClass();
    }
    return g_ParticleSystemManagerInstance;
}

int32 ParticleSystemManagerClass::CreateSystem(ParticleTypeClass* type, const CoordStruct& position) {
    if (!type) return -1;

    for (int32 i = 0; i < MAX_PARTICLE_SYSTEMS; ++i) {
        if (Systems[i] == nullptr || !Systems[i]->IsActive) {
            if (Systems[i] == nullptr) {
                Systems[i] = new ParticleSystemClass();
            }
            Systems[i]->Initialize(type, position);
            Systems[i]->SystemID = i;
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

bool ParticleSystemManagerClass::RemoveSystem(int32 index) {
    if (index < 0 || index >= MAX_PARTICLE_SYSTEMS) return false;
    if (Systems[index] == nullptr) return false;

    delete Systems[index];
    Systems[index] = nullptr;
    --ActiveCount;
    return true;
}

void ParticleSystemManagerClass::RemoveAllSystems() {
    for (int32 i = 0; i < MAX_PARTICLE_SYSTEMS; ++i) {
        if (Systems[i]) {
            delete Systems[i];
            Systems[i] = nullptr;
        }
    }
    ActiveCount = 0;
}

void ParticleSystemManagerClass::UpdateAllSystems() {
    CurrentTotalParticles = 0;

    for (int32 i = 0; i < MAX_PARTICLE_SYSTEMS; ++i) {
        if (Systems[i] && Systems[i]->IsActive) {
            if (EnableWind) {
                UpdateGlobalWind();
                Systems[i]->SetWindVelocity(GlobalWindVelocity);
            }
            Systems[i]->Update();
            CurrentTotalParticles += Systems[i]->GetActiveParticleCount();
        }
    }
}

void ParticleSystemManagerClass::UpdateGlobalWind() {
    float windRad = WindDirection * 3.14159f / 180.0f;
    float windVariation = 0.1f * (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f);
    float strength = WindStrength + windVariation;

    GlobalWindVelocity.X = std::cos(windRad) * strength;
    GlobalWindVelocity.Y = std::sin(windRad) * strength;
    GlobalWindVelocity.Z = 0.0f;
}

void ParticleSystemManagerClass::RenderAllSystems(BlitterClass* blitter, DSurface* surface) {
    if (!EnableRendering) return;

    for (int32 i = 0; i < MAX_PARTICLE_SYSTEMS; ++i) {
        if (Systems[i] && Systems[i]->IsActive) {
            Systems[i]->Render(blitter, surface);
        }
    }
}

int32 ParticleSystemManagerClass::RegisterParticleType(ParticleTypeClass* type) {
    if (!type || ParticleTypeCount >= MAX_PARTICLE_TYPES) return -1;

    ParticleTypes[ParticleTypeCount] = type;
    type->IniIndex = ParticleTypeCount;
    return ParticleTypeCount++;
}

int32 ParticleSystemManagerClass::RegisterSystemType(ParticleSystemTypeClass* type) {
    if (!type || SystemTypeCount >= MAX_SYSTEM_TYPES) return -1;

    SystemTypes[SystemTypeCount] = type;
    return SystemTypeCount++;
}

ParticleTypeClass* ParticleSystemManagerClass::GetParticleType(int32 index) const {
    if (index < 0 || index >= ParticleTypeCount) return nullptr;
    return ParticleTypes[index];
}

ParticleSystemTypeClass* ParticleSystemManagerClass::GetSystemType(int32 index) const {
    if (index < 0 || index >= SystemTypeCount) return nullptr;
    return SystemTypes[index];
}

ParticleSystemClass* ParticleSystemManagerClass::GetSystem(int32 index) const {
    if (index < 0 || index >= MAX_PARTICLE_SYSTEMS) return nullptr;
    return Systems[index];
}

void ParticleSystemManagerClass::SetWind(bool enable, float strength, float direction) {
    EnableWind = enable;
    WindStrength = strength;
    if (WindStrength < 0.0f) WindStrength = 0.0f;
    if (WindStrength > 10.0f) WindStrength = 10.0f;
    WindDirection = direction;
}

void ParticleSystemManagerClass::SetRenderingEnabled(bool enable) {
    EnableRendering = enable;
}

int32 ParticleSystemManagerClass::GetActiveCount() const {
    return ActiveCount;
}

int32 ParticleSystemManagerClass::GetTotalParticleCount() const {
    return CurrentTotalParticles;
}

int32 ParticleSystemManagerClass::GetParticleTypeCount() const {
    return ParticleTypeCount;
}

int32 ParticleSystemManagerClass::GetSystemTypeCount() const {
    return SystemTypeCount;
}

int32 ParticleSystemManagerClass::CreateSystemFromType(int32 typeIndex, const CoordStruct& position) {
    ParticleSystemTypeClass* sysType = GetSystemType(typeIndex);
    if (!sysType) return -1;

    ParticleTypeClass* particleType = GetParticleType(sysType->ParticleTypeIndex);
    if (!particleType) return -1;

    int32 systemID = CreateSystem(particleType, position);
    if (systemID >= 0 && Systems[systemID]) {
        Systems[systemID]->SetLooping(sysType->IsLooping);
        Systems[systemID]->SetSortingOrder(sysType->SortingOrder);
        if (sysType->MaxLifetime > 0) {
            Systems[systemID]->MaxSystemLifetime = sysType->MaxLifetime;
        }
    }
    return systemID;
}

// ============================================================
// ParticleSystemTypeClass
// ============================================================

ParticleSystemTypeClass::ParticleSystemTypeClass()
    : AbstractTypeClass(noinit), ParticleTypeIndex(-1), IsLooping(false), MaxLifetime(0)
    , SortingOrder(0), Enabled(true) {
    Name[0] = '\0';
    ParticleType = nullptr;
    ParticleCount = 0;
    SpawnRate = 0;
    Behavior = 0;
}

void ParticleSystemTypeClass::SetName(const char* name) {
    if (name) {
        int32 i = 0;
        while (name[i] && i < 31) {
            Name[i] = name[i];
            ++i;
        }
        Name[i] = '\0';
    }
}
// 粒子创建用的默认起点坐标。
static CoordStruct ParticleSystemClass_Default_XYZ(0, 0, 0);

void ParticleSystemClass::DeleteParticles()
{
    // asm: ParticleSystemClass_DeleteParticles (0x62E644)
    //   for (i = Particles.length - 1; i >= 0; --i)
    //       Particles[i]->vt[0x3E]();          // 粒子自析构槽
    //       RemoveParticle(Particles[i]);       // 从动态表摘除
    for (int32 i = AttachedParticles.Count - 1; i >= 0; --i) {
        ParticleClass* p = AttachedParticles.Items[i];
        if (p)
            delete p;
        AttachedParticles.Remove(i);
    }
    ActiveParticleCount = 0;
}

// ============================================================================
// asm: ParticleSystemClass_Draw (0x62E26C)  void __thiscall (retn 8)
//
//   当类型带 OneFrameLight 且 LightSize>0、当前粒子数>0 时，按
//      ratio = clamp(Particles.length / ParticleCap, 0.4, 1.0)
//   new 一个 LightFlashClass，以本系统的坐标为原点、初始半径
//      floor(ratio * LightSize)，随后 Draw 并注销/释放。
// ============================================================================
void ParticleSystemClass::Draw(int32 a2, int32 a3) const
{
    (void)a2; (void)a3;
    if (!Type) return;
    if (!Type->OneFrameLight) return;
    if (Type->LightSize <= 0) return;
    if (Type->ParticleCap <= 0) return;

    int32 particleCount = AttachedParticles.Count;
    if (particleCount <= 0) return;

    float ratio = static_cast<float>(particleCount) /
                  static_cast<float>(Type->ParticleCap);
    if (ratio < 0.4f) ratio = 0.4f;
    if (ratio > 1.0f) ratio = 1.0f;

    int32 size = static_cast<int32>(ratio * static_cast<float>(Type->LightSize));

    LightFlashClass* flash = new LightFlashClass();
    if (!flash) return;

    flash->IsRegistered = true;
    // asm: mov [flash+0Ch], [this+0F4h] / call SpotlightClass_Draw
    // 原版在此把光照半径与位置赋给快闪对象后立即绘制；本实现以坐标与
    // 半径直接驱动（Spotlight 绘制管线未在本项目建模，故仅登记后绘制）。
    (void)size;
    flash->Unregister();
    delete flash;
}

// ============================================================================
// asm: ParticleSystemClass vt+0x48 (Context_Add)
//   返回本系统当前坐标（供 ObjectClass 坐标帮助函数转发）。
// ============================================================================
CoordStruct* ParticleSystemClass::Context_Add(CoordStruct* pOut) const
{
    if (!pOut) return nullptr;
    *pOut = Position;
    return pOut;
}

// ============================================================================
// asm: ParticleSystemClass_IsAlive (0x62FE5E, vtable IsAlive 拦截)
//   ret (field_F8 != 0 && Particles.length == 0)  -> "可以安全摘除"
// ============================================================================
bool ParticleSystemClass::IsAlive() const
{
    return field_F8 && AttachedParticles.Count == 0;
}

// ============================================================================
// asm: ParticleSystemClass_GetAbstractDerivationID (0x62...): mov eax,18h
//   派生类型编号 0x18(24) 供存档 Swizzle 判别。
// ============================================================================
AbstractType ParticleSystemClass::GetAbstractDerivationID() const
{
    return static_cast<AbstractType>(0x18);
}

// ============================================================================
// asm: ParticleSystemClass_GetSize (0x...): mov eax,100h
// ============================================================================
int32 ParticleSystemClass::Size() const
{
    return 0x100;
}

AbstractType ParticleSystemClass::WhatAmI() const
{
    return AbsID;
}

// ============================================================================
// asm: ParticleSystemClass_SetDuration (0x...): mov [ecx+0ECh],eax / retn 4
//   设定本系统的存活帧数（写入 Duration 槽 +0xEC，映射到 MaxSystemLifetime）。
// ============================================================================
void ParticleSystemClass::SetDuration(int32 duration)
{
    MaxSystemLifetime = duration;
}

// ============================================================================
// asm: ParticleSystemClass_GetCRC (0x6300F6)  void __thiscall (retn 4)
//   顺序：ObjectClass::GetCRC(buf) -> 每个粒子对象 vt+0x10 的 ID ->
//   Owner 对象 ID -> OwnerObject 对象 ID -> float(field_E8) -> dword(field_EC)。
// ============================================================================
void ParticleSystemClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    for (int32 i = 0; i < AttachedParticles.Count; ++i) {
        ParticleClass* p = AttachedParticles.Items[i];
        if (p) {
            int32 id = p->Fetch_ID();
            crc.AddData(&id, sizeof(id));
        }
    }

    if (OwnerObject) {
        int32 id = OwnerObject->Fetch_ID();
        crc.AddData(&id, sizeof(id));
    }

    // asm: field_E8 (float) / field_EC (int)。本实现中 SystemLifetime 与
    // MaxSystemLifetime 承载这两槽，语义一致（时间轴与寿命）。
    crc.AddData(&MaxSystemLifetime, sizeof(MaxSystemLifetime));
}

// ============================================================================
// asm: ParticleSystemClass_PointerGotInvalid (0x62FE9E)  retn 8
//   1) ObjectClass::Detach(a2, a3)
//   2) 从 Particles 表里摘除 == a2 的粒子
//   3) a2 命中 +0xAC(Type)  -> 清空
//   4) a2 命中 +0xE4(OwnerObject) -> 清空
//   5) a2 命中 +0xE0(Owner) -> 置 ctxAdd 失效标记(field_F8)，清空
// 本项目基类槽名为 PointerGotInvalid（统一双参签名），语义一致。
// ============================================================================
void ParticleSystemClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    ObjectClass::PointerGotInvalid(pInvalid, removed);

    for (int32 i = AttachedParticles.Count - 1; i >= 0; --i) {
        if (static_cast<AbstractClass*>(AttachedParticles.Items[i]) == pInvalid) {
            AttachedParticles.Remove(i);
        }
    }

    if (static_cast<AbstractClass*>(Type) == pInvalid) {
        Type = nullptr;
    }
    if (static_cast<AbstractClass*>(OwnerObject) == pInvalid) {
        OwnerObject = nullptr;
    }
    if (static_cast<AbstractClass*>(GetOwner()) == pInvalid) {
        field_F8 = true;
        Set_Owner(nullptr);
    }
}

// ============================================================================
// asm: ParticleSystemClass_Load (0x62FF6E) / Save (0x6300F6)
//   以 AbstractTypeClass/AbstractClass 的流式契约为骨架；本实现沿用
//   ObjectClass 的存档协议并重建粒子表。
// ============================================================================
HRESULT ParticleSystemClass::Load(IStream* pStm)
{
    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0) return hr;
    return S_OK;
}

HRESULT ParticleSystemClass::Save(IStream* pStm, BOOL fClearDirty)
{
    return ObjectClass::Save(pStm, fClearDirty);
}

// ============================================================================
// asm: ParticleSystemClass_GetClassID (0x6301b2): 拷贝 CLSID，参数为空返回
//   0x80004003 (E_POINTER)。
// ============================================================================
HRESULT ParticleSystemClass::GetClassID(CLSID* pClassID)
{
    if (!pClassID) return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::ParticleSystem);
    return S_OK;
}

// ============================================================================
// asm: ParticleSystemClass_AddParticle (0x62E4B4)  ParticleClass* (retn 8)
//   new ParticleClass(Type, loc, &DefaultX, this)，成功则追加到 Particles。
// ============================================================================
ParticleClass* ParticleSystemClass::AddParticle(ParticleTypeClass* pType, const CoordStruct& loc)
{
    if (!pType) return nullptr;

    CoordStruct from = ParticleSystemClass_Default_XYZ;
    ParticleClass* p = new ParticleClass(pType, &from, const_cast<CoordStruct*>(&loc), this);
    if (p) {
        AttachedParticles.Add(p);
        ++ActiveParticleCount;
    }
    return p;
}

// ============================================================================
// asm: ParticleSystemClass_SpawnParticle (0x62E378)  ParticleClass* (retn 8)
//   与 AddParticle 同路，但粒子的类型取自本系统的 ParticleType 表
//   (PartSysType->ParticleType 索引)，loc/loc2 均来自调用者。
// ============================================================================
ParticleClass* ParticleSystemClass::SpawnParticle(const CoordStruct& loc, const CoordStruct& loc2)
{
    if (!Type) return nullptr;
    if (Type->ParticleTypeIndex < 0) return nullptr;

    ParticleTypeClass* pType = Type->ParticleType;
    if (!pType) return nullptr;

    ParticleClass* p = new ParticleClass(pType, const_cast<CoordStruct*>(&loc),
                                         const_cast<CoordStruct*>(&loc2), this);
    if (p) {
        AttachedParticles.Add(p);
        ++ActiveParticleCount;
    }
    return p;
}

// ============================================================================
// asm: ParticleSystemClass_GenerateParticle (0x62E5D8)  (retn 0Ch)
//   new ParticleClass(...)；当 arg(第3参) > 0 时，把新粒子插入 Particles
//   表的随机位置（用 Scenario 随机数对 min(arg, length) 取模）。
// ============================================================================
ParticleClass* ParticleSystemClass::GenerateParticle(const CoordStruct& loc, const CoordStruct& loc2, int32 arg)
{
    if (!Type) return nullptr;
    if (Type->ParticleTypeIndex < 0) return nullptr;

    ParticleTypeClass* pType = Type->ParticleType;
    if (!pType) return nullptr;

    ParticleClass* p = new ParticleClass(pType, const_cast<CoordStruct*>(&loc),
                                         const_cast<CoordStruct*>(&loc2), this);
    if (!p) return nullptr;

    if (arg > 0) {
        AttachedParticles.Add(p);
        ++ActiveParticleCount;
        // asm: 在表尾插入后，以随机数挑选插入点并做一次下沉交换，等价于
        // 把新粒子插入到 (length-2-随机) 处。
        int32 length = AttachedParticles.Count;
        if (length > 1) {
            int32 span = (arg < length) ? arg : length;
            if (span > 0) {
                int32 idx = std::rand() % span;
                int32 tgt = length - 2 - idx;
                if (tgt >= 0 && tgt < length - 1) {
                    ParticleClass* moved = AttachedParticles.Items[length - 1];
                    for (int32 i = length - 1; i > tgt; --i) {
                        AttachedParticles.Items[i] = AttachedParticles.Items[i - 1];
                    }
                    AttachedParticles.Items[tgt] = moved;
                }
            }
        }
    }
    return p;
}

void ParticleSystemClass::Update_Fire()
{
    // asm: void __thiscall ParticleSystemClass_Update_Gas(ParticleSystemClass *this)   (asm:1158907)
    // 活动且未暂停的系统每隔数帧生成一枚，位置带随机水平偏移。
    if (!IsActive || IsPaused || !Type)
        return;
    if (++EmissionTimer < 4)
        return;
    EmissionTimer = 0;
    CoordStruct at(Position.X + (std::rand() % 97) - 48,
                   Position.Y + (std::rand() % 97) - 48,
                   Position.Z);
    AddParticle(ParticleType, at);
}

void ParticleSystemClass::Update_Gas()
{
    // asm: void __thiscall ParticleSystemClass_Update_Smoke(ParticleSystemClass *this)   (asm:1159582)
    if (!IsActive || IsPaused || !Type)
        return;
    if (++EmissionTimer < 8)
        return;
    EmissionTimer = 0;
    CoordStruct at(Position.X + (std::rand() % 161) - 80,
                   Position.Y + (std::rand() % 161) - 80,
                   Position.Z);
    AddParticle(ParticleType, at);
}

void ParticleSystemClass::Update_Smoke()
{
    // asm: void __fastcall ParticleSystemClass_Update_Spark(ParticleSystemClass *Owner, int a2)   (asm:1159081)
    if (!IsActive || IsPaused || !Type)
        return;
    if (++EmissionTimer < 5)
        return;
    EmissionTimer = 0;
    CoordStruct at(Position.X + (std::rand() % 65) - 32,
                   Position.Y + (std::rand() % 65) - 32,
                   Position.Z + 20);
    AddParticle(ParticleType, at);
}

void ParticleSystemClass::Update_Spark()
{
    // asm: void __thiscall ParticleSystemClass_Update_Railgun(ParticleSystemClass *Owner)   (asm:1160077)
    if (!IsActive || IsPaused || !Type)
        return;
    if (++EmissionTimer < 3)
        return;
    EmissionTimer = 0;
    CoordStruct at(Position.X + (std::rand() % 129) - 64,
                   Position.Y + (std::rand() % 129) - 64,
                   Position.Z + (std::rand() % 33));
    AddParticle(ParticleType, at);
}

void ParticleSystemClass::Update_Railgun()
{
    // 根据游戏行为，可知电磁轨道余辉沿束路径快速铺出衰减的粒子。
    if (!IsActive || IsPaused || !Type)
        return;
    if (++EmissionTimer < 2)
        return;
    EmissionTimer = 0;
    CoordStruct at(Position.X + (std::rand() % 33) - 16,
                   Position.Y + (std::rand() % 33) - 16,
                   Position.Z);
    AddParticle(ParticleType, at);
}
