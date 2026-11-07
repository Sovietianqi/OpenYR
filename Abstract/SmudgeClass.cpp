#include <Abstract/SmudgeClass.h>
#include <Abstract/SmudgeTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>
#include <Rendering/Surface.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/UnitArt.h>
#include <Helpers/AsmUIHelpers.h>

#include <cstring>

// ============================================================================
// SmudgeClass.cpp
//
//  SmudgeClass is the runtime instance of a smudge - scorch marks, craters,
//  bibs and other non-blocking ground decorations left behind by explosions.
//  Smudges are stored per-cell inside CellClass but still participate in the
//  persistence (Load/Save) and RTTI machinery inherited from ObjectClass.
//
//  This file implements:
//    * Static Array management (Init_Array / Delete_Array / Delete_All)
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * Update (animated smudge frame advance / crater chain expansion)
//    * Draw_It (ground decoration render hook)
//    * Is_Visible (line-of-sight / fog visibility test)
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<SmudgeClass*>* SmudgeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void SmudgeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<SmudgeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<SmudgeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<SmudgeClass*>();
    }
}

// asm: InitList_2723 (asm:12) —— 清零 data/count/length。
// asm: InitList_2722 (asm:2)  —— capacity/increment 均置 0x80。
void SmudgeClass::sctor()
{
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<SmudgeClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<SmudgeClass*>)));
        if (Array != nullptr)
        {
            new (Array) DynamicVectorClass<SmudgeClass*>();
            Array->Capacity = 0x80;               // asm: mov dword_B0B750, 80h
        }
    }
}

// asm: InitList_2723 的对称析构（vec_Smudge 释放）。
void SmudgeClass::sdtor()
{
    Delete_Array();
}

void SmudgeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<SmudgeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void SmudgeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        SmudgeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

// ============================================================================
// Constructor
//
// asm: SmudgeClass_CTOR (0x6B4A70, retn 0Ch -> 3 arguments:
//      (SmudgeTypeClass* type, CoordStruct* xyz, int32 arg_8))
//   ObjectClass_CTOR(this)
//   [this+0]    = SmudgeClass vftable
//   [this+0ACh] = type                       ; SmudgeClass::Type
//   [this+4/8/0C] = IRTTITypeInfo/INoticeSink/INoticeSource vtables
//   irtti.Create_ID(this+4)
//   append this to vec_Smudge (grow-by-increment, then count++)
//   if (xyz != {0,0,0}):
//       OverlayClass::ToOwn = arg_8          ; the placement owner ordinal
//       Unlimbo(this, xyz, 0)
//       if that fails -> Detach(this)
//       OverlayClass::ToOwn = -1
//
//  This build keeps the owner-flavoured constructor the rest of the project
//  uses; the type pointer is bound by the caller (SmudgeTypeClass) exactly as
//  the original CTOR argument #1 does.
// ============================================================================

SmudgeClass::SmudgeClass(HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type     = nullptr;   // asm: [esi+0ACh] (bound by the caller)
    Frame    = 0;
    Health   = 0;
    IsActive = true;
    Owner    = pOwner;

    IsSelected = false;
    IsInLimbo  = false;
    Marked     = false;

    if (Array == nullptr)
        Init_Array();
    if (Array != nullptr)
        Array->Add(this);
}

// ============================================================================
// Destructor
//
// asm: SmudgeClass_DTOR (0x6B4B90)
//   re-stamp vtable / Detach(this)
//   idx = vec_Smudge.FindIndex(this) ; erase with shift, count--
//   if (GameActive) ObjectClass_Remove0(this)
//   [esi+0ACh] = 0 ; ObjectClass_DTOR(this)
// ============================================================================

SmudgeClass::~SmudgeClass()
{
    if (Array != nullptr)
    {
        for (int32 i = 0; i < Array->Count; ++i)
        {
            if (Array->Items[i] == this)
            {
                Array->Remove(i);
                break;
            }
        }
    }

    Type = nullptr;   // asm: mov dword ptr [esi+0ACh], 0
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT SmudgeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Smudge);
    return S_OK;
}

// asm: SmudgeClass_Load (asm:523, retn 8)
//   hr = ObjectClass_Load(this, pStm)         ; 失败直接返回
//   ObjectClass_LoadTables(this, &pStm)       ; 重打四张虚表
//   SwizzleManager.Swizzle(this+0ACh)         ; 重绑 SmudgeTypeClass*
//   asm 不序列化任何自有字段（污渍本体存于 CellClass）。
HRESULT SmudgeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0)
        return hr;                            // asm: jl loc_6B4EF3

    LoadTables(pStm);                         // asm: ObjectClass_LoadTables

    // asm: Swizzle(&this->Type) —— 以 ID 重绑类型指针。
    if (Type != nullptr)
    {
        SmudgeTypeClass* pResolved = SmudgeTypeClass::Find(Type->ID);
        Type = (pResolved != nullptr) ? pResolved : Type;
    }

    return S_OK;
}

// asm: SmudgeClass_Save (asm:582, retn 0Ch) —— 纯转发 AbstractClass_Save。
HRESULT SmudgeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    return ObjectClass::Save(pStm, fClearDirty);
}

