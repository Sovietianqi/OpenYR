#include "EMPulseClass.h"
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
// EMPulseClass
// ============================================================

EMPulseClass::EMPulseClass()
    : Position(0, 0, 0), Radius(0), MaxRadius(512), Duration(0), MaxDuration(300)
    , ExpandSpeed(8), CurrentRadius(0), CurrentDuration(0)
    , IsActive(false), IsExpanding(true), HasExpanded(false)
    , PulseColor(64, 128, 255), PulseAlpha(128), FlashAlpha(255)
    , RingThickness(16), PulseCount(0), MaxPulses(3)
    , PulseInterval(60), PulseTimer(0), CurrentPulse(0)
    , DamageAmount(0), DisableDuration(300), OwnerHouse(-1) {
}

EMPulseClass::~EMPulseClass() {
}

void EMPulseClass::Initialize(const CoordStruct& pos, int32 radius, int32 duration, int32 damage) {
    Position = pos;
    Radius = 0;
    MaxRadius = radius;
    Duration = duration;
    MaxDuration = duration;
    DamageAmount = damage;
    CurrentRadius = 0;
    CurrentDuration = 0;
    ExpandSpeed = radius / 30;
    if (ExpandSpeed < 1) ExpandSpeed = 1;
    if (ExpandSpeed > 32) ExpandSpeed = 32;
    IsActive = true;
    IsExpanding = true;
    HasExpanded = false;
    PulseTimer = 0;
    CurrentPulse = 0;
}

void EMPulseClass::Update() {
    if (!IsActive) return;

    ++CurrentDuration;
    if (CurrentDuration >= MaxDuration) {
        IsActive = false;
        return;
    }

    if (IsExpanding && !HasExpanded) {
        CurrentRadius += ExpandSpeed;
        if (CurrentRadius >= MaxRadius) {
            CurrentRadius = MaxRadius;
            HasExpanded = true;
            ApplyEMPDisable();
        }
    }

    ++PulseTimer;
    if (PulseTimer >= PulseInterval && CurrentPulse < MaxPulses) {
        PulseTimer = 0;
        ++CurrentPulse;
        CreatePulseRing();
    }

    // Fade out over time
    float lifeProgress = static_cast<float>(CurrentDuration) / static_cast<float>(MaxDuration);
    PulseAlpha = static_cast<uint8>(128.0f * (1.0f - lifeProgress));
    FlashAlpha = static_cast<uint8>(255.0f * (1.0f - lifeProgress));
}

void EMPulseClass::CreatePulseRing() {
    float pulseProgress = static_cast<float>(CurrentPulse) / static_cast<float>(MaxPulses);
    float ringRadius = static_cast<float>(MaxRadius) * (0.5f + 0.5f * pulseProgress);
    RingThickness = static_cast<int32>(16.0f * (1.0f - pulseProgress));
    if (RingThickness < 2) RingThickness = 2;
    (void)ringRadius;
}

void EMPulseClass::ApplyEMPDisable() {
    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (MaxRadius + LeptonsPerCell - 1) / LeptonsPerCell;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (MapClass::Instance) {
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX >= MapClass::Instance->MapWidth) maxX = MapClass::Instance->MapWidth - 1;
        if (maxY >= MapClass::Instance->MapHeight) maxY = MapClass::Instance->MapHeight - 1;
    }

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            CoordStruct cellCenter = CellClass::Cell2Coord(cell);
            cellCenter.X += LeptonsPerCell / 2;
            cellCenter.Y += LeptonsPerCell / 2;

            int32 dx = cellCenter.X - Position.X;
            int32 dy = cellCenter.Y - Position.Y;
            int32 distance = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));

            if (distance <= MaxRadius) {
                ApplyEMPToCell(cell, distance);
            }
        }
    }
}

void EMPulseClass::ApplyEMPToCell(const CellStruct& cell, int32 distance) {
    CellClass* pCell = MapClass::Instance ? MapClass::Instance->GetCellAt(cell) : nullptr;
    if (!pCell) return;

    ObjectClass* pObj = pCell->Occupier;
    if (!pObj) return;

    float rangeFactor = 1.0f - static_cast<float>(distance) / static_cast<float>(MaxRadius);

    TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);

    if (OwnerHouse >= 0) {
        HouseClass* pHouse = pTechno->GetOwningHouse();
        if (pHouse && pHouse->GetOwningHouseIndex() == OwnerHouse) {
            return;
        }
    }

    if (IsEMPImmune(pObj)) {
        return;
    }

    if (DamageAmount > 0) {
        int32 actualDamage = static_cast<int32>(DamageAmount * rangeFactor);
        if (actualDamage < 1) actualDamage = 1;
        pTechno->TakeDamage(actualDamage, nullptr, nullptr);
    }

    // EMP disable is handled by setting a flag on the cell
    int32 disableDuration = static_cast<int32>(DisableDuration * rangeFactor);
    if (disableDuration > 0) {
        pCell->SetFlag(CellFlags::EMPPresent, true);
    }
}

bool EMPulseClass::IsEMPImmune(ObjectClass* pObj) const {
    if (!pObj) return true;

    if (pObj->WhatAmI() == AbstractType::Aircraft) {
        return true;
    }

    if (pObj->WhatAmI() == AbstractType::Building) {
        return true;
    }

    return false;
}

