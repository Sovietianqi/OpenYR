#include "WaveClass.h"
#include "../Map/MapClass.h"
#include "../Scenario/ScenarioClass.h"

#include <cmath>
#include <cstdlib>

// ============================================================
// WaveClass
// ============================================================

WaveClass::WaveClass()
    : Amplitude(0.0f), Frequency(0.0f), Phase(0.0f), Speed(0.0f)
    , Wavelength(0.0f), Direction(0.0f), DecayRate(0.005f)
    , CurrentTime(0.0f), MaxAmplitude(10.0f), MinAmplitude(0.0f)
    , IsActive(false), WaveForm(WaveType::Sine)
    , DampingFactor(0.95f), ReflectionFactor(0.3f)
    , SourceX(0), SourceY(0), WaveRadius(0), MaxRadius(0) {
}

WaveClass::~WaveClass() {
}

void WaveClass::Initialize(float amplitude, float frequency, float phase, float wavelength, float direction) {
    Amplitude = amplitude;
    Frequency = frequency;
    Phase = phase;
    Wavelength = wavelength;
    Direction = direction;
    CurrentTime = 0.0f;
    IsActive = true;
    DampingFactor = 0.95f;
    ReflectionFactor = 0.3f;
}

void WaveClass::InitializeFromSource(int32 sourceX, int32 sourceY, float amplitude, float radius, float speed) {
    SourceX = sourceX;
    SourceY = sourceY;
    Amplitude = amplitude;
    MaxAmplitude = amplitude;
    WaveRadius = 0;
    MaxRadius = radius;
    Speed = speed;
    CurrentTime = 0.0f;
    IsActive = true;
    WaveForm = WaveType::Circular;
}

void WaveClass::UpdateWave() {
    if (!IsActive) return;
    CurrentTime += 0.016f;

    if (WaveForm == WaveType::Circular) {
        WaveRadius += Speed;
        if (WaveRadius >= MaxRadius) {
            Amplitude *= DampingFactor;
        }
        if (Amplitude < 0.1f) {
            IsActive = false;
            Amplitude = 0.0f;
        }
    } else {
        Amplitude *= DampingFactor;
        if (Amplitude < MinAmplitude) {
            Amplitude = 0.0f;
            IsActive = false;
        }
    }
}

float WaveClass::CalculateHeight(int32 x, int32 y) const {
    if (!IsActive) return 0.0f;

    float height = 0.0f;

    switch (WaveForm) {
        case WaveType::Sine:
            height = CalculateSineWave(x, y);
            break;
        case WaveType::Cosine:
            height = CalculateCosineWave(x, y);
            break;
        case WaveType::Circular:
            height = CalculateCircularWave(x, y);
            break;
        case WaveType::Square:
            height = CalculateSquareWave(x, y);
            break;
        case WaveType::Triangle:
            height = CalculateTriangleWave(x, y);
            break;
        case WaveType::Sawtooth:
            height = CalculateSawtoothWave(x, y);
            break;
        default:
            height = CalculateSineWave(x, y);
            break;
    }

    return height;
}

float WaveClass::CalculateSineWave(int32 x, int32 y) const {
    float dirRad = Direction * 3.14159f / 180.0f;
    float dirX = std::cos(dirRad);
    float dirY = std::sin(dirRad);

    float distance = (dirX * static_cast<float>(x) + dirY * static_cast<float>(y)) * Frequency;
    float angularPhase = distance + Phase + CurrentTime * Speed;

    return Amplitude * std::sin(angularPhase);
}

float WaveClass::CalculateCosineWave(int32 x, int32 y) const {
    float dirRad = Direction * 3.14159f / 180.0f;
    float dirX = std::cos(dirRad);
    float dirY = std::sin(dirRad);

    float distance = (dirX * static_cast<float>(x) + dirY * static_cast<float>(y)) * Frequency;
    float angularPhase = distance + Phase + CurrentTime * Speed;

    return Amplitude * std::cos(angularPhase);
}

float WaveClass::CalculateCircularWave(int32 x, int32 y) const {
    float dx = static_cast<float>(x - SourceX);
    float dy = static_cast<float>(y - SourceY);
    float distance = std::sqrt(dx * dx + dy * dy);

    float diff = distance - WaveRadius;
    float angularPhase = diff * Frequency + Phase + CurrentTime * Speed;

    float height = Amplitude * std::sin(angularPhase);

    if (distance > WaveRadius + 1.0f) {
        float decay = std::exp(-(distance - WaveRadius) * DecayRate);
        height *= decay;
    }

    if (distance < WaveRadius * 0.5f) {
        float innerFactor = distance / (WaveRadius * 0.5f);
        height *= innerFactor * innerFactor;
    }

    return height;
}

float WaveClass::CalculateSquareWave(int32 x, int32 y) const {
    float sineVal = CalculateSineWave(x, y);
    return (sineVal >= 0.0f) ? Amplitude : -Amplitude;
}

