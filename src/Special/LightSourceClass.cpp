#include "LightSourceClass.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"
#include "../Game/Game.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ============================================================
// File-local helpers for advanced light computation
// ============================================================
namespace {

// ----------------------------------------------------------------
// Flicker pattern generators.
// Each returns a multiplier in roughly [0, 1.5] that scales the
// base intensity for the current frame.
// ----------------------------------------------------------------

// Candle-style flicker: slow drift with occasional sharp dips.
float FlickerCandle(int32 frame, int32 seed) {
    float t = static_cast<float>(frame) * 0.08f + static_cast<float>(seed) * 0.37f;
    float slow = 0.92f + 0.08f * std::sin(t);
    float fast = 0.06f * std::sin(t * 7.3f + 1.2f);
    float spike = (std::sin(t * 13.7f) > 0.93f) ? -0.22f : 0.0f;
    return slow + fast + spike;
}

// Fluorescent flicker: mostly stable with intermittent on/off.
float FlickerFluorescent(int32 frame, int32 seed) {
    float t = static_cast<float>(frame) * 0.04f + static_cast<float>(seed) * 0.71f;
    float buzz = 0.98f + 0.02f * std::sin(t * 31.0f);
    // Occasional dropouts every ~90 frames.
    int32 cycle = (frame + seed * 17) % 90;
    if (cycle < 3) return 0.15f * buzz;
    if (cycle < 5) return 0.45f * buzz;
    return buzz;
}

// Broken / faulty light: irregular stutters.
float FlickerBroken(int32 frame, int32 seed) {
    float t = static_cast<float>(frame) * 0.12f + static_cast<float>(seed) * 0.53f;
    float base = 0.70f + 0.30f * std::sin(t * 2.1f);
    int32 phase = (frame + seed * 23) % 17;
    if (phase == 0 || phase == 5 || phase == 11) return 0.08f;
    if (phase == 3 || phase == 9) return 0.35f;
    return base;
}

// Storm / lightning flicker: long dark periods with bright flashes.
float FlickerStorm(int32 frame, int32 seed) {
    int32 cycle = (frame + seed * 41) % 120;
    if (cycle < 2) return 1.45f;          // bright flash
    if (cycle < 4) return 0.85f;          // afterglow
    if (cycle < 6) return 1.25f;          // secondary flash
    return 0.30f;                          // dim ambient
}

// Torcher: steady with gentle breathing.
float FlickerTorch(int32 frame, int32 seed) {
    float t = static_cast<float>(frame) * 0.06f + static_cast<float>(seed) * 0.19f;
    return 0.88f + 0.12f * std::sin(t * 1.7f) + 0.04f * std::sin(t * 5.9f);
}

// ----------------------------------------------------------------
// Pulse waveform generators.
// Each returns a value in [0, 1] for the given phase [0, 2*PI).
// ----------------------------------------------------------------

float PulseSine(float phase) {
    return 0.5f + 0.5f * std::sin(phase);
}

float PulseTriangle(float phase) {
    float norm = std::fmod(phase, 6.28318f) / 6.28318f;  // [0,1)
    if (norm < 0.5f) return norm * 2.0f;
    return 2.0f - norm * 2.0f;
}

float PulseSquare(float phase) {
    float norm = std::fmod(phase, 6.28318f) / 6.28318f;
    return (norm < 0.5f) ? 1.0f : 0.0f;
}

float PulseSawtooth(float phase) {
    float norm = std::fmod(phase, 6.28318f) / 6.28318f;
    return norm;
}

float PulseReverseSawtooth(float phase) {
    float norm = std::fmod(phase, 6.28318f) / 6.28318f;
    return 1.0f - norm;
}

// ----------------------------------------------------------------
// Blending helpers for combining light contributions on a cell.
// ----------------------------------------------------------------

void BlendAdditive(int32& existingR, int32& existingG, int32& existingB,
                   int32 r, int32 g, int32 b) {
    existingR += r;
    existingG += g;
    existingB += b;
    if (existingR > 255) existingR = 255;
    if (existingG > 255) existingG = 255;
    if (existingB > 255) existingB = 255;
}

void BlendMultiplicative(int32& existingR, int32& existingG, int32& existingB,
                         int32 r, int32 g, int32 b) {
    existingR = (existingR * r) / 255;
    existingG = (existingG * g) / 255;
    existingB = (existingB * b) / 255;
}

void BlendMax(int32& existingR, int32& existingG, int32& existingB,
              int32 r, int32 g, int32 b) {
    if (r > existingR) existingR = r;
    if (g > existingG) existingG = g;
    if (b > existingB) existingB = b;
}

void BlendScreen(int32& existingR, int32& existingG, int32& existingB,
                 int32 r, int32 g, int32 b) {
    existingR = 255 - ((255 - existingR) * (255 - r)) / 255;
    existingG = 255 - ((255 - existingG) * (255 - g)) / 255;
    existingB = 255 - ((255 - existingB) * (255 - b)) / 255;
}

} // end anonymous namespace

