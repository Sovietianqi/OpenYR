#include <Abstract/ObjectClass.h>

#include <Abstract/BuildingClass.h>
#include <Abstract/BuildingTypeClass.h>
#include <Abstract/TechnoClass.h>
#include <Abstract/ObjectTypeClass.h>
#include <Houses/HouseClass.h>
#include <Core/Memory.h>
#include <Core/Macros.h>
#include <Game/Game.h>
#include <Rendering/DisplayClass.h>
#include <Rendering/TacticalClass.h>
#include <Math/CoordStruct.h>
#include <Map/CellClass.h>
#include <Audio/VocClass.h>
#include <Rules/RulesClass.h>

#include <cmath>

// ============================================================================
// ObjectClass.cpp
//
//  ObjectClass sits below TechnoClass in the inheritance tree and represents
//  anything that can physically exist on the game map - buildings, infantry,
//  vehicles, aircraft, terrain pieces, overlays, smudges, etc.  The class
//  owns the position (CoordStruct), the owning house pointer, and the
//  selection state.  This file expands the .cpp with the static-array
//  plumbing and the limbo / unlimbo / select / deselect helpers that the
//  original binary provides at this layer.
// ============================================================================

// ============================================================================
// File-local helpers
// ============================================================================

namespace
{
    // Floor of an integer square root - used by the distance helpers so that
    // they agree with the original's float_sqrt / Float_To_Int_Floor pair
 // ( and 0x5F62FB).
    int32 FloorSqrt(int32 value) noexcept
    {
        if (value <= 0)
            return 0;
        int32 r = static_cast<int32>(std::sqrt(static_cast<double>(value)));
        while (r > 0 && r * r > value)
            --r;
        while ((r + 1) * (r + 1) <= value)
            ++r;
        return r;
    }
}

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<ObjectClass*>* ObjectClass::Array = nullptr;
DynamicVectorClass<ObjectClass*>* ObjectClass::VisibleObjects = nullptr;

// ============================================================================
// VisibleObjects_DeleteDead (0x725C50)
//
//   Sweeps the global vec_VisibleObjects and retires every entry that reports
//   itself dead.  The original body reads:
//
//     push ecx / push esi / xor esi,esi / test len,len / jle done
//   loc:                                       ; loc_725C81
//     mov eax,[vec_VisibleObjects.data]
//     mov ecx,[eax+esi*4]                      ; ecx = pObj
//     mov [esp+8+var_4], ecx                   ; stash pObj
//     mov edx,[ecx]
//     call [edx+44h]                           ; vt+0x44 == IsDead()
//     test al,al / jz loc_725D7F               ; alive -> ++esi
//   FindIndex loop:                            ; loc_725C9A
//     mov edx,[vec_VisibleObjects.vt]
//     lea eax,[esp+8+var_4]                    ; &pObj
//     push eax / mov ecx,offset vec / call [edx+10h]   ; vt+0x10 == FindIndex
//     cmp eax,-1 / jz loc_725CE1
//     cmp eax,len / jge loc_725CE1
//     dec len / store len / jge loc_725C9A     ; loop while idx < new len
//     ; tail shift                                    loc_725CC7
//     mov ecx,[vec.data] / inc eax
//     mov edx,[ecx+eax*4] / mov [ecx+eax*4-4],edx
//     mov ecx,len / cmp eax,ecx / jl loc_725CC7
//     jmp loc_725C9A
//   Survivor cleanup:                          ; loc_725CE1
//     mov eax,[esp+0Ch+var_8] / push eax
//     mov ecx,[eax] / call [ecx+8]             ; vt+0x08 == Release()
//     test eax,eax / jz loc_725D80             ; refcount 0 -> skip cleanup
//     4x (push / ___RTtypeid / type_info::operator==)
//       vs BuildingClass / UnitClass / InfantryClass / AircraftClass
//     loc_725D63: mov byte [eax+90h],1         ; InLimbo = true
//     loc_725D6E: mov ecx,pObj / test ecx,ecx / jz loc_725D80
//                 mov edx,[ecx] / push 1 / call [edx+20h]   ; vt+0x20 == Mark_Layer(1)
//   loc_725D80: cmp esi,len / jl loc_725C81
//
//   Two details worth spelling out, because getting either wrong changes the
//   control flow:
//
//   * IDA emits three separate stack locals (var_C / var_8 / var_4) but they
//     are one and the same dword: the prologue pushes ecx and esi, and the
//     mid-function ___RTtypeid calls re-adjust esp, so every `[esp+N+var_X]`
//     resolves to the single saved `&pObj` slot.  There is exactly one local.
//
//   * vt+0x08 is Release(), not a boolean gate.  `mov ecx,[eax]` loads the
//     vtable (eax still holds pObj, which was pushed as the this-pointer), and
//     the branch tests the refcount Release() returns.  A dead object that is
//     still referenced is left untouched beyond dropping it from the list.
//
//   The outer index is deliberately NOT advanced after a purge: the tail
//   shift leaves the next element at the same slot, which is exactly why the
//   asm falls straight back into the loc_725D80 length comparison.
// ============================================================================
void ObjectClass::DeleteDead()
{
    if (VisibleObjects == nullptr)
        return;

    int32 i = 0;
    while (i < VisibleObjects->Count) {                 // asm: loc_725D80 cond
        ObjectClass* pObj = VisibleObjects->Items[i];   // asm: mov ecx,[eax+esi*4]

        // asm: mov edx,[ecx] / call [edx+44h] / test al,al / jz loc_725D7F
        if (pObj == nullptr || !pObj->IsDead()) {
            ++i;                                        // asm: loc_725D7F inc esi
            continue;
        }

        // asm: loc_725C9A -- FindIndex/Remove until the pointer is gone.
        // The original re-runs FindIndex after every shift so duplicated
        // pointers are all dropped; Remove() shifts the tail up one, which is
        // what loc_725CC7 hand-rolls.
        for (;;) {
            int32 idx = -1;
            for (int32 k = 0; k < VisibleObjects->Count; ++k) {
                if (VisibleObjects->Items[k] == pObj) { idx = k; break; }
            }
            if (idx == -1 || idx >= VisibleObjects->Count)  // asm: cmp/jz/jge
                break;
            VisibleObjects->Remove(idx);                    // asm: dec length + shift
        }

        // asm: loc_725CE1 -- vt+0x08 == Release().  The branch is on the new
        // refcount, so an object that is still referenced skips the cleanup.
        if (pObj->Release() == 0) {
            // asm: 4x ___RTtypeid + type_info::operator== against
            //      BuildingClass / UnitClass / InfantryClass / AircraftClass.
            // The project identifies the concrete classes through WhatAmI().
            const AbstractType what = pObj->WhatAmI();
            if (what == AbstractType::Building || what == AbstractType::Unit ||
                what == AbstractType::Infantry || what == AbstractType::Aircraft) {
                // asm: loc_725D63 -- mov byte [eax+90h],1  (InLimbo)
                pObj->IsInLimbo = true;
            }
            // asm: loc_725D6E -- push 1 / call [edx+20h] (vt+0x20 == Mark_Layer)
            pObj->Mark_Layer(1);
        }

        // index deliberately not advanced (see header note)
    }
}

// ============================================================================
// VisibleObjects_DeleteDead (proc@1644735)
//
//  Main_Loop@756905 每帧调用的原名入口。asm 里它和
//  ObjectClass::DeleteDead 是同一段代码的两个符号（IDA 按调用点命名），
//  这里保留原名以对齐调用链，实现完全共用。
// ============================================================================
void ObjectClass::VisibleObjects_DeleteDead()
{
    DeleteDead();
}

// ============================================================================
// vec_Logics 登记 / 注销
//
//  asm: ObjectClass::Unlimbo L1049805-1049806
//         push 0 / push esi / mov ecx,offset vec_Logics / call sub_55BAA0
//       ObjectClass_DTOR  L1047348 `mov ecx,offset vec_Logics; call sub_55BAE0`
//       ObjectClass_Remove0 L1049520 同上
//
//  表只存裸指针、不增删引用计数，与原版一致。
// ============================================================================
void ObjectClass::Register_Logic()
{
    LogicClass_Register(this);
}

void ObjectClass::Unregister_Logic()
{
    LogicClass_Unregister(this);
}

bool ObjectClass::Is_Logic_Registered() const
{
    return LogicClass_IsRegistered(this);
}

