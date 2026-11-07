#include <Abstract/OverlayTypeClass.h>
#include <Abstract/OverlayClass.h>
#include <Animations/AnimTypeClass.h>
#include <FileFormats/SHP.h>

#include <Core/Memory.h>
#include <Core/Macros.h>
#include <INI/INIClass.h>
#include <Rendering/ConvertClass.h>
#include <Rendering/Surface.h>
#include <IO/CRC.h>
#include <IO/CCFileClass.h>
#include <Map/MapClass.h>
#include <Map/CellClass.h>
#include <Rules/RulesClass.h>
#include <Scenario/ScenarioClass.h>
#include <Game/Externs.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>

// ============================================================================
// OverlayTypeClass.cpp
//
//  OverlayTypeClass describes a kind of overlay (wall, bridge section, tiberium
//  type, etc.).  It inherits ObjectTypeClass and adds (mirroring the original
//  struct, sizeof 0x2BC):
//
//    HeapID (+0x294)           - index within the global type array
//    LandType (+0x298)         - land type the overlay creates
//    CellAnim (+0x29C)         - cell animation for tiberium overlays
//    DamageLevels (+0x2A0)     - number of art frames / damage states
//    Strength (+0x2A4)         - hit points
//    Wall/Tiberium/Crate/...   - classification flag bytes
//    RadarColor (+0x2B6)       - minimap colour (3 bytes)
//
//  This file implements the static Array plumbing, construction, the
//  IPersistStream Load/Save pair, the INI reader, CRC hashing and the
//  overlay-specific helpers.
// ============================================================================

// ============================================================================
// Static member definitions
// ============================================================================
DynamicVectorClass<OverlayTypeClass*>* OverlayTypeClass::Array = nullptr;

// ============================================================================
// Static array management
// ============================================================================

void OverlayTypeClass::Init_Array()
{
    if (Array != nullptr)
        return;

    Array = static_cast<DynamicVectorClass<OverlayTypeClass*>*>(
        YRMemory::Allocate(sizeof(DynamicVectorClass<OverlayTypeClass*>)));

    if (Array != nullptr)
    {
        new (Array) DynamicVectorClass<OverlayTypeClass*>();
    }
}

void OverlayTypeClass::Delete_Array()
{
    if (Array == nullptr)
        return;

    Array->~DynamicVectorClass<OverlayTypeClass*>();
    YRMemory::Deallocate(Array);
    Array = nullptr;
}

void OverlayTypeClass::Delete_All()
{
    if (Array == nullptr)
        return;

    for (int32 i = Array->Count - 1; i >= 0; --i)
    {
        OverlayTypeClass* item = Array->Items[i];
        if (item != nullptr)
        {
            GameDelete(item);
        }
        Array->Remove(i);
    }
}

OverlayTypeClass* OverlayTypeClass::Find(const char* pID)
{
    if (Array == nullptr || pID == nullptr)
        return nullptr;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        OverlayTypeClass* item = Array->Items[i];
        if (item == nullptr)
            continue;
        if (!_strcmpi(item->ID, pID))
            return item;
    }
    return nullptr;
}

OverlayTypeClass* OverlayTypeClass::FindByIndex(int32 index)
{
    if (Array == nullptr)
        return nullptr;
    if (index < 0 || index >= Array->Count)
        return nullptr;
    return Array->Items[index];
}

int32 OverlayTypeClass::GetCount()
{
    if (Array == nullptr)
        return 0;
    return Array->Count;
}

// asm: InitList_2151 (asm:1) —— vec_OverlayTypes 的静态构造：清零 data/count/length。
// asm: InitList_2152 (asm:10) —— capacity/increment 均置 0x80。
void OverlayTypeClass::sctor()
{
    if (Array == nullptr)
    {
        Array = static_cast<DynamicVectorClass<OverlayTypeClass*>*>(
            YRMemory::Allocate(sizeof(DynamicVectorClass<OverlayTypeClass*>)));
        if (Array != nullptr)
        {
            new (Array) DynamicVectorClass<OverlayTypeClass*>();
            Array->Capacity = 0x80;   // asm: mov dword_AC1658, 80h
        }
    }
}