// ============================================================
// LightSourceClass
// ============================================================

LightSourceClass::LightSourceClass()
    : Position(0, 0, 0), Radius(256), Intensity(255), MaxIntensity(255)
    , Color(255, 255, 255), RedComponent(255), GreenComponent(255), BlueComponent(255)
    , FalloffType(LightFalloffType::Linear), FalloffExponent(2.0f)
    , IsActive(false), IsFlickering(false), FlickerIntensity(0.0f)
    , FlickerInterval(3), FlickerTimer(0), FlickerOffset(0.0f)
    , PulsePhase(0.0f), PulseSpeed(0.05f), Pulsing(false)
    , CellBuffer(nullptr), CellBufferSize(0), LastUpdateFrame(0)
    , PoolIndex(-1), IsPooled(false), Priority(0), LightID(0)
    , ZRange(0), ZRangeTop(0), OverlayAlpha(0)
    , InnerRadius(0), OuterRadius(0), UseSpotlight(false)
    , SpotAngle(45.0f), SpotDirection(0.0f, 0.0f, -1.0f), SpotExponent(1.0f) {
}

LightSourceClass::~LightSourceClass() {
    Release();
}

void LightSourceClass::Initialize(const CoordStruct& pos, int32 radius, int32 intensity, const ColorStruct& color) {
    Position = pos;
    Radius = radius;
    Intensity = intensity;
    Color = color;
    RedComponent = color.R;
    GreenComponent = color.G;
    BlueComponent = color.B;
    MaxIntensity = intensity;
    IsActive = true;
    IsPooled = false;
    PoolIndex = -1;
    LastUpdateFrame = 0;
    InnerRadius = radius / 2;
    OuterRadius = radius;

    // Normalise the spotlight direction if it is non-zero.
    float len = std::sqrt(SpotDirection.X * SpotDirection.X
                        + SpotDirection.Y * SpotDirection.Y
                        + SpotDirection.Z * SpotDirection.Z);
    if (len > 0.001f) {
        SpotDirection.X /= len;
        SpotDirection.Y /= len;
        SpotDirection.Z /= len;
    } else {
        SpotDirection = CoordStruct(0.0f, 0.0f, -1.0f);
    }
}

void LightSourceClass::Release() {
    IsActive = false;
    IsPooled = true;
    IsFlickering = false;
    Pulsing = false;
    if (CellBuffer) {
        std::free(CellBuffer);
        CellBuffer = nullptr;
    }
    CellBufferSize = 0;
    FlickerOffset = 0.0f;
    PulsePhase = 0.0f;
}

void LightSourceClass::Update() {
    if (!IsActive) return;

    if (IsFlickering) {
        UpdateFlicker();
    }
    if (Pulsing) {
        UpdatePulse();
    }

    ++LastUpdateFrame;
}

void LightSourceClass::UpdateFlicker() {
    ++FlickerTimer;
    if (FlickerTimer >= FlickerInterval) {
        FlickerTimer = 0;
        // The FlickerInterval member doubles as a flicker pattern selector.
        // Values 1-5 map to distinct patterns; anything else uses the
        // original random-offset behaviour.
        int32 seed = LightID * 7 + 13;
        switch (FlickerInterval) {
            case 1:
                FlickerOffset = (FlickerCandle(LastUpdateFrame, seed) - 1.0f) * FlickerIntensity;
                break;
            case 2:
                FlickerOffset = (FlickerFluorescent(LastUpdateFrame, seed) - 1.0f) * FlickerIntensity;
                break;
            case 3:
                FlickerOffset = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * FlickerIntensity;
                break;
            case 4:
                FlickerOffset = (FlickerBroken(LastUpdateFrame, seed) - 1.0f) * FlickerIntensity;
                break;
            case 5:
                FlickerOffset = (FlickerStorm(LastUpdateFrame, seed) - 1.0f) * FlickerIntensity;
                break;
            default:
                FlickerOffset = (static_cast<float>(std::rand() % 1000) / 1000.0f - 0.5f) * FlickerIntensity;
                break;
        }
    }
}

