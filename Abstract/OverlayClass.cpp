#include <Abstract/OverlayClass.h>
#include <Abstract/OverlayTypeClass.h>
#include <Animations/AnimTypeClass.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <IO/CRC.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Rendering/Surface.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>
#include <Game/GameInit.h>
#include <Game/Externs.h>

#include <cstring>

// ============================================================================
// OverlayClass.cpp
//
//  OverlayClass is the runtime instance of an overlay - walls, bridges,
//  tiberium, etc.  Overlays are map decorations that occupy a single cell.
//  They are stored directly inside CellClass rather than the global
//  AbstractClass array, but still participate in the persistence (Load/Save)
//  and RTTI machinery.
//
//  This file implements:
//    * Constructor / destructor
//    * Load / Save (stream serialization)
//    * WhatAmI / Size / ComputeCRC / Get_CRC
//    * Update (tiberium growth / wall decay)
//    * Destroy / Is_Destroyed
//    * Get_Overlay_Data / Set_Overlay_Data
//    * Is_Tiberium / Get_Tiberium_Value
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<OverlayClass*>* OverlayClass::Array = nullptr;

// ============================================================================
// Constructor
//
// asm: OverlayClass_CTOR (0x5FC3B0, retn 0Ch -> 3 arguments)
//   ObjectClass_CTOR(this)
//   [esi+0ACh] = type                       ; OverlayClass::Type
//   irtti.Create_ID(this+4)
//   append esi to vec_Overlays (grow-by-increment, then length++)
//   if (coords != {0,0,0}):
//       OverlayClass::ToOwn = owner
//       cell = Map.Coord_Cell(centerOf(coords))
//       if (cell->GetTerrain() == 0) Unlimbo(this, &xyz, 0)
//       OverlayClass::ToOwn = -1
//
//  This build keeps the owner-flavoured signature used across the project and
//  reproduces the vec_Overlays registration plus the field initialisation.
// ============================================================================

OverlayClass::OverlayClass(HouseClass* pOwner) noexcept
    : ObjectClass()
{
    Type          = nullptr;   // asm: [esi+0ACh] (bound by the caller)
    TiberiumValue = 0;
    OverlayData   = 0;
    Health        = 0;
    Owner         = pOwner;
    IsSelected    = false;
    IsInLimbo     = false;
    Marked        = false;     // asm: [esi+74h]
    InOpenTopped  = false;

    // asm: cmp vec_Overlays.count, vec_Overlays.length / jl ... / mov
    //      vec_Overlays.data[length] = esi ; inc length
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<OverlayClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<OverlayClass*>)));
        if (Array != nullptr)
            new (Array) DynamicVectorClass<OverlayClass*>();
    }
    if (Array != nullptr)
        Array->Add(this);
}

// ============================================================================
// Destructor
//
// asm: OverlayClass_SDDTOR (0x5FDFA0)
//   re-stamp vtable / Detach(this)
//   idx = vec_Overlays.FindIndex(esi)      ; erase with shift
//   if (GameActive) ObjectClass_Remove0(this)
//   [esi+0ACh] = 0 ; ObjectClass_DTOR(this)
// ============================================================================

OverlayClass::~OverlayClass()
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

HRESULT OverlayClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::Overlay);
    return S_OK;
}

// asm: OverlayClass_Load (0x5FD8E0, retn 8 -> 2 args, the IPersistStream slot)
//   hr = ObjectClass_Load(this, pStm)          ; 失败直接返回 hr
//   if (this):
//       ObjectClass_LoadTables(this, &pStm)     ; 重打四张虚表
//       Swizzle(&this->Type)                    ; 解析 OverlayTypeClass* (this+0ACh)
//   注意：asm 不序列化任何自有字段（叠加物本体存于 CellClass），Type 指针
//   由 Swizzle 依据流里的 ID 重绑。
HRESULT OverlayClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectClass::Load(pStm);
    if (hr < 0)
        return hr;                            // asm: jl loc_5FD943

    // asm: ObjectClass_LoadTables(this, &pStm)
    LoadTables(pStm);

    // asm: SwizzleManager.Swizzle(&this->Type) —— 以 ID 重绑类型指针。
    if (Type != nullptr)
    {
        OverlayTypeClass* pResolved = OverlayTypeClass::Find(Type->ID);
        Type = (pResolved != nullptr) ? pResolved : Type;
    }

    return S_OK;
}

// asm: OverlayClass_Save (0x5FD900, retn 0Ch -> 3 args)
//   AbstractClass_Save(this, pStm, fClearDirty)   ; 无自有字段，纯转发。
HRESULT OverlayClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    return ObjectClass::Save(pStm, fClearDirty);
}

