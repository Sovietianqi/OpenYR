#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/AbstractClass.h"
#include "../Containers/DynamicVectorClass.h"

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

    // asm: LightSourceClass_ChangeLevels (0x554A80)
    // Re-applies the light's level/tint triple unless all values already
    // match; when the update-enable byte (+0x48) is set the cell lighting is
    // refreshed afterwards (sub_554AF0).  Called from RadSiteClass_Radiate.
    void ChangeLevels(int32 intensity, int32 tintR, int32 tintG, int32 tintB, int32 mode);
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

    // ------------------------------------------------------------------------
    // asm: LightSourceClass_Start (0x554A60, retn 4: mode)
    //   若 +0x48（启用字节）已置位则直接返回；否则置位并调用 ApplyLevels(mode)
    //   重算受影响格子的光照。
    // ------------------------------------------------------------------------
    void Start(int32 mode);

    // ------------------------------------------------------------------------
    // asm: LightSourceClass_Stop (0x554A80 区, retn 4: mode)
    //   与 Start 相反：+0x48 未置位则返回；否则清位并调用 ApplyLevels(mode)。
    // ------------------------------------------------------------------------
    void Stop(int32 mode);

    // ------------------------------------------------------------------------
    // asm: sub_554AF0 —— Start / Stop / ChangeLevels 共用的"重算本光源覆盖格"
    //   例程（vecLightSources_UpdateCells 的逐光源入口）。
    // ------------------------------------------------------------------------
    void ApplyLevels(int32 mode);

    // ------------------------------------------------------------------------
    // asm: LightSourceClass_DeleteAll (0x5549A0)
    //   销毁全局 vec_LightSources 中的每一个光源并清空；随后释放
    //   vec_LightSourcePendingCells。OpenYR 以光源池等价实现。
    // ------------------------------------------------------------------------
    static void DeleteAll();

    // asm: LightSourceClass::ComputeCRC（vt+34h）—— 位置 / 半径 / 强度 /
    //   颜色四组主状态折进累积器。
    void ComputeCRC(CRCEngine& crc) const;

    // asm: LightSourceClass_GetSize —— 对象尺寸 0x4C
    int32 GetSize() const;

    // asm: LightSourceClass_GetAbstractDerivationID —— 抽象派生号 0x36
    int32 GetAbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: vec_LightSourcePendingCells 的元素（0x1Ch 字节）
    //   +0x00 a2 写回槽（apply 阶段当 Convert 用）
    //   +0x04 arg0 写回槽（apply 阶段当 Intensity 用）
    //   +0x08 Intensity（apply 阶段当 Ambient 用）
    //   +0x0A Ambient  （apply 阶段当 R 用）
    //   +0x0C tintB    （apply 阶段当 G 用）
    //   +0x0E tintG    （apply 阶段当 B 用）
    //   +0x10 CoordStruct（世界坐标）
    //   取样与推参的槽位在 asm 里就是这么错位对应的，原样保留。
    // ------------------------------------------------------------------------
    struct PendingCellStruct {
        int32       Field_00;    // +0x00
        int32       Field_04;    // +0x04
        uint16      Field_08;    // +0x08
        uint16      Field_0A;    // +0x0A
        uint16      Field_0C;    // +0x0C
        uint16      Field_0E;    // +0x0E
        CoordStruct Coords;      // +0x10
    };

    // ------------------------------------------------------------------------
    // asm: vecLightSources_UpdateCells (0x554D80 区, dl = flag, retn)
    //   两段式待刷新光照冲刷器。第一段倒着走待刷新游标（dword_ABCA7C），
    //   每 16 格探一次计时器预算，用取样例程填充 LightSource 条目；第二段
    //   （标记完成时）把每个条目经 CellClass::SetColourComponents 落到格子、
    //   释放条目、清空向量并强制重绘；尾部再衰减全局闪烁累加器。
    //   pThis 在 asm 里被当整数（fild var_28 = ecx 本身）参与比较，照抄。
    // ------------------------------------------------------------------------
    static void UpdatePendingCells(const void* pThis, bool flag);

    // asm: vec_LightSourcePendingCells —— 待刷新格子表
    static DynamicVectorClass<PendingCellStruct*>* PendingCells;
    // asm: dword_ABCA74 —— 帧计数
    static int32 PendingFrame;
    // asm: dword_ABCA78 —— 上次运行的帧号
    static int32 PendingLastRun;
    // asm: dword_ABCA7C —— 待刷新游标
    static int32 PendingCursor;
    // asm: dword_ABCA80 —— 剩余预算
    static int32 PendingBudget;
    // asm: byte_ABCA84 —— 第二段就绪标志
    static uint8 PendingPhaseDone;
    // asm: dword_829AE8 —— 闪烁衰减累加器
    static int32 TwinkleAccum;

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