void LightSourceClass::UpdatePulse() {
    PulsePhase += PulseSpeed;
    if (PulsePhase > 6.28318f) {
        PulsePhase -= 6.28318f;
    }
    // When the phase wraps we can optionally auto-disable pulsing.
    // This keeps the light cycling indefinitely by default.
}

// ----------------------------------------------------------------
// File-local pulse waveform dispatcher.
// The OverlayAlpha member is reused as a waveform selector:
//   0 = sine (default), 1 = triangle, 2 = square,
//   3 = sawtooth, 4 = reverse-sawtooth.
// ----------------------------------------------------------------
namespace {

float DispatchPulse(int32 waveform, float phase) {
    switch (waveform) {
        case 1:  return PulseTriangle(phase);
        case 2:  return PulseSquare(phase);
        case 3:  return PulseSawtooth(phase);
        case 4:  return PulseReverseSawtooth(phase);
        default: return PulseSine(phase);
    }
}

int32 CurrentPulseWaveform(const LightSourceClass& light) {
    // Map OverlayAlpha to a waveform index when the light is pulsing.
    if (light.OverlayAlpha >= 0 && light.OverlayAlpha <= 4) {
        return light.OverlayAlpha;
    }
    return 0;
}

} // end anonymous namespace

void LightSourceClass::CalculateIntensity(int32& red, int32& green, int32& blue) const {
    float baseIntensity = static_cast<float>(Intensity) / 255.0f;

    if (Pulsing) {
        int32 waveform = CurrentPulseWaveform(*this);
        float pulseFactor = DispatchPulse(waveform, PulsePhase);
        baseIntensity *= pulseFactor;
    }

    if (IsFlickering) {
        float flickerFactor = 1.0f + FlickerOffset;
        if (flickerFactor < 0.0f) flickerFactor = 0.0f;
        if (flickerFactor > 1.5f) flickerFactor = 1.5f;
        baseIntensity *= flickerFactor;
    }

    red = static_cast<int32>(RedComponent * baseIntensity);
    green = static_cast<int32>(GreenComponent * baseIntensity);
    blue = static_cast<int32>(BlueComponent * baseIntensity);

    if (red > 255) red = 255;
    if (green > 255) green = 255;
    if (blue > 255) blue = 255;
    if (red < 0) red = 0;
    if (green < 0) green = 0;
    if (blue < 0) blue = 0;
}

float LightSourceClass::CalculateFalloff(int32 distance) const {
    if (distance >= Radius) return 0.0f;
    if (distance <= InnerRadius) return 1.0f;
    if (Radius <= InnerRadius) return 1.0f;

    float t = static_cast<float>(distance - InnerRadius)
            / static_cast<float>(Radius - InnerRadius);

    // Clamp t to [0,1] for numerical safety.
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    switch (FalloffType) {
        case LightFalloffType::Linear:
            return 1.0f - t;
        case LightFalloffType::Quadratic:
            return (1.0f - t) * (1.0f - t);
        case LightFalloffType::Exponential:
            return std::exp(-t * FalloffExponent);
        case LightFalloffType::InverseSquared:
            if (distance > 0) {
                float factor = static_cast<float>(InnerRadius + 1)
                             / static_cast<float>(distance + 1);
                return factor * factor;
            }
            return 1.0f;
        case LightFalloffType::SmoothStep:
            return 1.0f - (t * t * (3.0f - 2.0f * t));
        default:
            return 1.0f - t;
    }
}

// ----------------------------------------------------------------
// Height-based attenuation.
// When ZRange is non-zero the light contribution diminishes with
// the vertical distance between the light source and the target.
// ----------------------------------------------------------------
float CalculateHeightAttenuation(const LightSourceClass& light, int32 targetZ) {
    if (light.ZRange <= 0) return 1.0f;
    int32 zDist = targetZ - light.Position.Z;
    if (zDist < 0) zDist = -zDist;
    if (zDist >= light.ZRange) return 0.0f;
    float t = static_cast<float>(zDist) / static_cast<float>(light.ZRange);
    return 1.0f - t * t * (3.0f - 2.0f * t);  // smoothstep falloff
}