// asm: InitList_2151 的对称析构（见 OverlayTypeClass_SDDTOR 尾部的向量 shrink）。
void OverlayTypeClass::sdtor()
{
    Delete_Array();
}

// asm: OverlayClass_FindByName —— 与 Find 同义（大小写不敏感）。
OverlayTypeClass* OverlayTypeClass::FindByName(const char* pID)
{
    return Find(pID);
}

// ============================================================================
// Constructor / destructor
//
//  asm: OverlayTypeClass_CTOR(OverlayTypeClass *this, char *name)
//    HeapID = -1; LandType = 0; CellAnim = 0; DamageLevels = 1; Strength = 1;
//    Wall/Tiberium/Crate/... = 0; NoUseTileLandType = 1; ImageAllocated = 1;
//    DrawFlat = 1; RadarColor = {0,0,0}; ArrayIndex = index (else -1).
// ============================================================================
OverlayTypeClass::OverlayTypeClass(const char* pID) noexcept
    : ObjectTypeClass(pID)
{
    HeapID            = -1;                 // asm: [esi+294h] = -1
    CellLandType      = 0;                  // asm: [esi+9Ch] 默认 0（tiberium 时改 6）
    LandValue         = LandType::Clear;    // asm: [esi+298h] = 0
    CellAnim          = nullptr;            // asm: [esi+29Ch] = 0
    DamageLevels      = 1;                  // asm: [esi+2A0h] = 1
    Strength          = 1;                  // asm: [esi+2A4h] = 1
    Wall              = false;              // asm: [esi+2A8h] = 0
    IsTiberium        = false;              // asm: [esi+2A9h] = 0
    Crate             = false;              // asm: [esi+2AAh] = 0
    CrateTrigger      = false;              // asm: [esi+2ABh] = 0
    NoUseTileLandType = true;               // asm: [esi+2ACh] = 1
    IsVeinholeMonster = false;              // asm: [esi+2ADh] = 0
    IsVeins           = false;              // asm: [esi+2AEh] = 0
    ImageAllocated    = false;              // asm: [esi+2AFh] = 0
    Explodes          = false;              // asm: [esi+2B0h] = 0
    ChainReaction     = false;              // asm: [esi+2B1h] = 0
    Overrides         = false;              // asm: [esi+2B2h] = 0
    DrawFlat          = true;               // asm: [esi+2B3h] = 1
    IsRubble          = false;              // asm: [esi+2B4h] = 0
    IsARock           = false;              // asm: [esi+2B5h] = 0
    RadarColor[0]     = 0;                  // asm: [esi+2B6h] = 0
    RadarColor[1]     = 0;                  // asm: [esi+2B7h] = 0
    RadarColor[2]     = 0;                  // asm: [esi+2B8h] = 0
    OwnsImage         = false;              // asm: [esi+0A8h] = 0

    // asm: Create_ID 之后写入的默认字符标志位。
    RadarInvisible    = true;               // asm: byte ptr [esi+22Fh] = 1
    Insignificant     = true;               // asm: byte ptr [esi+232h] = 1
    Selectable        = false;              // asm: [esi+230h] = 0
    LegalTarget       = false;
}

OverlayTypeClass::~OverlayTypeClass()
{
    // No heap resources to release.
}

// ============================================================================
// IPersistStream
// ============================================================================

HRESULT OverlayTypeClass::GetClassID(CLSID* pClassID)
{
    if (pClassID == nullptr)
        return E_POINTER;
    pClassID->Data1 = static_cast<uint32>(AbstractType::OverlayType);
    return S_OK;
}