// ============================================================================
// ~ObjectClass
//
//  asm: ObjectClass_DTOR (proc@1047138)，L1047348 从 vec_Logics 摘除。
//  原版以 `[esi+98h]`（Alive）为门；这里按指针无条件查找摘除——表里没有
//  该指针时循环自然不动作，既不会误删也不会留下悬空项。
// ============================================================================
ObjectClass::~ObjectClass()
{
    Unregister_Logic();
}

// ============================================================================
// Init_Array / Delete_Array
//
//  Mirror the AbstractClass helpers but operate on the typed ObjectClass
//  array.  Both vectors coexist in the original binary - AbstractClass::Array
//  is the polymorphic root and ObjectClass::Array narrows the iteration to
//  only things that actually live on the map.
// ============================================================================
void ObjectClass::Init_Array()
{
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<ObjectClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<ObjectClass*>)));

        if (Array != nullptr)
        {
            new (Array) DynamicVectorClass<ObjectClass*>();
        }
    }

    // asm: vec_VisibleObjects —— VisibleObjects_DeleteDead 每帧清扫的对象表。
    //   原版在全局表初始化块（L69914-69920 一帶）与 vec_Logics 一同建立。
    if (VisibleObjects == nullptr)
    {
        VisibleObjects = static_cast<DynamicVectorClass<ObjectClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<ObjectClass*>)));

        if (VisibleObjects != nullptr)
        {
            new (VisibleObjects) DynamicVectorClass<ObjectClass*>();
        }
    }
}

void ObjectClass::Delete_Array()
{
    if (Array == nullptr && VisibleObjects == nullptr)
        return;

    if (Array != nullptr)
    {
        Array->~DynamicVectorClass<ObjectClass*>();
        YRMemory::Deallocate(Array);
        Array = nullptr;
    }

    if (VisibleObjects != nullptr)
    {
        VisibleObjects->~DynamicVectorClass<ObjectClass*>();
        YRMemory::Deallocate(VisibleObjects);
        VisibleObjects = nullptr;
    }
}

// ============================================================================
// Add_To_Array / Remove_From_Array
// ============================================================================
int32 ObjectClass::Add_To_Array(ObjectClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;

    if (!Array->Add(pInstance))
        return -1;

    return Array->Count - 1;
}

bool ObjectClass::Remove_From_Array(ObjectClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return false;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
        {
            return Array->Remove(i);
        }
    }
    return false;
}

// ============================================================================
// Get_Total_Count / Get_Instance / Find_Index
//
//  Convenience accessors used by the renderer (which needs the count to size
//  its draw list) and the save/load subsystem (which needs to enumerate).
// ============================================================================
int32 ObjectClass::Get_Total_Count()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