float LightSourceClass::CalculateSpotlightFactor(const CoordStruct& targetPos) const {
    if (!UseSpotlight) return 1.0f;

    float dx = static_cast<float>(targetPos.X - Position.X);
    float dy = static_cast<float>(targetPos.Y - Position.Y);
    float dz = static_cast<float>(targetPos.Z - Position.Z);
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 0.001f) return 1.0f;

    float dirX = dx / dist;
    float dirY = dy / dist;
    float dirZ = dz / dist;

    float dotProduct = SpotDirection.X * dirX
                     + SpotDirection.Y * dirY
                     + SpotDirection.Z * dirZ;
    if (dotProduct < 0.0f) return 0.0f;

    float spotCos = std::cos(SpotAngle * 0.5f * 3.14159265f / 180.0f);
    if (dotProduct < spotCos) return 0.0f;

    // Soft edge: interpolate between spotCos and spotCos + 10%.
    float softBand = (1.0f - spotCos) * 0.10f;
    if (dotProduct < spotCos + softBand) {
        float edge = (dotProduct - spotCos) / softBand;
        float inner = std::pow((dotProduct - spotCos) / (1.0f - spotCos), SpotExponent);
        return edge * inner;
    }

    float spotFactor = std::pow((dotProduct - spotCos) / (1.0f - spotCos), SpotExponent);
    return spotFactor;
}

// ----------------------------------------------------------------
// Compute the total light contribution at an arbitrary world
// coordinate.  This is used by rendering code that needs to know
// the illumination level at a specific point without iterating
// over cells.
// ----------------------------------------------------------------
// (file-local helper, declared after the class definition)
namespace {

float ComputeLightContribution(const LightSourceClass& light, const CoordStruct& target) {
    int32 dx = target.X - light.Position.X;
    int32 dy = target.Y - light.Position.Y;
    int32 dz = target.Z - light.Position.Z;
    int32 distance = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz)));

    float falloff = light.CalculateFalloff(distance);
    if (falloff <= 0.0f) return 0.0f;

    float spot = light.CalculateSpotlightFactor(target);
    if (spot <= 0.0f) return 0.0f;

    int32 r, g, b;
    light.CalculateIntensity(r, g, b);

    float avg = (static_cast<float>(r) + static_cast<float>(g) + static_cast<float>(b)) / (3.0f * 255.0f);
    return avg * falloff * spot;
}

} // end anonymous namespace

void LightSourceClass::UpdateCellLighting() {
    if (!IsActive) return;
    if (!MapClass::Instance) return;

    CellStruct centerCell = CellClass::Coord2Cell(Position);
    int32 cellRadius = (Radius + LeptonsPerCell - 1) / LeptonsPerCell;
    if (cellRadius < 1) cellRadius = 1;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= MapClass::Instance->MapWidth) maxX = MapClass::Instance->MapWidth - 1;
    if (maxY >= MapClass::Instance->MapHeight) maxY = MapClass::Instance->MapHeight - 1;

    // Pre-compute the modulated colour once per update.
    int32 baseR, baseG, baseB;
    CalculateIntensity(baseR, baseG, baseB);

    for (int32 y = minY; y <= maxY; ++y) {
        for (int32 x = minX; x <= maxX; ++x) {
            CellStruct cell(static_cast<int16>(x), static_cast<int16>(y));
            CoordStruct cellCenter = CellClass::Cell2Coord(cell);
            cellCenter.X += LeptonsPerCell / 2;
            cellCenter.Y += LeptonsPerCell / 2;

            int32 distX = cellCenter.X - Position.X;
            int32 distY = cellCenter.Y - Position.Y;
            int32 distZ = cellCenter.Z - Position.Z;
            int32 distance = static_cast<int32>(std::sqrt(static_cast<float>(distX * distX + distY * distY + distZ * distZ)));

            if (distance >= Radius) continue;

            float falloff = CalculateFalloff(distance);
            float spotFactor = CalculateSpotlightFactor(cellCenter);
            float heightAtten = CalculateHeightAttenuation(*this, cellCenter.Z);

            float totalFactor = falloff * spotFactor * heightAtten;
            if (totalFactor <= 0.0f) continue;

            int32 finalR = static_cast<int32>(baseR * totalFactor);
            int32 finalG = static_cast<int32>(baseG * totalFactor);
            int32 finalB = static_cast<int32>(baseB * totalFactor);

            if (finalR > 0 || finalG > 0 || finalB > 0) {
                ApplyLightToCell(cell, finalR, finalG, finalB);
            }
        }
    }
}