// ============================================================================
// RTTI / size
// ============================================================================

AbstractType OverlayClass::WhatAmI() const
{
    return AbstractType::Overlay;
}

// asm: OverlayClass_GetSize (0x5FDEE0) -- "mov eax, 0B0h ; retn".
int32 OverlayClass::Size() const
{
    return 0xB0;
}

// asm: OverlayClass_GetType (0x5FDE90) -- "mov eax, [ecx+0ACh] ; retn".
ObjectTypeClass* OverlayClass::GetType() const
{
    return Type;
}

// asm: OverlayClass_GetAbstractDerivationID (0x5FDF0F) -- "mov eax, 14h ; retn".
int32 OverlayClass::GetAbstractDerivationID() const
{
    return 0x14;
}

// asm: OverlayClass_PointerGotInvalid (0x71D0xx, retn 8 -> 2 args).  Chains the
//  ObjectClass detach pass and then clears the type pointer when the invalid
//  object was the overlay's own OverlayTypeClass.
void OverlayClass::PointerGotInvalid(AbstractClass* pInvalid, bool removed)
{
    ObjectClass::PointerGotInvalid(pInvalid, removed);

    if (reinterpret_cast<AbstractClass*>(Type) == pInvalid)
        Type = nullptr;
}

// ============================================================================
// CRC
// ============================================================================

void OverlayClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectClass::ComputeCRC(crc);

    crc.AddData(&TiberiumValue, sizeof(TiberiumValue));
    crc.AddData(&OverlayData,   sizeof(OverlayData));
    crc.AddData(&Health,        sizeof(Health));
}

int32 OverlayClass::Get_CRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// Update
//
//  Called every frame by the main loop.  For tiberium overlays, this handles
//  growth and spread.  For walls, this handles damage propagation.  For other
//  overlay types, this is a no-op.
// ============================================================================

void OverlayClass::Update()
{
    if (Type == nullptr)
        return;

    // Tiberium growth: when the overlay is a tiberium type and the scenario
    // allows growth, increment the growth stage up to the maximum.
    if (Type->IsTiberium && Health > 0)
    {
        if (ScenarioClass::Instance != nullptr)
        {
            if (ScenarioClass::Instance->SpecialFlags.TiberiumGrows())
            {
                // The growth stage is encoded in OverlayData.  Each stage
                // increases the tiberium value.  The number of stages is the
                // overlay type's DamageLevels field (art INI).
                int32 maxStage = Type->DamageLevels;
                if (maxStage > 0 && OverlayData < maxStage)
                {
                    // Increment the growth stage.  The full binary only does
                    // this on specific frames based on the growth timer.
                    TiberiumValue = OverlayData + 1;
                }
            }
        }
    }

    // Wall decay: damaged walls eventually regenerate in the full binary.
    // The standalone build does not implement wall regeneration.

    // Destroyed overlay cleanup.
    if (Health <= 0 && Type->Wall)
    {
        // Walls with 0 health are removed from the cell.  The full binary
        // calls CellClass::Overlay_Destroyed to handle this.
        OverlayData = -1;
    }
}

// ============================================================================
// Destroy / Is_Destroyed
// ============================================================================

void OverlayClass::Destroy()
{
    // Set health to 0 to mark the overlay as destroyed.  The full binary
    // also plays a death animation and updates the cell.
    Health = 0;
    OverlayData = -1;
    TiberiumValue = 0;
}

bool OverlayClass::Is_Destroyed() const
{
    // An overlay is destroyed when its health is 0 or its data is -1.
    return Health <= 0 || OverlayData < 0;
}

// ============================================================================
// Overlay data accessors
// ============================================================================

int32 OverlayClass::Get_Overlay_Data() const
{
    return OverlayData;
}

void OverlayClass::Set_Overlay_Data(int32 data)
{
    OverlayData = data;

    // If this is a tiberium overlay, update the tiberium value to match
    // the growth stage stored in the data field.
    if (Type != nullptr && Type->IsTiberium)
    {
        TiberiumValue = (data >= 0) ? data + 1 : 0;
    }
}

// ============================================================================
// Tiberium helpers
// ============================================================================

bool OverlayClass::Is_Tiberium() const
{
    if (Type == nullptr)
        return false;
    return Type->IsTiberium;
}

