#pragma once

#include "AbstractClass.h"
#include "AbstractTypeClass.h"
#include "ObjectTypeClass.h"
#include <Game/LogicClass.h>

class AnimClass;
class FootClass;
class HouseClass;
class TechnoTypeClass;
class TagClass;

// ============================================================================
// ObjectClass (asm sizeof = 0xA4)
//
//  The common base of everything that lives on the map: it owns a world
//  position, an owner house, the cell-occupancy list link (+0x30) and the
//  attached-animation slot (+0x88).
//
//  ★ 继承链对齐：原版是 ObjectClass → AbstractClass，而逐帧驱动槽 0x5C
//    （Update）由 AbstractClass 提供，凡进入 vec_Logics 的实体都实现它。
//    OpenYR 把这个“可被 vec_Logics 驱动”的能力显式建模成 LogicClass
//    （无数据成员的纯接口），ObjectClass 以它为基类——布局、虚表槽位与
//    “直接继承 AbstractClass”完全一致，同时让
//    DynamicVectorClass<LogicClass*> 能直接存放 ObjectClass*。
// ============================================================================
class ObjectClass : public LogicClass {
public:
    static const AbstractType AbsID = AbstractType::Object;

    static DynamicVectorClass<ObjectClass*>* Array;

    // asm: vec_VisibleObjects -- the global vector of objects the tactical
    // pass currently keeps on screen.  DeleteDead() sweeps it.
    static DynamicVectorClass<ObjectClass*>* VisibleObjects;

    // ========================================================================
    // VisibleObjects_DeleteDead (0x725C50)
    //
    //   Walks vec_VisibleObjects and removes every entry whose virtual
    //   IsDead() (vt+0x44) reports true, then runs the per-object cleanup:
    //   the vt+0x08 gate, the RTTI check against the four concrete Techno
    //   subclasses (setting +0x90 InLimbo when it matches), and the vt+0x20
    //   mark-dirty call.
    //
    //   The removal loop reproduces the original's FindIndex-based purge: it
    //   keeps calling FindIndex on the same element so duplicates are dropped
    //   too, and the outer index is deliberately not advanced after a removal
    //   because the shift leaves the next element in place.
    // ========================================================================
    static void DeleteDead();

    // asm: VisibleObjects_DeleteDead (proc@1644735) —— 原名入口，Main_Loop@756905
    //   每帧调用。实现与 DeleteDead() 共用同一张表与同一套摘除规则。
    static void VisibleObjects_DeleteDead();

    // ========================================================================
    // vec_Logics 登记 / 注销（asm: ObjectClass::Unlimbo L1049805 /
    //   ObjectClass_DTOR L1047348 / ObjectClass_Remove0 L1049520）
    //
    //   Register_Logic 幂等：重复登记会被忽略（否则同一对象每帧被驱动两次）。
    //   注销按指针查找，找不到即静默返回。
    // ========================================================================
    void Register_Logic();
    void Unregister_Logic();
    bool Is_Logic_Registered() const;

    ObjectClass() noexcept
        : LogicClass()
        , Location()
        , CachedLocation()
        , Owner(nullptr)
        , IsSelected(false)
        , IsInLimbo(false)
        , Marked(false)
        , InOpenTopped(false)
        , PassengerBlocked(false)
        , Tag(nullptr)
        , NextObject(nullptr)
        , field_14(0)
        , field_6C(0)
        , field_74(1)
        , Selected(0)
        , Alive(0)
        , AttachedAnim(nullptr)
    {}
    virtual ~ObjectClass();

    virtual AbstractType WhatAmI() const override { return AbstractType::Object; }
    virtual int32 Size() const override { return sizeof(ObjectClass); }

    // 根据游戏行为，可知建造资格查询在对象面上只对建筑本体开放；
    // 电力链接绘制把两端对象各标记一次。
    bool Who_Can_Build_Me(TechnoTypeClass* pType, bool buildLimit, const HouseClass* pHouse) const;
    void DrawALinkTo(const ObjectClass* pTarget) const;

