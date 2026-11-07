#pragma once

#include "../Abstract/ObjectClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/CoordStruct.h"
#include "../Math/Rectangle.h"
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

    // ------------------------------------------------------------------------
    // asm: ParticleClass_Update_All 之外，粒子的每帧主体推进由 AI 承载；
    //   Update() 转译 AI()。同时 PointerGotInvalid 清除指向失效对象的引用槽。
    // ------------------------------------------------------------------------
    int32 AI();
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // ------------------------------------------------------------------------
    // asm: ParticleClass_GetAbstractDerivationID (0x62DA6E): mov eax,16h
    //   返回 AbstractType::Particle 的编号 0x16(22)，供存档 Swizzle 阶段
    //   判别具体派生类型。基类 AbstractClass 无此虚接口，故与 BulletClass
    //   一样以新增虚函数形式提供。
    // ------------------------------------------------------------------------
    virtual AbstractType GetAbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: ParticleClass_GetType (0x62DAA6):
    //        mov eax,[ecx+ParticleClass.ParticleType] / retn
    //   直接返回粒子类型指针。ParticleTypeClass 并非 ObjectTypeClass 的派生，
    //   无法协变覆盖基类 ObjectClass::GetType 的 ObjectTypeClass* 版本，
    //   故按本项目图层 Mark_Layer 的消歧惯例改名为 GetParticleType，
    //   语义与 asm 完全一致。
    // ------------------------------------------------------------------------
    ParticleTypeClass* GetParticleType() const { return Type; }

    // ------------------------------------------------------------------------
    // 图层三件套
    // ------------------------------------------------------------------------
    // asm: ParticleClass_GetLayer (0x62DA8A): mov eax,2  -> Layer::Ground
    virtual Layer GetLayer() const;
    // asm: ParticleClass_SetLayer (0x62DB64):
    //   push idxLayer / call ObjectClass::Mark / test al,al / setnz al / retn 4
    //   转发到基类 Mark_Layer 并把结果规格化为 0/1。
    virtual bool Mark_Layer(int32 idxLayer) override;

    // asm: ParticleClass_InWhichLayer (0x62DB8B): mov eax,Air(3)
    //   粒子一律画在 Air 层。
    virtual Layer InWhichLayer() const;

    // ------------------------------------------------------------------------
    // asm: ParticleClass_Update_All (0x62CE40) -- 顶层 per-frame 推进
    //   按 Type->BehavesLike 分派到对应 Update_*，随后 dec RemainingEC，
    //   归零则置 TimeToDie(Dead)。
    // ------------------------------------------------------------------------
    void Update_All();

    // ------------------------------------------------------------------------
    // 默认坐标（静态），供构造期回填 xyz_5 等未知向量槽。
    // asm: ParticleClass_Init_DefaultCellCoords (0x62D9E0)
    //      ParticleClass_Init_DefaultRoomCoords (0x62D9F0)
    // ------------------------------------------------------------------------
    static CellStruct DefaultCellCoords;
    static CoordStruct DefaultRoomCoords;
    static void Init_DefaultCellCoords();
    static void Init_DefaultRoomCoords();

    // Particle-specific
    void Fire();
    // asm: ParticleClass_Draw(0x62CEF0) 的两种入口：
    //   * Draw() 自行把世界坐标换算到战术面并绘制（供 DrawAllParticles 使用）；
    //   * Draw(pos, clip) 由战术层在 Draw_Transient_Objects 中按已有屏幕坐标调用，
    //     与 AnimClass::Draw 同形，避免重复换算。
    void Draw();
    void Draw(Point2D* pPos, const Rectangle* pClip);
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
    // ------------------------------------------------------------------------
    // 根据游戏行为，可知粒子补全各 BehavesLike 分支的推进入口与取帧。
    // ------------------------------------------------------------------------
    void Update_Fire();
    void Update_Gas();
    void Update_Smoke();
    void Update_Railgun();
    void Update_Spark();
    void Update_2();
    void Update2_Gas();
    void Update2_Smoke();
    int32 GetImageFrame();

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