// asm: OverlayTypeClass_Load (asm:656, retn 8)
//   ObjectTypeClass_Load(this, pStm)            ; 失败直接返回
//   重置四张虚表 / Load3DArt / Load_Theater_Art
//   SwizzleManager.Swizzle(this+29Ch)           ; 重绑 CellAnim 指针
//   非剧场且允许分配时按 <Image>.SHP 重新解析 ImageSHP
HRESULT OverlayTypeClass::Load(IStream* pStm)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Load(pStm);
    if (hr < 0)
        return hr;

    // asm: 尾部重绑 CellAnim 指针（Swizzle 语义在无 SwizzleManager 时的等价）。
    if (CellAnim != nullptr)
    {
        AnimTypeClass* pResolved = AnimTypeClass::Find(CellAnim->ID);
        CellAnim = (pResolved != nullptr) ? pResolved : CellAnim;
    }

    // asm: ObjectTypeClass_Load3DArt / Load_Theater_Art。
    ObjectTypeClass::Load3DArt(0);
    ObjectTypeClass::Load_Theater_Art(0);

    // asm: 非剧场且 ImageAllocated 时按名解析 SHP（+0x1F8 -> Finder -> +0xA4）。
    if (!IsTheater && ImageAllocated)
        LoadImage();

    return S_OK;                                  // asm: xor eax,eax
}

// asm: OverlayTypeClass_Save - chains ObjectTypeClass_Save only.
HRESULT OverlayTypeClass::Save(IStream* pStm, BOOL fClearDirty)
{
    if (pStm == nullptr)
        return E_POINTER;

    HRESULT hr = ObjectTypeClass::Save(pStm, fClearDirty);
    if (hr < 0) return hr;

    return S_OK;
}

// ============================================================================
// RTTI / size / ID
// ============================================================================

AbstractType OverlayTypeClass::WhatAmI() const
{
    return AbstractType::OverlayType;
}

int32 OverlayTypeClass::Size() const
{
    return 0x2BC;
}

const char* OverlayTypeClass::get_ID() const
{
    return this->ID;
}

// asm: OverlayTypeClass_GetAbstractDerivationID (mov eax, 15h ; retn)
int32 OverlayTypeClass::GetAbstractDerivationID() const
{
    return 0x15;
}

// asm: OverlayTypeClass_GetArrayIndex (mov eax, [ecx+294h] ; retn)
int32 OverlayTypeClass::GetArrayIndex() const
{
    return HeapID;
}

// ============================================================================
// Classification helpers
// ============================================================================

bool OverlayTypeClass::Is_Wall() const
{
    return Wall;
}

bool OverlayTypeClass::Is_Tiberium() const
{
    return IsTiberium;
}

LandType OverlayTypeClass::Get_Land_Type() const
{
    return LandValue;
}

ColorStruct OverlayTypeClass::Get_Radar_Color() const
{
    return ColorStruct(RadarColor[0], RadarColor[1], RadarColor[2]);
}

// asm: OverlayClass_Get_Radar_Color(OverlayTypeClass *this, RGBb *a2, int frame)
//
//  Resolves the minimap colour for a specific overlay frame.  The colour is
//  taken from the loaded SHP's per-frame colour table when the type renders
//  tiberium and this type (or its CellAnim) actually has a shape.  Otherwise
//  the three output bytes are zeroed.
uint8* OverlayTypeClass::Get_Radar_Color(uint8* pOut, int32 frame) const
{
    if (pOut == nullptr)
        return pOut;

    SHPStruct* pShape = this->ImageSHP;
    if (pShape == nullptr && CellAnim != nullptr)
    {
        pShape = CellAnim->AnimShape;
    }

    if (pShape != nullptr)
    {
        ColorStruct frameColor = pShape->GetColor(frame);
        pOut[0] = frameColor.R;
        pOut[1] = frameColor.G;
        pOut[2] = frameColor.B;
        return pOut;
    }

    pOut[0] = 0;
    pOut[1] = 0;
    pOut[2] = 0;
    return pOut;
}