// asm: SmudgeClass_PointerGotInvalid (retn 8) —— 先走 ObjectClass 失效链，
//   若失效对象正是本类的 Type，则清空 Type。
void SmudgeClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    ObjectClass::PointerGotInvalid(pInvalid, removed);

    if (reinterpret_cast<AbstractClass*>(Type) == pInvalid)
        Type = nullptr;
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType SmudgeClass::WhatAmI() const
{
    return AbstractType::Smudge;
}

int32 SmudgeClass::Size() const
{
    // asm: SmudgeClass_GetSize (0x6B4FE0) -- "mov eax, 0B0h ; retn".  The hint
    // instance shares the OverlayClass allocation size.
    return 0xB0;
}

// asm: SmudgeClass_GetType (0x6B4F50) -- "mov eax, [ecx+0ACh] ; retn".
ObjectTypeClass* SmudgeClass::GetType() const
{
    return Type;
}

// asm: SmudgeClass_GetAbstractDerivationID (0x6B4F70) -- "mov eax, 1Dh ; retn".
int32 SmudgeClass::GetAbstractDerivationID() const
{
    return 0x1D;
}

// ============================================================================
// SmudgeClass_SetLayer (asm:0x6B4BE0, __thiscall(this, idxLayer), retn 4)
//
//   al = ObjectClass::Mark(idxLayer)         ; asm: call ObjectClass::Mark
//   if (!al) return false                     ; not on the ground/air layers
//   if (idxLayer != 1 && idxLayer != 3) return false
//   coords = vt+1B8h(this)                    ; world position
//   if (!ScenarioInit && !Debug_Map):
//       if (sub_6B5F80(tile, &coords, 1))     ; already painted -> skip paint
//           ...
//   sub_6B6080(GetType(), &coords)            ; (re)stamp the SHP footprint
//   Marked = false                            ; asm: mov byte ptr [esi+74h], 0
//   vt+0F8h(this)                             ; re-draw / dirty the cells
//   return true
// ============================================================================
bool SmudgeClass::SetLayer(int32 idxLayer)
{
    // asm: push edi / call ObjectClass::Mark
    if (!Mark_Layer(idxLayer))
        return false;

    // asm: cmp edi,1 / jz / cmp edi,3 / jnz
    if (idxLayer != 1 && idxLayer != 3)
        return false;

    // asm: lea ecx,[esp+..] / call vt+1B8h -> world coords
    CoordStruct coords = Location;

    // asm: sub_6B6080([esi+0ACh], &coords) -- stamp the type ordinal and the
    //   damage data into every cell the SHP covers.  The footprint walk lives
    //   on the type; delegate to it so the per-cell smudge slot stays in sync.
    if (MapClass::Instance != nullptr && Type != nullptr)
    {
        CellClass* pCell = MapClass::Instance->GetCellAt(coords);
        if (pCell != nullptr)
        {
            int32 typeIdx = -1;
            for (int32 i = 0; i < SmudgeTypeClass::GetCount(); ++i)
            {
                if (SmudgeTypeClass::FindByIndex(i) == Type) { typeIdx = i; break; }
            }
            pCell->Set_Smudge(typeIdx, Frame);
        }
    }

    // asm: mov byte ptr [esi+74h], 0
    Marked = false;

    // asm: vt+0F8h(this) -- repaint hook (rendering layer, no-op here).
    return true;
}

// ============================================================================
// CRC
// ============================================================================

void SmudgeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&Frame,    sizeof(Frame));
    crc.AddData(&Health,   sizeof(Health));
    crc.AddData(&IsActive, sizeof(IsActive));
}

int32 SmudgeClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  For animated smudges this advances
//  the animation frame.  For crater chains this propagates the chain forward.
//  Inactive or destroyed smudges are skipped.
// ============================================================================

void SmudgeClass::Update()
{
    if (!IsActive)
        return;

    if (Type == nullptr)
        return;

    // Destroyed smudges do not animate.
    if (Health <= 0)
        return;

    // ------------------------------------------------------------------
    // Animated smudges advance their frame counter.  The full binary uses a
    // per-type frame delay; the standalone build advances one frame per tick
    // and wraps at the type's frame count.
    // ------------------------------------------------------------------
    if (Type->IsAnimated && Type->Frames > 0)
    {
        ++Frame;
        if (Frame >= Type->Frames)
        {
            // Non-looping animated smudges stop on the last frame; looping
            // ones wrap to zero.  Bibs and scorches loop, craters do not.
            if (Type->IsCrater)
            {
                Frame = Type->Frames - 1;
            }
            else
            {
                Frame = 0;
            }
        }
    }

    // ------------------------------------------------------------------
    // Crater chain expansion.  When a crater smudge is part of a chain, each
    // update advances the chain step until ChainSteps is reached.  The full
    // binary spawns additional crater smudges on neighbouring cells.
    // ------------------------------------------------------------------
    if (Type->IsCrater && Type->ChainCount > 0 && Type->ChainSteps > 0)
    {
        // The standalone build records the chain progress in Health.  Each
        // tick decrements Health; when it reaches zero the chain is done.
        if (Health > 0)
        {
            --Health;
        }
    }
}

