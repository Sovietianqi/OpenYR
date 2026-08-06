#include "DiskLaserClass.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Houses/HouseClass.h"
#include "../Rendering/Blitter.h"
#include "../Rendering/Surface.h"
#include "../Abstract/ObjectClass.h"
#include "../Abstract/TechnoClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================
// DiskLaserClass
// ============================================================

DiskLaserClass::DiskLaserClass()
    : Position(0, 0, 0), TargetPosition(0, 0, 0), BeamWidth(8), BeamLength(0)
    , CurrentAngle(0.0f), RotationSpeed(0.05f), ScanAngle(0.0f), ScanRange(45.0f)
    , ScanDirection(1.0f), IsActive(false), IsScanning(false)
    , DamagePerTick(10), DamageInterval(5), DamageTimer(0)
    , Lifetime(0), MaxLifetime(300), CurrentLifetime(0)
    , Color1(255, 0, 0), Color2(255, 255, 0), Color3(0, 255, 255)
    , ColorPhase(0.0f), ColorSpeed(0.02f), IsTracking(true)
    , FadeInTime(0), FadeOutTime(0), CurrentAlpha(255)
    , OwnerHouse(-1), TargetID(-1), BeamSegments(nullptr), SegmentCount(0) {
}

DiskLaserClass::~DiskLaserClass() {
    ReleaseSegments();
}

void DiskLaserClass::Initialize(const CoordStruct& pos, const CoordStruct& target, int32 damage, int32 lifetime) {
    Position = pos;
    TargetPosition = target;
    DamagePerTick = damage;
    MaxLifetime = lifetime;
    CurrentLifetime = 0;
    Lifetime = 0;
    CurrentAngle = 0.0f;
    DamageTimer = 0;
    IsActive = true;
    IsScanning = false;
    CurrentAlpha = 255;
    FadeInTime = 0;
    FadeOutTime = 0;
    ReleaseSegments();

    int32 dx = target.X - pos.X;
    int32 dy = target.Y - pos.Y;
    BeamLength = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
}

void DiskLaserClass::ReleaseSegments() {
    if (BeamSegments) {
        delete[] BeamSegments;
        BeamSegments = nullptr;
    }
    SegmentCount = 0;
}

void DiskLaserClass::Update() {
    if (!IsActive) return;

    ++CurrentLifetime;
    if (MaxLifetime > 0 && CurrentLifetime >= MaxLifetime) {
        IsActive = false;
        return;
    }

    UpdateRotation();
    UpdateColorCycle();

    ++DamageTimer;
    if (DamageTimer >= DamageInterval) {
        DamageTimer = 0;
        ApplyBeamDamage();
    }

    float lifeProgress = static_cast<float>(CurrentLifetime) / static_cast<float>(MaxLifetime);
    if (FadeInTime > 0 && CurrentLifetime < FadeInTime) {
        CurrentAlpha = static_cast<uint8>(255.0f * static_cast<float>(CurrentLifetime) / static_cast<float>(FadeInTime));
    } else if (FadeOutTime > 0 && MaxLifetime - CurrentLifetime < FadeOutTime) {
        float remaining = static_cast<float>(MaxLifetime - CurrentLifetime);
        CurrentAlpha = static_cast<uint8>(255.0f * remaining / static_cast<float>(FadeOutTime));
    } else {
        CurrentAlpha = 255;
    }
}

void DiskLaserClass::UpdateRotation() {
    if (IsScanning) {
        ScanAngle += ScanDirection * RotationSpeed;
        if (ScanAngle > ScanRange) {
            ScanAngle = ScanRange;
            ScanDirection = -1.0f;
        } else if (ScanAngle < -ScanRange) {
            ScanAngle = -ScanRange;
            ScanDirection = 1.0f;
        }
    }
    CurrentAngle += RotationSpeed;
    if (CurrentAngle >= 360.0f) CurrentAngle -= 360.0f;
    if (CurrentAngle < 0.0f) CurrentAngle += 360.0f;
}

void DiskLaserClass::UpdateColorCycle() {
    ColorPhase += ColorSpeed;
    if (ColorPhase > 6.28318f) ColorPhase -= 6.28318f;

    float r = std::sin(ColorPhase) * 0.5f + 0.5f;
    float g = std::sin(ColorPhase + 2.09439f) * 0.5f + 0.5f;
    float b = std::sin(ColorPhase + 4.18879f) * 0.5f + 0.5f;

    uint8 mixedR = static_cast<uint8>(Color1.R * (1.0f - r) + Color2.R * r);
    uint8 mixedG = static_cast<uint8>(Color1.G * (1.0f - g) + Color2.G * g);
    uint8 mixedB = static_cast<uint8>(Color1.B * (1.0f - b) + Color2.B * b);
}