    virtual HRESULT GetClassID(CLSID* pClassID) override { return E_FAIL; }
    virtual HRESULT Load(IStream* pStm) override { return S_OK; }
    virtual HRESULT Save(IStream* pStm, BOOL fClearDirty) override { return S_OK; }

    virtual CoordStruct* GetCoords(CoordStruct* pCrd) const override {
        *pCrd = Location;
        return pCrd;
    }

    virtual bool IsInAir() const override { return false; }
    virtual bool IsOnFloor() const override { return true; }

    // ========================================================================
    // Static Array management
    // ========================================================================
    static void Init_Array();
    static void Delete_Array();
    static int32 Add_To_Array(ObjectClass* pInstance);
    static bool Remove_From_Array(ObjectClass* pInstance);
    static int32 Get_Total_Count();
    static ObjectClass* Get_Instance(int32 index);
    static int32 Find_Index(ObjectClass* pInstance);

    // ========================================================================
    // Limbo / Unlimbo - remove from / re-attach to the map
    // ========================================================================
    virtual bool Limbo();
    virtual bool Unlimbo();

    // ------------------------------------------------------------------------
    // asm: vt_TechnoClass.r.m.o.Put（ObjectClass 虚槽，子类各自覆写）
    //
    //   Put 把对象直接落到给定 lepton 坐标：对象先被置于 limbo，再以新坐标
    //   Unlimbo 回地图。返回是否落位成功。原版 UnitClass / InfantryClass /
    //   AircraftClass 都覆写这一槽（步兵与飞行器的覆写见各自 .cpp）。
    // ------------------------------------------------------------------------
    virtual bool Put(const CoordStruct& coords, int32 a2);

    // ------------------------------------------------------------------------
    // 根据游戏行为，可知 Remove0 负责把一个对象从战场上彻底摘掉：
    //  只有在战场已经开打、并且这个对象不是在敞篷运输车里的时候才做——
    //  否则它只是被运载的货物，摘除由载具那边统一处理。摘除的动作包括：
    //  通知各子系统本对象要离场、把它从地图的占用名单里去掉、释放它名下的
    //  两段名字/描述缓冲区、并把"已离场"标记置起。返回真表示确实执行了。
    // ------------------------------------------------------------------------
    bool Remove0();

    // ========================================================================
    // ObjectClass_Update  (asm:1047494 / ObjectClass_Update proc)
    //
    //   对象层的逐帧主体，由 MissionClass_Update 的第一条调用
    //   （asm `MissionClass_Update +0: call ObjectClass_Update`）驱动。
    //   它做的是"不依赖 techno 语义"的那部分：环境音、离场摘除与
    //   高度/伞降收尾。TechnoClass 覆写了本槽，故本实现只在显式
    //   `ObjectClass::Update()` 时生效（例如 Anim/Bullet 一类直系对象）。
    // ========================================================================
    virtual void Update() override;

    // ========================================================================
    // ObjectClass coordinate-forwarding getters
    // ========================================================================
    CoordStruct Get_Coord() const;
    void Set_Coord(const CoordStruct& coord);
    void SetZ(int32 z);
 // ObjectClass_GetCoords1: the one-argument coordinate getter
    //   forwarded by the vtable slot at +0x50 / +0xA4.
    CoordStruct* GetCoords1(CoordStruct* pCrd) const;

 // ObjectClass_GetPos / ObjectClass_GetCoords_2 (asm
 //   0x5F55E0) / ObjectClass_GetExitCoords: thin
    //   vtable-forwarding coordinate getters.  All three marshal a temporary
    //   CoordStruct, invoke the object's own GetCoords slot (+0x48) and copy
    //   the result into the caller's buffer.  GetExitCoords additionally
    //   ignores its second argument (the binary's callers pass a direction
    //   code that the base class does not use).
    CoordStruct* GetPos(CoordStruct* pPos) const;
    CoordStruct* GetCoords2(CoordStruct* pPos) const;
    CoordStruct* GetExitCoords(CoordStruct* pPos, int32 a3) const;