void EMPulseClass::RenderEffect(BlitterClass* blitter, DSurface* surface) {
    if (!IsActive || !blitter || !surface) return;

    int32 screenX = Position.X;
    int32 screenY = Position.Y;

    // Render expanding ring
    float ringAlpha = static_cast<float>(PulseAlpha) / 255.0f;
    uint8 r = PulseColor.R;
    uint8 g = PulseColor.G;
    uint8 b = PulseColor.B;

    int32 ringRadius = CurrentRadius;
    for (int32 py = screenY - ringRadius - RingThickness; py <= screenY + ringRadius + RingThickness; ++py) {
        for (int32 px = screenX - ringRadius - RingThickness; px <= screenX + ringRadius + RingThickness; ++px) {
            int32 dx = px - screenX;
            int32 dy = py - screenY;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

            float ringDist = dist - static_cast<float>(ringRadius);
            if (ringDist < 0) ringDist = -ringDist;

            if (ringDist < RingThickness) {
                float alpha = ringAlpha * (1.0f - ringDist / RingThickness);
                alpha *= alpha;
                uint8 pixelAlpha = static_cast<uint8>(alpha * 255.0f);
                surface->SetPixelAlpha(px, py, r, g, b, pixelAlpha);
            }
        }
    }

    // Render flash center
    if (FlashAlpha > 0) {
        float flashFactor = static_cast<float>(FlashAlpha) / 255.0f;
        int32 flashRadius = ringRadius / 4;
        uint8 flashR = 255;
        uint8 flashG = 255;
        uint8 flashB = 255;

        for (int32 py = screenY - flashRadius; py <= screenY + flashRadius; ++py) {
            for (int32 px = screenX - flashRadius; px <= screenX + flashRadius; ++px) {
                int32 dx = px - screenX;
                int32 dy = py - screenY;
                float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
                if (dist < flashRadius) {
                    float fade = 1.0f - (dist / flashRadius);
                    fade = fade * fade;
                    uint8 pixelAlpha = static_cast<uint8>(flashFactor * fade * 255.0f);
                    surface->SetPixelAlpha(px, py, flashR, flashG, flashB, pixelAlpha);
                }
            }
        }
    }
}

void EMPulseClass::SetRadius(int32 radius) {
    MaxRadius = radius;
    if (MaxRadius < 0) MaxRadius = 0;
    if (MaxRadius > 2048) MaxRadius = 2048;
}

void EMPulseClass::SetDuration(int32 duration) {
    MaxDuration = duration;
    if (MaxDuration < 0) MaxDuration = 0;
    if (MaxDuration > 600) MaxDuration = 600;
}

void EMPulseClass::SetDisableDuration(int32 duration) {
    DisableDuration = duration;
    if (DisableDuration < 0) DisableDuration = 0;
    if (DisableDuration > 1800) DisableDuration = 1800;
}

void EMPulseClass::SetDamageAmount(int32 damage) {
    DamageAmount = damage;
    if (DamageAmount < 0) DamageAmount = 0;
}

void EMPulseClass::SetPulseColor(const ColorStruct& color) {
    PulseColor = color;
}

void EMPulseClass::SetMaxPulses(int32 count) {
    MaxPulses = count;
    if (MaxPulses < 1) MaxPulses = 1;
    if (MaxPulses > 10) MaxPulses = 10;
}

void EMPulseClass::SetPulseInterval(int32 interval) {
    PulseInterval = interval;
    if (PulseInterval < 1) PulseInterval = 1;
    if (PulseInterval > 300) PulseInterval = 300;
}

void EMPulseClass::SetOwnerHouse(int32 house) {
    OwnerHouse = house;
}

void EMPulseClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

int32 EMPulseClass::GetCurrentRadius() const {
    return CurrentRadius;
}

int32 EMPulseClass::GetCurrentDuration() const {
    return CurrentDuration;
}

bool EMPulseClass::IsActivePulse() const {
    return IsActive;
}

bool EMPulseClass::HasExpandedFully() const {
    return HasExpanded;
}

// ============================================================
// EMPulseManagerClass
// ============================================================

static EMPulseManagerClass* g_EMPulseManagerInstance = nullptr;

EMPulseManagerClass::EMPulseManagerClass()
    : ActiveCount(0) {
    for (int32 i = 0; i < MAX_EMPULSES; ++i) {
        Pulses[i] = nullptr;
    }
}

EMPulseManagerClass::~EMPulseManagerClass() {
    RemoveAllPulses();
}

EMPulseManagerClass* EMPulseManagerClass::GetInstance() {
    if (!g_EMPulseManagerInstance) {
        g_EMPulseManagerInstance = new EMPulseManagerClass();
    }
    return g_EMPulseManagerInstance;
}

int32 EMPulseManagerClass::CreateEMPulse(const CoordStruct& pos, int32 radius, int32 duration, int32 damage) {
    for (int32 i = 0; i < MAX_EMPULSES; ++i) {
        if (Pulses[i] == nullptr || !Pulses[i]->IsActive) {
            if (Pulses[i] == nullptr) {
                Pulses[i] = new EMPulseClass();
            }
            Pulses[i]->Initialize(pos, radius, duration, damage);
            ++ActiveCount;
            return i;
        }
    }
    return -1;
}