void DiskLaserClass::ApplyBeamDamage() {
    if (DamagePerTick <= 0) return;

    float angleRad = (CurrentAngle + ScanAngle) * 3.14159f / 180.0f;
    float dirX = std::cos(angleRad);
    float dirY = std::sin(angleRad);

    float stepX = dirX * 64.0f;
    float stepY = dirY * 64.0f;

    int32 steps = BeamLength / 64;
    if (steps < 1) steps = 1;

    float curX = static_cast<float>(Position.X);
    float curY = static_cast<float>(Position.Y);

    for (int32 i = 0; i < steps; ++i) {
        CoordStruct checkPos(static_cast<int32>(curX), static_cast<int32>(curY), Position.Z);
        CellStruct cell = CellClass::Coord2Cell(checkPos);

        if (MapClass::Instance) {
            CellClass* pCell = MapClass::Instance->GetCellAt(cell);
            if (pCell) {
                ObjectClass* pObj = pCell->Occupier;
                if (pObj) {
                    TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);

                    if (OwnerHouse >= 0) {
                        HouseClass* pHouse = pTechno->GetOwningHouse();
                        if (pHouse && pHouse->GetOwningHouseIndex() == OwnerHouse) {
                            curX += stepX;
                            curY += stepY;
                            continue;
                        }
                    }

                    int32 actualDamage = DamagePerTick;
                    float distFromCenter = static_cast<float>(i) / static_cast<float>(steps);
                    float damageFactor = 1.0f - std::fabs(distFromCenter - 0.5f) * 2.0f;
                    if (damageFactor < 0.1f) damageFactor = 0.1f;

                    actualDamage = static_cast<int32>(actualDamage * damageFactor);
                    if (actualDamage < 1) actualDamage = 1;
                    pTechno->TakeDamage(actualDamage, nullptr, nullptr);
                }
            }
        }

        curX += stepX;
        curY += stepY;
    }
}

void DiskLaserClass::RenderEffect(BlitterClass* blitter, DSurface* surface) {
    if (!IsActive || !blitter || !surface) return;
    if (CurrentAlpha <= 0) return;

    uint8 r = static_cast<uint8>(Color1.R);
    uint8 g = static_cast<uint8>(Color1.G);
    uint8 b = static_cast<uint8>(Color1.B);

    float angleRad = (CurrentAngle + ScanAngle) * 3.14159f / 180.0f;
    float dirX = std::cos(angleRad);
    float dirY = std::sin(angleRad);

    float stepX = dirX * 16.0f;
    float stepY = dirY * 16.0f;
    float perpX = -dirY * static_cast<float>(BeamWidth);
    float perpY = dirX * static_cast<float>(BeamWidth);

    int32 steps = BeamLength / 16;
    if (steps < 1) steps = 1;

    float curX = static_cast<float>(Position.X);
    float curY = static_cast<float>(Position.Y);

    for (int32 i = 0; i < steps; ++i) {
        float distFromCenter = static_cast<float>(i) / static_cast<float>(steps);
        float brightness = 1.0f - std::fabs(distFromCenter - 0.5f) * 2.0f;
        if (brightness < 0.1f) brightness = 0.1f;

        uint8 pixelAlpha = static_cast<uint8>(CurrentAlpha * brightness);

        int32 px = static_cast<int32>(curX);
        int32 py = static_cast<int32>(curY);

        for (int32 w = -BeamWidth; w <= BeamWidth; ++w) {
            surface->SetPixelAlpha(px + static_cast<int32>(perpX * w / BeamWidth),
                                   py + static_cast<int32>(perpY * w / BeamWidth),
                                   r, g, b, pixelAlpha);
        }

        curX += stepX;
        curY += stepY;
    }

    // Render disk center
    int32 diskRadius = BeamWidth * 2;
    for (int32 dy = -diskRadius; dy <= diskRadius; ++dy) {
        for (int32 dx = -diskRadius; dx <= diskRadius; ++dx) {
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist < diskRadius) {
                float fade = 1.0f - (dist / diskRadius);
                fade = fade * fade;
                uint8 pixelAlpha = static_cast<uint8>(CurrentAlpha * fade * 0.5f);
                surface->SetPixelAlpha(Position.X + dx, Position.Y + dy, r, g, b, pixelAlpha);
            }
        }
    }
}

void DiskLaserClass::SetBeamWidth(int32 width) {
    BeamWidth = width;
    if (BeamWidth < 1) BeamWidth = 1;
    if (BeamWidth > 64) BeamWidth = 64;
}

void DiskLaserClass::SetRotationSpeed(float speed) {
    RotationSpeed = speed;
    if (RotationSpeed < 0.0f) RotationSpeed = 0.0f;
    if (RotationSpeed > 1.0f) RotationSpeed = 1.0f;
}

void DiskLaserClass::SetScanRange(float range) {
    ScanRange = range;
    if (ScanRange < 0.0f) ScanRange = 0.0f;
    if (ScanRange > 180.0f) ScanRange = 180.0f;
}

void DiskLaserClass::SetScanning(bool scanning) {
    IsScanning = scanning;
}