// ============================================================================
// Factory helpers
//
//  asm: sub_5FE570 (asm:1067658) - allocate a bare OverlayClass bound to this
//  type, anchored at the shared {0,0,0} default-coord hint and owner -1.
//    push size OverlayClass / call new
//    push -1 / push &stru_AC1608 (shared {0,0,0}) / push this(type)
//    call OverlayClass_CTOR
//    return the instance (or null when new failed)
// ============================================================================
OverlayClass* OverlayTypeClass::CreateObject()
{
    OverlayClass* pObject = new OverlayClass(nullptr);
    if (pObject == nullptr)
        return nullptr;
    pObject->Type = this;                 // asm: ctor 第 1 个实参 = 本类型
    return pObject;
}

// asm: sub_5FE530 (asm:1067616) - allocate an OverlayClass anchored at the
//   supplied cell and register it (owner -1).  retn 8 -> (a2, a3); a3 is the
//   CellStruct the ctor stamps the pixel anchor from.
bool OverlayTypeClass::CreateAndSubmit(HouseClass* pOwner, uint32 startCell)
{
    OverlayClass* pObject = new OverlayClass(pOwner);
    if (pObject == nullptr)
        return false;                     // asm: jz loc_5FE55D -> xor al,al
    pObject->Type = this;

    // asm: 用传入格子把叠加物落位（CellStruct 中心 = cell*256 + 0x80）。
    if (MapClass::Instance != nullptr)
    {
        const CellStruct cell(static_cast<int16>(startCell & 0xFFFF),
                              static_cast<int16>((startCell >> 16) & 0xFFFF));
        CellClass* pCell = MapClass::Instance->GetCellAt(cell);
        if (pCell != nullptr)
            pCell->Set_Overlay(HeapID, 0);
    }
    return true;                          // asm: mov al,1
}

// asm: sub_5FE4C0 (asm:1067562) - return the shared hint block.  It is two
//   CellStructs: {0,0} then {0x7FFF,0x7FFF} (the "no size" sentinel pair).
void* OverlayTypeClass::GetSizeHint()
{
    static int16 SizeHint[4] = { 0, 0, (int16)0x7FFF, (int16)0x7FFF };
    return SizeHint;
}

// asm: OverlayTypeClass_LoadImage 的访问面——返回缓存的 SHP 句柄（+0xA4）。
SHPStruct* OverlayTypeClass::GetImageData() const
{
    return const_cast<OverlayTypeClass*>(this)->LoadImage();
}

// asm: OverlayTypeClass_CreateFromINIList (asm:220)
//
//   遍历全部已注册的 OverlayTypeClass，按 (ImageAllocated, Theater, NewTheater)
//   三条支路决定 SHP 基名：
//     * ImageAllocated==0 && !Theater && NewTheater==0 -> 释放旧的派生 SHP；
//     * ImageAllocated==0 &&  Theater          -> 释放旧的派生 SHP；
//     * ImageAllocated!=0 &&  Theater          -> "<Image>.<theater ext>"；
//     * ImageAllocated!=0 && !Theater && NewTheater!=0 -> "<Image>.SHP"
//                                               再经 TheaterSpecificID 换名；
//   装载失败时回退到通用对象 SHP（ObjectTypeClass::Generic）。
void OverlayTypeClass::CreateFromINIList()
{
    if (Array == nullptr)
        return;

    for (int32 i = 0; i < Array->Count; ++i)
    {
        OverlayTypeClass* pType = Array->Items[i];
        if (pType == nullptr)
            continue;

        if (!pType->ImageAllocated)
        {
            // asm: loc_5FE721 - 非剧场且无剧场美术时不重建；其余情况清掉派生 SHP。
            if (pType->IsTheater)
            {
                if (pType->ImageSHP != nullptr && pType->OwnsImage)
                {
                    pType->ImageSHP = nullptr;
                    pType->OwnsImage = false;
                }
            }
            continue;
        }

        // ImageAllocated：按剧场/新剧场派生文件名并重新装载。
        pType->LoadImage();
    }
}