void LightSourceClass::ApplyLightToCell(const CellStruct& cell, int32 r, int32 g, int32 b) {
    if (!MapClass::Instance) return;
    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (!pCell) return;

    // CellColor is the existing lighting member on CellClass
    int32 existingR = (pCell->CellColor >> 16) & 0xFF;
    int32 existingG = (pCell->CellColor >> 8) & 0xFF;
    int32 existingB = pCell->CellColor & 0xFF;

    // Default blending mode is additive.
    // The ZRangeTop member is reused as a blend-mode selector:
    //   0 = additive, 1 = multiplicative, 2 = max, 3 = screen
    switch (ZRangeTop) {
        case 1:
            BlendMultiplicative(existingR, existingG, existingB, r, g, b);
            break;
        case 2:
            BlendMax(existingR, existingG, existingB, r, g, b);
            break;
        case 3:
            BlendScreen(existingR, existingG, existingB, r, g, b);
            break;
        default:
            BlendAdditive(existingR, existingG, existingB, r, g, b);
            break;
    }

    pCell->CellColor = (existingR << 16) | (existingG << 8) | existingB;
}

// ----------------------------------------------------------------
// File-local: compute the number of cells within the light's
// radius.  Used for CellBuffer allocation and statistics.
// ----------------------------------------------------------------
namespace {

int32 CountCellsInRadius(const CoordStruct& pos, int32 radius) {
    if (!MapClass::Instance) return 0;
    CellStruct centerCell = CellClass::Coord2Cell(pos);
    int32 cellRadius = (radius + LeptonsPerCell - 1) / LeptonsPerCell;
    if (cellRadius < 1) cellRadius = 1;

    int32 minX = centerCell.X - cellRadius;
    int32 maxX = centerCell.X + cellRadius;
    int32 minY = centerCell.Y - cellRadius;
    int32 maxY = centerCell.Y + cellRadius;

    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX >= MapClass::Instance->MapWidth) maxX = MapClass::Instance->MapWidth - 1;
    if (maxY >= MapClass::Instance->MapHeight) maxY = MapClass::Instance->MapHeight - 1;

    return (maxX - minX + 1) * (maxY - minY + 1);
}

} // end anonymous namespace

// ----------------------------------------------------------------
// Ensure the CellBuffer is large enough to hold lighting data for
// every cell within the current radius.  Returns true on success.
// ----------------------------------------------------------------
bool LightSourceClass_EnsureBuffer(LightSourceClass& light) {
    int32 needed = CountCellsInRadius(light.Position, light.Radius);
    if (needed <= 0) return false;

    if (light.CellBuffer && light.CellBufferSize >= needed) return true;

    uint8* newBuffer = static_cast<uint8*>(std::malloc(static_cast<size_t>(needed) * 3));
    if (!newBuffer) return false;

    std::memset(newBuffer, 0, static_cast<size_t>(needed) * 3);
    if (light.CellBuffer) std::free(light.CellBuffer);
    light.CellBuffer = newBuffer;
    light.CellBufferSize = needed;
    return true;
}

// ============================================================
// LightSourceClass - setter implementations
// ============================================================

void LightSourceClass::SetColor(int32 r, int32 g, int32 b) {
    RedComponent = static_cast<uint8>(r);
    GreenComponent = static_cast<uint8>(g);
    BlueComponent = static_cast<uint8>(b);
    Color.R = static_cast<uint8>(r);
    Color.G = static_cast<uint8>(g);
    Color.B = static_cast<uint8>(b);
}

void LightSourceClass::SetIntensity(int32 intensity) {
    Intensity = intensity;
    if (Intensity < 0) Intensity = 0;
    if (Intensity > 255) Intensity = 255;
    if (MaxIntensity < Intensity) MaxIntensity = Intensity;
}

void LightSourceClass::SetRadius(int32 radius) {
    Radius = radius;
    if (Radius < 0) Radius = 0;
    if (Radius > 2048) Radius = 2048;
    InnerRadius = Radius / 2;
    OuterRadius = Radius;
}

void LightSourceClass::SetFlicker(bool enabled, float intensity, int32 interval) {
    IsFlickering = enabled;
    FlickerIntensity = intensity;
    if (FlickerIntensity < 0.0f) FlickerIntensity = 0.0f;
    if (FlickerIntensity > 2.0f) FlickerIntensity = 2.0f;
    FlickerInterval = interval;
    if (FlickerInterval < 1) FlickerInterval = 1;
    if (FlickerInterval > 5) FlickerInterval = 5;  // clamp to known patterns
    FlickerTimer = 0;
    FlickerOffset = 0.0f;
}