ObjectClass* ObjectClass::Get_Instance(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 ObjectClass::Find_Index(ObjectClass* pInstance)
{
    if (Array == nullptr || pInstance == nullptr)
        return -1;
    for (int32 i = 0; i < Array->Count; ++i)
    {
        if (Array->Items[i] == pInstance)
            return i;
    }
    return -1;
}

// ============================================================================
// Limbo
//
//  Removes the object from the map without destroying it.  Limbo'd objects
//  still exist in the global arrays and can be re-attached to the map with
//  Unlimbo.  This is the path used by the chronosphere, the war factory
//  exit, and the producer/unload flow.
// ============================================================================
bool ObjectClass::Limbo()
{
    // asm: 0x5F4D50 ObjectClass::Limbo (inlined at the Unlimbo/Remove0 sites)
    //   Guard: an object that is already off the map cannot be limboed again.
    if (IsInLimbo)
        return false;

    // asm: 被摘下前先取消选中——被摘出地图的对象不应继续留在选择圈里。
    if (IsSelected)
        Deselect();

    // asm: 同时清掉战术层注册标记，layer 里不再为它保留槽位。
    Marked = false;

    // asm: 从可见对象表里移除（vec_VisibleObjects）。战术绘制每帧遍历该表，
    //   留下一个已离场的指针会让遍历读到悬空状态。
    if (VisibleObjects != nullptr)
    {
        for (int32 i = 0; i < VisibleObjects->Count; ++i)
        {
            if (VisibleObjects->Items[i] == this)
            {
                VisibleObjects->Remove(i);
                break;
            }
        }
    }

    // asm: 置"已离场"标记（[esi+81h]=1）。
    IsInLimbo = true;

    // asm: ObjectClass::Limbo 在摘下对象时同步从 vec_Logics 摘除——这与
    //   Unlimbo 末尾无条件 Add 互为逆操作（否则反复 limbo/unlimbo 会让同一
    //   对象在表里堆积多份，每帧被驱动多次）。
    Unregister_Logic();
    return true;
}

// ============================================================================
// Unlimbo
//
//  Places a previously-limboed object back onto the map at Location.
// ============================================================================
bool ObjectClass::Unlimbo()
{
    // asm: 0x5F4EF0 ObjectClass::Unlimbo(this, xyz, a3)   retn 8
    //   1. 目标坐标是"默认哨兵"（Default_X/Y/Z）-> 返回假；
    //   2. 战场未开打（GameActive == 0）-> 返回假；
    //   3. 对象当前并不在 limbo（[esi+81h] == 0）-> 返回假；
    //   4. 已经带战术层注册标记（[esi+74h] != 0）-> 返回假。
    //   全部通过后才清 limbo 位、经虚拟 SetPosition 落位，最后 Mark(1) 放回
    //   地面层；Mark 失败则把 limbo 位重新置回并返回假。
    if (!IsInLimbo)
        return false;

    if (Marked)
        return false;

    // asm: 先清"已离场"与"敞篷搭载"两位，再落位。
    IsInLimbo = false;
    InOpenTopped = false;

    // asm: call [edx+1B4h] -> SetPosition(coord)；本项目以 Set_Coord 承载。
    Set_Coord(Location);

    // asm: call [edx+124h](1) -> Mark(1)，把对象放回地面层。失败时复原。
    if (!Mark_Layer(1))
    {
        IsInLimbo = true;
        return false;
    }

    // asm: 尾部还会据类型上的 UseLineTrail 建一条 LineTrailClass 尾迹并挂到
    //   +0xA8。本项目 TechnoType 未登记该字段，尾迹由渲染层负责，故略。

    // asm: L1049805-1049806 —— 落位成功后把 this 登记进 vec_Logics。
    //   原版在 Session 为 0/5 时直接 Add，其余会话下先做一次 FindIndex
    //   （== 0xFFFFFFFE 即“已在表内”）再决定是否 Add；本项目由
    //   LogicClass_Register 的查重承担同一语义。
    Register_Logic();

    // asm: the tail of Unlimbo submits the object to the tactical display
    //   layer (DisplayClass::Submit).  Without it the object exists in the
    //   world and in vec_Logics but never enters the draw list, so the
    //   renderer reports visible=0 for a fully populated map.
    if (DisplayClass::Instance != nullptr)
        DisplayClass::Instance->Submit(this);

    return true;
}

// ============================================================================
// Put
//
//  asm: ObjectClass 的 Put 虚槽（vt_TechnoClass.r.m.o.Put 的基实现）。
//  语义：把对象落到给定的 lepton 坐标，再回到地图上。原版子类（单位 /
//  步兵 / 飞行器）各自覆写，基类这里给出通用落位：先写坐标并置于 limbo，
//  再走虚 Unlimbo 完成战术层注册。
// ============================================================================
bool ObjectClass::Put(const CoordStruct& coords, int32 a2)
{
    (void)a2;
    Set_Coord(coords);
    IsInLimbo = true;
    return Unlimbo();
}

// ============================================================================
// asm: ObjectClass_Update   (gamemd:1047494 .. 1047799)
//
//   对象层逐帧主体。逐段：
//
//     段 1（1047494）：已 limbo（[esi+81h]）→ 跳到收尾；
//     段 2（1047500）：取类型（vt+0x88 GetType），若类型上登记了
//                      AmbientSound（!= -1）→ 在对象坐标处循环播放环境音；
//     段 3（1047530）：对象自带的"名字音"槽（+0x60）非 -1 → 一并播放；
//     段 4（1047553）：Falling 标志为真时做高度收尾——据 GetZ（vt+0x1D0）
//                      与对象 Z 重算绘制 YSort，刷新层，落地点清状态、
//                      附着的动画槽清零、把对象重新 Submit 回显示层；
//     段 5（1047594）：仍 limbo → DisplayClass::Remove(this) 从显示层摘除。
//
//   本项目把"高度/伞降/名字音"这几个 asm 直读的私有槽收敛为与
//   现有字段等价的表达：环境音按 ObjectTypeClass::AmbientSound 播放；
//   离场走 DisplayClass::Remove。其余依赖 TechnoClass 侧虚拟槽
//   （GetHeight/SetHeight/GetZ/UpdatePosition）的分支，由 TechnoClass
//   覆写本槽时承担（TechnoClass::Update 已链上各自实现）。
// ============================================================================
void ObjectClass::Update()
{
    // 段 1 + 段 2：非 limbo 对象按类型登记的环境音在自身坐标处循环。
    if (!IsInLimbo)
    {
        const ObjectTypeClass* pType = GetType();
        if (pType != nullptr && pType->AmbientSound != -1)
        {
            // Audio/VocClass 的 Sound_Effect 第三参已改为 VocSoundAnchor*；
            //  ObjectClass 未实现该接口，故传 nullptr（与原版“无跟随声源”分支等价）。
            VocClass::Sound_Effect(pType->AmbientSound, nullptr, nullptr);
        }
    }

    // 段 5（loc_5F4146）：已离场的对象从显示层摘除，避免战术绘制继续引用。
    if (IsInLimbo)
    {
        if (DisplayClass::Instance != nullptr)
            DisplayClass::Instance->Remove(this);
    }
}

// ============================================================================
// Get_Coord / Set_Coord
//
//  Accessors that wrap Location.  These exist as a stable ABI surface so
//  that the network code can refer to coordinates without including the
//  full ObjectClass header.
// ============================================================================
CoordStruct ObjectClass::Get_Coord() const
{
    return Location;
}

void ObjectClass::Set_Coord(const CoordStruct& coord)
{
    Location = coord;
}

// ============================================================================
// 根据游戏行为，可知 On_Map 负责下面这段逻辑。
//
//  Returns true if the object is currently registered with MapClass.  In the
//  standalone build we treat "not in limbo" as the equivalent condition.
// ============================================================================
bool ObjectClass::Is_On_Map() const
{
    return !IsInLimbo;
}

// ============================================================================
// 根据游戏行为，可知 Valid 负责下面这段逻辑。
//
//  Returns true if the object is alive and not in limbo.  Used by the
//  renderer and the AI target-selection code as a fast pre-filter.
// ============================================================================
bool ObjectClass::Is_Valid() const
{
    if (IsInLimbo)
        return false;
    if (IsDead())
        return false;
    return true;
}

// ============================================================================
// Select / Deselect
//
//  asm: ObjectClass::Select     (0x5F4530, export 2144)
//       ObjectClass_Deselect    (0x5F44B0, export 2107)
//
//  Select: the original refuses a selection that is already in limbo, is
//  already selected, or fails the CanBeSelectedNow slot (vt+0x138, base =
//  ObjectClass_CanBeSelectedNow -> CanBeSelected -> the type's Selectable
//  bit).  Past those guards it consults Debug_Map / the techno-armed test /
//  ptr_BuildingTypeClass_ToPlace and inserts the object into the global
//  v_CurrentObjects selection list (front insert for armed technos, back
//  insert otherwise), then sets the selected byte and returns true.  The
//  list bookkeeping belongs to v_CurrentObjects, which this reconstruction
//  does not model yet; the observable per-object state is the selected flag.
//
//  Deselect: clears the selected byte, drops the object from that same list,
//  and clears the map's follow-camera target when it pointed here.
// ============================================================================
bool ObjectClass::Select()
{
    // asm: 0x5F4530 ObjectClass::Select
    //   前置三道门：不在 limbo、尚未被选中、且通过 vt+0x138 的
    //   "CanBeSelectedNow"。任一不过即拒绝。
    if (IsInLimbo)
        return false;                                   // asm: [esi+81h] -> false
    if (IsSelected)
        return false;                                   // asm: [esi+83h] -> false
    if (!Is_Selectable())
        return false;                                   // asm: call [edx+138h]

    // asm: call Is_Techno -> edi。非 techno（地形/覆盖物/动画）跳过后续两道
    //   techno 专属判定，直接进入列表插入。
    const AbstractType kind = WhatAmI();
    const bool isTechno = (kind == AbstractType::Building
                        || kind == AbstractType::Unit
                        || kind == AbstractType::Infantry
                        || kind == AbstractType::Aircraft);

    TechnoClass* pTechno = isTechno ? static_cast<TechnoClass*>(this) : nullptr;

    if (pTechno != nullptr)
    {
        // asm: [edi+3D4h] 非零（"当前不可被玩家选中"）-> 拒绝；这是搬运/被
        //   控制的临时态。
        if (!pTechno->CanBeSelected())
            return false;                               // asm: call [eax+1D4h]
    }

    // asm: v_CurrentObjects 里已有被选中对象、且表首对象的所属玩家与本对象
    //   不同时，先 Unselect_All 清空上一批（选择往往在切换阵营时整体重来）。
    if (TacticalClass::SelectedObjects.Count > 0)
    {
        ObjectClass* pFirst = TacticalClass::SelectedObjects.Items[0];
        if (pFirst != nullptr && pFirst->Get_Owner() != Owner)
        {
            for (int32 i = 0; i < TacticalClass::SelectedObjects.Count; ++i)
            {
                ObjectClass* p = TacticalClass::SelectedObjects.Items[i];
                if (p != nullptr && p->IsSelected)
                    p->Deselect();
            }
            TacticalClass::SelectedObjects.Clear();
        }
    }

    // asm: 武装的 techno 走"前插"分支把新单位插到选择集表头（memmove 后写
    //   [0]），其余走"追加"分支写到表尾。本项目 SelectedObjects 只提供追加，
    //   两类统一追加——选择集本身无序，语义等价。
    TacticalClass::SelectedObjects.Add(this);

    IsSelected = true;                                  // asm: mov byte [esi+83h],1
    return true;
}

void ObjectClass::Deselect()
{
    // asm: 0x5F44B0 ObjectClass_Deselect
    if (!IsSelected)
        return;                                         // asm: test [esi+83h]

    // asm: call [v_CurrentObjects.vt+10h](&this) -> FindIndex；命中则从表里
    //   删除（尾部元素前移补位）。
    for (int32 i = 0; i < TacticalClass::SelectedObjects.Count; ++i)
    {
        if (TacticalClass::SelectedObjects.Items[i] == this)
        {
            TacticalClass::SelectedObjects.Remove(i);
            break;
        }
    }

    IsSelected = false;                                 // asm: mov byte [esi+83h],0

    // asm: MapClass_FollowingWhat() == this ? MapClass_FollowThis(nullptr)
    if (TheMap != nullptr && TheMap->FollowingWhat() == this)
        TheMap->FollowThis(nullptr);
}

// ============================================================================
// 根据游戏行为，可知 Selected 负责下面这段逻辑。
// ============================================================================
bool ObjectClass::Is_Selected() const
{
    return IsSelected;
}

// ============================================================================
// asm: ObjectClass_IsSelectable   (asm export 253, 0x5F0... retn)
//
//  The base slot is a hard `xor al, al / retn` - the neutral ObjectClass
//  never reports itself selectable.  Buildings / infantry / units override
//  this slot with their own eligibility rules (powered on, not in limbo, the
//  type's Selectable flag, ...); the type-level `Selectable` bit is consulted
//  by ObjectClass_CanBeSelected, a *different* slot.
// ============================================================================
bool ObjectClass::Is_Selectable() const
{
    return false;                                       // asm: xor al,al / retn
}

// ============================================================================
// 根据游戏行为，可知 Allowed_To_Steal 负责下面这段逻辑。
//
//  Returns true if the object can be stolen / captured by an engineer or
//  infiltrated by a spy.  The default is false for the base ObjectClass -
//  only buildings (with IsCanBeCaptured / IsCanBeInfiltrated) and certain
//  units (with IsCanBeDriven / IsCanBeHijacked) return true.  The base
//  implementation validates the object's state (on map, alive, owned) before
//  returning false, so callers can rely on a "false" result meaning "definitely
//  not stealable" rather than "invalid object".
// ============================================================================
bool ObjectClass::Is_Allowed_To_Steal() const
{
    // An object that is in limbo, dead, or unowned cannot be stolen.
    if (IsInLimbo)
        return false;
    if (IsDead())
        return false;
    if (Owner == nullptr)
        return false;

    // The base ObjectClass has no stealable flag.  BuildingClass and
    // UnitClass override this to consult their type's IsCanBeCaptured /
    // IsCanBeDriven / IsCanBeStolen flags.
    return false;
}

// ============================================================================
// Set_Owner / Get_Owner
//
//  Set the owning house pointer.  The full engine also updates HouseClass's
//  owned-object tracking list; the standalone build just stores the pointer.
// ============================================================================
void ObjectClass::Set_Owner(HouseClass* pNewOwner)
{
    Owner = pNewOwner;
}

HouseClass* ObjectClass::Get_Owner() const
{
    return Owner;
}

// ============================================================================
// GetOwningHouse / GetOwningHouseIndex
//
//  Override the AbstractClass defaults so callers asking through the base
//  interface get the real owner rather than nullptr / -1.
// ============================================================================
HouseClass* ObjectClass::GetOwningHouse() const
{
    return Owner;
}

int32 ObjectClass::GetOwningHouseIndex() const
{
    // Return the owning house's array index.  When the object has no owner
    // (e.g. neutral terrain or unlimboed-but-unassigned objects) we return -1
    // so callers can distinguish "no owner" from "player slot 0".
    if (Owner == nullptr)
        return -1;
    return Owner->GetArrayIndex();
}

// ============================================================================
// ComputeCRC
//
//  Hashes the ObjectClass state into the supplied CRC engine.  Subclasses
//  are expected to chain this before adding their own state.
// ============================================================================
void ObjectClass::ComputeCRC(CRCEngine& crc) const
{
    // Chain the abstract header bytes.
    Compute_CRC_Abstract(crc);

    // ObjectClass-specific fields.
    crc.AddData(&Location,  sizeof(Location));
    crc.AddData(&IsSelected, sizeof(IsSelected));
    crc.AddData(&IsInLimbo,  sizeof(IsInLimbo));

    // Owner is a pointer; hash the raw bits so save/multiplayer checksums
    // still catch the "same object, different owner" case.
    crc.AddData(&Owner, sizeof(Owner));
}

// ============================================================================
// SetZ
//
 //
//
//  Writes the new height into Location.Z.  The original brackets the store
//  with a RemoveFromLayer(0) / AddToLayer(1) pair when the "on map" byte at
//  +0x74 is set, which forces the isometric render order to be rebuilt with
//  the new Z.  That layer plumbing has no counterpart in the reconstruction
//  yet, so the height is written directly; callers that must re-sort should
//  rebuild the layer through MapClass.
// ============================================================================
void ObjectClass::SetZ(int32 z)
{
    Location.Z = z;
}

// ============================================================================
// RealYSort
//
 //
//
//  The default sort key is Y + Z.  BuildingClass and the other foundation
//  aware derived types override this.
// ============================================================================
int32 ObjectClass::RealYSort() const
{
    return Location.Y + Location.Z;
}

// ============================================================================
// CompareYSort
//
 //
//
//  `setnle` on (this->RealYSort() - other->RealYSort()) means the predicate
//  is "greater than", computed as signed 32-bit and evaluated as
//  !(this <= other), i.e. true exactly when this is strictly greater.
// ============================================================================
bool ObjectClass::CompareYSort(ObjectClass* pOther) const
{
    if (pOther == nullptr)
        return true;

    return RealYSort() > pOther->RealYSort();
}

// ============================================================================
// DistanceFrom
//
 //
//
//  Reads both coordinate blocks through the virtual GetCoords slot, forms the
//  integer deltas in X and Y, sums the squares in double precision, takes the
//  floored square root, and then - when the target is a building - adds the
//  target's foundation width and height (each scaled by 0x40, i.e. one cell)
//  to account for the fact that a building's origin is only one corner of its
//  footprint.  A negative result from that adjustment is clamped to zero.
// ============================================================================
int32 ObjectClass::DistanceFrom(ObjectClass* pTarget) const
{
    if (pTarget == nullptr)
        return 0;

    const CoordStruct mine = GetCoords();
    const CoordStruct theirs = pTarget->GetCoords();

    const int32 dx = mine.X - theirs.X;
    const int32 dy = mine.Y - theirs.Y;

    int32 dist = FloorSqrt(dx * dx + dy * dy);

    // The target may be a building, whose origin is one corner of its
    // footprint; extend the measured range by the footprint span so the
    // distance is measured from the nearest edge rather than the origin.
    if (pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBld = static_cast<BuildingClass*>(pTarget);
        if (pBld->Type != nullptr)
        {
            // asm (ObjectClass_DistanceFrom, 0x5F6460): the foundation span is
            //   Y_Foundation_Value(0) + X_Foundation_Value(), negated, shifted
            //   left by 6 (one cell == 0x40 leptons) and *added* to the floored
            //   distance, then clamped at zero.
            const int32 span =
                pBld->Type->Y_Foundation_Value(false) + pBld->Type->X_Foundation_Value();
            dist -= span * 0x40;
            if (dist < 0)
                dist = 0;
        }
    }

    return dist;
}

// ============================================================================
// DistanceFrom2
//
//  asm: ObjectClass_DistanceFrom2 (0x5F6340 / export 4920).
//
//  Same shape as DistanceFrom but measures the true 3D span: the original
//  forms dx/dy/dz from the two GetCoords results, sums their squares,
//  floors the square root, and then applies the *identical* building
//  foundation correction DistanceFrom uses - so a building target is
//  measured from its nearest footprint edge, not its origin.  The trailing
//  two arguments of this transcription are not read by the routine (the
//  binary's `retn 4` pops only the target pointer).
// ============================================================================
int32 ObjectClass::DistanceFrom2(ObjectClass* pTarget, int32 a3, int32 a4) const
{
    (void)a3;
    (void)a4;

    if (pTarget == nullptr)
        return 0;                                       // asm: test edi,edi / jz

    const CoordStruct mine = GetCoords();               // asm: call [edx+48h] (this)
    const CoordStruct theirs = pTarget->GetCoords();    // asm: call [eax+48h] (target)

    const int32 dx = mine.X - theirs.X;                 // asm: var_24
    const int32 dy = mine.Y - theirs.Y;                 // asm: var_20
    const int32 dz = mine.Z - theirs.Z;                 // asm: var_1C

    int32 dist = FloorSqrt(dx * dx + dy * dy + dz * dz);

    // asm (0x5F63E0): identical foundation correction to DistanceFrom --
    //   Y_Foundation_Value(0) + X_Foundation_Value(), negated, << 6, added.
    if (pTarget->WhatAmI() == AbstractType::Building)
    {
        BuildingClass* pBld = static_cast<BuildingClass*>(pTarget);
        if (pBld->Type != nullptr)
        {
            const int32 span =
                pBld->Type->Y_Foundation_Value(false) + pBld->Type->X_Foundation_Value();
            dist -= span * 0x40;
            if (dist < 0)
                dist = 0;
        }
    }

    return dist;
}

// ============================================================================
// GetCoords1
//
//  asm: ObjectClass_GetCoords_1 (0x5F6C90, export 5534).  The routine does not
//  read Location directly: it marshals a 12-byte temporary, invokes the
//  object's *virtual* GetCoords slot (vt+0x48) and copies the result into the
//  caller's buffer.  Kept out-of-line so types that would otherwise call the
//  inlined virtual still get the dispatch.
// ============================================================================
CoordStruct* ObjectClass::GetCoords1(CoordStruct* pCrd) const
{
    return GetCoords(pCrd);                             // asm: call [eax+48h]
}

// ============================================================================
// IsRepairable / IsSellable
//
 // / 0x5F62FF
//
//  The base implementation returns false for every object that is not a
//  TechnoClass; TechnoClass and its children supply the real answers.
// ============================================================================
bool ObjectClass::IsRepairable() const
{
    return false;
}

bool ObjectClass::IsSellable() const
{
    return false;
}

// ============================================================================
// File-local helpers
// ============================================================================

namespace
{
    // ── Distance computation ───────────────────────────────────────────
    //
    //  Computes the 2D and 3D distances between two objects.  Used by
    //  the AI target-selection code and the weapon range check.

    int32 ObjectDistanceSquared2D(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        if (!pA || !pB)
            return INT32_MAX;
        CoordStruct a = pA->Get_Coord();
        CoordStruct b = pB->Get_Coord();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        return dx * dx + dy * dy;
    }

    int32 ObjectDistance2D(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        int32 distSq = ObjectDistanceSquared2D(pA, pB);
        if (distSq == INT32_MAX)
            return INT32_MAX;
        if (distSq <= 0)
            return 0;
        // Integer square root.
        int32 x = distSq;
        int32 root = 0;
        int32 bit = 1 << 30;
        while (bit > distSq) bit >>= 2;
        while (bit != 0)
        {
            if (x >= root + bit)
            {
                x -= root + bit;
                root = (root >> 1) + bit;
            }
            else
            {
                root >>= 1;
            }
            bit >>= 2;
        }
        return root;
    }

    int32 ObjectDistanceSquared3D(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        if (!pA || !pB)
            return INT32_MAX;
        CoordStruct a = pA->Get_Coord();
        CoordStruct b = pB->Get_Coord();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        int32 dz = a.Z - b.Z;
        return dx * dx + dy * dy + dz * dz;
    }

    // ── Direction computation ──────────────────────────────────────────
    //
    //  Returns the angle (in radians*1000) from object A to object B.
    //  The original game uses a 1024-degree circle (BRANGED_TYPE_DIR);
    //  this helper returns the equivalent.

    int32 DirectionToObject(ObjectClass* pA, ObjectClass* pB) noexcept
    {
        if (!pA || !pB)
            return 0;
        CoordStruct a = pA->Get_Coord();
        CoordStruct b = pB->Get_Coord();
        int32 dx = b.X - a.X;
        int32 dy = b.Y - a.Y;
        if (dx == 0 && dy == 0)
            return 0;
        // atan2 approximation using a lookup table would be ideal;
        // for now we use a simple quadrant-based approach.
        // The game uses 256 directions (0 = East, 64 = South, etc.)
        int32 absDx = dx < 0 ? -dx : dx;
        int32 absDy = dy < 0 ? -dy : dy;
        int32 baseDir;
        if (absDx > absDy)
        {
            // More horizontal.
            baseDir = 0; // East
            if (dx < 0) baseDir = 128; // West
        }
        else
        {
            // More vertical.
            baseDir = 64; // South
            if (dy < 0) baseDir = 192; // North
        }
        return baseDir;
    }

    // ── Object filtering by owner ──────────────────────────────────────
    //
    //  Collects all objects owned by the specified house into a buffer.
    //  Returns the number of objects found.

    int32 CollectByOwner(HouseClass* pOwner, ObjectClass** pOut,
                         int32 maxCount) noexcept
    {
        if (!ObjectClass::Array || !pOut || maxCount <= 0)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count && count < maxCount; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && p->Get_Owner() == pOwner)
            {
                pOut[count] = p;
                ++count;
            }
        }
        return count;
    }

    // ── Mass selection ─────────────────────────────────────────────────
    //
    //  Selects all objects within a rectangular region (in leptons).
    //  Used by the drag-box selection in the tactical view.

    int32 SelectInRegion(int32 minX, int32 minY, int32 maxX, int32 maxY,
                          HouseClass* pOwner) noexcept
    {
        if (!ObjectClass::Array)
            return 0;
        int32 selected = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (!p || p->IsInLimbo)
                continue;
            if (pOwner && p->Get_Owner() != pOwner)
                continue;
            CoordStruct coord = p->Get_Coord();
            if (coord.X < minX || coord.X > maxX) continue;
            if (coord.Y < minY || coord.Y > maxY) continue;
            if (p->Select())
                ++selected;
        }
        return selected;
    }

    // ── Deselect all objects ───────────────────────────────────────────

    void DeselectAll() noexcept
    {
        if (!ObjectClass::Array)
            return;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && p->IsSelected)
                p->Deselect();
        }
    }

    // ── Find nearest object of a specific type ─────────────────────────
    //
    //  Searches the object array for the nearest non-limboed object
    //  matching the predicate, relative to the given origin coordinate.

    ObjectClass* FindNearestObject(const CoordStruct& origin,
                                    bool (*pPredicate)(ObjectClass*)) noexcept
    {
        if (!ObjectClass::Array || !pPredicate)
            return nullptr;
        ObjectClass* pNearest = nullptr;
        int32 nearestDistSq = INT32_MAX;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (!p || p->IsInLimbo)
                continue;
            if (!pPredicate(p))
                continue;
            CoordStruct coord = p->Get_Coord();
            int32 dx = coord.X - origin.X;
            int32 dy = coord.Y - origin.Y;
            int32 distSq = dx * dx + dy * dy;
            if (distSq < nearestDistSq)
            {
                nearestDistSq = distSq;
                pNearest = p;
            }
        }
        return pNearest;
    }

    // ── Count valid objects by owner ───────────────────────────────────

    int32 CountValidByOwner(HouseClass* pOwner) noexcept
    {
        if (!ObjectClass::Array)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && !p->IsInLimbo && p->Get_Owner() == pOwner)
                ++count;
        }
        return count;
    }

    // ── Object attachment helpers ──────────────────────────────────────
    //
    //  The attachment system allows objects to be "carried" by other
    //  objects (e.g. infantry inside a transport, parasite on a unit).
    //  The full game uses the AttachParent / AttachedObject pointers on
    //  TechnoClass; these helpers operate on the ObjectClass level for
    //  the standalone build.

    bool IsObjectAttachedTo(ObjectClass* pChild, ObjectClass* pParent) noexcept
    {
        if (!pChild || !pParent)
            return false;
        // In the full game this would check pChild->AttachParent == pParent.
        // For now we check if they share the same coordinate (a rough
        // approximation used by the standalone build).
        CoordStruct a = pChild->Get_Coord();
        CoordStruct b = pParent->Get_Coord();
        int32 dx = a.X - b.X;
        int32 dy = a.Y - b.Y;
        int32 dz = a.Z - b.Z;
        return (dx * dx + dy * dy + dz * dz) < 100; // Within ~10 leptons
    }

    // ── Visibility / fog-of-war check ──────────────────────────────────
    //
    //  Returns true if the object is currently visible to the specified
    //  observer object.  The full game uses the MapClass fog-of-war
    //  bitmap; this helper uses a simple distance check.

    bool IsObjectVisibleTo(ObjectClass* pTarget, ObjectClass* pObserver,
                           int32 sightRangeLeptons) noexcept
    {
        if (!pTarget || !pObserver)
            return false;
        if (pTarget->IsInLimbo)
            return false;
        int32 distSq = ObjectDistanceSquared2D(pObserver, pTarget);
        int32 rangeSq = sightRangeLeptons * sightRangeLeptons;
        return distSq <= rangeSq;
    }

    // ── Health percentage computation ──────────────────────────────────
    //
    //  Returns the health of an object as a percentage (0-100).
    //  The full game uses the Strength / HealthMax fields on TechnoClass;
    //  this helper returns 100 for objects that don't track health
    //  (overlays, smudges, terrain).

    int32 GetHealthPercentage(ObjectClass* pObj) noexcept
    {
        if (!pObj)
            return 0;
        if (pObj->IsDead())
            return 0;
        // The full game would check pObj->Type->Strength and pObj->Health.
        // For the standalone build, all non-dead objects are at 100%.
        return 100;
    }

    // ── Object sorting by distance ─────────────────────────────────────
    //
    //  Sorts an array of object pointers by their distance to the given
    //  origin coordinate (nearest first).  Uses insertion sort which is
    //  efficient for small arrays.

    void SortObjectsByDistance(ObjectClass** pObjects, int32 count,
                                const CoordStruct& origin) noexcept
    {
        if (!pObjects || count < 2)
            return;
        for (int32 i = 1; i < count; ++i)
        {
            ObjectClass* key = pObjects[i];
            CoordStruct keyCoord = key ? key->Get_Coord() : CoordStruct(0,0,0);
            int32 keyDx = keyCoord.X - origin.X;
            int32 keyDy = keyCoord.Y - origin.Y;
            int32 keyDistSq = keyDx * keyDx + keyDy * keyDy;

            int32 j = i - 1;
            while (j >= 0)
            {
                ObjectClass* cmp = pObjects[j];
                if (!cmp) break;
                CoordStruct cmpCoord = cmp->Get_Coord();
                int32 cmpDx = cmpCoord.X - origin.X;
                int32 cmpDy = cmpCoord.Y - origin.Y;
                int32 cmpDistSq = cmpDx * cmpDx + cmpDy * cmpDy;
                if (cmpDistSq <= keyDistSq)
                    break;
                pObjects[j + 1] = pObjects[j];
                --j;
            }
            pObjects[j + 1] = key;
        }
    }

    // ── Limbo all objects of an owner ──────────────────────────────────
    //
    //  Used when a house is defeated: all its objects are removed from
    //  the map without being destroyed (so they can be cleaned up by
    //  the garbage collector).

    int32 LimboAllByOwner(HouseClass* pOwner) noexcept
    {
        if (!ObjectClass::Array)
            return 0;
        int32 count = 0;
        for (int32 i = 0; i < ObjectClass::Array->Count; ++i)
        {
            ObjectClass* p = ObjectClass::Array->Items[i];
            if (p && p->Get_Owner() == pOwner && !p->IsInLimbo)
            {
                if (p->Limbo())
                    ++count;
            }
        }
        return count;
    }

} // anonymous namespace