 // ObjectClass_ReturnRealYSort: returns the object's Y
    //   coordinate offset for depth sorting by summing the Y components of
    //   two successive GetCoords calls (the object's own and its animation's).
    int32 ReturnRealYSort() const;

 // 根据游戏行为，可知 ReceivedRadioCommand 负责下面这段逻辑。
    //
    //  Handles two radio commands at the ObjectClass level:
    //    * cmd 0x0D (MarkGround): forwards a Mark() with Ground to the object,
    //      then reports success (1).
    //    * cmd 0x22 (0x22 = 'confirm'): returns 0x0A ("cannot comply") while
    //      the object's health fraction is below the rules-level threshold
    //      held just past GUIMoveOutSound; otherwise 1.
    //  Every other command is unhandled and returns 0.
    // asm: vtable slot vt_TechnoClass.r.m.o.Techno_ReceivedRadioCommand — a real
    // virtual in the original, so RadioClass / UnitClass / AircraftClass all
    // override the same slot.  OpenYR kept it non-virtual while the radio chain
    // was flat; it is virtual now that RadioClass exists as a base class.
    virtual int32 ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4);

    // ========================================================================
    // Map presence / validity
    // ========================================================================
    bool Is_On_Map() const;
    bool Is_Valid() const;

    // ========================================================================
    // Selection
    // ========================================================================
    virtual bool Select();
    virtual void Deselect();
    bool Is_Selected() const;
    virtual bool Is_Selectable() const;

    // ========================================================================
    // State predicates (vtable slots shared with the renderer / UI)
    // ========================================================================
    virtual bool IsRepairable() const;
    virtual bool IsSellable() const;
    virtual bool IsUndeployable() const;
    virtual bool IsDisguised() const;
    virtual bool IsDisguisedAs(int32 a2) const;
    virtual bool IsIronCurtained() const;
    virtual bool IsBeingWarpedOut() const;
    virtual bool IsWarpingIn() const;
    virtual bool IsWarpingSomethingOut() const;
    virtual bool IsNotWarping() const;
    virtual bool IsActive() const;
    virtual bool IsAnimated() const;
    virtual bool Ignite() const;
    virtual uint32 GetRemapColour() const;

    // ========================================================================
    // Engineer / spy steal check
    // ========================================================================
    virtual bool Is_Allowed_To_Steal() const;

    // ========================================================================
    // Owner accessors
    // ========================================================================
    void Set_Owner(HouseClass* pNewOwner);
    HouseClass* Get_Owner() const;
    virtual HouseClass* GetOwningHouse() const override;
    virtual int32 GetOwningHouseIndex() const override;

    // ========================================================================
    // CRC
    // ========================================================================
    virtual void ComputeCRC(CRCEngine& crc) const override;

    // ========================================================================
    // Y-sort key / comparison
    //
 //  RealYSort is the drawing sort key: the default is
    //  Y + Z, and the foundation-aware derived types (BuildingClass, ...)
 //  override it.  CompareYSort is the predicate the renderer
    //  uses to order two objects and is documented as `setnle`, i.e. strictly
    //  greater.
    // ========================================================================
    virtual int32 RealYSort() const;
    virtual bool CompareYSort(ObjectClass* pOther) const;

    // ========================================================================
 // DistanceFrom / DistanceFrom2
    //
    //  DistanceFrom  returns the floored 2D (X/Y) distance to the target's
    //  coordinates, extended by the target's foundation footprint when the
    //  target is a building.
    //  DistanceFrom2 returns the floored 3D (X/Y/Z) distance.
    //  Both return 0 when the target pointer is null.
    // ========================================================================
    int32 DistanceFrom(ObjectClass* pTarget) const;
    int32 DistanceFrom2(ObjectClass* pTarget, int32 a3, int32 a4) const;

    // ========================================================================
    // Type probes
    // ========================================================================
    virtual ObjectTypeClass* GetType() const;
    virtual TechnoTypeClass* GetTechnoType() const;
    virtual int32 GetSomeInt(int32 a2) const;

    // ========================================================================
    // Object-level probes and handlers
    // ========================================================================
 // ObjectClass_AnimPointerGotInvalid: clears the attached
    // animation slot (+0x88) when it still points at the invalidated anim.
    void AnimPointerGotInvalid(AnimClass* pAnim);
 // ObjectClass_Special_Draw_It: tail-calls the Draw vtable
    // slot (+0x114) with the two supplied arguments.
    void Special_Draw_It(int32 a2, int32 a3);
 // ObjectClass_GetDisguiseHouse: the base implementation
    // reports no disguise.
    HouseClass* GetDisguiseHouse(int32 a2) const;
 // ObjectClass_GetDisguise: the base implementation has no
    // disguise identity.
    int32 GetDisguise(int32 a2) const;
 // ObjectClass_KickOutUnit: base classes hold no passengers.
    bool KickOutUnit(FootClass* pUnit) const;
 // ObjectClass_ClickedMission: base classes ignore clicks.
    bool ClickedMission(int32 a2, int32 a3, int32 a4) const;
 // ObjectClass_GetCurrentMission: base classes report -1.
    int32 GetCurrentMission() const;
 // ObjectClass_CompareYSortValues: a non-reversing variant of
    // CompareYSort used by the map's per-layer insertion sort.  Reads the sort
    // key of *both* objects through the virtual RealYSort slot and returns
    // `setnle`, i.e. true exactly when this->RealYSort() > pOther->RealYSort().
    bool CompareYSortValues(ObjectClass* pOther) const;

    // ========================================================================
    // ObjectClass_GetImage / GetUIName / HasThisID / GetFactoryType
    //
    //  Thin forwarders onto the attached ObjectTypeClass.  The original
    //  reaches the type through the virtual GetType slot (+0x88) and then
    //  dispatches onto the matching type-level accessor:
    //
    //    GetImage    -> GetType()->GetImage()          (SHP drawn for this obj)
    //    GetUIName   -> the base returns the literal "No name"
    //    HasThisID   -> GetType()->HasThisID(id)       (ID/name match test)
    //    GetFactoryType -> the object's own type is asked which abstract
    //                      factory it belongs to; at object level this simply
    //                      yields the object's abstract kind.
    // ========================================================================
    virtual SHPStruct* GetImage() const;
    virtual const wchar_t* GetUIName() const;
    virtual bool HasThisID(const char* pID) const;
    virtual AbstractType GetFactoryType() const;

    // ========================================================================
    // ObjectClass_GetWeaponRange: base objects carry no weapon, so the
    //  default answers 0 (`xor eax,eax / retn 4`).  Armed subclasses override.
    // ========================================================================
    virtual int32 GetWeaponRange(int32 weaponIndex) const;

    // ========================================================================
    // ObjectClass_Get_Height? -- despite the IDA label this is not a height;
    //  the body loads the second argument's coords, the object's coords, and
    //  returns the facing/angle (radians) between them:
    //
    //    atan2(px - qx, py - qy) - pi/2
    //
    //  The caller supplies the reference object as the second argument and
    //  receives the angle (scaled by the game's direction constant) through
    //  the first argument's dwXY buffer.
    // ========================================================================
    virtual void GetFacingTowards(CoordStruct* pOut, const ObjectClass* pRef) const;

    // ========================================================================
    // Facing/angle helper used by GetFacingTowards: atan2 of the two deltas
    //  minus pi/2, scaled to the game's 0x100-direction convention.
    // ========================================================================
    static int32 FacingFromDeltas(int32 dx, int32 dy);

    // ========================================================================
    // ObjectClass_SetPosition: caches the object's draw position.  The
    //  original writes the three coordinate dwords through a cached slot
    //  (+0x9C) rather than through Location, so the renderer can keep the
    //  last projected origin separate from the logical position.
    // ========================================================================
    void SetPosition(const CoordStruct& coords);

    // ========================================================================
    // Neutral object-layer stubs.  Each is a literal transcription of the
    //  base slot the original installs at this level (`retn 0Ch`, `xor al,al`,
    //  ...); concrete subclasses replace them when the behaviour matters.
    //
    //    IronCurtain      : assign an iron-curtain timer (no-op at base)
    //    Scatter          : reflex-scatter the object (no-op at base)
    //    IsCellOccupied   : does the object occupy the given cell? base = 0
    //    MouseOverObject  : hit-test against the cursor (base returns 0)
    //    MouseOverCell    : cell hit-test (base returns 0)
    // ========================================================================
    virtual void IronCurtain(ObjectClass* pSource, int32 a3, int32 a4) const;
    virtual void Scatter(int32 a2, int32 a3, int32 a4) const;
    virtual bool IsCellOccupied(const CellStruct& cell, int32 a3, int32 a4,
                                int32 a5, int32 a6) const;
    virtual int32 MouseOverObject(int32 a2, int32 a3) const;
    virtual int32 MouseOverCell(int32 a2, int32 a3, int32 a4) const;

    // ========================================================================
    // Vtable-slot stubs (asm bodies are bare `retn` / `retn 4` / `xor eax,eax`)
    //
    //  Every one of these is a real object-layer slot the original leaves
    //  uninteresting at this level and lets the concrete classes fill in.
    // ========================================================================
    void UnCloak2() const;
    void FreeCaptured() const;
    void UnInit() const;
    void StopAirstrikeTimer1() const;
    void StopAirstrikeTimer2(int32 a2) const;
    void Sell(int32 a2) const;
    void UpdatePosition(int32 a2) const;
    void Flash(int32 a2) const;
    void DrawRadialIndicator(int32 a2) const;
    void RegisterDestruction(ObjectClass* pKiller) const;
    void RegisterDestruction_Counters(ObjectClass* pKiller) const;
    void Draw(int32 a2, int32 a3, int32 a4) const;
    void DrawExtras(int32 a2, int32 a3) const;
    void See(int32 a2, int32 a3) const;
    void AssignPlanningPath(int32 a2, int32 a3) const;

 // 根据游戏行为，可知 Mark 负责下面这段逻辑。
    //
    //  Registers / unregisters the object in a tactical layer:
    //    idxLayer == 2 (selected-layer): refused while the object is riding an
    //      open-topped transport or is already marked; otherwise the object is
    //      added to the layer through the +0x134 redraw slot.
    //    idxLayer == 0 (normal layer): toggles the highlight off, clearing the
    //      Marked byte and reporting success only when it had been set.
    //    idxLayer == 1 / 3: turns the highlight on by setting Marked.
    //  Returns true when a layer membership actually changed.
    //
    //  The original spells this slot `ObjectClass::Mark`, but the derived
    //  classes already expose TechnoClass::Mark(MarkType) for the unrelated
 //  occupation-bits pass; the layer variant therefore keeps
    //  its disambiguated name here.
    virtual bool Mark_Layer(int32 idxLayer);

 // ObjectClass_LoadTables: called by the container loaders
    // right after a saved object has been reconstructed.  It chains to
    // AbstractClass_LoadTables and then re-stamps the four interface vtable
    // pointers at offsets 0/4/8/0xC, restoring the COM identity that the
    // serialized payload does not carry.
    virtual void LoadTables(IStream* pStm) override;

 // ObjectClass_StopAmbientSound ( caller).
    //
    //  Silences the object's ambient audio loop.  `flag` is passed through
    //  unchanged; the callers use -1 to mean "stop immediately and do not
    //  restart".  The base implementation is a no-op because only audio-aware
    //  descendants override it.
    virtual void StopAmbientSound(int32 flag);

    CoordStruct GetCoords() const {
        CoordStruct ret;
        GetCoords(&ret);
        return ret;
    }

    void SetLocation(const CoordStruct& loc) { Location = loc; }