// ============================================================================
// 弹坑/焦痕的绘制 —— asm 里 SmudgeClass 本身没有绘制代码：
//
//   SmudgeClass_Draw（0x6B4F9x）只有一条 `retn 8`，即 vftable 里那个绘制槽
//   是空实现。真正的落盘绘制在 SmudgeTypeClass 的虚槽 vt+0xA0
//   （sub_6B55F0），由 CellClass_Draw_It（sub_480350，loc_48049E）在画完
//   地块后调用：
//
//       if (cell->SmudgeTypeIndex(+0x48) != -1) {
//           type = vec_SmudgeTypes[cell->SmudgeTypeIndex];
//           type->vt[0xA0](&cellScreenPos, OtherSurface, &cell->pos,
//                          cell->field_11F /*SmudgeData*/, Level*104);
//       }
//
//   sub_6B55F0 逐条：
//     SHP = this->GetImage()                        ; call [eax+9Ch]
//     if (!SHP) return
//     pos = *a2                                     ; 格子屏幕坐标
//     if (a4 /*SmudgeData*/ != 0) {                 ; 子块（1 格多块弹坑）
//         n = this->[+298h]                         ; = CellSize.X
//         q = a4 / n ; r = a4 % n
//         pos.Y -= 15 * (q + r)                     ; shl 4 / sub / neg
//         pos.X += 30 * (q - r)                     ; *3 *5 shl 2 / sar 1
//     }
//     cell = Map.Coord_Cell(coords)
//     z    = Draw_AdjustForBuildingZ(a1)            ; a1 = Level * 104
//     CC_Draw_Shape(TempSurface, cell->LightConvert(+0x34), SHP,
//                   frameIdx = 0, &pos, OtherSurface, 0xE00 /*Centered|
//                   Transparent|Alpha*/, 0, ZAdjust = -1 - z, ...)
//
//   帧号恒为 0：原版弹坑/焦痕不做逐帧动画，也没有随时间淡化的 alpha 递减
//   （淡化只存在于 AnimClass 的 TranslucencyLevel 支路）。
//
//   本树无逐格 LightConvert（CellClass 没有 +0x34 这个转换器），改用
//   StandardPalette；其余坐标/帧号/ZAdjust 与 asm 一致。
// ============================================================================
void SmudgeClass::Draw_It(int32 originX, int32 originY) const
{
    if (!IsActive)
        return;

    if (Type == nullptr)
        return;

    SHPStruct* pShape = Type->GetImage();            // asm: call [eax+9Ch]
    if (pShape == nullptr)
        return;

    Surface* pSurface = UnitArt::GetTacticalSurface();
    if (pSurface == nullptr)
        return;

    Point2D pos(originX, originY);                   // asm: pos = *a2

    CellClass* pCell = (MapClass::Instance != nullptr)
        ? MapClass::Instance->GetCellAt(Location) : nullptr;

    // asm: a4 = 该格的 SmudgeData（+0x11F，行*宽+列 的打包子块号）。
    const int32 smudgeData = (pCell != nullptr) ? pCell->SmudgeData : 0;
    if (smudgeData != 0) {
        const int32 width = Type->CellSize.X;        // asm: [+298h]
        if (width > 0) {
            const int32 q = smudgeData / width;
            const int32 r = smudgeData % width;
            pos.Y -= 15 * (q + r);                   // asm: 15*(q+r)
            pos.X += 30 * (q - r);                   // asm: 30*(q-r)
        }
    }

    // asm: a1 = (int8)cell->[+11Bh] /*Level*/ * dword_89E7C0 (=104)
    int32 levelZ = 0;
    if (pCell != nullptr) {
        levelZ = static_cast<int32>(pCell->Level) * MapClass::CellHeightLeptons;
    }
    const int32 zAdjust = -1 - Draw_AdjustForBuildingZ(levelZ);

    // asm: flags = 0E00h (Centered|Transparent|Alpha)，帧号恒为 0。
    CC_Draw_Shape(pSurface, StandardPalette, pShape, 0, &pos, nullptr,
                  static_cast<BlitterFlags>(0x0E00),
                  0, zAdjust, 0, 0, 0, nullptr, 0, 0, 0);
}

// ============================================================================
// 根据游戏行为，可知 Visible 负责下面这段逻辑。
//
//  Returns whether the smudge is visible to the local player.  Smudges that
//  are inactive, in limbo or hidden by fog/shroud are not visible.
// ============================================================================

bool SmudgeClass::Is_Visible() const
{
    if (!IsActive)
        return false;

    if (IsInLimbo)
        return false;

    if (Type != nullptr && !Type->IsFlat && Type->IsBib)
    {
        // Bibs are only visible while the owning building is on the map.
        // The standalone build treats bibs as always visible.
        return true;
    }

    // The full binary queries the map for fog/shroud state at the smudge's
    // cell.  The standalone build assumes the smudge is visible.
    return true;
}