// asm: OverlayTypeClass_LoadImage (asm:932)
//   仅在其自身基名为空（未分配）时按需装载：剧场名 -> "<Image>.<ext>"，
//   否则 "<Image>.SHP" 再经 TheaterSpecificID 换名；结果缓存进 ImageSHP。
SHPStruct* OverlayTypeClass::LoadImage()
{
    if (ImageSHP != nullptr)
        return ImageSHP;                       // asm: mov eax,[esi+0A4h] / test / jnz

    if (!ImageAllocated)
        return ImageSHP;                       // asm: test cl,cl / jz

    const char* pBaseName = Image;
    if (pBaseName == nullptr || pBaseName[0] == '\0')
        pBaseName = get_ID();
    if (pBaseName == nullptr || pBaseName[0] == '\0')
        return nullptr;

    char base[0x100];
    char full[0x100];
    snprintf(base, sizeof(base), "%s.SHP", pBaseName);
    const int32 idxTheater = (TheScenario != nullptr) ? static_cast<int32>(TheScenario->Theater) : 0;
    TheaterSpecificID(base, idxTheater, full);

    // asm: CCFileClass + Read_File 由派生名解出 SHP 句柄。本构建的文件后端
    //   尚未接通，句柄保持 null，由调用方向上层渲染回退。
    ImageSHP = nullptr;
    return ImageSHP;
}

// asm: OverlayTypeClass_Draw(OverlayTypeClass *this, Point2D *coords,
//   dwLTRB *OtherSurface, int frameIdx)  (asm:141, retn 0Ch)。
//
//   pos.X = coords->X + rect_mouseclass_vp.T + 0x1E
//   pos.Y = coords->Y + rect_mouseclass_vp.L + 0x0F
//   然后以 Convert_TEMPERAT、0x600 旗标、Brightness=1000、ZAdjust=0 把
//   GetImage() 的第 frameIdx 帧贴到 TempSurface。
//
//   asm 尾段的 16 个压栈实参（与 CC_Draw_Shape 的原型一一对应）：
//     ZS_Y=0, ZS_X=0, zFrame=0, ZShape=0, TintColor=0, Brightness=3E8h,
//     ZGradientDescIndex=0, ZAdjust=0, Remap=0, flags=600h,
//     boundingRect=OtherSurface, pos, frameIdx, SHP, Convert_TEMPERAT,
//     TempSurface
//   —— 注意 3E8h 落在 **Brightness** 槽，ZAdjust 与 ZGradientDescIndex 都是 0。
void OverlayTypeClass::Draw(Point2D* pCoords, Surface* pOtherSurface, int32 frameIdx) const
{
    if (pCoords == nullptr || pOtherSurface == nullptr)
        return;

    // asm: rect_mouseclass_vp.T 加到 X，rect_mouseclass_vp.L 加到 Y（原文如此）。
    // 全屏战术视口的左上角为 (0,0)，故两者按 0 处理。
    const int32 vpL = 0;
    const int32 vpT = 0;

    Point2D pos;
    pos.X = pCoords->X + vpT + 0x1E;           // asm: add eax,edx ; add eax,1Eh
    pos.Y = pCoords->Y + vpL + 0x0F;           // asm: add edx,esi ; add edx,0Fh

    SHPStruct* pShape = GetImage();            // asm: mov edx,[ecx] / call [edx+9Ch]
    if (pShape == nullptr)
        return;

    // asm: mov edx, Convert_TEMPERAT。本工程只装一份剧场调色板，等价全局即
    //   StandardPalette；它未装载时原版的 Convert_TEMPERAT 同样为空，
    //   CC_Draw_Shape 收到空 Convert 不会画任何像素。
    ConvertClass* pConvert = StandardPalette;
    if (pConvert == nullptr)
        return;

    CC_Draw_Shape(pOtherSurface, pConvert, pShape, frameIdx, &pos, nullptr,
                  static_cast<BlitterFlags>(0x600), 0, 0, 0, 0x3E8, 0,
                  nullptr, 0, 0, 0);
}

