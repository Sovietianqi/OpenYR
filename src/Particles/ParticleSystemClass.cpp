#include "ParticleSystemClass.h"
#include "../Rendering/Blitter.h"
#include "../Rendering/Surface.h"
#include "../Map/MapClass.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

// ============================================================
// ParticleSystemClass
// ============================================================

ParticleSystemClass::ParticleSystemClass()
    : Type(nullptr), Position(0, 0, 0), IsActive(false), IsPaused(false)
    , EmissionTimer(0), EmissionCount(0), TotalEmitted(0)
    , ActiveParticleCount(0), ParticlePool(nullptr), PoolSize(0)
    , SystemLifetime(0), MaxSystemLifetime(0), LoopSystem(false)
    , SystemCompleted(false), InheritedVelocity(0.0f, 0.0f, 0.0f)
    , WindVelocity(0.0f, 0.0f, 0.0f), SortingOrder(0)
    , RenderOrder(RenderOrderType::BackToFront), OwnerObject(nullptr)
    , SystemID(0), Prewarmed(false), PrewarmComplete(false) {
}

ParticleSystemClass::~ParticleSystemClass() {
    Destroy();
}

void ParticleSystemClass::Initialize(ParticleTypeClass* type, const CoordStruct& position) {
    if (!type) return;

    Type = type;
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

    if (type->MaxSystemLifetime > 0) {
        MaxSystemLifetime = type->MaxSystemLifetime;
    } else {
        MaxSystemLifetime = 0;
    }

    PoolSize = type->MaxParticles;
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

    if (type->Prewarm > 0) {
        Prewarm(type->Prewarm);
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
    Type = nullptr;
}

void ParticleSystemClass::Update() {
    if (!IsActive || !Type || IsPaused) return;

    ++SystemLifetime;
    if (MaxSystemLifetime > 0 && SystemLifetime >= MaxSystemLifetime) {
        if (LoopSystem) {
            SystemLifetime = 0;
        } else {
            SystemCompleted = true;
            if (ActiveParticleCount <= 0) {
                IsActive = false;
            }
        }
    }

    // Emit new particles
    if (!SystemCompleted || LoopSystem) {
        EmitParticles();
    }

    // Update existing particles
    UpdateParticles();

    // Remove dead particles
    CleanupParticles();
}

void ParticleSystemClass::EmitParticles() {
    if (!Type || !ParticlePool) return;

    ++EmissionTimer;
    if (EmissionTimer < Type->EmissionDelay) return;

    int32 particlesToEmit = Type->EmissionRate;
    if (Type->OneShot && !Type->OneShotEmitted) {
        particlesToEmit = Type->OneShotCount;
        Type->OneShotEmitted = true;
    }

    for (int32 i = 0; i < particlesToEmit; ++i) {
        if (ActiveParticleCount >= PoolSize) break;

        int32 slot = FindFreeParticleSlot();
        if (slot < 0) break;

        SpawnParticle(slot);
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
                         static_cast<float>(Type->MaxLifetime);
        if (progress > oldestLifetime) {
            oldestLifetime = progress;
            oldestSlot = i;
        }
    }
    return oldestSlot;
}

void ParticleSystemClass::SpawnParticle(int32 slot) {
    if (slot < 0 || slot >= PoolSize || !Type) return;

    Particle& p = ParticlePool[slot];
    p.IsAlive = true;
    p.Lifetime = Type->MaxLifetime;
    p.MaxLifetime = Type->MaxLifetime;

    // Position based on emit shape
    p.Position = CalculateEmitPosition();

    // Velocity
    p.Velocity.X = Type->InitialVelocity.X + RandomVariation(Type->VelocityVariation.X);
    p.Velocity.Y = Type->InitialVelocity.Y + RandomVariation(Type->VelocityVariation.Y);
    p.Velocity.Z = Type->InitialVelocity.Z + RandomVariation(Type->VelocityVariation.Z);

    // Inherit velocity
    p.Velocity.X += InheritedVelocity.X * Type->InheritVelocity;
    p.Velocity.Y += InheritedVelocity.Y * Type->InheritVelocity;
    p.Velocity.Z += InheritedVelocity.Z * Type->InheritVelocity;

    // Size
    p.Size = Type->InitialSize + RandomVariation(Type->SizeVariation);

    // Rotation
    p.Rotation = Type->InitialRotation + RandomVariation(Type->RotationVariation);
    p.RotationSpeed = Type->RotationSpeed;

    // Color
    p.Color = Type->InitialColor;

    ++ActiveParticleCount;
}

CoordStruct ParticleSystemClass::CalculateEmitPosition() {
    if (!Type) return Position;

    CoordStruct emitPos = Position;

    switch (Type->EmitShape) {
        case EmitShapeType::Point:
            break;
        case EmitShapeType::Circle: {
            float angle = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
            float radius = (static_cast<float>(std::rand() % 1000) / 1000.0f) * Type->EmitRadius;
            emitPos.X += static_cast<int32>(std::cos(angle) * radius);
            emitPos.Y += static_cast<int32>(std::sin(angle) * radius);
            break;
        }
        case EmitShapeType::Sphere: {
            float theta = static_cast<float>(std::rand() % 360) * 3.14159f / 180.0f;
            float phi = static_cast<float>(std::rand() % 180) * 3.14159f / 180.0f;
            float radius = (static_cast<float>(std::rand() % 1000) / 1000.0f) * Type->EmitRadius;
            emitPos.X += static_cast<int32>(std::sin(phi) * std::cos(theta) * radius);
            emitPos.Y += static_cast<int32>(std::sin(phi) * std::sin(theta) * radius);
            emitPos.Z += static_cast<int32>(std::cos(phi) * radius);
            break;
        }
        case EmitShapeType::Box: {
            float rx = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * Type->EmitWidth;
            float ry = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * Type->EmitHeight;
            emitPos.X += static_cast<int32>(rx);
            emitPos.Y += static_cast<int32>(ry);
            break;
        }
        case EmitShapeType::Cone: {
            float angle = Type->EmitAngle + RandomVariation(Type->EmitAngleVariation);
            float angleRad = angle * 3.14159f / 180.0f;
            float radius = (static_cast<float>(std::rand() % 1000) / 1000.0f) * Type->EmitRadius;
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
        p.Velocity.X += Type->Acceleration.X;
        p.Velocity.Y += Type->Acceleration.Y;
        p.Velocity.Z += Type->Acceleration.Z;

        // Apply gravity
        p.Velocity.Z -= Type->Gravity;

        // Apply drag (air resistance)
        if (Type->Drag > 0.0f) {
            p.Velocity.X *= (1.0f - Type->Drag);
            p.Velocity.Y *= (1.0f - Type->Drag);
            p.Velocity.Z *= (1.0f - Type->Drag);
        }

        // Apply wind
        if (Type->UseWind) {
            p.Velocity.X += WindVelocity.X * Type->WindInfluence;
            p.Velocity.Y += WindVelocity.Y * Type->WindInfluence;
            p.Velocity.Z += WindVelocity.Z * Type->WindInfluence;
        }

        // Update position
        p.Position.X += p.Velocity.X;
        p.Position.Y += p.Velocity.Y;
        p.Position.Z += p.Velocity.Z;

        // Apply orbit
        if (Type->UseOrbit) {
            float orbitAngle = lifetimeT * Type->OrbitSpeed * 6.28318f;
            float ox = std::cos(orbitAngle) * Type->OrbitRadius;
            float oy = std::sin(orbitAngle) * Type->OrbitRadius;
            p.Position.X += static_cast<int32>(ox * Type->OrbitAxis.X);
            p.Position.Y += static_cast<int32>(oy * Type->OrbitAxis.Y);
            p.Position.Z += static_cast<int32>(oy * Type->OrbitAxis.Z);
        }

        // Apply noise
        if (Type->UseNoise) {
            float noiseX = std::sin(lifetimeT * Type->NoiseFrequency) * Type->NoiseStrength;
            float noiseY = std::cos(lifetimeT * Type->NoiseFrequency) * Type->NoiseStrength;
            p.Position.X += static_cast<int32>(noiseX);
            p.Position.Y += static_cast<int32>(noiseY);
        }

        // Collision detection
        if (Type->UseCollision) {
            CheckCollision(p);
        }

        // Update size
        p.Size = Type->CalculateSize(lifetimeT);

        // Update rotation
        p.Rotation += p.RotationSpeed;

        // Update color
        p.Color = Type->CalculateColor(lifetimeT);

        // Update alpha
        float alpha = Type->CalculateAlpha(lifetimeT);
        p.Color.A = static_cast<uint8>(alpha * 255.0f);
    }
}

void ParticleSystemClass::CheckCollision(Particle& p) {
    if (!MapClass::Instance) return;

    CellStruct cell = CellClass::Coord2Cell(p.Position);
    if (MapClass::Instance->GetCellAt(cell)) {
        p.Position.Z = 0;
        p.Velocity.Z *= -Type->CollisionBounce;
        p.Velocity.X *= Type->CollisionFriction;
        p.Velocity.Y *= Type->CollisionFriction;

        if (std::fabs(p.Velocity.Z) < 0.1f) {
            p.Velocity.Z = 0.0f;
        }

        if (Type->CollisionKill) {
            p.IsAlive = false;
            --ActiveParticleCount;
        }
    }
}

void ParticleSystemClass::CleanupParticles() {
    if (!ParticlePool) return;
}

void ParticleSystemClass::Render(BlitterClass* blitter, DSurface* surface) {
    if (!IsActive || !Type || !ParticlePool || !blitter || !surface) return;

    for (int32 i = 0; i < PoolSize; ++i) {
        Particle& p = ParticlePool[i];
        if (!p.IsAlive) continue;

        if (Type->ParticleType == ParticleRenderType::Sprite) {
            RenderSprite(blitter, surface, p);
        } else if (Type->ParticleType == ParticleRenderType::Trail) {
            RenderTrail(blitter, surface, p);
        }
    }
}

void ParticleSystemClass::RenderSprite(BlitterClass* blitter, DSurface* surface, const Particle& p) {
    if (!Type || !blitter || !surface) return;

    int32 screenX = p.Position.X;
    int32 screenY = p.Position.Y;
    int32 halfSize = static_cast<int32>(p.Size * 0.5f);

    uint8 r = p.Color.R;
    uint8 g = p.Color.G;
    uint8 b = p.Color.B;
    uint8 a = p.Color.A;

    if (a <= 0) return;

    if (Type->UseBillboard) {
        for (int32 dy = -halfSize; dy <= halfSize; ++dy) {
            for (int32 dx = -halfSize; dx <= halfSize; ++dx) {
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (dist < halfSize) {
                    float fade = 1.0f - (dist / halfSize);
                    fade = fade * fade;
                    uint8 pixelAlpha = static_cast<uint8>(a * fade);
                    surface->SetPixelAlpha(screenX + dx, screenY + dy, r, g, b, pixelAlpha);
                }
            }
        }
    } else {
        for (int32 dy = -halfSize; dy <= halfSize; ++dy) {
            for (int32 dx = -halfSize; dx <= halfSize; ++dx) {
                surface->SetPixelAlpha(screenX + dx, screenY + dy, r, g, b, a);
            }
        }
    }
}

void ParticleSystemClass::RenderTrail(BlitterClass* blitter, DSurface* surface, const Particle& p) {
    if (!Type || !blitter || !surface) return;
    if (Type->TrailLength <= 0) return;

    CoordStruct trailStart = p.Position;
    CoordStruct trailEnd = p.Position;
    trailEnd.X -= p.Velocity.X * Type->TrailLength;
    trailEnd.Y -= p.Velocity.Y * Type->TrailLength;

    float dx = static_cast<float>(trailEnd.X - trailStart.X);
    float dy = static_cast<float>(trailEnd.Y - trailStart.Y);
    float segLen = std::sqrt(dx * dx + dy * dy);
    if (segLen < 0.001f) return;

    float dirX = dx / segLen;
    float dirY = dy / segLen;
    float perpX = -dirY * Type->TrailWidth;
    float perpY = dirX * Type->TrailWidth;

    int32 steps = static_cast<int32>(segLen);
    if (steps < 1) steps = 1;

    float curX = static_cast<float>(trailStart.X);
    float curY = static_cast<float>(trailStart.Y);

    for (int32 i = 0; i < steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        float alpha = p.Color.A * (1.0f - t);

        int32 px = static_cast<int32>(curX);
        int32 py = static_cast<int32>(curY);

        surface->SetPixelAlpha(px, py, Type->TrailColor.R, Type->TrailColor.G, Type->TrailColor.B,
                               static_cast<uint8>(alpha));

        curX += dirX;
        curY += dirY;
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

ParticleTypeClass* ParticleSystemClass::GetType() const {
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