protected:
    explicit ObjectClass(noinit_t) noexcept : LogicClass() {}

public:
    CoordStruct Location;

    // -- Cached draw position (+0x9C) -----------------------------------------
    // ObjectClass_SetPosition writes here rather than into Location: the
    // renderer keeps the last projected origin separate from the logical
    // position.  Defaults to the origin.
    CoordStruct CachedLocation;

    HouseClass* Owner;
    bool IsSelected;
    bool IsInLimbo;

    // -- Layer highlight flags -------------------------------------------------
    // Marked (+0x74): set while the object is registered in the tactical layer
    //   it was last Mark()ed into, or while the engine is flashing it (rally
    //   points, crate pickups, flag carriers).  ObjectClass::Mark owns it.
    // InOpenTopped (+0x81): the object rides inside an open-topped transport,
    //   so it is drawn by its carrier instead of by the layer itself; Mark
    //   refuses to place such an object on a layer.
    bool Marked;
    bool InOpenTopped;

    // 根据游戏行为，可知 PassengerBlocked 记录"身处的敞篷运输载具已经被打瘫，
    // 因此暂时不许对外开火"这一状态：运输车被摧毁或瘫痪时会把车上所有敞篷乘客
    // 的这一个位置一，避免乘客继续射击；恢复正常后由上层重新放行。
    bool PassengerBlocked;

    // 根据游戏行为，可知每个对象身带一个控制组编号：编组按键写入，解散
    // 与重新编组时改写；-1 表示未编组。
    int8 Group = -1;

    // -- Tag (+0x34) ----------------------------------------------------------
    // The trigger tag attached to this object.  Tag::SpringAll owns the
    // dispatch; ObjectClass only stores the pointer so a trigger action can
    // test membership (ActionClass::WakeupAttachedObjects) or re-attach it
    // when control of the object changes house.
    TagClass*       Tag;

    // -- Cell occupancy list link (+0x30) -------------------------------------
    // Every cell keeps a singly-linked list of the objects standing on it,
    // threaded through this member.  CellClass::GetUnit/GetAircraft/GetInfantry
    // walk it, stopping at the first entry whose WhatAmI() matches the
    // requested abstract type.
    ObjectClass*    NextObject;

    // -- Damage_At_Cell support fields ------------------------------------
    // 原版 ObjectClass（sizeof=0xA4）字段布局（偏移取自 IDA 结构定义）：
    //   +0x14  可被震屏标志字节（Damage_At_Cell 读 bit0）
    //   +0x6C  扫描/绘制标志字节（>0 表示参与结算）
    //   +0x74  战术层注册标记（byte）
    //   +0x81  Selected 选中标志
    //   +0x88  Alive 存活标志
    // 注：原版 Health(+0x64)/BackupHealth(+0x68) 在本项目中由
    // TechnoClass 同名字段承载（见 TechnoClass.h），此处不重复声明以免遮蔽。
    uint8           field_14;              // +0x14  震屏相关标志位
    uint8           field_6C;              // +0x6C  扫描/绘制标志字节
    uint8           field_74;              // +0x74  战术层注册标记
    uint8           Selected;              // +0x81  选中标志
    uint8           Alive;                 // +0x88  存活标志

    // -- Attached animation (+0x88) -------------------------------------------
    // The single animation the engine hangs off the object (damage smoke,
    // idle effects).  ObjectClass_AnimPointerGotInvalid clears it when the
    // anim it names is destroyed.
    AnimClass*      AttachedAnim;
};