void LightSourceClass::SetPulse(bool enabled, float speed) {
    Pulsing = enabled;
    PulseSpeed = speed;
    if (PulseSpeed < 0.001f) PulseSpeed = 0.001f;
    if (PulseSpeed > 1.0f) PulseSpeed = 1.0f;
    PulsePhase = 0.0f;
}

void LightSourceClass::SetFalloffType(LightFalloffType type, float exponent) {
    FalloffType = type;
    FalloffExponent = exponent;
    if (FalloffExponent < 0.1f) FalloffExponent = 0.1f;
    if (FalloffExponent > 10.0f) FalloffExponent = 10.0f;
}

void LightSourceClass::SetSpotlight(bool enabled, float angle, float exponent, const CoordStruct& direction) {
    UseSpotlight = enabled;
    SpotAngle = angle;
    if (SpotAngle < 1.0f) SpotAngle = 1.0f;
    if (SpotAngle > 180.0f) SpotAngle = 180.0f;
    SpotExponent = exponent;
    if (SpotExponent < 0.1f) SpotExponent = 0.1f;
    if (SpotExponent > 10.0f) SpotExponent = 10.0f;
    SpotDirection = direction;

    // Normalise the direction vector.
    float len = std::sqrt(SpotDirection.X * SpotDirection.X
                        + SpotDirection.Y * SpotDirection.Y
                        + SpotDirection.Z * SpotDirection.Z);
    if (len > 0.001f) {
        SpotDirection.X /= len;
        SpotDirection.Y /= len;
        SpotDirection.Z /= len;
    } else {
        SpotDirection = CoordStruct(0.0f, 0.0f, -1.0f);
    }
}

void LightSourceClass::SetPosition(const CoordStruct& pos) {
    Position = pos;
}

void LightSourceClass::SetZRange(int32 range, int32 top) {
    ZRange = range;
    if (ZRange < 0) ZRange = 0;
    ZRangeTop = top;
}

void LightSourceClass::SetOverlayAlpha(int32 alpha) {
    OverlayAlpha = alpha;
    if (OverlayAlpha < 0) OverlayAlpha = 0;
    if (OverlayAlpha > 255) OverlayAlpha = 255;
}

void LightSourceClass::SetPriority(int32 priority) {
    Priority = priority;
}

void LightSourceClass::ToggleActive() {
    IsActive = !IsActive;
}

void LightSourceClass::SetActive(bool active) {
    IsActive = active;
}

// ============================================================
// File-local diagnostics and query helpers
// ============================================================
namespace {

// Return a human-readable name for a falloff type.
const char* FalloffTypeName(LightFalloffType type) {
    switch (type) {
        case LightFalloffType::Linear:         return "Linear";
        case LightFalloffType::Quadratic:      return "Quadratic";
        case LightFalloffType::Exponential:    return "Exponential";
        case LightFalloffType::InverseSquared: return "InverseSquared";
        case LightFalloffType::SmoothStep:     return "SmoothStep";
        default:                                return "Unknown";
    }
}

// Return a human-readable name for a flicker pattern.
const char* FlickerPatternName(int32 interval) {
    switch (interval) {
        case 1: return "Candle";
        case 2: return "Fluorescent";
        case 3: return "Random";
        case 4: return "Broken";
        case 5: return "Storm";
        default: return "Off";
    }
}

// Return a human-readable name for a pulse waveform.
const char* PulseWaveformName(int32 waveform) {
    switch (waveform) {
        case 0:  return "Sine";
        case 1:  return "Triangle";
        case 2:  return "Square";
        case 3:  return "Sawtooth";
        case 4:  return "ReverseSawtooth";
        default: return "Unknown";
    }
}

// Return a human-readable name for a blend mode.
const char* BlendModeName(int32 mode) {
    switch (mode) {
        case 0:  return "Additive";
        case 1:  return "Multiplicative";
        case 2:  return "Max";
        case 3:  return "Screen";
        default: return "Unknown";
    }
}

} // end anonymous namespace

// ============================================================
// LightSourcePoolClass
// ============================================================

static LightSourcePoolClass* g_LightSourcePoolInstance = nullptr;

LightSourcePoolClass::LightSourcePoolClass()
    : ActiveCount(0), PoolSize(MAX_POOLED_LIGHTS), NextLightID(1) {
    for (int32 i = 0; i < MAX_POOLED_LIGHTS; ++i) {
        LightSources[i].PoolIndex = i;
        LightSources[i].IsPooled = true;
        LightSources[i].IsActive = false;
        LightSources[i].LightID = 0;
    }
}