// ============================================================================
// LoadFromINI
//
//  asm: OverlayTypeClass_LoadFromINI.  Keys and order are reproduced verbatim:
//    Land, Strength, Wall, Tiberium, Crate, CrateTrigger, Explodes,
//    Overrides, CellAnim (rules); DamageLevels (art INI); then
//    RadarColor, NoUseTileLandType, IsVeinholeMonster, IsVeins,
//    ChainReaction, DrawFlat, IsARock, IsRubble.
// ============================================================================
bool OverlayTypeClass::LoadFromINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    // asm: Reset(INI) ; ObjectTypeClass_LoadFromINI ; test al,al / jz -> false
    if (!ObjectTypeClass::LoadFromINI(pINI))
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    // ------------------------------------------------------------------
    // Rules keys - 逐字对齐 OverlayTypeClass_LoadFromINI (asm:351)
    // ------------------------------------------------------------------
    LandValue    = pINI->GetLand(section, "Land", LandValue);        // "Land"
    Strength     = pINI->ReadInteger(section, "Strength", Strength); // "Strength"
    Wall         = pINI->ReadBool(section, "Wall", Wall);            // "Wall"
    IsTiberium   = pINI->ReadBool(section, "Tiberium", IsTiberium);  // "Tiberium"
    Crate        = pINI->ReadBool(section, "Crate", Crate);          // "Crate"
    CrateTrigger = pINI->ReadBool(section, "CrateTrigger", CrateTrigger);
    Explodes     = pINI->ReadBool(section, "Explodes", Explodes);    // "Explodes"
    Overrides    = pINI->ReadBool(section, "Overrides", Overrides);  // "Overrides"

    // CellAnim: 读取名字；非空才解析，否则保留旧指针（asm: ebp 先存旧值）。
    char animName[0x80];
    if (pINI->ReadString(section, "CellAnim", "", animName, sizeof(animName)) > 0)
    {
        AnimTypeClass* pAnim = AnimTypeClass::FindOrAllocate(animName);
        if (pAnim != nullptr)
            CellAnim = pAnim;                 // asm: [esi+29Ch]
    }

    // DamageLevels 在 art INI，section = Image 名（+0x1F8）。
    const char* artSection = (Image[0] != '\0') ? Image : section;
    DamageLevels = CCINIClass::INI_Art.ReadInteger(artSection, "DamageLevels", DamageLevels);

    // Tiberium 覆盖物强制把格子地形改矿（asm: mov dword ptr [esi+9Ch],6），
    // 且当 Land 缺省时默认成 Tiberium(5)。
    if (IsTiberium)
    {
        CellLandType = 6;                                     // asm: [esi+9Ch]=6
        if (LandValue == LandType::Clear)
            LandValue = LandType::Tiberium;                   // asm: [esi+298h]=5
    }

    // 非剧场且允许分配图像时，解析 "<Image>.SHP" 到 ImageSHP。
    if (!IsTheater && ImageAllocated)
    {
        LoadImage();                                          // asm: Finder -> [esi+0A4h]
    }

    // ------------------------------------------------------------------
    // 剩余 rules 键
    // ------------------------------------------------------------------
    uint8 rgb[3] = { RadarColor[0], RadarColor[1], RadarColor[2] };
    pINI->Get3Bytes(section, "RadarColor", rgb);
    RadarColor[0] = rgb[0];
    RadarColor[1] = rgb[1];
    RadarColor[2] = rgb[2];

    NoUseTileLandType = pINI->ReadBool(section, "NoUseTileLandType", NoUseTileLandType);
    IsVeinholeMonster = pINI->ReadBool(section, "IsVeinholeMonster", IsVeinholeMonster);
    IsVeins           = pINI->ReadBool(section, "IsVeins", IsVeins);
    ChainReaction     = pINI->ReadBool(section, "ChainReaction", ChainReaction);
    DrawFlat          = pINI->ReadBool(section, "DrawFlat", DrawFlat);
    IsARock           = pINI->ReadBool(section, "IsARock", IsARock);
    IsRubble          = pINI->ReadBool(section, "IsRubble", IsRubble);

    return true;
}