bool EMPulseManagerClass::RemoveEMPulse(int32 index) {
    if (index < 0 || index >= MAX_EMPULSES) return false;
    if (Pulses[index] == nullptr) return false;

    delete Pulses[index];
    Pulses[index] = nullptr;
    --ActiveCount;
    return true;
}

void EMPulseManagerClass::RemoveAllPulses() {
    for (int32 i = 0; i < MAX_EMPULSES; ++i) {
        if (Pulses[i]) {
            delete Pulses[i];
            Pulses[i] = nullptr;
        }
    }
    ActiveCount = 0;
}

void EMPulseManagerClass::UpdateAllPulses() {
    for (int32 i = 0; i < MAX_EMPULSES; ++i) {
        if (Pulses[i] && Pulses[i]->IsActive) {
            Pulses[i]->Update();
        }
    }
}

void EMPulseManagerClass::RenderAllEffects(BlitterClass* blitter, DSurface* surface) {
    for (int32 i = 0; i < MAX_EMPULSES; ++i) {
        if (Pulses[i] && Pulses[i]->IsActive) {
            Pulses[i]->RenderEffect(blitter, surface);
        }
    }
}

int32 EMPulseManagerClass::GetActiveCount() const {
    return ActiveCount;
}

EMPulseClass* EMPulseManagerClass::GetEMPulse(int32 index) const {
    if (index < 0 || index >= MAX_EMPULSES) return nullptr;
    return Pulses[index];
}

// ============================================================================
// File-local EMP helper functions
//
//  These utilities implement the detailed EMP burst animation pipeline,
//  expanding-ring geometry, unit disable logic, and immunity screening
//  that support the EMPulseClass above.  Because the header cannot be
//  modified, these are declared as free functions in the anonymous
//  namespace and operate on the public state exposed by EMPulseClass.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// EMP burst phases
//
//  An EMP pulse goes through several visual phases during its lifetime.
//  The phase boundaries are expressed as fractions of the total duration
//  so they scale correctly with any MaxDuration value.
// --------------------------------------------------------------------------
enum class EMPPhase : int32
{
    Flash       = 0,   // Initial bright flash at the epicenter
    Expand      = 1,   // Ring expands outward rapidly
    Hold        = 2,   // Ring holds at full radius
    Fade        = 3,   // Ring fades out gradually
    Complete    = 4,   // Effect finished
};

constexpr float PHASE_FLASH_END   = 0.05f;   // 5% of duration
constexpr float PHASE_EXPAND_END  = 0.35f;   // 35% of duration
constexpr float PHASE_HOLD_END    = 0.60f;   // 60% of duration
constexpr float PHASE_FADE_END    = 1.00f;   // 100% of duration

// --------------------------------------------------------------------------
// GetEMPPhase - Returns the current visual phase of the EMP pulse based
// on its lifetime progress (0.0 = just started, 1.0 = complete).
// --------------------------------------------------------------------------
EMPPhase GetEMPPhase(float lifeProgress)
{
    if (lifeProgress < 0.0f) lifeProgress = 0.0f;
    if (lifeProgress > 1.0f) lifeProgress = 1.0f;

    if (lifeProgress < PHASE_FLASH_END)   return EMPPhase::Flash;
    if (lifeProgress < PHASE_EXPAND_END)  return EMPPhase::Expand;
    if (lifeProgress < PHASE_HOLD_END)    return EMPPhase::Hold;
    if (lifeProgress < PHASE_FADE_END)    return EMPPhase::Fade;
    return EMPPhase::Complete;
}

// --------------------------------------------------------------------------
// PhaseIntensity - Returns the visual intensity (0.0 to 1.0) of the EMP
// effect during the given phase.  The flash peaks instantly then decays;
// the expand phase ramps down; the hold phase is steady; the fade phase
// decays to zero.
// --------------------------------------------------------------------------
float PhaseIntensity(EMPPhase phase, float phaseProgress)
{
    if (phaseProgress < 0.0f) phaseProgress = 0.0f;
    if (phaseProgress > 1.0f) phaseProgress = 1.0f;

    switch (phase) {
    case EMPPhase::Flash:
        // Peak at start, decay to 0.6 by end of flash.
        return 1.0f - 0.4f * phaseProgress;

    case EMPPhase::Expand:
        // Ramp from 0.6 down to 0.4 during expansion.
        return 0.6f - 0.2f * phaseProgress;

    case EMPPhase::Hold:
        // Steady at 0.4 during hold.
        return 0.4f;

    case EMPPhase::Fade:
        // Decay from 0.4 to 0 during fade.
        return 0.4f * (1.0f - phaseProgress);

    default:
        return 0.0f;
    }
}