LightSourcePoolClass::~LightSourcePoolClass() {
    ReleaseAllLights();
    if (g_LightSourcePoolInstance == this) {
        g_LightSourcePoolInstance = nullptr;
    }
}

LightSourcePoolClass* LightSourcePoolClass::GetInstance() {
    if (!g_LightSourcePoolInstance) {
        g_LightSourcePoolInstance = new LightSourcePoolClass();
    }
    return g_LightSourcePoolInstance;
}

LightSourceClass* LightSourcePoolClass::AllocateLight() {
    // First pass: look for a pooled (free) slot.
    for (int32 i = 0; i < PoolSize; ++i) {
        if (LightSources[i].IsPooled) {
            LightSources[i].IsPooled = false;
            LightSources[i].IsActive = true;
            LightSources[i].LightID = NextLightID++;
            ++ActiveCount;
            return &LightSources[i];
        }
    }

    // Second pass: if every slot is active, try to reclaim the
    // lowest-priority active light whose priority is below a
    // threshold.  This prevents the pool from starving when many
    // transient lights are created.
    int32 reclaimIndex = -1;
    int32 lowestPriority = 0x7FFFFFFF;
    for (int32 i = 0; i < PoolSize; ++i) {
        if (LightSources[i].IsActive && LightSources[i].Priority < lowestPriority) {
            lowestPriority = LightSources[i].Priority;
            reclaimIndex = i;
        }
    }
    if (reclaimIndex >= 0 && lowestPriority < 5) {
        LightSources[reclaimIndex].Release();
        LightSources[reclaimIndex].IsPooled = false;
        LightSources[reclaimIndex].IsActive = true;
        LightSources[reclaimIndex].LightID = NextLightID++;
        ++ActiveCount;
        return &LightSources[reclaimIndex];
    }

    return nullptr;
}

void LightSourcePoolClass::ReleaseLight(LightSourceClass* light) {
    if (!light) return;
    // Verify the pointer actually belongs to this pool.
    if (light < &LightSources[0] || light > &LightSources[PoolSize - 1]) return;
    light->Release();
    if (ActiveCount > 0) --ActiveCount;
}

void LightSourcePoolClass::UpdateAllLights() {
    for (int32 i = 0; i < PoolSize; ++i) {
        if (LightSources[i].IsActive) {
            LightSources[i].Update();
            LightSources[i].UpdateCellLighting();
        }
    }
}

void LightSourcePoolClass::ReleaseAllLights() {
    for (int32 i = 0; i < PoolSize; ++i) {
        LightSources[i].Release();
    }
    ActiveCount = 0;
}

int32 LightSourcePoolClass::GetActiveCount() const {
    return ActiveCount;
}

LightSourceClass* LightSourcePoolClass::FindLightAtPosition(const CoordStruct& pos, int32 threshold) {
    LightSourceClass* best = nullptr;
    int32 bestDist = threshold + 1;

    for (int32 i = 0; i < PoolSize; ++i) {
        if (!LightSources[i].IsActive) continue;

        int32 dx = LightSources[i].Position.X - pos.X;
        int32 dy = LightSources[i].Position.Y - pos.Y;
        int32 dz = LightSources[i].Position.Z - pos.Z;
        int32 dist = static_cast<int32>(std::sqrt(static_cast<float>(dx * dx + dy * dy + dz * dz)));

        if (dist <= threshold && dist < bestDist) {
            bestDist = dist;
            best = &LightSources[i];
        }
    }
    return best;
}