float WaveClass::CalculateTriangleWave(int32 x, int32 y) const {
    float dirRad = Direction * 3.14159f / 180.0f;
    float dirX = std::cos(dirRad);
    float dirY = std::sin(dirRad);

    float distance = (dirX * static_cast<float>(x) + dirY * static_cast<float>(y)) * Frequency;
    float phase = distance + Phase + CurrentTime * Speed;

    float val = phase / (2.0f * 3.14159f);
    val = val - std::floor(val + 0.5f);
    val = 4.0f * std::fabs(val) - 1.0f;

    return Amplitude * val;
}

float WaveClass::CalculateSawtoothWave(int32 x, int32 y) const {
    float dirRad = Direction * 3.14159f / 180.0f;
    float dirX = std::cos(dirRad);
    float dirY = std::sin(dirRad);

    float distance = (dirX * static_cast<float>(x) + dirY * static_cast<float>(y)) * Frequency;
    float phase = distance + Phase + CurrentTime * Speed;

    float val = phase / (2.0f * 3.14159f);
    val = 2.0f * (val - std::floor(val)) - 1.0f;

    return Amplitude * val;
}

float WaveClass::CalculateVertexDisplacement(int32 x, int32 y) const {
    return CalculateHeight(x, y);
}

float WaveClass::CalculateWaterDisturbance(int32 x, int32 y) const {
    float height = CalculateHeight(x, y);
    float dx = height * 0.01f;
    float dy = height * 0.01f;
    return std::sqrt(dx * dx + dy * dy);
}

void WaveClass::SetAmplitude(float amplitude) {
    Amplitude = amplitude;
    if (Amplitude < 0.0f) Amplitude = 0.0f;
    if (Amplitude > MaxAmplitude) Amplitude = MaxAmplitude;
}

void WaveClass::SetFrequency(float frequency) {
    Frequency = frequency;
    if (Frequency < 0.0f) Frequency = 0.0f;
    if (Frequency > 100.0f) Frequency = 100.0f;
}

void WaveClass::SetPhase(float phase) {
    Phase = phase;
}

void WaveClass::SetSpeed(float speed) {
    Speed = speed;
    if (Speed < 0.0f) Speed = 0.0f;
    if (Speed > 100.0f) Speed = 100.0f;
}

void WaveClass::SetWavelength(float wavelength) {
    Wavelength = wavelength;
    if (Wavelength > 0.0f) {
        Frequency = 2.0f * 3.14159f / Wavelength;
    }
}

void WaveClass::SetDirection(float direction) {
    Direction = direction;
    while (Direction >= 360.0f) Direction -= 360.0f;
    while (Direction < 0.0f) Direction += 360.0f;
}

void WaveClass::SetDamping(float factor) {
    DampingFactor = factor;
    if (DampingFactor < 0.0f) DampingFactor = 0.0f;
    if (DampingFactor > 1.0f) DampingFactor = 1.0f;
}

void WaveClass::SetReflection(float factor) {
    ReflectionFactor = factor;
    if (ReflectionFactor < 0.0f) ReflectionFactor = 0.0f;
    if (ReflectionFactor > 1.0f) ReflectionFactor = 1.0f;
}

void WaveClass::SetDecayRate(float rate) {
    DecayRate = rate;
    if (DecayRate < 0.0f) DecayRate = 0.0f;
    if (DecayRate > 0.1f) DecayRate = 0.1f;
}

void WaveClass::SetWaveType(WaveType type) {
    WaveForm = type;
}

void WaveClass::Reset() {
    CurrentTime = 0.0f;
    IsActive = true;
    Amplitude = MaxAmplitude;
    if (WaveForm == WaveType::Circular) {
        WaveRadius = 0;
    }
}

void WaveClass::Stop() {
    IsActive = false;
    Amplitude = 0.0f;
}

// ============================================================
// WaveManagerClass
// ============================================================

static WaveManagerClass* g_WaveManagerInstance = nullptr;

WaveManagerClass::WaveManagerClass()
    : WaveCount(0), GlobalAmplitude(1.0f), GlobalFrequency(1.0f)
    , GlobalSpeed(1.0f), WindDirection(0.0f), WindStrength(0.0f)
    , WaterLevel(0), EnableReflection(true), EnableSuperposition(true) {
    for (int32 i = 0; i < MAX_WAVES; ++i) {
        Waves[i] = nullptr;
    }
}

WaveManagerClass::~WaveManagerClass() {
    RemoveAllWaves();
}

WaveManagerClass* WaveManagerClass::GetInstance() {
    if (!g_WaveManagerInstance) {
        g_WaveManagerInstance = new WaveManagerClass();
    }
    return g_WaveManagerInstance;
}

