#pragma once

#include "../Abstract/ObjectClass.h"
#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"
#include "../Containers/DynamicVectorClass.h"
#include "../Math/CoordStruct.h"
#include "../Rendering/Surface.h"

// ============================================================================
// AnimStruct
// ============================================================================

struct AnimStruct {
    int32 Rate;
    int32 Start;
    int32 LoopStart;
    int32 LoopEnd;
    int32 End;
    int32 LoopCount;
    int32 Value;
    int32 CurrentLoop;
    bool IsActive;
    bool IsLooping;
    bool HasStarted;
    char PaletteName[16];

    AnimStruct() : Rate(1), Start(0), LoopStart(-1), LoopEnd(-1), End(0),
        LoopCount(0), Value(0), CurrentLoop(0),
        IsActive(false), IsLooping(false), HasStarted(false) {
        for (int32 i = 0; i < 16; ++i) PaletteName[i] = 0;
    }

    bool IsAtEnd() const { return Value >= End; }
    void Reset() { Value = Start; CurrentLoop = 0; }
    void Advance() { ++Value; }
    bool HasLooped() const { return Value > End; }
};

// ============================================================================
// Forward declarations
// ============================================================================

class AnimTypeClass;
class ObjectClass;
class HouseClass;
class BulletClass;
class SHPStruct;

// ============================================================================
// AnimClass
// ============================================================================

class NOVTABLE AnimClass : public ObjectClass {
public:
    static const AbstractType AbsID = AbstractType::Anim;

    static DynamicVectorClass<AnimClass*>* Array;

    AnimClass(AnimTypeClass* pAnimType, const CoordStruct& loc,
        int32 loopDelay = 0, int32 loopCount = 1,
        uint32 flags = 0x600, int32 forceZAdjust = 0,
        bool reverse = false) noexcept;
    AnimClass() noexcept;
    virtual ~AnimClass();

    // ------------------------------------------------------------------------
    // asm: AnimClass_CTOR2 (asm:116499) —— 原版动画的第二构造路径（与
    //   AnimClass::AnimClass 并列，被 TechnoClass 构造路径经 sub_6C3A10 调用）。
    //   它只做基类构造 + 一次性默认值铺底 + 登记进 vec_Anims，随后由
    //   AnimClass::Start 补齐运行期状态。为避免改动既有构造重载签名（外部有
    //   调用方），此处以独立初始化函数表达同一语义，可安全地在默认构造之后
    //   调用。函数名对齐 asm proc（`AnimClass_CTOR2`）。
    // ------------------------------------------------------------------------
    void CTOR2() noexcept;

    virtual HRESULT GetClassID(CLSID* pClassID) override;
    virtual HRESULT Load(IStream* pStm) override;
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override;

    virtual AbstractType WhatAmI() const override;
    virtual int32 Size() const override;

    virtual void Update() override;
    virtual void PointerGotInvalid(AbstractClass* pAbstract, bool removed) override;

    // ------------------------------------------------------------------------
    // asm: AnimClass_Update (asm:118595, ~0xC00 字节)——Update 的子实现。
    //   把原版逐帧推进的全部控制流集中到此处：可见性判定、附加行为、
    //   迭代计时器推帧(Rate)、伤害累积结算、Middle 触发、循环计数与
    //   Type->Next 链式切换、MakeInfantry 与到期 UnInit。
    //   Update() 转译本函数。返回非 0 表示本帧已请求自毁(UnInit)。
    // ------------------------------------------------------------------------
    int32 AI();

    // ------------------------------------------------------------------------
    // asm: AnimClass_GetType (0x426xxx) —— mov eax,[ecx+0C8h]；直接返回
    //   动画类型指针。AnimTypeClass 派生自 ObjectTypeClass，可协变覆盖。
    // ------------------------------------------------------------------------
    virtual ObjectTypeClass* GetType() const override;

    // ------------------------------------------------------------------------
    // asm: AnimClass_GetAbstractDerivationID —— mov eax,4 / retn；
    //   返回 AbstractType::Anim 的抽象派生编号，供存档 Swizzle 判别类型。
    //   注：本工程 AbstractClass 未声明该槽，故按兄弟类（BulletClass/
    //   TagTypeClass）惯例以普通虚函数表达，不加 override。
    // ------------------------------------------------------------------------
    virtual AbstractType GetAbstractDerivationID() const;

    // ------------------------------------------------------------------------
    // asm: AnimClass_GetCRC —— 并入 OwnerObject 派生 ID 与
    //   unknown_D0/TintColor/ZAdjust/YSort/field_184/Damage/状态位/剩余循环等。
    // ------------------------------------------------------------------------
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ------------------------------------------------------------------------
    // asm: AnimClass_GetCoords —— 以基类坐标为中心；若挂有 OwnerObject，
    //   再叠加 OwnerObject 的坐标，得到动画在场景中的真实坐标。
    // ------------------------------------------------------------------------
    virtual CoordStruct* GetCoords(CoordStruct* pCrd) const override;

