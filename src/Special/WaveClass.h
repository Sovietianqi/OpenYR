#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"

enum class WaveType : int32 {
    Sine = 0,
    Cosine = 1,
    Circular = 2,
    Square = 3,
    Triangle = 4,
    Sawtooth = 5
};

static constexpr int32 MAX_WAVES = 16;

class WaveClass {
public:
    static DynamicVectorClass<WaveClass*>* Array;

    WaveClass();
    ~WaveClass();

    void Initialize(float amplitude, float frequency, float phase, float wavelength, float direction);
    void InitializeFromSource(int32 sourceX, int32 sourceY, float amplitude, float radius, float speed);
    void UpdateWave();
    float CalculateHeight(int32 x, int32 y) const;
    float CalculateSineWave(int32 x, int32 y) const;
    float CalculateCosineWave(int32 x, int32 y) const;
    float CalculateCircularWave(int32 x, int32 y) const;
    float CalculateSquareWave(int32 x, int32 y) const;
    float CalculateTriangleWave(int32 x, int32 y) const;
    float CalculateSawtoothWave(int32 x, int32 y) const;
    float CalculateVertexDisplacement(int32 x, int32 y) const;
    float CalculateWaterDisturbance(int32 x, int32 y) const;

    void SetAmplitude(float amplitude);
    void SetFrequency(float frequency);
    void SetPhase(float phase);
    void SetSpeed(float speed);
    void SetWavelength(float wavelength);
    void SetDirection(float direction);
    void SetDamping(float factor);
    void SetReflection(float factor);
    void SetDecayRate(float rate);
    void SetWaveType(WaveType type);
    void Reset();
    void Stop();

    float Amplitude;
    float Frequency;
    float Phase;
    float Speed;
    float Wavelength;
    float Direction;
    float DecayRate;
    float CurrentTime;
    float MaxAmplitude;
    float MinAmplitude;
    bool IsActive;
    WaveType WaveForm;
    float DampingFactor;
    float ReflectionFactor;
    int32 SourceX;
    int32 SourceY;
    float WaveRadius;
    float MaxRadius;
};

class WaveManagerClass {
public:
    WaveManagerClass();
    ~WaveManagerClass();

    static WaveManagerClass* GetInstance();

    int32 AddWave(const WaveClass& wave);
    bool RemoveWave(int32 index);
    void RemoveAllWaves();
    void UpdateAllWaves();
    float CalculateCompositeHeight(int32 x, int32 y) const;
    float CalculateDisturbance(int32 x, int32 y) const;

    void SetGlobalAmplitude(float amplitude);
    void SetGlobalFrequency(float frequency);
    void SetGlobalSpeed(float speed);
    void SetWindDirection(float direction);
    void SetWindStrength(float strength);
    void SetWaterLevel(int32 level);
    void SetEnableReflection(bool enable);
    void SetEnableSuperposition(bool enable);
    int32 GetWaveCount() const;
    WaveClass* GetWave(int32 index) const;
    void CreateCircularWave(int32 x, int32 y, float amplitude, float radius, float speed);
    void CreatePlaneWave(float amplitude, float frequency, float direction, float speed);

    WaveClass* Waves[MAX_WAVES];
    int32 WaveCount;
    float GlobalAmplitude;
    float GlobalFrequency;
    float GlobalSpeed;
    float WindDirection;
    float WindStrength;
    int32 WaterLevel;
    bool EnableReflection;
    bool EnableSuperposition;
};