int32 WaveManagerClass::AddWave(const WaveClass& wave) {
    if (WaveCount >= MAX_WAVES) return -1;

    for (int32 i = 0; i < MAX_WAVES; ++i) {
        if (Waves[i] == nullptr) {
            Waves[i] = new WaveClass(wave);
            ++WaveCount;
            return i;
        }
    }
    return -1;
}

bool WaveManagerClass::RemoveWave(int32 index) {
    if (index < 0 || index >= MAX_WAVES) return false;
    if (Waves[index] == nullptr) return false;

    delete Waves[index];
    Waves[index] = nullptr;
    --WaveCount;
    return true;
}

void WaveManagerClass::RemoveAllWaves() {
    for (int32 i = 0; i < MAX_WAVES; ++i) {
        if (Waves[i]) {
            delete Waves[i];
            Waves[i] = nullptr;
        }
    }
    WaveCount = 0;
}

void WaveManagerClass::UpdateAllWaves() {
    for (int32 i = 0; i < MAX_WAVES; ++i) {
        if (Waves[i] && Waves[i]->IsActive) {
            Waves[i]->UpdateWave();
        }
    }
}

float WaveManagerClass::CalculateCompositeHeight(int32 x, int32 y) const {
    float totalHeight = 0.0f;

    if (EnableSuperposition) {
        for (int32 i = 0; i < MAX_WAVES; ++i) {
            if (Waves[i] && Waves[i]->IsActive) {
                totalHeight += Waves[i]->CalculateHeight(x, y);
            }
        }
    } else {
        float maxHeight = 0.0f;
        for (int32 i = 0; i < MAX_WAVES; ++i) {
            if (Waves[i] && Waves[i]->IsActive) {
                float height = Waves[i]->CalculateHeight(x, y);
                if (std::fabs(height) > std::fabs(maxHeight)) {
                    maxHeight = height;
                }
            }
        }
        totalHeight = maxHeight;
    }

    totalHeight *= GlobalAmplitude * GlobalFrequency;
    return totalHeight;
}

float WaveManagerClass::CalculateDisturbance(int32 x, int32 y) const {
    float totalDisturbance = 0.0f;

    for (int32 i = 0; i < MAX_WAVES; ++i) {
        if (Waves[i] && Waves[i]->IsActive) {
            totalDisturbance += Waves[i]->CalculateWaterDisturbance(x, y);
        }
    }

    return totalDisturbance;
}

void WaveManagerClass::SetGlobalAmplitude(float amplitude) {
    GlobalAmplitude = amplitude;
    if (GlobalAmplitude < 0.0f) GlobalAmplitude = 0.0f;
    if (GlobalAmplitude > 5.0f) GlobalAmplitude = 5.0f;
}

void WaveManagerClass::SetGlobalFrequency(float frequency) {
    GlobalFrequency = frequency;
    if (GlobalFrequency < 0.0f) GlobalFrequency = 0.0f;
    if (GlobalFrequency > 5.0f) GlobalFrequency = 5.0f;
}

void WaveManagerClass::SetGlobalSpeed(float speed) {
    GlobalSpeed = speed;
    if (GlobalSpeed < 0.0f) GlobalSpeed = 0.0f;
    if (GlobalSpeed > 5.0f) GlobalSpeed = 5.0f;
}

void WaveManagerClass::SetWindDirection(float direction) {
    WindDirection = direction;
    while (WindDirection >= 360.0f) WindDirection -= 360.0f;
    while (WindDirection < 0.0f) WindDirection += 360.0f;
}

void WaveManagerClass::SetWindStrength(float strength) {
    WindStrength = strength;
    if (WindStrength < 0.0f) WindStrength = 0.0f;
    if (WindStrength > 1.0f) WindStrength = 1.0f;
}

void WaveManagerClass::SetWaterLevel(int32 level) {
    WaterLevel = level;
}

void WaveManagerClass::SetEnableReflection(bool enable) {
    EnableReflection = enable;
}

void WaveManagerClass::SetEnableSuperposition(bool enable) {
    EnableSuperposition = enable;
}

int32 WaveManagerClass::GetWaveCount() const {
    return WaveCount;
}

WaveClass* WaveManagerClass::GetWave(int32 index) const {
    if (index < 0 || index >= MAX_WAVES) return nullptr;
    return Waves[index];
}

void WaveManagerClass::CreateCircularWave(int32 x, int32 y, float amplitude, float radius, float speed) {
    WaveClass* wave = new WaveClass();
    wave->InitializeFromSource(x, y, amplitude, radius, speed);
    AddWave(*wave);
    delete wave;
}

void WaveManagerClass::CreatePlaneWave(float amplitude, float frequency, float direction, float speed) {
    WaveClass* wave = new WaveClass();
    wave->Initialize(amplitude, frequency, 0.0f, 2.0f * 3.14159f / frequency, direction);
    wave->SetSpeed(speed);
    wave->SetWaveType(WaveType::Sine);
    AddWave(*wave);
    delete wave;
}