// --------------------------------------------------------------------------
// ComputePhaseProgress - Given the overall life progress and the phase,
// returns the progress within that phase (0.0 to 1.0).
// --------------------------------------------------------------------------
float ComputePhaseProgress(float lifeProgress, EMPPhase phase)
{
    switch (phase) {
    case EMPPhase::Flash: {
        float span = PHASE_FLASH_END;
        if (span <= 0.0f) return 1.0f;
        return lifeProgress / span;
    }
    case EMPPhase::Expand: {
        float span = PHASE_EXPAND_END - PHASE_FLASH_END;
        if (span <= 0.0f) return 1.0f;
        return (lifeProgress - PHASE_FLASH_END) / span;
    }
    case EMPPhase::Hold: {
        float span = PHASE_HOLD_END - PHASE_EXPAND_END;
        if (span <= 0.0f) return 1.0f;
        return (lifeProgress - PHASE_EXPAND_END) / span;
    }
    case EMPPhase::Fade: {
        float span = PHASE_FADE_END - PHASE_HOLD_END;
        if (span <= 0.0f) return 1.0f;
        return (lifeProgress - PHASE_HOLD_END) / span;
    }
    default:
        return 1.0f;
    }
}

// --------------------------------------------------------------------------
// InterpolateColor - Linearly interpolates between two colors.
// --------------------------------------------------------------------------
ColorStruct InterpolateColor(const ColorStruct& a, const ColorStruct& b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return ColorStruct(
        static_cast<uint8>(a.R + (b.R - a.R) * t),
        static_cast<uint8>(a.G + (b.G - a.G) * t),
        static_cast<uint8>(a.B + (b.B - a.B) * t));
}

// --------------------------------------------------------------------------
// GetPhaseColor - Returns the EMP ring color for the current phase.  The
// color shifts from white-hot during the flash to the configured pulse
// color during expansion, then darkens as it fades.
// --------------------------------------------------------------------------
ColorStruct GetPhaseColor(const ColorStruct& baseColor, EMPPhase phase, float phaseProgress)
{
    static const ColorStruct flashColor(255, 255, 255);
    static const ColorStruct dimColor(30, 30, 60);

    switch (phase) {
    case EMPPhase::Flash:
        return InterpolateColor(flashColor, baseColor, phaseProgress);

    case EMPPhase::Expand:
        return baseColor;

    case EMPPhase::Hold:
        return InterpolateColor(baseColor, dimColor, phaseProgress * 0.3f);

    case EMPPhase::Fade:
        return InterpolateColor(baseColor, dimColor, phaseProgress);

    default:
        return dimColor;
    }
}

// --------------------------------------------------------------------------
// RingGeometry - Describes the geometry of a single EMP ring at a given
// point in time.
// --------------------------------------------------------------------------
struct RingGeometry
{
    int32 Radius;       // Outer radius in pixels
    int32 Thickness;    // Ring band thickness in pixels
    float Alpha;        // Overall opacity (0.0 to 1.0)
};

// --------------------------------------------------------------------------
// ComputeRingGeometry - Returns the ring geometry for the given pulse
// index and life progress.  Multiple rings are staggered so that later
// pulses appear as the earlier ones fade.
// --------------------------------------------------------------------------
RingGeometry ComputeRingGeometry(int32 maxRadius, int32 pulseIndex,
                                 int32 maxPulses, float lifeProgress,
                                 float baseAlpha)
{
    RingGeometry geom;
    geom.Radius = 0;
    geom.Thickness = 16;
    geom.Alpha = 0.0f;

    if (maxPulses <= 0) return geom;

    // Each pulse starts at a staggered fraction of the total lifetime.
    float pulseStart = static_cast<float>(pulseIndex) / static_cast<float>(maxPulses);
    float pulseSpan = 1.0f / static_cast<float>(maxPulses);

    float pulseProgress = (lifeProgress - pulseStart) / pulseSpan;
    if (pulseProgress < 0.0f || pulseProgress > 1.0f) {
        geom.Alpha = 0.0f;
        return geom;
    }

    // Radius grows from 10% to 100% of maxRadius during the pulse lifetime.
    float radiusFraction = 0.1f + 0.9f * pulseProgress;
    geom.Radius = static_cast<int32>(static_cast<float>(maxRadius) * radiusFraction);
    if (geom.Radius < 1) geom.Radius = 1;

    // Thickness shrinks as the ring expands.
    geom.Thickness = static_cast<int32>(16.0f * (1.0f - pulseProgress * 0.7f));
    if (geom.Thickness < 2) geom.Thickness = 2;

    // Alpha peaks at the middle of the pulse lifetime and fades at the ends.
    float alphaCurve = 1.0f - std::abs(pulseProgress - 0.5f) * 2.0f;
    alphaCurve = alphaCurve * alphaCurve;
    geom.Alpha = baseAlpha * alphaCurve;

    return geom;
}