// ============================================================================
// SaveToINI
// ============================================================================
bool OverlayTypeClass::SaveToINI(CCINIClass* pINI)
{
    if (pINI == nullptr)
        return false;

    const char* section = this->ID;
    if (section == nullptr || section[0] == '\0')
        return false;

    ObjectTypeClass::SaveToINI(pINI);

    pINI->WriteLand(section, "Land", LandValue);
    pINI->WriteInteger(section, "Strength", Strength);
    pINI->WriteBool(section, "Wall", Wall);
    pINI->WriteBool(section, "Tiberium", IsTiberium);
    pINI->WriteBool(section, "Crate", Crate);
    pINI->WriteBool(section, "CrateTrigger", CrateTrigger);
    pINI->WriteBool(section, "Explodes", Explodes);
    pINI->WriteBool(section, "Overrides", Overrides);
    pINI->WriteInteger(section, "DamageLevels", DamageLevels);
    pINI->WriteBool(section, "NoUseTileLandType", NoUseTileLandType);
    pINI->WriteBool(section, "IsVeinholeMonster", IsVeinholeMonster);
    pINI->WriteBool(section, "IsVeins", IsVeins);
    pINI->WriteBool(section, "ChainReaction", ChainReaction);
    pINI->WriteBool(section, "DrawFlat", DrawFlat);
    pINI->WriteBool(section, "IsARock", IsARock);
    pINI->WriteBool(section, "IsRubble", IsRubble);

    return true;
}

// ============================================================================
// CRC
//
//  asm: OverlayTypeClass_GetCRC.  Hashes AbstractTypeClass state followed by
//    HeapID / LandType / DamageLevels / Strength (DWORDs) and the Wall /
//    Tiberium / Crate / CrateTrigger / Explodes flags.
// ============================================================================
void OverlayTypeClass::ComputeCRC(CRCEngine& crc) const
{
    ObjectTypeClass::ComputeCRC(crc);

    crc.AddData(&HeapID,       sizeof(HeapID));
    crc.AddData(&LandValue,    sizeof(LandValue));
    crc.AddData(&DamageLevels, sizeof(DamageLevels));
    crc.AddData(&Strength,     sizeof(Strength));
    crc.AddData(&Wall,         sizeof(Wall));
    crc.AddData(&IsTiberium,   sizeof(IsTiberium));
    crc.AddData(&Crate,        sizeof(Crate));
    crc.AddData(&CrateTrigger, sizeof(CrateTrigger));
    crc.AddData(&Explodes,     sizeof(Explodes));
}

int32 OverlayTypeClass::GetCRC() const
{
    CRCEngine crc;
    ComputeCRC(crc);
    return static_cast<int32>(crc.GetCRC());
}

// ============================================================================
// OverlayTypeClass - static lookup helpers
// ============================================================================

OverlayTypeClass* OverlayTypeClass::FindOrAllocate(const char* pID)
{
    if (!pID || !_strcmpi(pID, "<none>") || !_strcmpi(pID, "none")) return nullptr;
    OverlayTypeClass* found = Find(pID);
    if (found) return found;
    if (!Array) Init_Array();
    OverlayTypeClass* newItem = GameCreate<OverlayTypeClass>(pID);
    if (newItem && Array) Array->Add(newItem);
    return newItem;
}