int32 OverlayClass::Get_Tiberium_Value() const
{
    // The tiberium value represents the amount of tiberium in the cell.
    // It is derived from the growth stage (OverlayData) and the tiberium
    // type's base value.  The full binary uses a per-type value table;
    // the standalone build returns the stored TiberiumValue.
    if (!Is_Tiberium())
        return 0;

    if (TiberiumValue > 0)
        return TiberiumValue;

    // Fall back to deriving from the growth stage.
    if (OverlayData >= 0)
        return OverlayData + 1;

    return 0;
}

// ============================================================================
// OverlayClass_Draw (asm:0x5FD980, __thiscall(this, coords, OtherSurface),
//   retn 8 -> two arguments)
//
//   控制流（asm:1601 起）：
//     SHP = Type->GetImage()                       ; vt+9Ch
//     typeIdx = Type->GetArrayIndex()
//     yOffset = 0
//     if (IsTiberium || Wall || typeIdx==0x7E || Crate) yOffset = -0Ch
//     if (Type 的 LandValue == 9) --yOffset
//     if (typeIdx == 0x7E) --yOffset
//     pos.X = coords->X
//     pos.Y = coords->Y + yOffset + viewport.L
//     三条绘制支路，均以 0xE00 旗标落到 TempSurface：
//       * Type->IsTiberium : 走矿脉调色板（Tiberium 调色 + CellAnim 兜底）
//       * Type->IsVeins    : 0xA7 叠加物向上抬 0x26
//       * 其它             : 用格子 Convert 与格子帧号
// ============================================================================
void OverlayClass::Draw(Point2D* pCoords, Surface* pOtherSurface) const
{
    if (pCoords == nullptr || Type == nullptr || pOtherSurface == nullptr)
        return;

    const int32 typeIdx = Type->GetArrayIndex();

    // asm: mov eax,[ecx] / call [eax+9Ch]  -> Type->GetImage()
    SHPStruct* pShape = Type->GetImage();
    if (pShape == nullptr)
        return;

    // asm: 默认 ebx=0；tiberium/墙/crate(7Eh)/NoUseTileLandType -> ebx=-0Ch
    int32 yOffset = 0;
    if (Type->IsTiberium || Type->Wall || typeIdx == 0x7E || Type->Crate)
        yOffset = -0x0C;

    // asm: cmp dword ptr [edx+298h], 9 / jnz / dec ebx
    if (static_cast<int32>(Type->Get_Land_Type()) == 9)
        --yOffset;

    // asm: cmp eax, 7Eh / jnz / dec ebx
    if (typeIdx == 0x7E)
        --yOffset;

    Point2D pos;
    pos.X = pCoords->X;                        // asm: mov edx,[eax] ; pos.X
    pos.Y = pCoords->Y + yOffset;              // asm: add eax,ebx

    const int32 vpL = 0;                       // rect_mouseclass_vp.L
    pos.Y += vpL;                              // asm: add eax, rect_mouseclass_vp.L

    // ------------------------------------------------------------------
    // 三条绘制支路。SHP / Convert / frame 的取法在 asm 中不同，这里统一
    // 取 SHP，并按支路挑选 Convert 与帧号。
    // ------------------------------------------------------------------
    ConvertClass* pConvert = nullptr;
    int32 frameIdx = 0;

    if (Type->IsTiberium)
    {
        // asm: 由矿脉类型解出调色板与 CellAnim 形状（CellAnim 非空时兜底）。
        if (Type->CellAnim != nullptr && Type->CellAnim->AnimShape != nullptr)
            pShape = Type->CellAnim->AnimShape;
        frameIdx = (OverlayData > 0) ? OverlayData : 0;
        pConvert = TheConvert;                // asm: ColorScheme.data[HeapID]
    }
    else if (Type->IsVeins)
    {
        // asm: cmp [eax+294h],0A7h / jnz ; 若为 0xA7 则 pos.Y -= 0x26
        if (typeIdx == 0xA7)
            pos.Y -= 0x26;
        pConvert = TheConvert;
    }
    else
    {
        // asm: 取目标格的 Convert（+34h）与帧号（+10Ch）。
        if (MapClass::Instance != nullptr)
        {
            CellClass* pCell = MapClass::Instance->GetCellAt(Location);
            if (pCell != nullptr)
                frameIdx = pCell->Get_Overlay();
        }
        pConvert = TheConvert;
    }

    // asm: lct_Draw -> CC_Draw_Shape(TempSurface, Convert, SHP, frameIdx,
    //        &pos, OtherSurface, 0xE00, ...)
    CC_Draw_Shape(pOtherSurface, pConvert, pShape, frameIdx, &pos, nullptr,
                  static_cast<BlitterFlags>(0xE00), 0, 0, 0x3E8, 0, 0,
                  nullptr, 0, 0, 0);
}