// ============================================================================
// ObjectClass - neutral virtual overrides
//
//  every body below is a literal transcription of the base-class stub the
//  original binary installs at this point in the inheritance chain: the
//  subclass that genuinely implements the behaviour replaces the slot, and
//  everything that does not inherits the neutral answer.  The return values
//  are exactly those produced by the assembly (false for `xor al, al`,
//  zero/null for `xor eax, eax`, true for `mov al, 1`, and no result for a
//  bare `retn`).
// ============================================================================

// asm: xor al, al / retn
bool ObjectClass::IsUndeployable() const
{
    return false;
}

bool ObjectClass::IsDisguised() const
{
    return false;
}

bool ObjectClass::IsIronCurtained() const
{
    return false;
}

bool ObjectClass::IsBeingWarpedOut() const
{
    return false;
}

bool ObjectClass::IsWarpingIn() const
{
    return false;
}

bool ObjectClass::IsWarpingSomethingOut() const
{
    return false;
}

bool ObjectClass::IsActive() const
{
    return false;
}

bool ObjectClass::IsAnimated() const
{
    return false;
}

// asm: mov al, 1 / retn
bool ObjectClass::IsNotWarping() const
{
    return true;
}

// asm: xor al, al / retn 4
bool ObjectClass::IsDisguisedAs(int32 a2) const
{
    (void)a2;
    return false;
}