    // ------------------------------------------------------------------------
    // asm: AnimClass_GetZ —— 取本对象 Location.Z；若挂有 OwnerObject，
    //   再叠加其 Location.Z。返回动画的绝对高度。
    // ------------------------------------------------------------------------
    int32 GetZ() const;

    // ------------------------------------------------------------------------
    // asm: AnimClass_ReturnRealYSort —— 基类 Y 排序值 + YSortAdjust(+0x104)。
    // ------------------------------------------------------------------------
    int32 ReturnRealYSort() const;

    // ------------------------------------------------------------------------
    // asm: AnimClass_In_Which_Layer —— 有 OwnerObject 返回 2（对象层）；
    //   否则有 Type 返回 Type->Layer；都没有返回 3。
    // ------------------------------------------------------------------------
    int32 In_Which_Layer() const;

    // ------------------------------------------------------------------------
    // asm: AnimClass_SetLayer(idxLayer) —— 转发 ObjectClass::Mark(idxLayer)。
    // ------------------------------------------------------------------------
    bool SetLayer(int32 idxLayer);

    // ------------------------------------------------------------------------
    // asm: AnimClass_Remove0 —— 先清掉 Type 标志成立时该地块的
    //   AnimAttached 位，再走基类 Remove0；基类成功则从显示层摘除。
    //   注：基类 ObjectClass::Remove0 为非虚，故此处同名遮蔽、不加 override。
    // ------------------------------------------------------------------------
    bool Remove0();

    // Animation extras
    int32 AnimExtras();
    int32 GetEnd() const;
    void SetOwnerObject(ObjectClass* pOwner);
    void SetCoords(const CoordStruct& coord) { Location = coord; }
    void SetOwner(HouseClass* pOwner) { Owner = pOwner; }

    // ------------------------------------------------------------------------
    // asm: AnimClass::Attach_To(a2) —— 把动画挂到/摘下某个宿主对象。
    //   a2==nullptr 时从旧宿主摘下（坐标转为绝对），否则挂上新宿主（坐标转为相对）。
    //   挂载变化会同步刷新显示层归属。
    // ------------------------------------------------------------------------
    void Attach_To(ObjectClass* pObject);

    // ------------------------------------------------------------------------
    // asm: AnimClass::Detach —— 通知旧宿主解除引用、Attach_To(nullptr)、
    //   停掉循环音效，再做基类 Detach。
    // ------------------------------------------------------------------------
    void Detach();

    // ------------------------------------------------------------------------
    // asm: AnimClass::Start —— 动画（重新）开始播放：播 StartSound、
    //   释放两段 StrX、无 Middle 帧时直接进 Middle、处理 Tiberium 连锁。
    // ------------------------------------------------------------------------
    void Start();

    // ------------------------------------------------------------------------
    // asm: AnimClass::Middle —— 动画进入中段：按 SpawnsParticle 生成粒子、
    //   处理 IsTiberium 的扩散。
    // ------------------------------------------------------------------------
    void Middle();

    // ------------------------------------------------------------------------
    // asm: AnimClass_Update 尾段的"到末帧收束"路径 —— 动画播放至 End 帧时
    //   的收尾：按 Type->Next 派生后继链动画、生成 ExpireAnim、施加
    //   scorch/crater、播 StopSound，最后置 TimeToDie。Update 与
    //   OnAnimationEnd 均汇入此实现。
    // ------------------------------------------------------------------------
    void End();

    // ------------------------------------------------------------------------
    // asm: AnimClass_Draw (0x422CF0) —— __thiscall(this, Point2D* pos,
    //   Rectangle* boundingRect), retn 8。把 Type->Start + CurrentFrame 这一
    //   帧的 SHP 贴到 TempSurface（本树取战术面），pClip 为裁剪矩形。
    // ------------------------------------------------------------------------
    void Draw(Point2D* pPos, const Rectangle* pClip) const;

    // ------------------------------------------------------------------------
    // 链式生成：以 Type->Next 为模板在当前位置派生后继动画。
    //   asm: AnimClass_DTOR / AnimClass_Update 内 "mov Type->Next / call
    //   AnimClass::AnimClass(type, coords, 0,1,600h,0,0)" 的循环。
    // ------------------------------------------------------------------------
    AnimClass* SpawnNext() const;
    static AnimClass* SpawnAnim(AnimTypeClass* pType, const CoordStruct& coords,
                                int32 loopCount = 1, uint32 flags = 0x600);

    void Pause();
    void Unpause();

    // Animation control
    void Play();
    void Stop();
    void SetFrame(int32 frame);
    void NextFrame();

    // Special effects
    void MakeInfantry();
    void CreateExplosion();
    void CreateFire();
    void CreateSmoke();
    void CreateSpark();
    void KillAnim();
    void FireNuke();