// ============================================================================
// OverlayClass::Place (asm:0x5FC480 OverlayClass_Place, retn 4)
//
//   控制流（asm:148 起）：
//     if (!ObjectClass::Mark(idxLayer)) return false
//     if (idxLayer != 1 && idxLayer != 3) return false
//     cell = Map.Coord_Cell(GetCoords())                ; 目标格
//     typeIdx = Type->GetArrayIndex()                   ; [type+294h]
//     if (cell->Land > 4 && typeIdx != 0xB2) return false
//     若 Type->Wall : Is_Clear_To_Build ? 盖墙 + WallOwner + 邻格占用++ : 重绘 & false
//     若 LandValue==9 : 盖矿（清 overlay 标志位）
//     若 typeIdx==0x7E : SetupVeins()
//     否则 : 普通叠加落位
// ============================================================================
bool OverlayClass::Place(CoordStruct* pCoords, int32 overlayData, int32 a4)
{
    if (pCoords == nullptr || MapClass::Instance == nullptr || Type == nullptr)
        return false;

    // asm: ObjectClass::Mark(idxLayer)。本类固定用高亮层 idxLayer==1。
    if (!Mark_Layer(1))
        return false;                             // asm: loc_5FC784 -> xor al,al

    Location = *pCoords;
    CellClass* pCell = MapClass::Instance->GetCellAt(*pCoords);
    if (pCell == nullptr)
        return false;

    const int32 typeIdx = Type->GetArrayIndex();

    // asm: cmp al,4 / jbe ; cmp esi,0B2h / jnz -> false
    if (pCell->Get_Land_Byte() > 4 && typeIdx != 0xB2)
        return false;

    // ------------------------------------------------------------------
    // 墙：先判可否建造，可则落位并标记墙主，否则重绘并失败。
    // ------------------------------------------------------------------
    if (Type->Wall)                              // asm: mov cl,[eax+2A8h] / test
    {
        (void)a4;
        if (!pCell->Is_Clear_To_Build(1, nullptr, nullptr))
            return false;                        // asm: loc_5FC77C -> 重绘 & false

        pCell->Set_Overlay(typeIdx, overlayData);
        pCell->SetWallOwner();                   // asm: mov [ebp+CellClass.WallOwner]

        // asm: 对 8 个邻格占用计数 +1（该计数在 CellClass+122h 内维护，
        //       OpenYR 侧由 CellClass 自身负责，这里不再外接）。
        return true;
    }

    // ------------------------------------------------------------------
    // 矿脉地形（LandValue==9）：直接把叠加索引写进格子。
    // ------------------------------------------------------------------
    if (static_cast<int32>(Type->Get_Land_Type()) == 9)
    {
        pCell->Set_Overlay(typeIdx, overlayData);
        return true;
    }

    // asm: cmp esi,7Eh / jnz ; ScenarioInit==0 时 SetupVeins()
    if (typeIdx == 0x7E)
    {
        if (ScenarioInit == 0)
        {
            pCell->SetupVeins();
        }
        return true;
    }

    // ------------------------------------------------------------------
    // 普通叠加落位。
    // ------------------------------------------------------------------
    pCell->Set_Overlay(typeIdx, overlayData);
    OverlayData = overlayData;
    if (Type->IsTiberium)
        TiberiumValue = overlayData;
    Health = (Type->Strength > 0) ? Type->Strength : 0;
    IsInLimbo = false;
    return true;
}

// ============================================================================
// OverlayClass_Put (asm:0x5FD070, retn 8)
//
//   cell = Map.Coord_Cell(pixel >> 8)
//   if (cell->GetTerrain() != null) return false
//   ObjectClass::Unlimbo(this, xyz, a3)
// ============================================================================
bool OverlayClass::Put(CoordStruct* pCoords, int32 a3)
{
    (void)a3;
    if (pCoords == nullptr || MapClass::Instance == nullptr)
        return false;

    // asm: 像素坐标 -> 格子坐标（>>8，负数向零取整）。
    const int32 cellX = pCoords->X >> 8;
    const int32 cellY = pCoords->Y >> 8;
    CellClass* pCell = MapClass::Instance->GetCellAt(cellX, cellY);
    if (pCell == nullptr)
        return false;

    // asm: call CellClass_GetTerrain / test eax,eax / jnz -> xor al,al
    if (pCell->Get_Terrain() != nullptr)
        return false;

    Location = *pCoords;
    IsInLimbo = true;                            // 需要处于 limbo 才能 Unlimbo
    return Unlimbo();                            // asm: ObjectClass::Unlimbo(this, xyz, a3)
}