void DiskLaserClass::SetTracking(bool tracking) {
    IsTracking = tracking;
}

void DiskLaserClass::SetDamagePerTick(int32 damage) {
    DamagePerTick = damage;
    if (DamagePerTick < 0) DamagePerTick = 0;
}

void DiskLaserClass::SetDamageInterval(int32 interval) {
    DamageInterval = interval;
    if (DamageInterval < 1) DamageInterval = 1;
}

void DiskLaserClass::SetLifetime(int32 lifetime) {
    MaxLifetime = lifetime;
    if (MaxLifetime < 0) MaxLifetime = 0;
}

void DiskLaserClass::SetColors(const ColorStruct& c1, const ColorStruct& c2, const ColorStruct& c3) {
    Color1 = c1;
    Color2 = c2;
    Color3 = c3;
}

void DiskLaserClass::SetColorSpeed(float speed) {
    ColorSpeed = speed;
    if (ColorSpeed < 0.0f) ColorSpeed = 0.0f;
    if (ColorSpeed > 0.5f) ColorSpeed = 0.5f;
}

void DiskLaserClass::SetFadeInTime(int32 time) {
    FadeInTime = time;
    if (FadeInTime < 0) FadeInTime = 0;
}

void DiskLaserClass::SetFadeOutTime(int32 time) {
    FadeOutTime = time;
    if (FadeOutTime < 0) FadeOutTime = 0;
}

void DiskLaserClass::SetOwnerHouse(int32 house) {
    OwnerHouse = house;
}

void DiskLaserClass::SetTarget(const CoordStruct& target) {
    TargetPosition = target;
    int32 dx = target.X - Position.X;
    int32 dy = target.Y - Position.Y;
    BeamLength = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
}

void DiskLaserClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

bool DiskLaserClass::IsActiveLaser() const {
    return IsActive;
}

int32 DiskLaserClass::GetCurrentLifetime() const {
    return CurrentLifetime;
}

int32 DiskLaserClass::GetBeamWidth() const {
    return BeamWidth;
}

float DiskLaserClass::GetCurrentAngle() const {
    return CurrentAngle + ScanAngle;
}

// ============================================================
// DiskLaserManagerClass
// ============================================================

static DiskLaserManagerClass* g_DiskLaserManagerInstance = nullptr;

DiskLaserManagerClass::DiskLaserManagerClass()
    : ActiveCount(0) {
    for (int32 i = 0; i < MAX_DISK_LASERS; ++i) {
        Lasers[i] = nullptr;
    }
}

DiskLaserManagerClass::~DiskLaserManagerClass() {
    RemoveAllLasers();
}

DiskLaserManagerClass* DiskLaserManagerClass::GetInstance() {
    if (!g_DiskLaserManagerInstance) {
        g_DiskLaserManagerInstance = new DiskLaserManagerClass();
    }
    return g_DiskLaserManagerInstance;
}

int32 DiskLaserManagerClass::CreateDiskLaser(const CoordStruct& pos, const CoordStruct& target,
    int32 damage, int32 lifetime) {
    for (int32 i = 0; i < MAX_DISK_LASERS; ++i) {
        if (Lasers[i] == nullptr || !Lasers[i]->IsActive) {
            if (Lasers[i] == nullptr) {
                Lasers[i] = new DiskLaserClass();
            }
            Lasers[i]->Initialize(pos, target, damage, lifetime);
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

bool DiskLaserManagerClass::RemoveDiskLaser(int32 index) {
    if (index < 0 || index >= MAX_DISK_LASERS) return false;
    if (Lasers[index] == nullptr) return false;

    delete Lasers[index];
    Lasers[index] = nullptr;
    --ActiveCount;
    return true;
}

void DiskLaserManagerClass::RemoveAllLasers() {
    for (int32 i = 0; i < MAX_DISK_LASERS; ++i) {
        if (Lasers[i]) {
            delete Lasers[i];
            Lasers[i] = nullptr;
        }
    }
    ActiveCount = 0;
}

void DiskLaserManagerClass::UpdateAllLasers() {
    for (int32 i = 0; i < MAX_DISK_LASERS; ++i) {
        if (Lasers[i] && Lasers[i]->IsActive) {
            Lasers[i]->Update();
        }
    }
}

void DiskLaserManagerClass::RenderAllEffects(BlitterClass* blitter, DSurface* surface) {
    for (int32 i = 0; i < MAX_DISK_LASERS; ++i) {
        if (Lasers[i] && Lasers[i]->IsActive) {
            Lasers[i]->RenderEffect(blitter, surface);
        }
    }
}

int32 DiskLaserManagerClass::GetActiveCount() const {
    return ActiveCount;
}

DiskLaserClass* DiskLaserManagerClass::GetDiskLaser(int32 index) const {
    if (index < 0 || index >= MAX_DISK_LASERS) return nullptr;
    return Lasers[index];
}