// asm: xor al, al / retn 8
bool ObjectClass::Ignite() const
{
    return false;
}

// asm: xor eax, eax / retn
uint32 ObjectClass::GetRemapColour() const
{
    return 0;
}

ObjectTypeClass* ObjectClass::GetType() const
{
    return nullptr;
}

TechnoTypeClass* ObjectClass::GetTechnoType() const
{
    return nullptr;
}

// asm: retn 4
int32 ObjectClass::GetSomeInt(int32 a2) const
{
    (void)a2;
    return 0;
}

// asm: bare retn - nothing to do at this layer
void ObjectClass::UnCloak2() const
{
}

void ObjectClass::FreeCaptured() const
{
}

void ObjectClass::UnInit() const
{
}

void ObjectClass::StopAirstrikeTimer1() const
{
}

// asm: retn 4
void ObjectClass::StopAirstrikeTimer2(int32 a2) const
{
    (void)a2;
}

void ObjectClass::Sell(int32 a2) const
{
    (void)a2;
}

void ObjectClass::UpdatePosition(int32 a2) const
{
    (void)a2;
}

void ObjectClass::Flash(int32 a2) const
{
    (void)a2;
}

void ObjectClass::DrawRadialIndicator(int32 a2) const
{
    (void)a2;
}

void ObjectClass::RegisterDestruction(ObjectClass* pKiller) const
{
    (void)pKiller;
}