    // Static factory methods
    static AnimClass* CreateNukeAnim(const CoordStruct& pos);
    static AnimClass* CreateIonCannonAnim(const CoordStruct& pos);
    static AnimClass* CreateConYardAnim(const CoordStruct& pos);
    static AnimClass* CreateWarFactoryAnim(const CoordStruct& pos);
    static AnimClass* CreateExplosionAnim(const CoordStruct& pos, int32 damageLevel = 1);
    static AnimClass* CreateFireAnim(const CoordStruct& pos);
    static AnimClass* CreateSmokeAnim(const CoordStruct& pos);

    // Rendering
    void Render(bool isometric = true);
    Point2D ConvertCoordToDrawPos() const;
    int32 GetDrawLayer() const;
    int32 GetYSort() const;
    int32 GetZAdjust() const;
    bool IsVisible() const;
    bool ShouldRemoveInFog() const;

    // Static utility methods
    static AnimClass* FindByID(const char* id);
    static int32 GetActiveCount();
    static void RemoveAll();
    static void UpdateAll();
    static void RenderAll();

protected:
    explicit AnimClass(noinit_t) noexcept : ObjectClass(noinit) {}

    // Internal update methods
    void UpdateAnimation();
    void UpdateBounce();
    void UpdateExtras();
    void UpdateSound();
    void UpdateLight();
    void UpdateTrailer();
    void ApplyDamage();
    void CheckFlamingGuy();
    void ProcessTiberium();
    void ProcessVeins();
    void ProcessSpawns();
    void ProcessScorch();
    void ProcessCrater();
    void OnAnimationEnd();
    void OnLoop();

public:
    AnimTypeClass* Type;
    AnimStruct Animation;
    ObjectClass* OwnerObject;
    int32 unknown_D0;
    ObjectClass* LightConvert;
    int32 LightConvertIndex;
    uint32 TintColor;
    // asm +0xD4：Tiberiums_SetupRemap 写入的 remap 索引
    // （取自 ColorScheme 的 +0x30C）。原版直接写 dword，这里用具名槽承接。
    int32 RenderRemapIndex;
    int32 ZAdjust;
    int32 YSortAdjust;
    CoordStruct FlamingGuyCoords;
    int32 FlamingGuyRetries;
    bool IsBuildingAnim;
    bool UnderTemporal;
    bool Paused;
    bool Unpaused;
    int32 PausedAnimFrame;
    bool Reverse;
    uint32 unknown_124;
    BYTE TranslucencyLevel;
    bool TimeToDie;
    BulletClass* AttachedBullet;

    // AnimClass_SetBullet (asm 0x421xxx).
    //   Links the anim to the bullet that spawned it so AnimClass_DTOR can
    //   clear the back-reference when either side dies.  The original stores
    //   the raw pointer into the AttachedBullet slot.
    void SetBullet(BulletClass* pBullet);
    void Update_FlamingGuy();
    HouseClass* Owner;
    int32 LoopDelay;
    double Accum;
    BlitterFlags AnimFlags;
    bool HasExtras;
    int32 RemainingIterations;
    BYTE unknown_196;
    BYTE unknown_197;
    bool IsPlaying_;
    bool IsFogged;
    bool FlamingGuyExpire;
    bool UnableToContinue;
    bool SkipProcessOnce;
    bool Invisible;
    bool PowerOff;
    bool unused_19F;
    bool IsFlaming;
    bool IsNuke;
    bool IsIonCannon;
    bool IsLooping_;

    // Bounce physics
    struct BounceData {
        CoordStruct Velocity;
        float Elasticity;
        int32 Bounces;
        int32 MaxBounces;
        bool IsActive;
        bool padding[3];

        BounceData() : Velocity(0, 0, 0), Elasticity(0.5f), Bounces(0),
            MaxBounces(3), IsActive(false) {
            std::memset(padding, 0, sizeof(padding));
        }

        void Update() {
            // Apply gravity
            if (IsActive) {
                Velocity.Z -= 1;
            }
        }
    };
    BounceData Bounce;

    // State tracking
    int32 TrailerCount;
    int32 TrailerTimer;
    int32 LightTimer;
    int32 SoundTimer;
    // asm: AnimClass.IterationTimer (+0xE8 起) —— 承载 Rate 推帧的帧计时器。
    //   Update 里 `FrameTimerClass_Get_Remaining`==0 且 Rate!=0 时推一帧，
    //   再把 TimeStart=Frame、Duration=Rate 重新武装。此处以"到期帧"整数表达。
    int32 IterationTimer;
    // asm: AnimClass.field_11B/field_11C —— 帧计数切换标记与暂存帧。
    int32 FrameSwitchFrame;
    bool ScorchCreated;
    bool CraterCreated;
    bool InfantryCreated;
    bool TiberiumCreated;
    bool ChainAnimCreated;

    char PaletteName[0x20];
};