// --------------------------------------------------------------------------
// DrawRing - Renders a single EMP ring onto the surface using the given
// geometry and color.  The ring is drawn as a band of pixels whose alpha
// falls off from the ring centerline.
// --------------------------------------------------------------------------
void DrawRing(DSurface* surface, int32 centerX, int32 centerY,
              const RingGeometry& geom, const ColorStruct& color)
{
    if (!surface || geom.Alpha <= 0.0f || geom.Radius <= 0) return;

    int32 outerBound = geom.Radius + geom.Thickness;
    if (outerBound <= 0) return;

    float invThickness = 1.0f / static_cast<float>(geom.Thickness);
    uint8 baseR = color.R;
    uint8 baseG = color.G;
    uint8 baseB = color.B;

    for (int32 py = centerY - outerBound; py <= centerY + outerBound; ++py) {
        for (int32 px = centerX - outerBound; px <= centerX + outerBound; ++px) {
            int32 dx = px - centerX;
            int32 dy = py - centerY;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

            float ringDist = dist - static_cast<float>(geom.Radius);
            if (ringDist < 0.0f) ringDist = -ringDist;

            if (ringDist < static_cast<float>(geom.Thickness)) {
                float falloff = 1.0f - ringDist * invThickness;
                falloff = falloff * falloff;
                float pixelAlpha = geom.Alpha * falloff;
                uint8 alphaByte = static_cast<uint8>(pixelAlpha * 255.0f);
                if (alphaByte > 0) {
                    surface->SetPixelAlpha(px, py, baseR, baseG, baseB, alphaByte);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// DrawCenterFlash - Renders the bright flash at the EMP epicenter.  The
// flash is a radial gradient that shrinks and dims over time.
// --------------------------------------------------------------------------
void DrawCenterFlash(DSurface* surface, int32 centerX, int32 centerY,
                     int32 maxRadius, float intensity, float lifeProgress)
{
    if (!surface || intensity <= 0.0f) return;

    // Flash radius is largest at the start and shrinks.
    float flashFraction = 0.3f * (1.0f - lifeProgress * 0.5f);
    int32 flashRadius = static_cast<int32>(static_cast<float>(maxRadius) * flashFraction);
    if (flashRadius < 2) flashRadius = 2;

    float invRadius = 1.0f / static_cast<float>(flashRadius);

    for (int32 py = centerY - flashRadius; py <= centerY + flashRadius; ++py) {
        for (int32 px = centerX - flashRadius; px <= centerX + flashRadius; ++px) {
            int32 dx = px - centerX;
            int32 dy = py - centerY;
            float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (dist < static_cast<float>(flashRadius)) {
                float fade = 1.0f - dist * invRadius;
                fade = fade * fade * fade;
                uint8 alpha = static_cast<uint8>(intensity * fade * 255.0f);
                if (alpha > 0) {
                    surface->SetPixelAlpha(px, py, 255, 255, 255, alpha);
                }
            }
        }
    }
}

// --------------------------------------------------------------------------
// DrawAllRings - Renders all active EMP rings for a pulse, including the
// center flash and the staggered pulse rings.
// --------------------------------------------------------------------------
void DrawAllRings(DSurface* surface, int32 centerX, int32 centerY,
                  int32 maxRadius, int32 maxPulses, float lifeProgress,
                  const ColorStruct& baseColor, float baseAlpha)
{
    if (!surface) return;

    EMPPhase phase = GetEMPPhase(lifeProgress);
    float phaseProgress = ComputePhaseProgress(lifeProgress, phase);
    float intensity = PhaseIntensity(phase, phaseProgress);
    ColorStruct phaseColor = GetPhaseColor(baseColor, phase, phaseProgress);

    // Draw the center flash during the early phases.
    if (phase == EMPPhase::Flash || phase == EMPPhase::Expand) {
        DrawCenterFlash(surface, centerX, centerY, maxRadius, intensity, lifeProgress);
    }

    // Draw each staggered pulse ring.
    for (int32 i = 0; i < maxPulses; ++i) {
        RingGeometry geom = ComputeRingGeometry(maxRadius, i, maxPulses,
                                                lifeProgress, baseAlpha);
        if (geom.Alpha > 0.0f) {
            DrawRing(surface, centerX, centerY, geom, phaseColor);
        }
    }
}

// --------------------------------------------------------------------------
// DisableStrengthFactor - Computes the disable strength multiplier (0.0 to
// 1.0) based on the distance from the EMP epicenter.  Units at the center
// receive full disable strength; units at the edge receive reduced
// strength.
// --------------------------------------------------------------------------
float DisableStrengthFactor(int32 distance, int32 maxRadius)
{
    if (maxRadius <= 0) return 1.0f;
    if (distance <= 0) return 1.0f;
    if (distance >= maxRadius) return 0.0f;

    float normalized = static_cast<float>(distance) / static_cast<float>(maxRadius);
    // Quadratic falloff for a more realistic EMP field.
    return 1.0f - normalized * normalized;
}

// --------------------------------------------------------------------------
// ComputeDisableDuration - Returns the actual disable duration for a unit
// at the given distance, accounting for the falloff and a minimum
// threshold below which the unit is not disabled at all.
// --------------------------------------------------------------------------
int32 ComputeDisableDuration(int32 baseDuration, int32 distance, int32 maxRadius)
{
    float strength = DisableStrengthFactor(distance, maxRadius);
    if (strength < 0.15f) return 0;   // Below 15% strength, no effect.

    int32 duration = static_cast<int32>(static_cast<float>(baseDuration) * strength);
    if (duration < 1) duration = 1;
    return duration;
}

// --------------------------------------------------------------------------
// ComputeDamageFalloff - Returns the damage amount adjusted for distance
// from the EMP epicenter.  Uses a linear falloff with a floor of 1.
// --------------------------------------------------------------------------
int32 ComputeDamageFalloff(int32 baseDamage, int32 distance, int32 maxRadius)
{
    if (baseDamage <= 0) return 0;
    if (maxRadius <= 0) return baseDamage;

    float normalized = static_cast<float>(distance) / static_cast<float>(maxRadius);
    if (normalized >= 1.0f) return 0;

    float factor = 1.0f - normalized;
    int32 damage = static_cast<int32>(static_cast<float>(baseDamage) * factor);
    if (damage < 1 && factor > 0.0f) damage = 1;
    return damage;
}

// --------------------------------------------------------------------------
// EMPImmuneByType - Returns true if the given abstract type is inherently
// immune to EMP effects.  Aircraft (in flight) and buildings are immune
// because the EMP pulse travels along the ground.
// --------------------------------------------------------------------------
bool EMPImmuneByType(AbstractType type)
{
    switch (type) {
    case AbstractType::Aircraft:
    case AbstractType::Building:
        return true;
    default:
        return false;
    }
}

// --------------------------------------------------------------------------
// EMPResistanceByArmor - Returns a resistance factor (0.0 = fully
// vulnerable, 1.0 = fully immune) for the given armor type.  Heavy armor
// units are more resistant to EMP disruption.
// --------------------------------------------------------------------------
float EMPResistanceByArmor(ArmorType armor)
{
    switch (armor) {
    case ArmorType::None:
        return 0.0f;
    case ArmorType::Flak:
        return 0.1f;
    case ArmorType::Plate:
        return 0.15f;
    case ArmorType::Light:
        return 0.05f;
    case ArmorType::Medium:
        return 0.2f;
    case ArmorType::Heavy:
        return 0.4f;
    case ArmorType::Wood:
        return 0.0f;
    case ArmorType::Steel:
        return 0.3f;
    case ArmorType::Concrete:
        return 0.5f;
    case ArmorType::Drone:
        return 0.6f;   // Drones are highly EMP-resistant
    case ArmorType::Special_1:
        return 0.8f;
    default:
        return 0.0f;
    }
}

// --------------------------------------------------------------------------
// IsUnitEMPVulnerable - Comprehensive check that combines type immunity
// and armor resistance to determine if a unit should be affected by EMP.
// Returns true if the unit is vulnerable (not immune and resistance < 1.0).
// --------------------------------------------------------------------------
bool IsUnitEMPVulnerable(AbstractType type, ArmorType armor)
{
    if (EMPImmuneByType(type)) return false;
    float resistance = EMPResistanceByArmor(armor);
    return resistance < 1.0f;
}

// --------------------------------------------------------------------------
// EffectiveDisableDuration - Combines the distance falloff and armor
// resistance to compute the final disable duration for a specific unit.
// --------------------------------------------------------------------------
int32 EffectiveDisableDuration(int32 baseDuration, int32 distance,
                               int32 maxRadius, ArmorType armor)
{
    int32 distanceAdjusted = ComputeDisableDuration(baseDuration, distance, maxRadius);
    if (distanceAdjusted <= 0) return 0;

    float resistance = EMPResistanceByArmor(armor);
    int32 finalDuration = static_cast<int32>(
        static_cast<float>(distanceAdjusted) * (1.0f - resistance));
    if (finalDuration < 1 && distanceAdjusted > 0) finalDuration = 1;
    return finalDuration;
}

// --------------------------------------------------------------------------
// EffectiveDamage - Combines the distance falloff and armor resistance
// to compute the final EMP damage for a specific unit.
// --------------------------------------------------------------------------
int32 EffectiveDamage(int32 baseDamage, int32 distance, int32 maxRadius,
                      ArmorType armor)
{
    int32 distanceAdjusted = ComputeDamageFalloff(baseDamage, distance, maxRadius);
    if (distanceAdjusted <= 0) return 0;

    float resistance = EMPResistanceByArmor(armor);
    int32 finalDamage = static_cast<int32>(
        static_cast<float>(distanceAdjusted) * (1.0f - resistance));
    if (finalDamage < 1 && distanceAdjusted > 0) finalDamage = 1;
    return finalDamage;
}

// --------------------------------------------------------------------------
// EMPStatistic - Tracks runtime statistics for a single EMP pulse.
// --------------------------------------------------------------------------
struct EMPStatistic
{
    int32 UnitsDisabled;
    int32 UnitsDamaged;
    int32 UnitsImmune;
    int32 CellsAffected;
    int32 PeakRadius;
    int32 TotalDamageDealt;

    EMPStatistic()
        : UnitsDisabled(0), UnitsDamaged(0), UnitsImmune(0)
        , CellsAffected(0), PeakRadius(0), TotalDamageDealt(0) {}
};

// --------------------------------------------------------------------------
// ResetStatistic - Zeros out a statistic structure.
// --------------------------------------------------------------------------
void ResetStatistic(EMPStatistic& stat)
{
    stat.UnitsDisabled = 0;
    stat.UnitsDamaged = 0;
    stat.UnitsImmune = 0;
    stat.CellsAffected = 0;
    stat.PeakRadius = 0;
    stat.TotalDamageDealt = 0;
}

// --------------------------------------------------------------------------
// RecordDisable - Updates the statistic when a unit is disabled.
// --------------------------------------------------------------------------
void RecordDisable(EMPStatistic& stat, int32 disableDuration)
{
    if (disableDuration > 0) {
        ++stat.UnitsDisabled;
    }
}

// --------------------------------------------------------------------------
// RecordDamage - Updates the statistic when damage is dealt.
// --------------------------------------------------------------------------
void RecordDamage(EMPStatistic& stat, int32 damage)
{
    if (damage > 0) {
        ++stat.UnitsDamaged;
        stat.TotalDamageDealt += damage;
    }
}

// --------------------------------------------------------------------------
// RecordImmune - Updates the statistic when a unit is found immune.
// --------------------------------------------------------------------------
void RecordImmune(EMPStatistic& stat)
{
    ++stat.UnitsImmune;
}

// --------------------------------------------------------------------------
// RecordCell - Updates the statistic when a cell is processed.
// --------------------------------------------------------------------------
void RecordCell(EMPStatistic& stat)
{
    ++stat.CellsAffected;
}

// --------------------------------------------------------------------------
// UpdatePeakRadius - Tracks the maximum radius reached during the pulse.
// --------------------------------------------------------------------------
void UpdatePeakRadius(EMPStatistic& stat, int32 currentRadius)
{
    if (currentRadius > stat.PeakRadius) {
        stat.PeakRadius = currentRadius;
    }
}

// --------------------------------------------------------------------------
// CellDistance - Returns the 2D distance from the EMP center to the center
// of the given cell, in leptons.
// --------------------------------------------------------------------------
int32 CellDistance(const CoordStruct& empCenter, const CellStruct& cell)
{
    CoordStruct cellCenter = CellClass::Cell2Coord(cell);
    cellCenter.X += LeptonsPerCell / 2;
    cellCenter.Y += LeptonsPerCell / 2;

    int32 dx = cellCenter.X - empCenter.X;
    int32 dy = cellCenter.Y - empCenter.Y;
    return static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy)));
}

// --------------------------------------------------------------------------
// IsCellInRadius - Returns true if the cell center is within the EMP
// radius.
// --------------------------------------------------------------------------
bool IsCellInRadius(const CoordStruct& empCenter, const CellStruct& cell,
                    int32 radius)
{
    int32 dist = CellDistance(empCenter, cell);
    return dist <= radius;
}

// --------------------------------------------------------------------------
// CellsInRadius - Counts the number of map cells whose centers fall within
// the given EMP radius.  Used for statistics and area coverage estimation.
// --------------------------------------------------------------------------
int32 CellsInRadius(const CoordStruct& empCenter, int32 radius)
{
    CellStruct centerCell = CellClass::Coord2Cell(empCenter);
    int32 cellRadius = (radius + LeptonsPerCell - 1) / LeptonsPerCell;
    if (cellRadius < 0) cellRadius = 0;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (MapClass::Instance) {
        if (minX < 0) minX = 0;
        if (minY < 0) minY = 0;
        if (maxX >= MapClass::Instance->MapWidth) maxX = MapClass::Instance->MapWidth - 1;
        if (maxY >= MapClass::Instance->MapHeight) maxY = MapClass::Instance->MapHeight - 1;
    }

    int32 count = 0;
    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            if (IsCellInRadius(empCenter, cell, radius)) {
                ++count;
            }
        }
    }
    return count;
}