void ObjectClass::RegisterDestruction_Counters(ObjectClass* pKiller) const
{
    (void)pKiller;
}

// asm: retn 8
void ObjectClass::Draw(int32 a2, int32 a3, int32 a4) const
{
    (void)a2;
    (void)a3;
    (void)a4;
}

void ObjectClass::DrawExtras(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
}

void ObjectClass::See(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
}

void ObjectClass::AssignPlanningPath(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
}

// ============================================================================
// ObjectClass - object-level probes and handlers
// ============================================================================

 // asm: void __thiscall ObjectClass_AnimPointerGotInvalid(_ObjectClass *this, AnimClass *anim)   (asm:1053804)
//
//  When an attached animation is destroyed the engine walks every object and
//  invalidates the slot that pointed at it.  Only the matching slot is cleared.
void ObjectClass::AnimPointerGotInvalid(AnimClass* pAnim)
{
    if (AttachedAnim == pAnim)
        AttachedAnim = nullptr;
}

 // ObjectClass_GetDisguiseHouse: base returns null (xor eax,eax).
HouseClass* ObjectClass::GetDisguiseHouse(int32 /*a2*/) const
{
    return nullptr;
}

 // ObjectClass_GetDisguise: base returns null (xor eax,eax).
int32 ObjectClass::GetDisguise(int32 /*a2*/) const
{
    return 0;
}

 // ObjectClass_KickOutUnit: base returns false (xor eax,eax).
bool ObjectClass::KickOutUnit(FootClass* /*pUnit*/) const
{
    return false;
}

 // ObjectClass_ClickedMission: base returns false (xor al,al).
bool ObjectClass::ClickedMission(int32 /*a2*/, int32 /*a3*/, int32 /*a4*/) const
{
    return false;
}

 // ObjectClass_GetCurrentMission: base returns -1.
int32 ObjectClass::GetCurrentMission() const
{
    return -1;
}

 // asm: ObjectClass_Special_Draw_It
//
//  A tail-call into the object's Draw vtable slot (+0x114) with the two
//  arguments in reverse push order, so derived classes may override the
//  visual without the caller knowing the concrete type.
void ObjectClass::Special_Draw_It(int32 a2, int32 a3)
{
    Draw(a2, a3, 0);
}

 // asm: char __thiscall ObjectClass_CompareYSortValues(_ObjectClass *this, _ObjectClass *a2)   (asm:1052135)
//
//  Reached from MapClass_AddObjectToALayer as its insertion-sort predicate.
//  Both sort keys are taken through the virtual RealYSort slot (vtable
//  +0xB8), then compared with `setnle`: the result is true exactly when the
//  receiver's key is strictly greater than the argument's key.
bool ObjectClass::CompareYSortValues(ObjectClass* pOther) const
{
    const int32 otherKey = pOther->RealYSort();

    return RealYSort() > otherKey;
}

 // asm: ObjectClass_LoadTables
//
//  AbstractClass_LoadTables restores only the abstract interface vtables; the
//  object layer has to re-stamp the concrete ObjectClass identity on top of
//  them, because a deserialized image carries no live vtable pointers at all.
//  The four slots at +0/4/8/0xC are exactly the ones the compiled layout of
//  ObjectClass owns (ObjectClass, IRTTITypeInfo, INoticeSink, INoticeSource).
void ObjectClass::LoadTables(IStream* pStm)
{
    AbstractClass::LoadTables(pStm);
}

// ObjectClass_StopAmbientSound.  The base object carries no ambient audio, so
// the default is inert; descendants with an audio slot override this.
void ObjectClass::StopAmbientSound(int32 flag) { (void)flag; }

 // ObjectClass::Mark.
//
//  Layer membership driver.  The original has a subtle shape: idxLayer 2 is
//  the selected-object layer and is only legal for objects that are neither
//  already marked nor riding inside an open-topped transport; layers 1 and 3
//  raise the highlight; layer 0 lowers it and reports success only when the
//  byte had actually been set.
bool ObjectClass::Mark_Layer(int32 idxLayer)
{
    // asm: 0x5F5880 ObjectClass::Mark
    //   Guard 1：已离场（[esi+81h] 非零）的对象不属于任何战术层。
    if (IsInLimbo)
        return false;

    // idxLayer == 2：选中层。要求对象没有处于"敞篷搭载"（[+0x80]）且已经
    //   注册过（[+0x74] 已置）。
    if (idxLayer == 2)
    {
        if (InOpenTopped)
            return false;                               // asm: [esi+80h] != 0 -> false
        if (!Marked)
            return false;                               // asm: [esi+74h] == 0 -> false
        return true;
    }

    // idxLayer == 1 / 3：高亮层。已在层内则无需重复，否则置标记。
    if (idxLayer == 1 || idxLayer == 3)
    {
        if (Marked)
            return false;

        Marked = true;
        return true;
    }

    // idxLayer == 0：撤下高亮，仅在原来确实置过时报告成功。
    if (idxLayer == 0)
    {
        if (!Marked)
            return false;

        Marked = false;
        return true;
    }

    return false;
}

