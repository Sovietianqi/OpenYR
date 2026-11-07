#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Math/CoordStruct.h"
#include "../Abstract/ObjectClass.h"
#include "../Containers/DynamicVectorClass.h"
#include "ParticleTypeClass.h"
#include "ParticleSystemTypeClass.h"

class BlitterClass;
class DSurface;
class ParticleClass;

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

// ----------------------------------------------------------------------------
// 原版 ParticleSystemClass 以 ObjectClass 为基类（asm: CTOR 首行即
// ObjectClass_CTOR），并承载 IRTTITypeInfo/INoticeSink/INoticeSource 三张
// 接口表。粒子系统本身就是一个世界对象，靠 ObjectClass::Unlimbo 登记进
// vec_VisibleObjects 供战术层绘制。
// ----------------------------------------------------------------------------
class NOVTABLE ParticleSystemClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::ParticleSystem;

    static DynamicVectorClass<ParticleSystemClass*>* Array;

    ParticleSystemClass();
    virtual ~ParticleSystemClass();

    // ------------------------------------------------------------------------
    // 原版虚表槽位映射（asm vftable）：
    //   Draw           +0x114 (8字节参数)
    //   Context_Add    vt+0x48  (被 ObjectClass 坐标帮助函数转发)
    //   IsAlive(拦截)  vt+0x30  (field_F8 且无粒子时报告"可摘除")
    //   PointerGotInvalid 直接虚槽
    //   ComputeCRC / Load / Save / GetClassID 延续 AbstractClass 契约
    // ------------------------------------------------------------------------
    virtual void Draw(int32 a2, int32 a3) const;
    virtual CoordStruct* Context_Add(CoordStruct* pOut) const;

    // asm: ParticleSystemClass_IsAlive (0x62FE5E)。基类 ObjectClass 无同名
    // 虚接口，故按本项目惯例以独立虚函数提供（语义与 asm 一致）。
    virtual bool IsAlive() const;

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;
    virtual void ComputeCRC(CRCEngine& crc) const override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;
    virtual AbstractType GetAbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: retn 8 / a2=AbstractClass* / a3=byte  -> 统一签名。
    // 转发到 ObjectClass/AbstractClass 的失效指针清理并清空本类型的指针槽。
    // ------------------------------------------------------------------------
    virtual void PointerGotInvalid(AbstractClass* pInvalid, bool removed) override;

    // ------------------------------------------------------------------------
    // asm: ParticleSystemClass_SetDuration (0x...) -> mov [ecx+0ECh],eax
    // 设定本系统的存活帧数（写入 Duration 槽 +0xEC）。
    // ------------------------------------------------------------------------
    void SetDuration(int32 duration);

    // asm: ParticleSystemClass_AddParticle (0x62E4B4)  ParticleClass* (retn 8)。
    ParticleClass* AddParticle(ParticleTypeClass* pType, const CoordStruct& loc);
    ParticleClass* SpawnParticle(const CoordStruct& loc, const CoordStruct& loc2);
    ParticleClass* GenerateParticle(const CoordStruct& loc, const CoordStruct& loc2, int32 arg);

    void Initialize(ParticleTypeClass* type, const CoordStruct& position);
    void Destroy();
    void Update();
    // asm: ParticleSystemClass_Update (0x62FD5E) 主体 —— Update() 转译本函数。
    int32 AI();
    void EmitParticles();
    int32 FindFreeParticleSlot();
    // 池内粒子槽的发轫（本项目模拟层内部辅助，非原版 proc）。
    void SpawnParticleInSlot(int32 slot);
    CoordStruct CalculateEmitPosition();
    void UpdateParticles();
    void CheckCollision(Particle& p);
    void CleanupParticles();
    void Render(BlitterClass* blitter, DSurface* surface);
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

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知粒子系统补全原版命名形态：粒子创建/生成/清理与
    // 各 BehavesLike 分支的发射推进。
    // ------------------------------------------------------------------------
    void DeleteParticles();
    void Update_Fire();
    void Update_Gas();
    void Update_Smoke();
    void Update_Spark();
    void Update_Railgun();

    bool IsActiveSystem() const;
    bool IsPausedSystem() const;
    bool IsCompleted() const;
    int32 GetActiveParticleCount() const;
    int32 GetTotalEmitted() const;
    // asm: ParticleSystemClass_GetType (0x62...): mov eax,[ecx+0ACh] / retn。
    // 基类 ObjectClass::GetType 返回 ObjectTypeClass*，无法协变覆盖，故按
    // ParticleClass::GetParticleType 的消歧惯例改名为 GetSystemType。
    ParticleSystemTypeClass* GetSystemType() const;
    ObjectClass* GetOwner() const;

    // asm: ParticleSystemClass.PartSysType (+0xACh) -- 本系统所属的粒子系统
    // 类型（ParticleSystemTypeClass），Update/Spark/Draw 均从它读取
    // BehavesLike / ParticleCap / LightSize / SpawnSparkPercentage 等参数。
    ParticleSystemTypeClass* Type;

    // 本系统发射的粒子类型，取自 Type->ParticleType（asm: SpawnParticle 里
    // unk_A83D9C[PartSysType->ParticleType]）。模拟与渲染据此读取
    // ParticleTypeClass 的逐粒子参数。
    ParticleTypeClass* ParticleType;
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

    // 根据游戏行为，可知粒子系统名下登记其生成的粒子对象。
    DynamicVectorClass<ParticleClass*> AttachedParticles;
    int32 SystemID;
    bool Prewarmed;
    bool PrewarmComplete;

    // asm: ParticleSystemClass.field_F0 -- "剩余帧数" 计数器，Update 每帧自减，
    // 归零时调用 vt+0x?? 的 UnInit 槽把系统从战场撤下。Update_Spark 也读它
    // 决定是否继续喷射火花。
    int32 field_F0;
    // asm: ParticleSystemClass.field_F8 -- ctxAdd 失效标记。指针槽被
    // PointerGotInvalid 清空后置 1，Update 随后检测 (Alive && field_F8 &&
    // 无粒子) 时把自身从 vec_VisibleObjects 摘除。
    bool field_F8;
};

// ----------------------------------------------------------------------------
// asm 全局: vec_ParticleSystems -- 所有存活的粒子系统对象表。构造期登记，
// 析构期反注册；PointerGotInvalid 等路径也用其统计。
// ----------------------------------------------------------------------------
DynamicVectorClass<ParticleSystemClass*>* ParticleSystems_Vector();
int32 ParticleSystems_Count();

// ----------------------------------------------------------------------------
// asm 全局: ParticleSystem_Gas —— 毒气/烟雾类动画共用的那个粒子系统实例。
//   构造者（ParticleSystem 类型表初始化）尚未搬迁，目前只在存档往返里读写
//   该槽位（LoadGame_slave1 / ExtraData0_Save 都按 4 字节整体存取）。
// ----------------------------------------------------------------------------
extern ParticleSystemClass* ParticleSystem_Gas;

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