// --------------------------------------------------------------------------
// EMPAuraAlpha - Computes the screen-wide alpha overlay for the EMP
// "aura" effect that briefly tints the screen when a pulse fires.
// --------------------------------------------------------------------------
uint8 EMPAuraAlpha(float lifeProgress, uint8 maxAlpha)
{
    if (lifeProgress < 0.0f || lifeProgress > 1.0f) return 0;

    // Aura is strongest at the start and fades to zero by 30% lifetime.
    if (lifeProgress > 0.3f) return 0;

    float t = lifeProgress / 0.3f;
    float alpha = static_cast<float>(maxAlpha) * (1.0f - t) * (1.0f - t);
    return static_cast<uint8>(alpha);
}

// --------------------------------------------------------------------------
// ScreenShakeOffset - Computes the screen shake offset caused by the EMP
// detonation.  The shake is strongest at the start and decays to zero.
// --------------------------------------------------------------------------
void ScreenShakeOffset(float lifeProgress, int32 maxOffset,
                       int32& outDx, int32& outDy)
{
    outDx = 0;
    outDy = 0;

    if (lifeProgress < 0.0f || lifeProgress > 1.0f) return;
    if (maxOffset <= 0) return;

    // Shake only during the first 15% of the lifetime.
    if (lifeProgress > 0.15f) return;

    float t = lifeProgress / 0.15f;
    float magnitude = static_cast<float>(maxOffset) * (1.0f - t);

    // Pseudo-random direction based on lifeProgress to avoid a fixed shake.
    float angle = lifeProgress * 47.0f;
    outDx = static_cast<int32>(std::cos(angle) * magnitude);
    outDy = static_cast<int32>(std::sin(angle * 1.3f) * magnitude);
}