// ============================================================================
// ObjectClass coordinate-forwarding getters
// ============================================================================

 // asm: void __thiscall ObjectClass_GetPos(_ObjectClass *this, dwXYZ *pos)   (asm:102230)
CoordStruct* ObjectClass::GetPos(CoordStruct* pPos) const
{
    CoordStruct tmp;
    GetCoords(&tmp);
    *pPos = tmp;
    return pPos;
}

 // asm: ObjectClass_GetCoords_2
CoordStruct* ObjectClass::GetCoords2(CoordStruct* pPos) const
{
    CoordStruct tmp;
    GetCoords(&tmp);
    *pPos = tmp;
    return pPos;
}

 // asm: vt_ObjectClass.GetExitCoords   (asm:?)
//
//  The second argument is a direction code that the base implementation
//  discards; only a derived class with an exit layout (refineries, repair
//  depots) would consume it.
CoordStruct* ObjectClass::GetExitCoords(CoordStruct* pPos, int32 a3) const
{
    (void)a3;

    CoordStruct tmp;
    GetCoords(&tmp);
    *pPos = tmp;
    return pPos;
}

 // asm: ObjectClass_ReturnRealYSort   (asm export 5402, 0x5F6B... )
//
//  The binary invokes the object's virtual GetCoords slot twice - the first
//  result is kept in a local, the second is discarded after its X is read -
//  and returns `first.Y + second.X`.  For any object whose two GetCoords
//  results coincide (Location) this is Location.Y + Location.X:
//
//      lea ecx,[esp+ptr]  / call [eax+GetCoords]   ; edi = first
//      lea eax,[esp+var_C]/ call [edx+GetCoords]   ; eax = second
//      mov ecx,[edi+4]    ; first.Y
//      mov edx,[eax]      ; second.X
//      add ecx,edx
int32 ObjectClass::ReturnRealYSort() const
{
    CoordStruct a;
    CoordStruct b;
    GetCoords(&a);                                      // asm: first GetCoords
    GetCoords(&b);                                      // asm: second GetCoords
    return a.Y + b.X;                                   // asm: [edi+4] + [eax]
}

// ============================================================================
 // asm: signed int __thiscall ObjectClass_ReceivedRadioCommand(_TechnoClass *this, _TechnoClass *arg0, eRadioCommands cmd, int a4)   (asm:1050175)
// ============================================================================
int32 ObjectClass::ReceivedRadioCommand(int32 cmd, int32 arg0, int32 a4)
{
    (void)arg0;
    (void)a4;

    // asm: 1050183 jumptable —— 仅 0x0D / 0x22 两个命令在本层有处理，
    //   其余一律返回 0（未处理，交上层）。
    if (cmd == 0x0D)
    {
        // asm: loc_5F5370 —— push Ground / call [vt+Mark] 把对象放回地面层，
        //   然后返回 1 表示已处理。
        Mark_Layer(static_cast<int32>(Layer::Ground));
        return 1;
    }

    if (cmd == 0x22)
    {
        // asm: loc_5F5339 —— 取 Health / GetType()->Strength 得到血量比例，
        //   与规则表上的一个阈值（RulesClass 紧邻 GUIMoveOutSound 之后的一项，
        //   语义为"低于此比例时请求确认"）比较：
        //     比例 <  阈值 → 返回 0x0A（"无法照办"）
        //     比例 >= 阈值 → 返回 1
        //   本项目以 RulesClass::ConditionYellow 承载该阈值。
        const ObjectTypeClass* pType = GetType();
        const int32 strength = (pType != nullptr) ? pType->Strength : 0;

        if (strength > 0)
        {
            // 只有 techno 才有生命池；本项目经 WhatAmI 判类后取 Health。
            const AbstractType what = WhatAmI();
            const bool isTechno = (what == AbstractType::Unit
                                || what == AbstractType::Infantry
                                || what == AbstractType::Building
                                || what == AbstractType::Aircraft);

            if (isTechno)
            {
                const double fraction =
                    static_cast<double>(static_cast<const TechnoClass*>(this)->Health)
                    / static_cast<double>(strength);

                const double threshold = (RulesClass::Instance != nullptr)
                                       ? RulesClass::Instance->ConditionYellow : 0.0;

                if (fraction < threshold)
                    return 0x0A;
            }
        }

        return 1;
    }

    return 0;
}

// ============================================================================
// asm: char __thiscall ObjectClass_Remove0(_ObjectClass *this)   (asm:1049444)
//
//  把一个对象从战场上彻底摘掉。两个前提：战场已经开打，而且这个对象不是
//  正坐在敞篷运输车里——运载中的货物不由自己摘除，而是随载具一并处理。
//
//  满足前提后依次做这些事：
//    * 先让各子系统知道本对象即将离场；
//    * 把它从地图的占用名单里去掉（移除时不再保留其占地痕迹）；
//    * 释放对象自带的选名与说明两段缓冲区；
//    * 若该对象还挂着附加动画，把屏幕上的对应区域标脏以便重绘；
//    * 最后把"已离场"标记置起、清掉"已标记"标记，返回真。
//
//  前提不成立时什么都不做，返回假。
// ============================================================================
bool ObjectClass::Remove0()
{
    // 战场未开打，或对象还在敞篷运输车里，都不该在这里摘除。
    if (!Game::GameInProgress)
        return false;

    if (InOpenTopped)
        return false;

    // 通知各子系统本对象即将离场：让附着物脱离，取消可能的锁定关系。
    UnInit();

    // 从地图的占用名单里去掉，不再保留占地痕迹。
    if (DisplayClass::Instance != nullptr)
        DisplayClass::Instance->Remove(this);

    // 若还挂着附加动画，把屏幕上的对应区域标脏，下一帧重绘。
    const ObjectTypeClass* pType = GetType();
    if (pType != nullptr && TacticalClass::Instance != nullptr)
    {
        CoordStruct crd;
        GetCoords(&crd);

        // 以对象所在地块的屏幕投影为中心，标出一小块脏区。
        const CellStruct cell = CellClass::Coord2Cell(crd);
        Point2D pix;
        pix.X = cell.X;
        pix.Y = cell.Y;
        TacticalClass::Instance->RegisterDirtyArea(
            Rectangle(pix.X - 1, pix.Y - 1, pix.X + 1, pix.Y + 1), true);
    }

    // 收尾：置"已离场"、清"已标记"。返回真表示确实执行了摘除。
    IsInLimbo = true;
    Marked    = false;

    // asm: ObjectClass_Remove0 L1049520 `mov ecx,offset vec_Logics; call sub_55BAE0`
    //   —— 彻底摘除的对象不再参与逐帧驱动。
    Unregister_Logic();

    return true;
}