// ----------------------------------------------------------------
// Find a light by its unique ID.
// ----------------------------------------------------------------
LightSourceClass* LightSourcePoolClass_FindByID(LightSourcePoolClass* pool, int32 id) {
    if (!pool) return nullptr;
    for (int32 i = 0; i < pool->PoolSize; ++i) {
        if (pool->LightSources[i].IsActive && pool->LightSources[i].LightID == id) {
            return &pool->LightSources[i];
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------
// Collect all active lights sorted by priority (descending).
// The caller provides an array and receives the count written.
// ----------------------------------------------------------------
int32 LightSourcePoolClass_SortByPriority(LightSourcePoolClass* pool,
                                           LightSourceClass** outArray,
                                           int32 maxCount) {
    if (!pool || !outArray || maxCount <= 0) return 0;

    int32 count = 0;
    for (int32 i = 0; i < pool->PoolSize && count < maxCount; ++i) {
        if (pool->LightSources[i].IsActive) {
            outArray[count++] = &pool->LightSources[i];
        }
    }

    // Simple insertion sort by priority (descending).
    for (int32 i = 1; i < count; ++i) {
        LightSourceClass* key = outArray[i];
        int32 j = i - 1;
        while (j >= 0 && outArray[j]->Priority < key->Priority) {
            outArray[j + 1] = outArray[j];
            --j;
        }
        outArray[j + 1] = key;
    }
    return count;
}

// ----------------------------------------------------------------
// Compute the total light contribution at a world coordinate from
// all active lights in the pool.  This is used by the renderer to
// determine the ambient light level at a pixel.
// ----------------------------------------------------------------
float LightSourcePoolClass_TotalContributionAt(LightSourcePoolClass* pool,
                                                const CoordStruct& pos) {
    if (!pool) return 0.0f;
    float total = 0.0f;
    for (int32 i = 0; i < pool->PoolSize; ++i) {
        if (!pool->LightSources[i].IsActive) continue;
        total += ComputeLightContribution(pool->LightSources[i], pos);
    }
    if (total > 1.0f) total = 1.0f;
    return total;
}

// ----------------------------------------------------------------
// Cull lights whose position is farther than maxDistance from the
// given reference point.  Deactivates them to save update cost.
// ----------------------------------------------------------------
int32 LightSourcePoolClass_CullDistantLights(LightSourcePoolClass* pool,
                                              const CoordStruct& refPos,
                                              int32 maxDistance) {
    if (!pool) return 0;
    int32 culled = 0;
    int64 maxDistSq = static_cast<int64>(maxDistance) * maxDistance;
    for (int32 i = 0; i < pool->PoolSize; ++i) {
        if (!pool->LightSources[i].IsActive) continue;
        int64 dx = pool->LightSources[i].Position.X - refPos.X;
        int64 dy = pool->LightSources[i].Position.Y - refPos.Y;
        int64 dz = pool->LightSources[i].Position.Z - refPos.Z;
        int64 distSq = dx * dx + dy * dy + dz * dz;
        if (distSq > maxDistSq) {
            pool->LightSources[i].SetActive(false);
            ++culled;
        }
    }
    return culled;
}

// ----------------------------------------------------------------
// Dump diagnostic information for all active lights.
// Writes a compact text summary into the provided buffer.
// ----------------------------------------------------------------
int32 LightSourcePoolClass_DumpDiagnostics(LightSourcePoolClass* pool,
                                            char* buffer, int32 bufferSize) {
    if (!pool || !buffer || bufferSize <= 0) return 0;

    int32 offset = 0;
    int32 active = 0;

    for (int32 i = 0; i < pool->PoolSize; ++i) {
        LightSourceClass& ls = pool->LightSources[i];
        if (!ls.IsActive) continue;
        ++active;

        int32 r, g, b;
        ls.CalculateIntensity(r, g, b);

        int32 remaining = bufferSize - offset;
        if (remaining < 80) break;

        int32 written = std::snprintf(buffer + offset, static_cast<size_t>(remaining),
            "Light #%d  pos=(%d,%d,%d)  R=%d  I=%d  F=%s  Flk=%s  Pul=%s  Blend=%s\n",
            ls.LightID,
            ls.Position.X, ls.Position.Y, ls.Position.Z,
            ls.Radius, ls.Intensity,
            FalloffTypeName(ls.FalloffType),
            ls.IsFlickering ? FlickerPatternName(ls.FlickerInterval) : "Off",
            ls.Pulsing ? PulseWaveformName(CurrentPulseWaveform(ls)) : "Off",
            BlendModeName(ls.ZRangeTop));

        if (written > 0) offset += written;
    }

    int32 headerRemaining = bufferSize;
    if (offset > 0) {
        // Prepend a summary header by shifting.
        char header[64];
        int32 headerLen = std::snprintf(header, sizeof(header),
            "Active lights: %d / %d\n", active, pool->PoolSize);
        if (headerLen > 0 && headerLen + offset < bufferSize) {
            std::memmove(buffer + headerLen, buffer, static_cast<size_t>(offset));
            std::memcpy(buffer, header, static_cast<size_t>(headerLen));
            offset += headerLen;
        }
    } else {
        offset = std::snprintf(buffer, static_cast<size_t>(bufferSize),
            "Active lights: 0 / %d\n", pool->PoolSize);
    }

    return offset;
}