// --------------------------------------------------------------------------
// ExpandSpeedForRadius - Computes the optimal expansion speed for a given
// max radius so that the ring reaches full expansion in approximately 30
// frames regardless of radius size.
// --------------------------------------------------------------------------
int32 ExpandSpeedForRadius(int32 maxRadius)
{
    if (maxRadius <= 0) return 1;
    int32 speed = maxRadius / 30;
    if (speed < 1) speed = 1;
    if (speed > 32) speed = 32;
    return speed;
}

// --------------------------------------------------------------------------
// IsValidEMPulseIndex - Returns true if the given index is a valid slot
// in the EMPulseManagerClass pulse array.
// --------------------------------------------------------------------------
bool IsValidEMPulseIndex(int32 index)
{
    return index >= 0 && index < MAX_EMPULSES;
}

// --------------------------------------------------------------------------
// FindFreeEMPulseSlot - Returns the index of the first free (or inactive)
// slot in the pulse array, or -1 if all slots are occupied by active
// pulses.
// --------------------------------------------------------------------------
int32 FindFreeEMPulseSlot(EMPulseClass* const* pulses, int32 count)
{
    for (int32 i = 0; i < count; ++i) {
        if (pulses[i] == nullptr || !pulses[i]->IsActive) {
            return i;
        }
    }
    return -1;
}