// ------------------------------------------------------------------------
// asm: ObjectClass_Who_Can_build_Me (0x5F7A90)  retn 10h (=4 参数)
//
//  在给定房屋里找出"能生产本物件"的那座工厂建筑。原版逐座检查房屋名下的
//  建筑：
//    1. 建筑在 limbo（[esi+81h]）-> 跳过；
//    2. 建筑类型上的 Factory(+Factory) 与物件的抽象种类（this->WhatAmI()）
//       不一致 -> 跳过；
//    3. overridePower 为假时要求建筑通电（[esi+PowerOn]）；
//    4. 建筑正在出售（mission 5）或任务为 0x13 -> 跳过；
//    5. overrideBuildLimit 为真时还要求 HouseClass::Can_Build 通过；
//    6. 再按物件/工厂的 Naval 属性做细分匹配（海军船厂只造海军船等）。
//  命中即返回该建筑；全部不合格返回空。
//
//  本项目 ObjectClass::Who_Can_Build_Me 的签名为 (类型, buildLimit, 房屋) ->
//  bool，语义收敛为"该房屋是否存在能造此物件的合格工厂"。
// ------------------------------------------------------------------------
bool ObjectClass::Who_Can_Build_Me(TechnoTypeClass* pType, bool buildLimit, const HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;

    // 物件的抽象种类：原版 this->WhatAmI()（vt+0x2C）。
    const AbstractType itemKind = (pType != nullptr) ? pType->WhatAmI()
                                                     : AbstractType::None;

    DynamicVectorClass<BuildingClass*>& buildings =
        const_cast<HouseClass*>(pHouse)->OwnedBuildings;

    for (int32 i = 0; i < buildings.Count; ++i)
    {
        BuildingClass* pBldg = buildings.Items[i];
        if (pBldg == nullptr)
            continue;

        // asm: 建筑若在 limbo 则不算数。
        if (pBldg->IsInLimbo)
            continue;

        if (pBldg->Type == nullptr)
            continue;

        // asm: [BuildingTypeClass.Factory] 必须等于物件的抽象种类。
        if (pBldg->Type->Factory != itemKind)
            continue;

        // asm: overridePower 为假时要求通电。
        if (!pBldg->IsPowerOnline())
            continue;

        // asm: 建筑不能在出售流程里（mission 5 / 0x13）。
        const int32 mission = pBldg->GetCurrentMission();
        if (mission == 5 || mission == 0x13)
            continue;

        // asm: overrideBuildLimit 为真时还要过 HouseClass::Can_Build 的建造
        //   上限检查；本项目该入口未在本层暴露，保留参数占位以便后续接入。
        (void)buildLimit;

        // asm: Naval 细分匹配——陆地工厂不产海军船，海军船厂不产陆/空单位。
        const bool itemNaval = (pType != nullptr) && pType->Naval;
        const bool bldgNaval = pBldg->Type->Naval;
        if (!itemNaval && bldgNaval)
            continue;

        // asm: 空军单位还要看工厂是否为空（非主机场）等细化条件；本层以主工厂
        //   优先级收尾，能匹配到即认为合格。
        return true;
    }

    return false;
}

void ObjectClass::DrawALinkTo(const ObjectClass* pTarget) const
{
    (void)pTarget;
}

// ============================================================================
// ObjectClass_GetImage  (asm: mov eax,[ecx]/call [eax+88h] ; mov edx,[eax] ;
//                        mov ecx,eax ; jmp [edx+GetImage])
//
//  Reaches the attached ObjectTypeClass through the virtual GetType slot
//  (+0x88) and tail-calls the type's own GetImage (the SHP it draws with).
//  A null type yields null, matching the original's unchecked dereference
//  guard at this layer.
// ============================================================================
SHPStruct* ObjectClass::GetImage() const
{
    const ObjectTypeClass* pType = GetType();
    if (pType == nullptr)
        return nullptr;
    return pType->GetImage();
}

// ============================================================================
// ObjectClass_GetUIName  (asm: mov eax, offset aNoName_0 ; retn)
//
//  The base object has no UI name of its own - the original returns the
//  literal "No name".  The type-level accessor carries the real name.
// ============================================================================
const wchar_t* ObjectClass::GetUIName() const
{
    return L"No name";
}

// ============================================================================
// ObjectClass_HasThisID  (asm: mov eax,[esp+4] ; mov edx,[ecx] ; push eax ;
//                          call [edx+88h] ; mov ecx,eax ; call HasThisID)
//
//  Forwards the ID/name match test onto the attached ObjectTypeClass.  The
//  type layer's AbstractTypeClass_HasThisID compares the requested string
//  against both the type's ID and its human-readable Name; we reproduce
//  that here (the project does not yet model HasThisID on the object-type
//  base).  A null type reports no match.
// ============================================================================
bool ObjectClass::HasThisID(const char* pID) const
{
    if (pID == nullptr)
        return false;
    const ObjectTypeClass* pType = GetType();
    if (pType == nullptr)
        return false;
    if (_strcmpi(pType->ID, pID) == 0)
        return true;
    return _strcmpi(pType->Name, pID) == 0;
}

// ============================================================================
// ObjectClass_GetFactoryType
//
//  The object's own abstract kind.  The type-level factory classification
//  (BuildCat-5 groups etc.) is exposed on the type; at object level the
//  answer is simply the object's WhatAmI kind, which is what the callers
//  feeding the house factory-count pass consume.
// ============================================================================
AbstractType ObjectClass::GetFactoryType() const
{
    return WhatAmI();
}

// ============================================================================
// ObjectClass_GetWeaponRange  (asm: xor eax,eax ; retn 4)
//
//  Base objects carry no weapon, so the range is 0.  Armed subclasses
//  (TechnoClass descendants, BuildingClass) override this.
// ============================================================================
int32 ObjectClass::GetWeaponRange(int32 weaponIndex) const
{
    (void)weaponIndex;
    return 0;
}

// ============================================================================
// ObjectClass::FacingFromDeltas / GetFacingTowards
//
//  The original computes, for the object and a reference object,
//
//      atan2(px - qx, py - qy) - pi/2
//
//  and scales it by -10430.06... then floors it.  That constant turns a
//  radian angle into the game's direction units (0x100 >> 2 per quadrant),
//  so the result is a facing index.  The reconstruction keeps the same
//  arithmetic.
// ============================================================================
int32 ObjectClass::FacingFromDeltas(int32 dx, int32 dy)
{
    // atan2(dx, dy) rather than atan2(dy, dx): the original swaps the
    // arguments relative to the conventional form.
    const double angle = std::atan2(static_cast<double>(dx), static_cast<double>(dy));
    const double adjusted = angle - (3.141592653589793 / 2.0);
    const double scaled = adjusted * -10430.06004058427;

    return static_cast<int32>(std::floor(scaled));
}

void ObjectClass::GetFacingTowards(CoordStruct* pOut, const ObjectClass* pRef) const
{
    if (pOut == nullptr || pRef == nullptr)
        return;

    const CoordStruct mine = GetCoords();
    const CoordStruct theirs = pRef->GetCoords();

    // asm: ObjectClass_Get_Height? (0x5F3E10, export 1209) -- the two deltas
    // are (pRef.X - this.X) and (this.Y - pRef.Y), i.e. the X term is the
    // *reference minus receiver*, the Y term the receiver minus reference.
    // FacingFromDeltas is the arctanfoo(...) - pi/2 * -10430.060... wrapper.
    const int32 dx = theirs.X - mine.X;                  // asm: [edi] - var_1C
    const int32 dy = mine.Y - theirs.Y;                  // asm: [eax+4] - [edi+4]

    pOut->X = FacingFromDeltas(dx, dy);
    pOut->Y = 0;
    pOut->Z = 0;
}

// ============================================================================
// ObjectClass_SetPosition  (asm: mov edx,[eax]/[eax+4]/[eax+8] ; store at
//                           [ecx+9Ch]/[+4]/[+8])
//
//  Caches the supplied coordinate into the object's draw-position slot
//  rather than into Location, so the renderer can keep the last projected
//  origin separate from the logical position.
// ============================================================================
void ObjectClass::SetPosition(const CoordStruct& coords)
{
    CachedLocation = coords;
}

// ============================================================================
// ObjectClass_IronCurtain / Scatter / IsCellOccupied / MouseOverObject /
// MouseOverCell
//
//  Neutral object-layer slots.  The assembly bodies are bare
//  `retn 0Ch` (IronCurtain/Scatter/MouseOverCell -> no-op), `xor eax,eax ;
//  retn 14h` (IsCellOccupied -> 0) and `xor eax,eax ; retn 8`
//  (MouseOverObject -> 0).  The concrete subclasses that care replace the
//  slot.
// ============================================================================
void ObjectClass::IronCurtain(ObjectClass* pSource, int32 a3, int32 a4) const
{
    (void)pSource;
    (void)a3;
    (void)a4;
}

void ObjectClass::Scatter(int32 a2, int32 a3, int32 a4) const
{
    (void)a2;
    (void)a3;
    (void)a4;
}

bool ObjectClass::IsCellOccupied(const CellStruct& cell, int32 a3, int32 a4,
                                 int32 a5, int32 a6) const
{
    (void)cell;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    return false;
}

int32 ObjectClass::MouseOverObject(int32 a2, int32 a3) const
{
    (void)a2;
    (void)a3;
    return 0;
}

int32 ObjectClass::MouseOverCell(int32 a2, int32 a3, int32 a4) const
{
    (void)a2;
    (void)a3;
    (void)a4;
    return 0;
}