// --------------------------------------------------------------------------
// CountActivePulses - Returns the number of active pulses in the array.
// --------------------------------------------------------------------------
int32 CountActivePulses(EMPulseClass* const* pulses, int32 count)
{
    int32 active = 0;
    for (int32 i = 0; i < count; ++i) {
        if (pulses[i] && pulses[i]->IsActive) {
            ++active;
        }
    }
    return active;
}

// --------------------------------------------------------------------------
// EMPulseToString - Returns a human-readable description of the EMP pulse
// state for debugging.
// --------------------------------------------------------------------------
const char* EMPulsePhaseToString(EMPPhase phase)
{
    switch (phase) {
    case EMPPhase::Flash:    return "Flash";
    case EMPPhase::Expand:   return "Expand";
    case EMPPhase::Hold:     return "Hold";
    case EMPPhase::Fade:     return "Fade";
    case EMPPhase::Complete: return "Complete";
    default:                 return "Unknown";
    }
}

// --------------------------------------------------------------------------
// LifeProgress - Computes the lifetime progress (0.0 to 1.0) of an EMP
// pulse from its current and maximum duration.
// --------------------------------------------------------------------------
float LifeProgress(int32 currentDuration, int32 maxDuration)
{
    if (maxDuration <= 0) return 1.0f;
    float progress = static_cast<float>(currentDuration) / static_cast<float>(maxDuration);
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}

} // end anonymous namespace

// ============================================================================
// Enhanced rendering entry point
//
//  This section provides an enhanced rendering routine that uses the
//  file-local helpers above to produce a richer visual effect than the
//  basic RenderEffect method.  It is invoked by the manager when the
//  enhanced visual mode is active.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// RenderEnhancedEffect - Draws the EMP effect using the multi-phase,
// multi-ring pipeline defined above.  This produces a more visually
// impressive result than the basic single-ring renderer.
// --------------------------------------------------------------------------
void RenderEnhancedEffect(EMPulseClass* pulse, DSurface* surface)
{
    if (!pulse || !surface || !pulse->IsActive) return;

    float lifeProg = LifeProgress(pulse->CurrentDuration, pulse->MaxDuration);
    float baseAlpha = static_cast<float>(pulse->PulseAlpha) / 128.0f;
    if (baseAlpha > 1.0f) baseAlpha = 1.0f;

    DrawAllRings(surface,
                 pulse->Position.X,
                 pulse->Position.Y,
                 pulse->MaxRadius,
                 pulse->MaxPulses,
                 lifeProg,
                 pulse->PulseColor,
                 baseAlpha);
}

// --------------------------------------------------------------------------
// RenderAuraOverlay - Draws the screen-wide EMP aura tint.  This is a
// subtle blue overlay that briefly covers the entire screen when an EMP
// fires, giving the player visual feedback that an EMP event occurred.
// --------------------------------------------------------------------------
void RenderAuraOverlay(EMPulseClass* pulse, DSurface* surface,
                       int32 screenWidth, int32 screenHeight)
{
    if (!pulse || !surface || !pulse->IsActive) return;

    float lifeProg = LifeProgress(pulse->CurrentDuration, pulse->MaxDuration);
    uint8 auraAlpha = EMPAuraAlpha(lifeProg, 40);
    if (auraAlpha == 0) return;

    uint8 r = pulse->PulseColor.R;
    uint8 g = pulse->PulseColor.G;
    uint8 b = pulse->PulseColor.B;

    for (int32 y = 0; y < screenHeight; ++y) {
        for (int32 x = 0; x < screenWidth; ++x) {
            surface->SetPixelAlpha(x, y, r, g, b, auraAlpha);
        }
    }
}

// --------------------------------------------------------------------------
// ComputeEMPulseStatistics - Analyzes the EMP pulse's area of effect and
// returns statistics about the number of cells and potential targets.
// --------------------------------------------------------------------------
EMPStatistic ComputeEMPulseStatistics(EMPulseClass* pulse)
{
    EMPStatistic stat;
    if (!pulse) return stat;

    stat.CellsAffected = CellsInRadius(pulse->Position, pulse->MaxRadius);
    stat.PeakRadius = pulse->CurrentRadius;
    return stat;
}

} // end anonymous namespace
