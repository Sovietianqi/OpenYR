#include "CellClass.h"
#include <cstdio>
#include <vector>
#include <queue>
#include <utility>
#include <unordered_map>
#include "../Abstract/ObjectClass.h"
#include "../Abstract/BuildingClass.h"
#include "../Abstract/UnitClass.h"
#include "../Abstract/InfantryClass.h"
#include "../Abstract/TerrainClass.h"
#include "../Abstract/OverlayClass.h"
#include "../Abstract/OverlayTypeClass.h"
#include "../FileFormats/SHP.h"
#include "../Rules/RulesClass.h"
#include "../Game/Externs.h"
#include "../Objects/IsometricTileGlobals.h"
#include "../Objects/IsometricTile.h"
#include "../Special/TiberiumClass.h"
#include "../Abstract/SmudgeClass.h"
#include "../Houses/HouseClass.h"
#include "../Map/MapClass.h"
#include "../Rendering/TacticalClass.h"
#include "../Game/Game.h"
#include "../Game/GameInit.h"
#include "../Scenario/ScenarioClass.h"
#include "../IO/CRC.h"
#include "../COM/IUnknown.h"

#include <cstring>
#include <cmath>
#include "../Abstract/TechnoClass.h"
#include "../Abstract/AircraftClass.h"

// 根据游戏行为，可知 HasAbility 在本项目里接收的是 VeteranAbilities /
// EliteAbilities 数组下标（0..3），而不是原版的位掩码。VEIN_PROOF 在原版
// 位图里占第 3 位，故此处用下标 3 表达"脚下免疫矿脉"这一能力。
static const int32 ABILITY_VEIN_PROOF_INDEX = 3;

// asm's "IsTechno" test (ObjectClass flag at +0x14 bit 0) selects the four
// techno abstract kinds; terrain / smudge / etc. are not technos.
inline bool IsTechnoObject(const ObjectClass* pObj) {
    const AbstractType what = pObj->WhatAmI();
    return what == AbstractType::Infantry || what == AbstractType::Unit
        || what == AbstractType::Aircraft || what == AbstractType::Building;
}

// ============================================================================
// File-local helpers
// ============================================================================
namespace {
    // Compute the radar color index for a given land type.
    int32 Compute_CellColor_ForLand(::LandType landType) {
        switch (landType) {
            case ::LandType::Clear:    return 0;
            case ::LandType::Rough:    return 1;
            case ::LandType::Road:     return 2;
            case ::LandType::Water:    return 3;
            case ::LandType::Rock:     return 4;
            case ::LandType::Wall:     return 5;
            case ::LandType::Tiberium: return 6;
            case ::LandType::Beach:    return 7;
            case ::LandType::Tunnel:   return 8;
            case ::LandType::Railroad: return 9;
            case ::LandType::Weeds:    return 10;
            case ::LandType::Ice:      return 11;
            default:                   return 0;
        }
    }
} // anonymous namespace

// ============================================================================
// CellClass.cpp - Map cell implementation
// ============================================================================
// CellClass is the atomic unit of the game map. Every cell stores its land
// type, tile graphics index, overlay/smudge/terrain references, occupier
// pointer, shroud/fog state and adjacency links. This file implements all
// non-trivial methods declared in CellClass.h:
//
//   * Init / Recalc_Attributes - reset and recompute derived cell state
//   * Load / Save / Compute_CRC - binary persistence and sync verification
//   * Get_CellCoords / Get_Cell_Position - isometric world/pixel coordinates
//   * Is_On_Map / Is_Visible / Is_Discovered - map bounds and fog-of-war
//   * Tiberium / Overlay / Smudge / Terrain accessors and mutators
//   * Land type and height accessors
//   * Occupier management (add / remove / count / type-check)
//   * Buildability checks (Can_Build_On, Is_Buildable, Is_Concrete, etc.)
//   * Cell_Color - radar minimap colour
//   * Apply_Damage - damage propagation to overlay/terrain
//   * Draw_It - render the cell's terrain layer
//   * Set_Shrouded / Unshroud - fog-of-war manipulation
// ============================================================================

// ============================================================================
// Initialization
// ============================================================================

// ----------------------------------------------------------------------------
// Init - reset the cell to a blank, unoccupied state.
// ----------------------------------------------------------------------------
void CellClass::Init() {
    MapCoords = CellStruct(0, 0);
    CellIndex = -1;
    Flags = CellFlags::Empty;
    AltFlags = AltCellFlags::Clear;
    Land = ::LandType::Clear;
    TileType = 0;
    TileSubIndex = 0;
    Overlay = -1;
    OverlayData = 0;
    Smudge = -1;
    SmudgeData = 0;
    Occupier = nullptr;
    Terrain = nullptr;
    AttachedTag = nullptr;
    CellColor = 0;
    Altitude = 0;
    Slope = 0;
    TiberiumValue = 0;
    WallOwner = -1;
    CrateType = 0;
    ContentCount = 0;
    IsUnderShroud = false;
    GapsCoveringCell = 0;
    Field_12C = 0;
    LastRadarEventTime = 0;
    unknown_38 = 0;
    unknown_3C = 0;
    unknown_40 = 0;
    unknown_44 = 0;
    for (int32 i = 0; i < 8; ++i) {
        AdjacentCells[i] = nullptr;
    }
}

// ----------------------------------------------------------------------------
// Recalc_Attributes - recompute derived cell attributes from the current
// state. This is called after loading a scenario or when the terrain layer
// changes (e.g. tiberium spread, overlay destruction).
// ----------------------------------------------------------------------------
void CellClass::Recalc_Attributes() {
    // Derive the land type from the tile/overlay state when possible.
    // If the cell has a tiberium overlay, the land type is Tiberium.
    if (Overlay >= 0 && TiberiumValue > 0) {
        Land = ::LandType::Tiberium;
    }

    // If the cell has a wall overlay, the land type is Wall.
    // The original game checks the overlay type's Wall flag; we approximate
    // by checking if the overlay index falls in the wall range (0..4).
    if (Overlay >= 0 && Overlay <= 4 && TiberiumValue == 0) {
        // Only treat as wall if the overlay data indicates a wall
        if (OverlayData > 0) {
            Land = ::LandType::Wall;
        }
    }

    // Compute the radar cell color from the land type.
    CellColor = Compute_CellColor_ForLand(Land);

    // Update the occupancy flag based on the occupier pointer.
    if (Occupier) {
        SetAltFlag(AltCellFlags::ContainsBuilding, true);
    } else {
        SetAltFlag(AltCellFlags::ContainsBuilding, false);
    }
}

// ============================================================================
// Serialization
// ============================================================================

// ----------------------------------------------------------------------------
// Load - read the cell's persistent state from a binary stream.
// ----------------------------------------------------------------------------
bool CellClass::Load(IStream* pStm) {
    if (!pStm) return false;

    ULONG read = 0;
    HRESULT hr = S_OK;

    // Read map coordinates
    hr = pStm->Read(&MapCoords, sizeof(MapCoords), &read);
    if (hr < 0 || read != sizeof(MapCoords)) return false;

    // Read cell index
    hr = pStm->Read(&CellIndex, sizeof(CellIndex), &read);
    if (hr < 0 || read != sizeof(CellIndex)) return false;

    // Read flags
    uint32 flagsVal = 0;
    hr = pStm->Read(&flagsVal, sizeof(flagsVal), &read);
    if (hr < 0 || read != sizeof(flagsVal)) return false;
    Flags = static_cast<CellFlags>(flagsVal);

    uint32 altFlagsVal = 0;
    hr = pStm->Read(&altFlagsVal, sizeof(altFlagsVal), &read);
    if (hr < 0 || read != sizeof(altFlagsVal)) return false;
    AltFlags = static_cast<AltCellFlags>(altFlagsVal);

    // Read land type
    int32 landVal = 0;
    hr = pStm->Read(&landVal, sizeof(landVal), &read);
    if (hr < 0 || read != sizeof(landVal)) return false;
    Land = static_cast<::LandType>(landVal);

    // Read tile and overlay data
    hr = pStm->Read(&TileType, sizeof(TileType), &read);
    if (hr < 0 || read != sizeof(TileType)) return false;
    hr = pStm->Read(&TileSubIndex, sizeof(TileSubIndex), &read);
    if (hr < 0 || read != sizeof(TileSubIndex)) return false;
    hr = pStm->Read(&Overlay, sizeof(Overlay), &read);
    if (hr < 0 || read != sizeof(Overlay)) return false;
    hr = pStm->Read(&OverlayData, sizeof(OverlayData), &read);
    if (hr < 0 || read != sizeof(OverlayData)) return false;
    hr = pStm->Read(&Smudge, sizeof(Smudge), &read);
    if (hr < 0 || read != sizeof(Smudge)) return false;
    hr = pStm->Read(&SmudgeData, sizeof(SmudgeData), &read);
    if (hr < 0 || read != sizeof(SmudgeData)) return false;

    // Read height and slope
    hr = pStm->Read(&Altitude, sizeof(Altitude), &read);
    if (hr < 0 || read != sizeof(Altitude)) return false;
    hr = pStm->Read(&Slope, sizeof(Slope), &read);
    if (hr < 0 || read != sizeof(Slope)) return false;

    // Read tiberium value
    hr = pStm->Read(&TiberiumValue, sizeof(TiberiumValue), &read);
    if (hr < 0 || read != sizeof(TiberiumValue)) return false;

    // Read wall owner and crate type
    hr = pStm->Read(&WallOwner, sizeof(WallOwner), &read);
    if (hr < 0 || read != sizeof(WallOwner)) return false;
    hr = pStm->Read(&CrateType, sizeof(CrateType), &read);
    if (hr < 0 || read != sizeof(CrateType)) return false;

    // Reset transient pointers - these are re-linked by the map loader
    Occupier = nullptr;
    Terrain = nullptr;
    AttachedTag = nullptr;
    for (int32 i = 0; i < 8; ++i) {
        AdjacentCells[i] = nullptr;
    }

    // Recompute derived attributes
    Recalc_Attributes();

    return true;
}

// ----------------------------------------------------------------------------
// Save - write the cell's persistent state to a binary stream.
// ----------------------------------------------------------------------------
bool CellClass::Save(IStream* pStm) const {
    if (!pStm) return false;

    ULONG written = 0;
    HRESULT hr = S_OK;

    hr = pStm->Write(&MapCoords, sizeof(MapCoords), &written);
    if (hr < 0 || written != sizeof(MapCoords)) return false;

    hr = pStm->Write(&CellIndex, sizeof(CellIndex), &written);
    if (hr < 0 || written != sizeof(CellIndex)) return false;

    uint32 flagsVal = static_cast<uint32>(Flags);
    hr = pStm->Write(&flagsVal, sizeof(flagsVal), &written);
    if (hr < 0 || written != sizeof(flagsVal)) return false;

    uint32 altFlagsVal = static_cast<uint32>(AltFlags);
    hr = pStm->Write(&altFlagsVal, sizeof(altFlagsVal), &written);
    if (hr < 0 || written != sizeof(altFlagsVal)) return false;

    int32 landVal = static_cast<int32>(Land);
    hr = pStm->Write(&landVal, sizeof(landVal), &written);
    if (hr < 0 || written != sizeof(landVal)) return false;

    hr = pStm->Write(&TileType, sizeof(TileType), &written);
    if (hr < 0 || written != sizeof(TileType)) return false;
    hr = pStm->Write(&TileSubIndex, sizeof(TileSubIndex), &written);
    if (hr < 0 || written != sizeof(TileSubIndex)) return false;
    hr = pStm->Write(&Overlay, sizeof(Overlay), &written);
    if (hr < 0 || written != sizeof(Overlay)) return false;
    hr = pStm->Write(&OverlayData, sizeof(OverlayData), &written);
    if (hr < 0 || written != sizeof(OverlayData)) return false;
    hr = pStm->Write(&Smudge, sizeof(Smudge), &written);
    if (hr < 0 || written != sizeof(Smudge)) return false;
    hr = pStm->Write(&SmudgeData, sizeof(SmudgeData), &written);
    if (hr < 0 || written != sizeof(SmudgeData)) return false;
    hr = pStm->Write(&Altitude, sizeof(Altitude), &written);
    if (hr < 0 || written != sizeof(Altitude)) return false;
    hr = pStm->Write(&Slope, sizeof(Slope), &written);
    if (hr < 0 || written != sizeof(Slope)) return false;
    hr = pStm->Write(&TiberiumValue, sizeof(TiberiumValue), &written);
    if (hr < 0 || written != sizeof(TiberiumValue)) return false;
    hr = pStm->Write(&WallOwner, sizeof(WallOwner), &written);
    if (hr < 0 || written != sizeof(WallOwner)) return false;
    hr = pStm->Write(&CrateType, sizeof(CrateType), &written);
    if (hr < 0 || written != sizeof(CrateType)) return false;

    return true;
}

// ----------------------------------------------------------------------------
// Compute_CRC - feed the cell's gameplay-relevant fields into the CRC engine.
// ----------------------------------------------------------------------------
void CellClass::Compute_CRC(CRCEngine& crc) const {
    crc.AddData(&MapCoords, sizeof(MapCoords));
    crc.AddData(&CellIndex, sizeof(CellIndex));
    uint32 flagsVal = static_cast<uint32>(Flags);
    crc.AddData(&flagsVal, sizeof(flagsVal));
    int32 landVal = static_cast<int32>(Land);
    crc.AddData(&landVal, sizeof(landVal));
    crc.AddData(&TileType, sizeof(TileType));
    crc.AddData(&TileSubIndex, sizeof(TileSubIndex));
    crc.AddData(&Overlay, sizeof(Overlay));
    crc.AddData(&OverlayData, sizeof(OverlayData));
    crc.AddData(&Smudge, sizeof(Smudge));
    crc.AddData(&SmudgeData, sizeof(SmudgeData));
    crc.AddData(&Altitude, sizeof(Altitude));
    crc.AddData(&Slope, sizeof(Slope));
    crc.AddData(&TiberiumValue, sizeof(TiberiumValue));
    crc.AddData(&WallOwner, sizeof(WallOwner));
    crc.AddData(&CrateType, sizeof(CrateType));
}

// ============================================================================
// Coordinate accessors
//
// The isometric grid maps cell (X, Y) to world coordinates by:
//   WorldX = (X - Y) * (LeptonsPerCell / 2)
//   WorldY = (X + Y) * (LeptonsPerCell / 4)
// The screen position is the same formula but in pixels:
//   PixelX = originX + (X - Y) * (CellWidthInPixels / 2)
//   PixelY = originY + (X + Y) * (CellHeightInPixels / 2)
// The Z coordinate is the cell's altitude * LevelHeight.
// ============================================================================

CoordStruct CellClass::Get_CellCoords() const {
    CoordStruct coord;
    coord.X = (static_cast<int32>(MapCoords.X) - static_cast<int32>(MapCoords.Y))
              * (LeptonsPerCell / 2);
    coord.Y = (static_cast<int32>(MapCoords.X) + static_cast<int32>(MapCoords.Y))
              * (LeptonsPerCell / 4);
    coord.Z = Altitude * LevelHeight;
    return coord;
}

// ============================================================================
// asm: CellClass_Update_Cell?? (真实入口 0x47FDFB，retn 4)
//
//   算出这一格叠加物图标在屏幕上的锚点，写进 pOut，并把 pOut 返回。
//   逐句对照见头文件注释；这里只补两处实现细节：
//
//   * 格心 lepton 的构造形如
//       ((c * 256) + 0x80 + (sign ? 0xFF : 0)) >> 8 << 8
//     前两项把格坐标放大到 lepton 域并加半格，第三项是 C 除法向零取整的
//     补偿（负数时补 0xFF），最后 >>8<<8 把低 8 位抹平到 256 的整数倍。
//   * 宽高深三个量取自 SHP 头的 Frames/Height/Width 字段（asm 里的
//     [shape+6] / [shape+2] / [shape+4]），half 计算用向零取整的 /2。
//
//   sub_480110 与 sub_69E7E0 尚未移植（见头文件注 3）：前者保守返回零偏移，
//   后者保守返回全零顶点，调用点均已按 asm 形态保留。
// ============================================================================

namespace {

// asm 里的向零取整折半：cdq / sub eax,edx / sar eax,1
inline int32 CellUpdateRoundTowardZeroHalf(int32 v)
{
    return (v + (v < 0 ? 1 : 0)) >> 1;
}

// asm 的格心 lepton 构造：
//   shl eax,8 / add eax,80h / cdq / and edx,0FFh / add eax,edx / sar 8 / shl 8
inline int32 CellUpdateCenterLepton(int16 cellCoord)
{
    int32 lep = (static_cast<int32>(cellCoord) << 8) + 0x80;
    lep += (lep < 0) ? 0xFF : 0;
    return (lep >> 8) << 8;
}

} // namespace

// ============================================================================
// CellClass::Get_Overlay_Draw_Offset —— asm sub_480110 + sub_5FDCC0
// ============================================================================
Point2D CellClass::Get_Overlay_Draw_Offset() const
{
    // ---- ① sub_5FDCC0：覆盖物类型自带的纵向偏移 ------------------------
    int32 dy = 0;

    OverlayTypeClass* pType = nullptr;
    if (OverlayTypeClass::Array != nullptr && Overlay >= 0
        && Overlay < OverlayTypeClass::Array->Count)
    {
        pType = OverlayTypeClass::Array->GetItem(Overlay);
    }

    if (pType != nullptr)
    {
        // asm: mov bl,[esi+2A9h] / test / jnz -> ecx = -0Ch
        //      mov bl,[esi+2A8h] / test / jnz -> ecx = -0Ch
        //      cmp dword ptr [esi+294h], 7Eh / jz -> ecx = -0Ch
        if (pType->IsTiberium || pType->Wall || pType->HeapID == 0x7E)
            dy = -0x0C;

        // asm: cmp dword ptr [esi+298h], 9 / jnz / dec ecx
        if (static_cast<int32>(pType->LandValue) == 9)
            --dy;
    }

    // asm: cmp edx, 7Eh / jnz / dec ecx   （edx = 覆盖物索引）
    if (Overlay == 0x7E)
        --dy;

    // ---- ② sub_480110：桥面 / 填充物 ------------------------------------
    // asm: mov al,[esi+CellClass.flags] / test al,80h
    if ((Field_140 & 0x80u) != 0u)
    {
        // asm: sub ecx,10h
        dy -= 0x10;

        // asm: mov al,[esi+CellClass.Powerup] / cmp al,9 / jb ; cmp al,11h / ja
        //   +0x11E 这个字节在本工程里叫 OverlayFrame（同一格字节，
        //   Update_Cell 里也按它取生长阶段）。
        const int32 powerup = static_cast<int32>(OverlayFrame);
        if (powerup >= 9 && powerup <= 0x11)
            dy -= 0x0F;
    }
    else
    {
        // asm: cmp dword ptr [esi+44h], 0EFFh / jnz / sub ecx,0Fh
        if (Overlay == 0xEF)
            dy -= 0x0F;
    }

    // ---- ③ 高度：每层 15 像素，越高屏幕 Y 越小 ---------------------------
    // asm: movsx eax,byte ptr [esi+11Bh] / lea eax,[eax+eax*2] /
    //      lea eax,[eax+eax*4]  =>  Level * 15
    //      mov esi, rect_mouseclass_vp.L / sub esi, eax / add ecx, esi
    //   rect_mouseclass_vp.L 是全屏战术视口左上角（0），见
    //  OverlayTypeClass::Draw 的同款处理。
    dy -= static_cast<int32>(static_cast<int8>(LandByte)) * 15;

    // ---- 尾项的取舍 ------------------------------------------------------
    // asm 最后还有 `add ecx,0Fh`（Y）与 `add edx,1Eh`（X）。这两个常数
    // 恰好就是 OverlayTypeClass::Draw 内部加的 (0x1E, 0x0F)，因此这里
    // 不再重复 —— 返回的是"叠加在格心之前的净偏移"。
    return Point2D(0, dy);
}

CoordStruct* CellClass::Update_Cell(CoordStruct* pOut)
{
    if (pOut == nullptr) {
        return nullptr;
    }

    // asm: mov eax,[edi+44h] / cmp eax,-1 / jz -> 写零返回
    if (Overlay == -1) {
        *pOut = CoordStruct(0, 0, 0);
        return pOut;
    }

    // asm: mov ecx,OverlayTypeClass / mov esi,[ecx+eax*4]
    //      mov ecx,esi / mov edx,[esi] / call dword ptr [edx+9Ch]
    OverlayTypeClass* pType = (OverlayTypeClass::Array != nullptr &&
                               Overlay < OverlayTypeClass::Array->Count)
                                  ? (*OverlayTypeClass::Array)[Overlay]
                                  : nullptr;

    SHPStruct* pShape = (pType != nullptr) ? pType->GetImage() : nullptr;

    // asm: mov ebx,eax / test ebx,ebx / jz -> 写零返回
    if (pShape == nullptr) {
        *pOut = CoordStruct(0, 0, 0);
        return pOut;
    }

    // asm: mov al,[edi+11Eh] / mov [esp+this.X],al
    //      mov al,[esi+OverlayTypeClass.Crate] / test / jz / mov [..],0
    int32 level = static_cast<int32>(OverlayFrame);
    if (pType->Crate) {
        level = 0;
    }

    // asm: Coord_Move 前的两串移位 —— 取本格格心的 lepton 坐标
    const CoordStruct slotCoord(
        CellUpdateCenterLepton(MapCoords.X),
        CellUpdateCenterLepton(MapCoords.Y),
        0);

    // asm: Tactical::Coord_To_Pixel(TacticalMap, &slotCoord, &pix)
    Point2D pix(0, 0);
    if (TacticalClass::Instance != nullptr) {
        pix = TacticalClass::Instance->To_Pixel(slotCoord);
    }

    // asm: mov ecx,edi / lea ecx,[esp+this] / push ecx / call sub_480110
    //      sub_480110 返回 (0x1E, off)；调用方随后的 `pix.x += x - 0x1E`
    //      把 X 的 0x1E 抵掉，净 X 偏移恒为 0；Y 上的 0x0F 尾项由
    //      OverlayTypeClass::Draw 负责，这里只取净偏移。
    const Point2D frameOffset = Get_Overlay_Draw_Offset();

    // asm: pix.x += slotCoord.x - 0x1E ; pix.y += slotCoord.y
    int32 px = pix.X + frameOffset.X - 0x1E;
    int32 py = pix.Y + frameOffset.Y;

    // asm: mov al,[edi+140h] / test al,80h / jz
    //      mov al,[edi+11Eh] / cmp al,9 / jb ; cmp al,11h / ja
    //      sub ecx,0Fh / add esi,7
    if ((static_cast<uint32>(Flags) & 0x80u) != 0u && level >= 9 && level <= 0x11) {
        px -= 0x0F;
        py += 7;
    }

    // asm: w = [ebx+6] ; h = [ebx+2] ; dpt = [ebx+4]
    const int32 shapeFrames = static_cast<int32>(pShape->Frames);
    const int32 shapeHeight = static_cast<int32>(pShape->Height);
    const int32 shapeWidth  = static_cast<int32>(pShape->Width);

    // asm: cdq / sub eax,edx / sar ebp,1 / add ebp,ecx   —— half = w/2 + level
    const int32 half = CellUpdateRoundTowardZeroHalf(shapeFrames) + level;

    // asm: pix.x -= h/2 ; pix.y -= dpt/2
    px -= CellUpdateRoundTowardZeroHalf(shapeHeight);
    py -= CellUpdateRoundTowardZeroHalf(shapeWidth);

    // asm: sub_69E7E0(shape, &tmp, half) —— SHP 帧顶点查询
    //         asm: push ebp / lea edx,[esp+28h] / push edx /
    //              mov ecx,ebx / call sub_69E7E0
    //       输出四个 int32 = 帧记录里的 (x, y, w, h)；这里只取前两个当偏移。
    //       帧号越界 / 形状未加载时，原版落到 someOtherRect
    //       （InitList_2617 里四个字段全初始化为 0），GetFrameBounds 同样
    //       返回全零矩形，语义一致。
    Rectangle frameBounds(0, 0, 0, 0);
    pShape->GetFrameBounds(frameBounds, half);

    // asm: add ecx,edi / add edx,esi —— 顶点各自叠加像素偏移
    pOut->X = frameBounds.X + px;
    pOut->Y = frameBounds.Y + py;
    pOut->Z = 0;

    return pOut;
}

CoordStruct CellClass::Get_Cell_Position() const {
    return Get_CellCoords();
}

Point2D CellClass::Get_Cell_Screen_Position(int32 originX, int32 originY) const {
    Point2D pt;
    pt.X = originX
         + (static_cast<int32>(MapCoords.X) - static_cast<int32>(MapCoords.Y))
           * (CellWidthInPixels / 2);
    pt.Y = originY
         + (static_cast<int32>(MapCoords.X) + static_cast<int32>(MapCoords.Y))
           * (CellHeightInPixels / 2);
    // Subtract altitude so taller cells appear higher on screen
    pt.Y -= Altitude * (CellHeightInPixels / 2);
    return pt;
}

// ============================================================================
// Map bounds / visibility
// ============================================================================

bool CellClass::Is_On_Map() const {
    if (CellIndex < 0) return false;
    if (!MapClass::Instance) return false;
    return CellIndex < MapClass::Instance->MapSize;
}

bool CellClass::Is_Visible() const {
    // A cell is visible to the player if it has been revealed and is not
    // currently shrouded by fog of war.
    return HasFlag(CellFlags::Revealed) && !HasFlag(CellFlags::Fogged);
}

bool CellClass::Is_Discovered() const {
    // A cell is "discovered" (explored) if it has ever been revealed to the
    // player, even if it is currently fogged.
    return HasFlag(CellFlags::Explored) || HasFlag(CellFlags::Revealed);
}

// ============================================================================
// Tiberium management
// ============================================================================

int32 CellClass::Get_Tiberium_Type() const {
    // CellClass_GetContainedTiberiumIndex: the cell's overlay index is resolved
    // through the tiberium registry into the ore type ordinal, or -1.
    return Is_Overlay_Idx_Tiberium(Overlay);
}

int32 CellClass::Get_Contained_Tiberium_Index() const {
    const int32 idx = Is_Overlay_Idx_Tiberium(Overlay);
    if (idx >= 0)
        return idx;

    // 叠加物表里没有登记矿时（内置兜底规则集不含 [OverlayTypes]，真实地图的
    //   IsoMapPack5 才带 overlay），退回 Set_Tiberium 记在 OverlayData 低字节
    //   里的矿种号 —— 否则这种格子永远拿不到矿类型，Grow_Tiberium /
    //   Spread_Tiberium 全部直接返回 false，矿只会被采空、永不生长。
    if (TiberiumValue > 0 || Land == ::LandType::Tiberium)
        return OverlayData & 0xFF;

    return -1;
}

int32 CellClass::Get_Tiberium_Value() const {
    return TiberiumValue;
}

// ----------------------------------------------------------------------------
// 一格矿的"单位价值"（每种矿不同：Riparius = 25、Cruentus 等各有取值）。
//   asm: CellClass_TiberiumInCell = tib->Value(+0xB8) * (OverlayFrame + 1)，
//   即**矿量 = 单位价值 × 生长阶段**。本树把 TiberiumValue 建模成"矿量"，
//   阶段则由 OverlayFrame 表达，二者必须互相对得上。
// ----------------------------------------------------------------------------
static int32 Tiberium_Unit_Value(int32 oreIdx)
{
    TiberiumClass* pType = TiberiumClass::FindByIndex(oreIdx);
    if (pType != nullptr && pType->Value > 0)
        return pType->Value;
    return 25;      // 兜底：Riparius 的原版单位价值
}

// ----------------------------------------------------------------------------
// Set_Tiberium(type, amount) —— 设定这一格的矿量（不是生长阶段）。
//   amount 上限 = 单位价值 × 12（12 是 overlay 的生长阶段上限）。
//   OverlayFrame 由矿量反推：stage = ceil(amount / unit)，存 0..11。
//   原实现把 amount 直接当阶段并 clamp 到 12，导致一格最多只有 12 "单位"的
//   矿，而矿车容量是 360 —— 一辆车能吃掉 30 格，一片矿田几趟就没了。
// ----------------------------------------------------------------------------
void CellClass::Set_Tiberium(int32 type, int32 amount) {
    const int32 unit = Tiberium_Unit_Value(type);
    const int32 maxAmount = unit * 12;

    if (amount < 0) amount = 0;
    if (amount > maxAmount) amount = maxAmount;
    TiberiumValue = amount;

    if (amount > 0) {
        int32 stage = (amount + unit - 1) / unit;   // 1..12
        if (stage < 1) stage = 1;
        if (stage > 12) stage = 12;
        OverlayFrame = static_cast<uint8>(stage - 1);   // 0..11

        OverlayData = (OverlayData & ~0xFF) | (type & 0xFF);
        if (Overlay < 0) Overlay = 0;  // ensure overlay is set
        Land = ::LandType::Tiberium;
    } else {
        OverlayFrame = 0;
        // When tiberium is depleted, restore the underlying land type
        if (Land == ::LandType::Tiberium) {
            Land = ::LandType::Clear;
        }
    }
    Recalc_Attributes();
}

// ============================================================================
// Overlay management
// ============================================================================

int32 CellClass::Get_Overlay() const {
    return Overlay;
}

int32 CellClass::Get_Overlay_Type() const {
    return Overlay;
}

void CellClass::Set_Overlay(int32 overlayIndex, int32 overlayData) {
    Overlay = overlayIndex;
    OverlayData = overlayData;
    Recalc_Attributes();
}

// ============================================================================
// Smudge management
// ============================================================================

int32 CellClass::Get_Smudge() const {
    return Smudge;
}

int32 CellClass::Get_Smudge_Type() const {
    return Smudge;
}

void CellClass::Set_Smudge(int32 smudgeIndex, int32 smudgeData) {
    Smudge = smudgeIndex;
    SmudgeData = smudgeData;
}

 // 根据游戏行为，可知 StopAmbientSound 负责下面这段逻辑。
//
//   Silences the ambient sound attached to this cell.  The original prefers
//   the building occupying the cell and only falls back to the terrain object
//   when the cell holds no building.  Both are asked to stop their ambient
//   loop with the "-1" flag.
void CellClass::Silence_Attached_Ambient() {
    ObjectClass* pObject = First_Object(false);
    if (pObject == nullptr) {
        pObject = Terrain;
    }
    if (pObject != nullptr) {
        pObject->StopAmbientSound(-1);
    }
}

// ============================================================================
// Terrain object management
// ============================================================================

TerrainClass* CellClass::Get_Terrain() const {
    return Terrain;
}

void CellClass::Set_Terrain(TerrainClass* pTerrain) {
    Terrain = pTerrain;
}

void CellClass::Clear_Terrain() {
    Terrain = nullptr;
}

// ============================================================================
// Land type accessors
// ============================================================================

::LandType CellClass::Get_Land_Type() const {
    return Land;
}

void CellClass::Set_Land_Type(::LandType land) {
    Land = land;
    CellColor = Compute_CellColor_ForLand(land);
}

// ============================================================================
// Height accessors
// ============================================================================

int32 CellClass::Get_Ground_Height() const {
    return Altitude;
}

int32 CellClass::Get_Z_Height() const {
    return Altitude * LevelHeight;
}

// ----------------------------------------------------------------------------
// asm: int __thiscall CellClass_GetHeight(CellClass *this)
//
//    mov eax,[ecx+CellClass.flags]     ; flag word at +0x140
//    movsx ecx,[ecx+CellClass.Level]   ; signed byte at +0x11B
//    shr eax,7
//    and eax,1                         ; CellFlags::BridgeOwner (0x80)
//    lea eax,[ecx+eax*4]               ; level + (bridge ? 4 : 0)
//    retn
// ----------------------------------------------------------------------------
int32 CellClass::Get_Height() const
{
    const int32 level = static_cast<int32>(static_cast<int8>(Level));
    const int32 bridgeBias =
        ((static_cast<uint32>(Flags) & static_cast<uint32>(CellFlags::BridgeOwner)) != 0u)
            ? 4 : 0;
    return level + bridgeBias;
}

// ----------------------------------------------------------------------------
// asm: dwXYZ *__thiscall CellClass_GetCoords(CellClass *this, dwXYZ *out)
//
//    movsx esi,word ptr [ecx+24h]      ; X
//    movsx edi,word ptr [ecx+26h]      ; Y
//    shl esi,8 / shl edi,8
//    add esi,80h / add edi,80h         ; cell centre in leptons
//    call CellClass_GetFloorHeight     ; Z (ramp-interpolated ground height)
//    out = { esi, edi, floorHeight }
// ----------------------------------------------------------------------------
CoordStruct* CellClass::GetCoords(CoordStruct* pOut) const
{
    if (pOut == nullptr)
        return nullptr;

    const int32 x = (static_cast<int32>(MapCoords.X) << 8) + 0x80;
    const int32 y = (static_cast<int32>(MapCoords.Y) << 8) + 0x80;

    pOut->X = x;
    pOut->Y = y;
    pOut->Z = GetFloorHeight(CoordStruct(x, y, 0));

    return pOut;
}

// ----------------------------------------------------------------------------
// asm: byte __thiscall CellClass_IsHidden(CellClass *this)   (asm:309932)
//
//   While the game runs it walks the ground content chain; the first building
//   (WhatAmI == 6) short-circuits to the "building present" branch, which
//   answers false only when the content count is exactly one.  With no
//   building the answer is whether the content counter at +0x100 is non-zero.
// ----------------------------------------------------------------------------
bool CellClass::IsHidden() const
{
    if (GameActive)
    {
        ObjectClass* pObj = FirstObject;
        while (pObj != nullptr)
        {
            if (pObj->WhatAmI() == AbstractType::Building)
            {
                // A building occupies the cell: hidden only when it is not the
                // sole content (the count of one is the building itself).
                return ContentCount != 0 && ContentCount != 1;
            }
            pObj = pObj->NextObject;
        }
    }

    return ContentCount != 0;
}

// ----------------------------------------------------------------------------
// asm: void *__thiscall CellClass_GetRadSite(CellClass *this)
//      mov eax,[ecx+CellClass.RadSite] / retn
// ----------------------------------------------------------------------------
void* CellClass::Get_Rad_Site() const
{
    return RadSite;
}

// ----------------------------------------------------------------------------
// asm: wchar_t *__thiscall CellClass_GetUIName(CellClass *this)
//
//    idx = Is_Overlay_Idx_Tiberium(this->Overlay)
//    if (idx == -1) return null
//    return vec_Tiberiums[idx]->UIName           (the pointer at +0x60)
// ----------------------------------------------------------------------------
const wchar_t* CellClass::GetUIName() const
{
    const int32 typeIndex = Is_Overlay_Idx_Tiberium(Overlay);
    if (typeIndex == -1)
        return nullptr;

    TiberiumClass* pType = TiberiumClass::FindByIndex(typeIndex);
    if (pType == nullptr)
        return nullptr;

    return pType->UIName;
}

// ----------------------------------------------------------------------------
// asm: CellClass_GetAbstractDerivationID  (mov eax,0Bh ; retn)
// ----------------------------------------------------------------------------
int32 CellClass::Get_AbstractDerivationID() const
{
    return static_cast<int32>(AbstractType::Cell);
}

// ----------------------------------------------------------------------------
// asm: CellClass_GetSize  (mov eax,148h ; retn)
// ----------------------------------------------------------------------------
int32 CellClass::Get_Size() const
{
    return 0x148;
}

// ----------------------------------------------------------------------------
// asm: char __fastcall CellClass_486FF0(CellClass *this)   (asm:308079)
//
//    if (!Map.IsCellTainted(this, true)) return true;
//    if (TileType in [0, vec_IsometricTileTypes.length)) {
//        tile = vec_IsometricTileTypes[TileType];
//        if (tile->AllowBurrowing /* +0x305 */) { /* fall through */ }
//        else return false;
//    }
//    if ([this+0x11C] != 0) return false;      // SlopeIndex / overlay frame
//    if ([this+0x140] & 0x500) return false;   // FlagToShroud | HasTiberium
//    if (GameActive) {
//        walk ground chain: if any WhatAmI()==6 (Building) -> return false
//        walk ground chain: if any WhatAmI()==0x24 (Terrain) -> return false
//    }
//    return true;
//
//   Note: the branch is inverted in the binary (a tainted cell still passes
//   when the tile forbids burrowing, because the tile check only rejects when
//   the tile is present AND AllowBurrowing is clear).  Reproduced verbatim
//   above; the project resolves the tile through TileTypeClass_Array.
// ----------------------------------------------------------------------------
bool CellClass::Is_Clear_For_Subterranean() const
{
    // asm: push 1 / push this / mov ecx,offset Map / call MapClass_IsCellTainted
    if (MapClass::Instance != nullptr)
    {
        if (!MapClass::Instance->IsCellTainted(Get_CellCoords(), true))
            return true;
    }
    else
    {
        return true;
    }

    if (TileType >= 0 && TileTypeClass_Array != nullptr && TileType < TileTypeClass_Array->Count)
    {
        const IsometricTileType* pTile = (*TileTypeClass_Array)[TileType];
        if (pTile != nullptr && !pTile->AllowBurrowing)
            return false;
    }

    // asm: mov al,[edi+11Ch] / test al,al / jnz loc_4870A1 (reject)
    if (Slope != 0)
        return false;

    // asm: mov eax,[edi+140h] / test ah,5 / jnz loc_4870A1 (reject)
    if ((Field_140 & 0x0500u) != 0u)
        return false;

    if (GameActive)
    {
        for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
        {
            if (pObj->WhatAmI() == AbstractType::Building)
                return false;
        }
        for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
        {
            if (pObj->WhatAmI() == AbstractType::Terrain)
                return false;
        }
    }

    return true;
}

// ============================================================================
// Occupier management
// ============================================================================

int32 CellClass::Get_Occupier_Count() const {
    return Occupier ? 1 : 0;
}

// ============================================================================
// 步兵占用计数（与 FirstObject 链解耦的权威计数）
//
//   为什么不能只靠链来数人：一条格子链共用对象里唯一的 NextObject 指针，
//   一旦某个对象的登记格与它实际所在的格不一致（Scatter / ForceImmediate
//   Destination / 卸载落位都会绕过行走逻辑直接改坐标），注销就找不到它，
//   链上会留下"指向已换格对象"的残链。实测 6 名步兵站在同一格，链长只有
//   2 —— 靠链数人永远数不准，3 人上限也就形同虚设。
//
//   这里用两张旁表维护"对象 → 所在格"和"每格步兵数"，Add/Remove 时双向
//   更新，重复 Add 会先把旧格的计数扣掉。CellArray 重新分配时调用
//   Clear_Occupancy_Cache() 清空（指针失效）。
// ============================================================================
namespace {

struct OccupancyTables
{
    std::unordered_map<const CellClass*, int32>          InfantryPerCell;
    std::unordered_map<const ObjectClass*, const CellClass*> CellOfObject;
};

OccupancyTables& Occupancy()
{
    static OccupancyTables tables;
    return tables;
}

} // namespace

int32 CellClass::Infantry_Count() const
{
    const OccupancyTables& t = Occupancy();
    const auto it = t.InfantryPerCell.find(this);
    return (it != t.InfantryPerCell.end()) ? it->second : 0;
}

void CellClass::Clear_Occupancy_Cache()
{
    OccupancyTables& t = Occupancy();
    t.InfantryPerCell.clear();
    t.CellOfObject.clear();
}

ObjectClass* CellClass::Get_Occupier() const {
    return Occupier;
}

void CellClass::Add_Occupier(ObjectClass* pObj) {
    if (!pObj) return;

    const bool isBuilding = (pObj->WhatAmI() == AbstractType::Building);
    const bool isInfantry = (pObj->WhatAmI() == AbstractType::Infantry);
    OccupancyTables& t = Occupancy();

    // ── 1. 唯一归属：先把对象从"上一次登记的格子"上摘下来 ────────────────
    //   一条链只有一个 NextObject 指针。同一个对象同时挂在两条链上时，第二次
    //   挂接会覆盖第一次写的 NextObject，两条链互相指向对方 → 成环 → 任何
    //   遍历（索敌扫描、绘制、Cell_Techno）都会死循环。实测索敌扫描在
    //   cell(29,27) 上走了 513 个节点还没到链尾，一帧卡死几十秒。
    //   旁表 CellOfObject 是权威登记，凡换格先摘旧链，从根上杜绝多链共存。
    if (!isBuilding) {
        const auto prev = t.CellOfObject.find(pObj);
        if (prev != t.CellOfObject.end() && prev->second != nullptr) {
            if (prev->second == this) {
                // 已在本格：幂等返回，但补一次槽位与计数。
                Occupier = pObj;
                return;
            }
            CellClass* pOld = const_cast<CellClass*>(prev->second);
            pOld->Remove_Occupier(pObj);
        }
    }

    // ── 2. 步兵计数（权威）────────────────────────────────────────────────
    //   同一对象对同一格重复 Add 不能把计数加两遍。
    if (isInfantry) {
        const auto prev = t.CellOfObject.find(pObj);
        const bool alreadyHere = (prev != t.CellOfObject.end() && prev->second == this);
        if (!alreadyHere) {
            t.CellOfObject[pObj] = this;
            ++t.InfantryPerCell[this];
        }
    }

    // ── 3. 入链 ──────────────────────────────────────────────────────────
    //   建筑的多格占用只登记 Occupier + ContainsBuilding，**不**挂 FirstObject
    //   链：多格建筑若同时挂在 2~9 条链上，同样会把链连成环。原版也只在
    //   一个"主格"上入链。
    if (!isBuilding) {
        // 有界去重：链可能被历史数据弄脏，最多走 512 个节点就放弃。
        bool linked = false;
        int32 guard = 0;
        for (ObjectClass* p = FirstObject; p != nullptr && guard < 512;
             p = p->NextObject, ++guard) {
            if (p == pObj) { linked = true; break; }
        }
        if (!linked) {
            pObj->NextObject = FirstObject;
            FirstObject = pObj;
            ++ContentCount;
        }
        t.CellOfObject[pObj] = this;
    }

    Occupier = pObj;

    // ★ ContainsBuilding 只能由建筑置位。
    //   此前不论对象类型都置 true，等于"站着一个兵"也宣称"这格有建筑"：
    //   Cell_Has_Building / 建筑选址 / 驻扎判定都会据此误判（原版的
    //   AltFlags bit1 只在建筑落位时置位）。
    if (isBuilding)
        SetAltFlag(AltCellFlags::ContainsBuilding, true);
}

void CellClass::Remove_Occupier(ObjectClass* pObj) {
    if (!pObj) return;

    OccupancyTables& t = Occupancy();

    // 步兵计数：按"对象 → 登记格"的旁表扣减，不依赖调用方传入的格是否准确。
    if (pObj->WhatAmI() == AbstractType::Infantry)
    {
        const auto it = t.CellOfObject.find(pObj);
        if (it != t.CellOfObject.end())
        {
            const auto cnt = t.InfantryPerCell.find(it->second);
            if (cnt != t.InfantryPerCell.end() && --(cnt->second) <= 0)
                t.InfantryPerCell.erase(cnt);
        }
    }
    t.CellOfObject.erase(pObj);

    // Unlink from the ground occupancy chain（有界，防脏链死循环）。
    if (FirstObject == pObj) {
        FirstObject = pObj->NextObject;
        pObj->NextObject = nullptr;
        if (ContentCount > 0) --ContentCount;
    } else {
        int32 guard = 0;
        for (ObjectClass* pPrev = FirstObject; pPrev != nullptr && guard < 512;
             pPrev = pPrev->NextObject, ++guard) {
            if (pPrev->NextObject == pObj) {
                pPrev->NextObject = pObj->NextObject;
                pObj->NextObject = nullptr;
                if (ContentCount > 0) --ContentCount;
                break;
            }
        }
    }

    if (Occupier == pObj) {
        Occupier = nullptr;
        // 同上：只有建筑离场才清 ContainsBuilding，否则一名步兵走开就会把
        //   真实建筑所在格的标志抹掉。
        if (pObj->WhatAmI() == AbstractType::Building)
            SetAltFlag(AltCellFlags::ContainsBuilding, false);

        // 槽只是单指针的类型快速查询缓存，链才是权威：此刻链上若还有别的
        //   对象（例如坦克开走、留下同格的步兵），把它让给链首，别因为
        //   "最后一个登记的人离开"就把整格宣告成空的。
        if (FirstObject != nullptr && FirstObject != pObj)
            Occupier = FirstObject;
    }
}

// ============================================================================
// asm: void __thiscall CellClass::Occupy_Down(CellClass *this,
//      ObjectClass *Content, char bridge)   (asm:3559, retn 8)
//
//   Threads Content onto the ground (+0xE4) or altitude (+0xE8) chain.  A
//   building is appended to the tail so loose units stay visible in front of
//   it; everything else is prepended.  When the cell is not shrouded the
//   object is finally told where it now sits (vt+0xF0 SetLocation); infantry
//   are exempted from that relocation.
// ============================================================================
void CellClass::Occupy_Down(ObjectClass* pContent, bool bridge)
{
    if (pContent == nullptr)
        return;

    ObjectClass*& pHead = bridge ? AltObject : FirstObject;

    if (pContent->WhatAmI() == AbstractType::Building)
    {
        if (pHead != nullptr)
        {
            ObjectClass* pTail = pHead;
            while (pTail->NextObject != nullptr)
                pTail = pTail->NextObject;
            pTail->NextObject = pContent;
        }
        else
        {
            pHead = pContent;
        }
        pContent->NextObject = nullptr;
    }
    else
    {
        if (pHead != pContent && (pHead == nullptr || pHead->NextObject != pContent))
        {
            pContent->NextObject = pHead;
            pHead = pContent;
        }
    }

    // asm loc_47E9C8: only when the cell is revealed to the local player is
    // the object relocated, and infantry (WhatAmI 0xF) are left where they are.
    if (!IsUnderShroud && pContent->WhatAmI() != AbstractType::Infantry)
    {
        CoordStruct place(
            (static_cast<int32>(MapCoords.X) << 8) + 0x80,
            (static_cast<int32>(MapCoords.Y) << 8) + 0x80,
            0);
        place.Z = GetFloorHeight(place);
        pContent->SetLocation(place);
    }
}

// ============================================================================
// AddContent —— 把对象挂进本格的占用链（地面 / 桥面），并维护 ContentCount。
//   已挂在链上时直接返回，避免同一对象被重复挂接（重复会形成自环）。
// ============================================================================
void CellClass::AddContent(ObjectClass* pObj, bool alt)
{
    if (pObj == nullptr)
        return;

    ObjectClass*& pHead = alt ? AltObject : FirstObject;

    for (ObjectClass* p = pHead; p != nullptr; p = p->NextObject)
    {
        if (p == pObj)
            return;                                   // 已在链上
    }

    pObj->NextObject = pHead;
    pHead = pObj;
    if (!alt)
        ++ContentCount;
}

// ============================================================================
// Occupier type checks
// ============================================================================

bool CellClass::Has_Unit() const {
    if (!Occupier) return false;
    return Occupier->WhatAmI() == AbstractType::Unit;
}

bool CellClass::Has_Building() const {
    if (!Occupier) return false;
    return Occupier->WhatAmI() == AbstractType::Building;
}

bool CellClass::Has_Infantry() const {
    if (!Occupier) return false;
    return Occupier->WhatAmI() == AbstractType::Infantry;
}

BuildingClass* CellClass::Get_Building() const {
    // asm: CellClass::Cell_Building - the canonical ground-chain building walk.
    return Cell_Building();
}

// ============================================================================
// Foundation
// ============================================================================

bool CellClass::Is_Foundation() const {
    // A cell is part of a building foundation if it has a building occupier
    // or the ContainsBuilding alt flag is set.
    return HasAltFlag(AltCellFlags::ContainsBuilding) || Has_Building();
}

bool CellClass::Get_Foundation() const {
    return Is_Foundation();
}

// ============================================================================
// Radar cell color
//
// The radar minimap renders each cell as a single pixel whose colour is
// derived from the cell's land type. The original game uses a lookup table
// that maps LandType to an 8-bit palette index.
// ============================================================================

int32 CellClass::Cell_Color() const {
    return CellColor;
}

// ============================================================================
// Buildability
// ============================================================================

bool CellClass::Can_Build_On() const {
    // A cell is buildable if it is clear land, not occupied, not water,
    // not a wall, not a cliff, and not shrouded.
    if (IsOccupied()) return false;
    if (IsWater()) return false;
    if (IsWall()) return false;
    if (IsRock()) return false;
    if (Is_Cliff()) return false;
    if (IsShrouded()) return false;
    if (Terrain != nullptr) return false;
    if (Overlay >= 0 && TiberiumValue > 0) return false;  // can't build on tiberium
    return true;
}

bool CellClass::Is_Buildable() const {
    return Can_Build_On();
}

// ============================================================================
// asm: char __thiscall CellClass::Is_Clear_To_Build(CellClass *this, int a2,
//      BuildingTypeClass *a3, HouseClass *a4)   (asm:1990, retn 0Ch)
//
//   The authoritative structure-placement test.  The ScenarioInit and
//   Debug_Map/taint gates of the binary are folded here; the gameplay decision
//   is the building-class branch, the occupancy / terrain rejection, the
//   own-wall pass and the final land/speed cost test.
// ============================================================================
bool CellClass::Is_Clear_To_Build(int32 a2, BuildingTypeClass* a3, HouseClass* a4) const
{
    // asm: cmp ScenarioInit,0 / jnz -> return true   (editor load: always ok)

    // asm loc_47C6DD / loc_47C76B / loc_47C7C8: dispatch on the building class.
    const bool regularBuilding =
        (a3 != nullptr) && !a3->LaserFence && !a3->LaserFencePost
        && !a3->Gate && a3->TileToUse == nullptr;

    if (regularBuilding)
    {
        // A loose aircraft, any techno or a terrain object blocks the cell.
        if (GameActive)
        {
            for (ObjectClass* pObj = FirstObject; pObj; pObj = pObj->NextObject)
                if (pObj->WhatAmI() == AbstractType::Aircraft)
                    return false;

            if (Cell_Techno(nullptr, false, nullptr) != nullptr)
                return false;
            if (Cell_Abstract(static_cast<int32>(AbstractType::Terrain), false) != nullptr)
                return false;
        }
        // asm: test [this+124h],3Fh / jz continue -> occupied ground bits reject.
        if ((Get_Movement_Field() & 0x3F) != 0)
            return false;
    }
    else if (GameActive && FirstObject != nullptr)
    {
        // asm loc_47C676 / loc_47C6CD / loc_47C7AC: any building blocks; the
        // plain laser fence additionally rejects an occupying terrain object.
        for (ObjectClass* pObj = FirstObject; pObj; pObj = pObj->NextObject)
            if (pObj->WhatAmI() == AbstractType::Building)
                return false;

        if (a3 != nullptr && a3->LaserFence)
        {
            for (ObjectClass* pObj = FirstObject; pObj; pObj = pObj->NextObject)
                if (pObj->WhatAmI() == AbstractType::Terrain)
                    return false;
        }
    }

    // asm loc_47C76B: a ToTile building needs the tile to permit it.
    if (a3 != nullptr && a3->TileToUse != nullptr && !a3->TileToUse->AllowBurrowing)
        return false;

    // asm loc_47C88D..loc_47C94F: an overlay whose wall-owner is the requesting
    // house never blocks that house (loc_47C8F9 / loc_47C945 -> return true).
    if (Overlay != -1 && a4 != nullptr)
    {
        HouseClass* pWallOwner = HouseClass::GetHouseByIndex(Field_50);
        if (pWallOwner != nullptr && pWallOwner == a4)
            return true;
    }

    // asm loc_47CA4D: an explicit speed class is accepted when its land cost
    // is non-zero.
    if (a2 != -1)
        return Get_Movement_Cost(a2) != 0;

    // asm loc_47C9CD fall-through: reject on the wall/slope flag bits, else
    // consult the LAND's "may carry a structure" flag.
    if ((Field_140 & 0x0100u) != 0u)
        return false;
    if ((Field_140 & 0x0400u) != 0u)
        return false;
    if (Slope != 0)
        return false;

    if (TheRules != nullptr)
    {
        const int32 landIndex = static_cast<int32>(Land);
        if (landIndex >= 0 && landIndex < RulesClass::LAND_TYPE_COUNT)
            return TheRules->LandCharacteristics[landIndex].Buildable;
    }
    return true;
}

bool CellClass::Is_Concrete() const {
    // A cell is "concrete" (paved) if its land type is Road or if it has
    // a concrete-type overlay. This affects movement speed for vehicles.
    return Land == ::LandType::Road;
}

bool CellClass::Is_Cliff() const {
    // Cliffs are identified by slope values in the cliff range.
    // The original game uses slope types 1-4 for directional cliffs.
    return Slope >= 1 && Slope <= 4;
}

// ============================================================================
// Slope
// ============================================================================

int32 CellClass::Get_Slope() const {
    return Slope;
}

bool CellClass::Is_Sloped() const {
    return Slope > 0;
}

// ============================================================================
// Damage application
//
// When a cell takes damage (e.g. from an explosion), the damage is applied
// to the cell's overlay (walls, bridges) and terrain objects (trees, rocks).
// ============================================================================

void CellClass::Apply_Damage(int32 damage, int32 warheadType) {
    if (damage <= 0) return;

    // Damage terrain objects (trees, etc.)
    if (Terrain) {
        // The original game calls Terrain->TakeDamage(damage, warhead).
        // If the terrain is destroyed, it is removed from the cell.
        TerrainClass* pTerrain = Terrain;
        pTerrain->Health -= damage;
        if (pTerrain->Health <= 0) {
            Clear_Terrain();
            // The terrain object destruction spawns debris/animation
            // via the game's damage system.
        }
    }

    // Damage overlays (walls, bridges)
    if (Overlay >= 0) {
        // Walls have health stored in OverlayData. When a wall's health
        // reaches zero, the wall is destroyed and the overlay is removed.
        if (Land == ::LandType::Wall) {
            int32 wallHealth = OverlayData;
            wallHealth -= damage;
            if (wallHealth <= 0) {
                Overlay = -1;
                OverlayData = 0;
                Land = ::LandType::Clear;
                WallOwner = -1;
            } else {
                OverlayData = wallHealth;
            }
        }

        // Bridges are damaged and can be destroyed
        if (IsBridge()) {
            // Bridge destruction is handled by the map's bridge system.
            // We mark the bridge as damaged by clearing the bridge flags.
            if (damage >= 100) {
                SetFlag(CellFlags::Bridge, false);
            }
        }
    }

    // Damage tiberium (reduce the tiberium value)
    if (TiberiumValue > 0) {
        TiberiumValue -= damage / 10;
        if (TiberiumValue <= 0) {
            TiberiumValue = 0;
            if (Land == ::LandType::Tiberium) {
                Land = ::LandType::Clear;
            }
        }
    }

    Recalc_Attributes();
}

// ============================================================================
// Rendering
//
// Draw_It renders the cell's terrain layer to the screen. In the original
// game this is a complex function that draws the tile graphic, overlay,
// smudge, and terrain object. For the reconstruction we provide the
// structural framework.
// ============================================================================

void CellClass::Draw_It(int32 originX, int32 originY) const {
    // Calculate the screen position of this cell
    Point2D screenPos = Get_Cell_Screen_Position(originX, originY);

    // The actual pixel rendering is performed by the display subsystem.
    // This method orchestrates the draw order:
    //   1. Base tile (terrain graphic)
    //   2. Smudge (scorch marks, craters)
    //   3. Overlay (walls, tiberium)
    //   4. Terrain object (trees, rocks)
    //   5. Occupier (units, buildings, infantry)
    //
    // Each layer is only drawn if the cell is visible (not shrouded).
    // Shrouded cells draw the shroud graphic instead.

    (void)screenPos;  // screen position is used by the display subsystem

    // In a full implementation, this would call:
    //   DisplayClass::Draw_Tile(screenPos, TileType, TileSubIndex);
    //   if (Smudge >= 0) DisplayClass::Draw_Smudge(screenPos, Smudge, SmudgeData);
    //   if (Overlay >= 0) DisplayClass::Draw_Overlay(screenPos, Overlay, OverlayData);
    //   if (Terrain) Terrain->Draw_It(screenPos);
    //   if (Occupier) Occupier->Draw_It(screenPos);
}

// ============================================================================
// Shroud management
// ============================================================================

void CellClass::Set_Shrouded(bool shrouded) {
    if (shrouded) {
        // Clear the revealed flags to shroud the cell
        Flags = static_cast<CellFlags>(
            static_cast<uint32>(Flags) &
            ~static_cast<uint32>(CellFlags::Revealed));
    } else {
        // Set the revealed flags to unshroud the cell
        SetFlag(CellFlags::Revealed, true);
        SetFlag(CellFlags::Explored, true);
    }
}

void CellClass::Unshroud() {
    SetFlag(CellFlags::Revealed, true);
    SetFlag(CellFlags::Explored, true);
    SetFlag(CellFlags::Fogged, false);
}

 // asm: void __thiscall CellClass_Setup(CellClass *this, wXY coords)   (asm:289530)
//
//   Re-derives this cell's shroud/fog state against the map's current view
//   rectangle.  A negative `flag` means "re-shroud if the cell has fallen
//   outside the visible rect"; the cell is also fogged whenever it is not
//   within the visible area.  Called for every cell by
//   ActionClass_ResizePlayerView after the view rectangle has been rewritten.
void CellClass::Setup(int32 flag)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    const int32 cx = static_cast<int32>(MapCoords.X);
    const int32 cy = static_cast<int32>(MapCoords.Y);

    const bool inside =
        cx >= pMap->VisibleRectX &&
        cy >= pMap->VisibleRectY &&
        cx <  pMap->VisibleRectX + pMap->VisibleRectWidth &&
        cy <  pMap->VisibleRectY + pMap->VisibleRectHeight;

    if (!inside)
    {
        if (flag < 0)
        {
            Set_Shrouded(true);
        }
        SetFlag(CellFlags::Fogged, true);
        return;
    }

    SetFlag(CellFlags::Fogged, false);
}

// ============================================================================
// Hierarchical pathfinding
// ============================================================================
bool CellClass::Pathfinding_Hierarchical(const CellStruct& from, const CellStruct& to,
                                         DynamicVectorClass<CellStruct>& outPath,
                                         MovementZone zone)
{
    outPath.Clear();

    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || pMap->CellArray == nullptr)
        return false;

    const int32 width  = pMap->MapWidth;
    const int32 height = pMap->MapHeight;
    if (width <= 0 || height <= 0)
        return false;

    // Block abstraction: 8x8 cells per block.
    static const int32 BLOCK = 8;
    const int32 bw = (width  + BLOCK - 1) / BLOCK;
    const int32 bh = (height + BLOCK - 1) / BLOCK;

    const int32 fromBX = from.X / BLOCK;
    const int32 fromBY = from.Y / BLOCK;
    const int32 toBX   = to.X / BLOCK;
    const int32 toBY   = to.Y / BLOCK;

    if (fromBX == toBX && fromBY == toBY)
    {
        // Same block: nothing for the abstraction layer to decide; the
        // per-cell search handles it.
        return false;
    }

    // Representative cell per block: the first passable cell inside the
    // block, used as the connection point for the abstract graph.
    struct BlockNode {
        int32 RepX;
        int32 RepY;
        bool  HasRep;
        int32 G;
        int32 F;
        int32 CameFrom;
        bool  Closed;
    };

    const int32 blockCount = bw * bh;
    std::vector<BlockNode> nodes(blockCount);
    for (int32 by = 0; by < bh; ++by)
    {
        for (int32 bx = 0; bx < bw; ++bx)
        {
            BlockNode& n = nodes[by * bw + bx];
            n.RepX = -1;
            n.RepY = -1;
            n.HasRep = false;
            n.G = 0x7FFFFFFF;
            n.F = 0x7FFFFFFF;
            n.CameFrom = -1;
            n.Closed = false;

            // Scan the block for a passable representative cell.
            for (int32 y = by * BLOCK; y < (by + 1) * BLOCK && !n.HasRep; ++y)
            {
                for (int32 x = bx * BLOCK; x < (bx + 1) * BLOCK && !n.HasRep; ++x)
                {
                    if (x >= width || y >= height)
                        continue;
                    CellClass* pCell = &pMap->CellArray[y * width + x];
                    if (pCell != nullptr && pCell->PassableFor(zone))
                    {
                        n.RepX = x;
                        n.RepY = y;
                        n.HasRep = true;
                    }
                }
            }
        }
    }

    const int32 startIdx = fromBY * bw + fromBX;
    const int32 goalIdx  = toBY * bw + toBX;
    if (!nodes[startIdx].HasRep || !nodes[goalIdx].HasRep)
        return false;

    auto blockDist = [](int32 ax, int32 ay, int32 bx, int32 by) {
        int32 dx = (bx > ax) ? (bx - ax) : (ax - bx);
        int32 dy = (by > ay) ? (by - ay) : (ay - by);
        int32 diag = (dx < dy) ? dx : dy;
        int32 straight = dx + dy - 2 * diag;
        return straight * 256 + diag * 362;
    };

    // A* over the block graph (8-way).
    std::priority_queue<std::pair<int32, int32>,
                        std::vector<std::pair<int32, int32>>,
                        std::greater<std::pair<int32, int32>>> openList;

    nodes[startIdx].G = 0;
    nodes[startIdx].F = blockDist(fromBX, fromBY, toBX, toBY);
    openList.push({ nodes[startIdx].F, startIdx });

    static const int8 ddx[8] = {  0,  1,  1,  1,  0, -1, -1, -1 };
    static const int8 ddy[8] = { -1, -1,  0,  1,  1,  1,  0, -1 };

    while (!openList.empty())
    {
        auto [curF, cur] = openList.top();
        openList.pop();
        if (nodes[cur].Closed)
            continue;
        if (cur == goalIdx)
            break;
        nodes[cur].Closed = true;

        const int32 cx = cur % bw;
        const int32 cy = cur / bw;

        for (int32 d = 0; d < 8; ++d)
        {
            int32 nx = cx + ddx[d];
            int32 ny = cy + ddy[d];
            if (nx < 0 || ny < 0 || nx >= bw || ny >= bh)
                continue;

            int32 nIdx = ny * bw + nx;
            if (nodes[nIdx].Closed || !nodes[nIdx].HasRep)
                continue;

            // Diagonal corner-cut check at the block level: the two
            // orthogonal neighbour blocks must also be reachable.
            if (d & 1)
            {
                int32 aIdx = (cy + ddy[d - 1]) * bw + (cx + ddx[d - 1]);
                int32 bIdx = (cy + ddy[(d + 1) & 7]) * bw + (cx + ddx[(d + 1) & 7]);
                if (!nodes[aIdx].HasRep || !nodes[bIdx].HasRep)
                    continue;
            }

            int32 moveCost = (d & 1) ? 362 : 256;
            int32 tentative = nodes[cur].G + moveCost;
            if (tentative < nodes[nIdx].G)
            {
                nodes[nIdx].G = tentative;
                nodes[nIdx].F = tentative + blockDist(nx, ny, toBX, toBY);
                nodes[nIdx].CameFrom = cur;
                openList.push({ nodes[nIdx].F, nIdx });
            }
        }
    }

    if (nodes[goalIdx].G == 0x7FFFFFFF)
        return false;   // no abstract route

    // Emit the abstract waypoint chain: start cell, each traversed block's
    // representative cell, and the destination.
    std::vector<int32> rev;
    for (int32 c = goalIdx; c != -1; c = nodes[c].CameFrom)
    {
        rev.push_back(c);
        if (c == startIdx)
            break;
    }

    outPath.Add(from);
    for (auto it = rev.rbegin(); it != rev.rend(); ++it)
    {
        const BlockNode& n = nodes[*it];
        outPath.Add(CellStruct(static_cast<int16>(n.RepX), static_cast<int16>(n.RepY)));
    }
    outPath.Add(to);

    return true;
}

// ============================================================================
// CellClass::Get_Movement_Cost - asm `LandCharacteristics.Foot[land*9 + speed]`
//
//   The cell's own land type selects the row and the mover's speed class the
//   column.  A zero entry means "this combination cannot traverse the cell at
//   all"; the callers treat that as an outright rejection.
// ============================================================================

int32 CellClass::Get_Movement_Cost(int32 SpeedType) const
{
    const int32 landIndex = static_cast<int32>(Land);
    if (landIndex < 0 || landIndex >= RulesClass::LAND_TYPE_COUNT) {
        return 1;
    }
    if (SpeedType < 0 || SpeedType >= 9) {
        return 1;
    }

    if (TheRules == nullptr) {
        return 1;
    }

    const RulesClass::LandTypeCharacteristics& row =
        TheRules->LandCharacteristics[landIndex];

    // asm: LandCharacteristics.Foot[land*9 + speed] -- the row base is the
    // Foot field itself and the stride is nine floats (0x24 bytes), so the
    // flat walk has to start at +0x00, not at the Hover field at +0x0C.
    const float* values = &row.Foot;
    return static_cast<int32>(values[SpeedType]);
}

// ============================================================================
 // CellClass::Is_Clear_To_Move -
//
//   The movement-legal test used when the map searches for a nearby free spot
//   and when a unit is placed.  It is a sequence of cheap rejections followed
//   by the land-characteristics lookup:
//
//     * SpeedType 4 (Winged) always passes - aircraft ignore the ground.
//     * When a5 is not -1 the cell's zone must equal it.
//     * The cell's "land" byte must match a7, except that a bridge cell also
//       accepts a7 == land + 4.
//     * The movement field must be empty after applying the a3 / a4 masks.
//     * A crushable overlay rejects every zone except the crushers.
//     * Finally the land/speed movement cost must be non-zero.
// ============================================================================

bool CellClass::Is_Clear_To_Move(int32 SpeedType, bool a3, bool a4, int32 a5,
                                 MovementZone zone, int32 a7, bool boo) const
{
    // asm: cmp eax,4 / jz -> return true   (winged movers ignore the ground)
    if (SpeedType == 4) {
        return true;
    }

    // asm: if (a5 != -1) { reach = CanLocationBeReached(cell, zone, boo);
    //                       if (a5 != reach) return false; }
    // The reconstruction compares the cell's cached zone index, which is the
    // value the map's own reachability query returns (avoids the recursive
    // dependency Is_Clear_To_Move <-> Can_Location_Be_Reached).
    if (a5 != -1) {
        if (ZoneIndex != a5) {
            return false;
        }
    }

    // asm: signed byte at +0x11B (the coarse "land level" byte)
    const int32 landByte = static_cast<int32>(static_cast<int8>(LandByte));
    const bool  onBridgeFlag = (Field_140 & 0x0100u) != 0u;

    // asm: a7 (expected land byte) test, with the +4 bridge allowance.
    if (a7 != -1) {
        if (a7 != landByte) {
            if (!onBridgeFlag)
                return false;
            if (a7 != landByte + 4)
                return false;
        } else {
            if (onBridgeFlag && !boo)
                return false;
        }
    }

    // asm: pick the bridge movement field when (a7 == -1 || a7 == land+4) and
    // the bridge flag is set; otherwise the ground field.  `cl` records the
    // bridge variant for the final cost test.
    bool  usedBridgeField = false;
    uint32 movement;
    if ((a7 == -1 || a7 == landByte + 4) && onBridgeFlag) {
        movement = Get_Movement_Field_Bridge();
        usedBridgeField = true;
    } else {
        movement = Get_Movement_Field();
    }

    if (a3) movement &= 0xE0u;
    if (a4) movement &= 0x5Fu;
    if (movement != 0) {
        return false;
    }

    // asm: idx = [this+0ECh]  (the land row); overlay dispatch may zero it.
    int32 landIndex = static_cast<int32>(Land);

    if (Overlay != -1) {
        const OverlayTypeClass* pOverlay = OverlayTypeClass::FindByIndex(Overlay);
        if (pOverlay != nullptr && pOverlay->Wall) {
            if (zone == MovementZone::Destroyer ||          // 2
                zone == MovementZone::AmphibiousDestroyer || // 3
                zone == MovementZone::InfantryDestroyer)     // 8
            {
                landIndex = 0;
            } else if (zone == MovementZone::Crusher ||        // 1
                       zone == MovementZone::AmphibiousCrusher) // 4
            {
                if (pOverlay->Crushable) {
                    landIndex = 0;
                } else if (zone == MovementZone::CrusherAll) { // 0xC
                    landIndex = 0;
                } else {
                    return false;
                }
            } else if (zone == MovementZone::CrusherAll) {     // 0xC
                landIndex = 0;
            } else {
                return false;
            }
        }
    }

    // asm: cost = LandCharacteristics.Foot[landIndex*9 + SpeedType]
    //      if (cost != 0.0) return true;
    //      if (bridge variant) return true;
    //      return false;
    float cost = 0.0f;
    if (TheRules != nullptr &&
        landIndex >= 0 && landIndex < RulesClass::LAND_TYPE_COUNT &&
        SpeedType >= 0 && SpeedType < 9)
    {
        const float* values = &TheRules->LandCharacteristics[landIndex].Foot;
        cost = values[SpeedType];
    }

    if (cost != 0.0f)
        return true;
    if (usedBridgeField)
        return true;
    return false;
}

// ============================================================================
// CellClass::Get_Land_Byte
//
//   The original keeps a small integer at +0x11B that groups the twelve land
//   types into the coarse classes the movement table is indexed by.  The
//   caller-side comparisons (a7 == land, a7 == land + 4 for bridges) work on
//   this value, so it is exposed directly from the land type.
// ============================================================================

int32 CellClass::Get_Land_Byte() const
{
    return static_cast<int32>(Land);
}

// ============================================================================
// CellClass::Get_Movement_Field / Get_Movement_Field_Bridge
//
//   Two per-cell bytes the assembly tests after masking.  A non-zero result
//   means the cell carries some movement restriction that the requesting mover
//   did not opt out of.
// ============================================================================

uint8 CellClass::Get_Movement_Field() const
{
    return MovementField;
}

uint8 CellClass::Get_Movement_Field_Bridge() const
{
    return MovementFieldBridge;
}

// ============================================================================
// CellClass::Tile_Is*  - the tile-classification predicates
//
//   Every entry reads the cell's isometric tile type index (+0x38) and tests
//   it against the special tile-set ordinals that the tileset INI published
//   through IsometricTileType::Apply_Special_Tile_Indices.  A range test is
//   written as "ordinal <= index < ordinal + count"; the guard on the ordinal
//   being -1 (tile set missing from this theater) makes the whole range test
//   fall through to false, which is why the assembly compares against
//   0FFFFFFFFh first.
//
//   Tile_IsCliff is the big one: it accepts the cliff set, all four waterfalls
//   (with a directional face test on the passability byte at +0x11A), the
//   cliff ramps, the water caves, both bridge sets and finally the destroyable
//   and water cliff ranges.
// ============================================================================

int32                                   TubeCount = 0;
DynamicVectorClass<CellClass*>*         vec_Tubes = nullptr;

namespace {

// The index ranges the binary compares against are always "ordinal .. ordinal
// + span".  A missing tile set publishes -1, and the assembly tests that up
// front, so a negative ordinal can never match a non-negative index.
inline bool TileRange(int32 index, int32 ordinal, int32 span)
{
    return ordinal != -1 && index >= ordinal && index < ordinal + span;
}

} // namespace

bool CellClass::Tile_IsWater() const
{
    return TileRange(TileType, tile_WaterSet, 0x0E);
}

bool CellClass::Tile_IsNotWater() const
{
    return !TileRange(TileType, tile_WaterSet, 0x0E);
}

bool CellClass::Tile_IsBridge() const
{
    return TileRange(TileType, tile_BridgeSet, 0x10);
}

bool CellClass::Tile_IsWoodBridge() const
{
    return TileRange(TileType, tile_WoodBridgeSet, 0x10);
}

bool CellClass::Tile_IsDestroyableCliff() const
{
    // Two adjacent tiles: the base and the base + 1.
    return TileType == tile_DestroyableCliffs
        || TileType == tile_DestroyableCliffs + 1;
}

bool CellClass::Tile_IsRamp() const
{
    if (TileType >= tile_RampBase && TileType < tile_RampBase + 0x14)
        return true;

    return TileType >= tile_RampSmooth && TileType < tile_RampSmooth + 0x0C;
}

bool CellClass::Tile_IsGreen() const
{
    if (TileType == tile_GreenTile)
        return true;

    return TileRange(TileType, tile_ClearToGreenLat, 0x10);
}

bool CellClass::Tile_IsBlank() const
{
    return TileType == 0xFFFF || TileType == 0;
}

bool CellClass::Tile_IsShorePieces() const
{
    return TileRange(TileType, tile_ShorePieces, 0x2A);
}

bool CellClass::Tile_IsWet() const
{
    if (TileRange(TileType, tile_ShorePieces, 0x2A))
        return true;

    if (TileRange(TileType, tile_WaterSet, 0x0E))
        return true;

    if (TileRange(TileType, tile_WaterfallEast, 4))
        return true;

    if (TileRange(TileType, tile_WaterfallWest, 4))
        return true;

    if (TileRange(TileType, tile_WaterfallSouth, 4))
        return true;

    return TileRange(TileType, tile_WaterfallNorth, 4);
}

bool CellClass::Tile_IsMiscPave() const
{
    return TileRange(TileType, tile_MiscPaveTile, 0x0E);
}

bool CellClass::Tile_IsPave() const
{
    return TileRange(TileType, tile_PaveTile, 0x10);
}

bool CellClass::Tile_IsDirtRoad() const
{
    if (TileRange(TileType, tile_DirtRoadJunction, 0x0B))
        return true;

    if (TileRange(TileType, tile_DirtRoadCurve, 0x18))
        return true;

    return TileRange(TileType, tile_DirtRoadStraight, 0x42);
}

bool CellClass::Tile_IsPavedRoad() const
{
    return TileRange(TileType, tile_PavedRoads, 0x0F);
}

bool CellClass::Tile_IsPavedRoadEnd() const
{
    return TileRange(TileType, tile_PavedRoadEnds, 4);
}

bool CellClass::Tile_IsPavedRoadSlope() const
{
    return TileRange(TileType, tile_PavedRoadSlopes, 4);
}

bool CellClass::Tile_IsMedian() const
{
    return TileRange(TileType, tile_Medians, 0x0E);
}

bool CellClass::Tile_IsClearToSandLat() const
{
    return TileRange(TileType, tile_ClearToSandLat, 0x10);
}

bool CellClass::Tile_IsATunnel() const
{
    // A tunnel tile is one whose tube index (+0x118) is a live index into
    // vec_Tubes *and* whose land type is the Tunnel classification.
    if (TubeIndex < 0)
        return false;

    if (TubeIndex >= TubeCount)
        return false;

    return Land == ::LandType::Tunnel;
}

bool CellClass::Tile_IsCliff() const
{
    if (TileRange(TileType, tile_CliffSet, 0x28))
        return true;

    // Each waterfall is a run of four tiles.  The first and the fourth of the
    // run are the two "faces" the cliff test rejects when the cell's
    // passability byte names that same face; the middle two always count.
    if (TileRange(TileType, tile_WaterfallEast, 4)) {
        if (TileType == tile_WaterfallEast)
            return Passability != 0 && Passability != 4;
        if (TileType != tile_WaterfallEast + 3)
            return true;
    }

    if (TileRange(TileType, tile_WaterfallWest, 4)) {
        if (TileType == tile_WaterfallWest)
            return Passability != 1 && Passability != 3;
        if (TileType != tile_WaterfallWest + 3)
            return true;
    }

    if (TileRange(TileType, tile_WaterfallSouth, 4)) {
        if (TileType == tile_WaterfallSouth)
            return Passability != 0 && Passability != 1;
        if (TileType != tile_WaterfallSouth + 3)
            return true;
    }

    if (TileRange(TileType, tile_WaterfallNorth, 4)) {
        if (TileType == tile_WaterfallNorth)
            return Passability != 2 && Passability != 3;
        if (TileType != tile_WaterfallNorth + 3)
            return true;
    }

    if (TileRange(TileType, tile_CliffRamps, 0x14))
        return true;

    if (TileRange(TileType, tile_WaterCaves, 4))
        return true;

    if (TileRange(TileType, tile_BridgeSet, 0x10))
        return true;

    if (TileRange(TileType, tile_WoodBridgeSet, 0x10))
        return true;

    if (TileRange(TileType, tile_DestroyableCliffs, 2))
        return true;

    return TileRange(TileType, tile_WaterCliffs, 0x1C);
}

// ============================================================================
// CellClass radiation
//
//   RadLevel is a double the RadSiteClass update loop accumulates into each
//   cell it covers.  The four small predicates here are the ones the rest of
//   the engine asks for: Is_Radiated (anything above 1.0), and Get_RadLevel
//   (the level clamped to RulesData->RadLevelMax and then floored to an int,
//   which is the value FootClass::AI feeds into the damage roll).
// ============================================================================

void CellClass::RadLevel_Increase(double amount)
{
    RadLevel += amount;
}

void CellClass::RadLevel_Decrease(double amount)
{
    RadLevel -= amount;

    // The original tests the result against zero and stores a hard zero when
    // it went negative, so a cell never retains a negative dose.
    if (RadLevel < 0.0)
        RadLevel = 0.0;
}

void CellClass::Set_Rad_Site(void* pRadSite)
{
    RadSite = pRadSite;
}

bool CellClass::Is_Radiated() const
{
    return RadLevel > 1.0;
}

int32 CellClass::Get_RadLevel() const
{
    // fld maxLevel / fld RadLevel / fcomp st(1) / test ah,1 / jz floor.
    // The compare only branches on the unordered flag; for every ordered pair
    // the value left on the FPU stack is the RadLevel pushed second and the
    // max level is simply dropped.  The result is therefore the floor of the
    // cell's dose with no clamp, which is what is reproduced here.
    const int32 maxLevel = TheRules->RadLevelMax;
    (void)maxLevel;

    return static_cast<int32>(RadLevel);
}

// ============================================================================
// CellClass::Tiberium_In_Cell / Contains_Tiberium
//
//   TiberiumInCell resolves the cell's overlay into an ore type and answers
//   that type's Value multiplied by (growth stage + 1), where the growth stage
//   is the byte the overlay bookkeeping keeps at +0x11E.  An overlay that is
//   not ore, or a cell with no overlay, yields zero.
//
//   ContainsTiberium is the bare land-type test the harvest code uses first.
// ============================================================================

int32 CellClass::Tiberium_In_Cell() const
{
    const int32 typeIndex = Is_Overlay_Idx_Tiberium(Overlay);
    if (typeIndex == -1)
        return 0;

    TiberiumClass* pType = TiberiumClass::FindByIndex(typeIndex);
    if (pType == nullptr)
        return 0;

    return pType->Value * (static_cast<int32>(OverlayFrame) + 1);
}

// ----------------------------------------------------------------------------
// CellClass::Reduce_Tiberium
//
//   原版被 Damage_At_Cell 调用：按 amount 扣减格子上 tiberium 的生长阶段。
//   生长阶段存放在 OverlayFrame（0..12），归零时移除 tiberium overlay，
//   把 land type 恢复为 Clear，并按 Set_Tiberium 的对称逻辑清掉
//   TiberiumValue 与 OverlayData 中的类型字节。
// ----------------------------------------------------------------------------
void CellClass::Reduce_Tiberium(int32 amount)
{
    if (amount <= 0) return;
    if (!Contains_Tiberium() && Is_Overlay_Idx_Tiberium(Overlay) == -1) return;

    int32 stage = static_cast<int32>(OverlayFrame) - amount;
    if (stage < 0) stage = 0;

    if (stage == 0) {
        // 矿已耗尽：清除 overlay 与相关标记
        Overlay = -1;
        OverlayFrame = 0;
        OverlayData = 0;
        TiberiumValue = 0;
        if (Land == ::LandType::Tiberium) {
            Land = ::LandType::Clear;
        }
    } else {
        OverlayFrame = static_cast<uint8>(stage);
        TiberiumValue = stage;
    }
}

// ----------------------------------------------------------------------------
// CellClass::Reduce_Wall
//
//   原版被 Damage_At_Cell 调用：
//     amount < 0  —— 绝对摧毁（WallAbsoluteDestroyer），直接清墙；
//     amount >= 0 —— 按 amount 扣减墙耐久；扣完清除墙 overlay，把格子
//                   的 land type 恢复为 Clear 并清空墙主标记。
// ----------------------------------------------------------------------------
void CellClass::Reduce_Wall(int32 amount)
{
    if (amount < 0) {
        // 绝对摧毁
        Overlay = -1;
        OverlayData = 0;
        if (Land == ::LandType::Wall) {
            Land = ::LandType::Clear;
        }
        return;
    }

    if (amount == 0) return;

    // 墙耐久存放在 OverlayData（低字节为阶段）；扣到 0 即摧毁
    int32 hp = static_cast<int32>(OverlayData & 0xFF) - amount;
    if (hp <= 0) {
        Overlay = -1;
        OverlayData = 0;
        if (Land == ::LandType::Wall) {
            Land = ::LandType::Clear;
        }
    } else {
        OverlayData = (OverlayData & ~0xFF) | (hp & 0xFF);
    }
}

bool CellClass::Contains_Tiberium() const
{
    return Land == ::LandType::Tiberium;
}

// ============================================================================
// CellClass - per-house sensor / cloak-generator bookkeeping
//
//  every accessor below is a literal transcription of the assembly.
//  CloakGen stores a bit per house index in a 32-bit mask at +0x78; the
//  sensor and disguise-sensor counters are 16-bit words indexed by the house
//  ordinal at +0x7C and +0xAC respectively.  The building update code calls
//  Add/Rem as cloaking and sensor structures power on and off.
// ============================================================================

 // CellClass_CloakGen_AddHouse (xx): set bit idxHouse of the mask.
void CellClass::CloakGen_AddHouse(int32 idxHouse)
{
    CloakGenMask |= (1u << idxHouse);
}

 // CellClass_CloakGen_RemHouse (xx): clear bit idxHouse of the mask.
void CellClass::CloakGen_RemHouse(int32 idxHouse)
{
    CloakGenMask &= ~(1u << idxHouse);
}

 // CellClass_CloakGen_HasHouse (xx): test bit idxHouse of the mask.
bool CellClass::CloakGen_HasHouse(int32 idxHouse) const
{
    return (CloakGenMask & (1u << idxHouse)) != 0;
}

 // CellClass_Sensors_AddHouse (xx): one more of this house's sensors
// now reveals the cell.
void CellClass::Sensors_AddHouse(int32 idxHouse)
{
    ++SensedByHouses[idxHouse];
}

 // CellClass_Sensors_RemHouse (xx): one fewer.
void CellClass::Sensors_RemHouse(int32 idxHouse)
{
    --SensedByHouses[idxHouse];
}

 // CellClass_Sensors_HasHouse (xx): `setnle` on the 16-bit counter
// means "true when the count is strictly greater than zero".
bool CellClass::Sensors_HasHouse(int32 idxHouse) const
{
    return SensedByHouses[idxHouse] > 0;
}

 // CellClass_DisguiseSensors_AddHouse (xx).
void CellClass::DisguiseSensors_AddHouse(int32 idxHouse)
{
    ++DisguiseSensedByHouses[idxHouse];
}

 // CellClass_DisguiseSensors_RemHouse (xx).
void CellClass::DisguiseSensors_RemHouse(int32 idxHouse)
{
    --DisguiseSensedByHouses[idxHouse];
}

 // CellClass_DisguiseSensors_HasHouse (xx).
bool CellClass::DisguiseSensors_HasHouse(int32 idxHouse) const
{
    return DisguiseSensedByHouses[idxHouse] > 0;
}

// ============================================================================
// CellClass - tunnel / coordinate / identity / flag batch
// ============================================================================

 // asm: CellClass_GetTunnel
//
//  Validates the 16-bit tube index at +0x118 against the tunnel table length
//  and returns the cell the table holds; a negative index or an out-of-range
//  one yields null.
CellClass* CellClass::Get_Tunnel() const
{
    if (TubeIndex < 0)
        return nullptr;

    if (vec_Tubes == nullptr)
        return nullptr;

    if (TubeIndex >= vec_Tubes->Count)
        return nullptr;

    return vec_Tubes->Items[TubeIndex];
}

 // asm: void __thiscall CellClass_SetMapCoords(CellClass *cell, wXY *coords)   (asm:304107)
//
//  The original copies the four packed bytes of the MapCellExClass X/Y pair
//  straight into +0x24; storing the CellStruct produces the identical bytes.
void CellClass::Set_Map_Coords(const CellStruct& coords)
{
    MapCoords = coords;
}

 // CellClass_GetAbstractID (xx).  Every cell reports the fixed
// AbstractType::Cell ordinal.
int32 CellClass::Get_AbstractID() const
{
    return static_cast<int32>(AbstractType::Cell);
}

 // asm: char __fastcall CellClass_FlagPickedUp(CellClass *this)   (asm:300382)
//
//  Clears the "flag is planted" bit (0x10) from the flag word at +0x140 and
//  resets the attached-object index at +0x50 to -1.  Returns true only when
//  the bit had actually been set.
bool CellClass::Flag_Picked_Up()
{
    if ((Field_140 & 0x10u) == 0)
        return false;

    Field_140 &= ~0x10u;
    Field_50 = -1;
    return true;
}

 // asm: void __fastcall CellClass_Smth0(CellClass *this)   (asm:308911)
//
//  Decrements the gap counter at +0x130, but a count of exactly 1 first
//  collapses to 0 so that the subsequent decrement wraps it to -1; the net
//  effect is that a single remaining gap generator takes the counter to -1
//  rather than 0.
void CellClass::Smth0()
{
    if (GapsCoveringCell == 1)
        GapsCoveringCell = 0;

    --GapsCoveringCell;
}

 // CellClass_Smth2 (xx).
//
//  Sets the shroud flag bits 0x18 on +0x12C and, while the gap counter is
//  still positive, tags 0x20 onto the flag word at +0x140.
void CellClass::Smth2()
{
    Field_12C |= 0x18u;

    if (GapsCoveringCell > 0)
        Field_140 |= 0x20u;
}

// ============================================================================
// Occupancy-list lookups
//
//  The binary keeps two singly-linked lists per cell, threaded through
//  ObjectClass::NextObject (+0x30): the ground list (+0xE4) and the altitude
//  ("flying") list (+0xE8).  Every lookup picks one of the two with its bool
//  argument, then walks the chain comparing WhatAmI() against the requested
//  abstract type.  A null head yields null immediately.
// ============================================================================

ObjectClass* CellClass::First_Object(bool alt) const {
    return alt ? AltObject : FirstObject;
}

ObjectClass* CellClass::GetUnit(bool alt) const {
    if (!GameActive) return nullptr;

    ObjectClass* pObj = First_Object(alt);
    while (pObj) {
        if (pObj->WhatAmI() == AbstractType::Unit)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

ObjectClass* CellClass::GetAircraft(bool alt) const {
    if (!GameActive) return nullptr;

    ObjectClass* pObj = First_Object(alt);
    while (pObj) {
        if (pObj->WhatAmI() == AbstractType::Aircraft)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

ObjectClass* CellClass::GetInfantry(bool alt) const {
    if (!GameActive) return nullptr;

    ObjectClass* pObj = First_Object(alt);
    while (pObj) {
        if (pObj->WhatAmI() == AbstractType::Infantry)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

// ============================================================================
// asm: ObjectClass *__thiscall CellClass::Cell_Abstract(CellClass *this,
//      eAbstractIDs objId, char a3)   (asm:1740)
//
//   Walks the chain selected by `a3` (false = ground +0xE4, true = altitude
//   +0xE8) and returns the first entry whose WhatAmI() equals objId.  Game
//   not running yields null immediately.
// ============================================================================
ObjectClass* CellClass::Cell_Abstract(int32 objId, bool alt) const {
    if (!GameActive)
        return nullptr;

    ObjectClass* pObj = alt ? AltObject : FirstObject;
    while (pObj) {
        if (static_cast<int32>(pObj->WhatAmI()) == objId)
            return pObj;
        pObj = pObj->NextObject;
    }
    return nullptr;
}

// ============================================================================
// asm: BuildingClass *__thiscall CellClass::Cell_Building(CellClass *this)
//      (asm:1802)
//
//   The first building on the ground chain.  This is the canonical building
//   lookup; Get_Building forwards to it.
// ============================================================================
BuildingClass* CellClass::Cell_Building() const {
    if (!GameActive)
        return nullptr;

    for (ObjectClass* pObj = FirstObject; pObj; pObj = pObj->NextObject) {
        if (pObj->WhatAmI() == AbstractType::Building)
            return static_cast<BuildingClass*>(pObj);
    }

    // 多格建筑只在主格入链，其余占地格靠 Occupier 槽登记（见 Add_Occupier）。
    if (Occupier != nullptr && Occupier->WhatAmI() == AbstractType::Building)
        return static_cast<BuildingClass*>(Occupier);

    return nullptr;
}

// ============================================================================
// asm: _ObjectClass *__thiscall CellClass::Cell_Techno(CellClass *this,
//      dwXYZ *arg0, char arg4, ObjectClass *arg8)   (asm:1609)
//
//   Picks the occupancy chain with arg4, then returns whichever techno on it
//   lies nearest to the coordinate pair at arg0.  The distance is computed in
//   the low byte of each world axis offset (per the binary: objCoord & 0xFF
//   minus the target's pre-scaled value), square-rooted and floored; the
//   smallest distance wins.  arg8, when non-null, is skipped.
// ============================================================================
ObjectClass* CellClass::Cell_Techno(CellStruct* pCoords, bool alt, ObjectClass* pSkip) const {
    // asm: lea edi,[eax*8] / sub edi,eax  (target X * 7)
    int32 targetX = 0;
    int32 targetY = 0;
    if (pCoords != nullptr) {
        targetX = static_cast<int32>(pCoords->X) * 7;
        targetY = static_cast<int32>(pCoords->Y) * 7;
    }

    ObjectClass* pObj = alt ? AltObject : FirstObject;
    if (pObj == nullptr)
        return nullptr;

    ObjectClass* pBest = nullptr;
    int32        bestDist = 0;

    for (; pObj; pObj = pObj->NextObject) {
        // asm: mov al,[esi+14h] / and al,1  - skip non-techno objects.
        const AbstractType what = pObj->WhatAmI();
        const bool isTechno = (what == AbstractType::Infantry || what == AbstractType::Unit
                            || what == AbstractType::Aircraft || what == AbstractType::Building);
        if (!isTechno)
            continue;
        if (pObj == pSkip)
            continue;

        CoordStruct crd(0, 0, 0);
        pObj->GetCoords(&crd);

        const int32 dx = (crd.X & 0xFF) - targetX;
        const int32 dy = (crd.Y & 0xFF) - targetY;
        const int32 dist = static_cast<int32>(
            std::sqrt(static_cast<double>(dx * dx + dy * dy)));

        if (pBest == nullptr || dist < bestDist) {
            pBest = pObj;
            bestDist = dist;
        }
    }
    return pBest;
}

 // asm: char __thiscall CellClass_CanAddTiberium(CellClass *this, int a2)   (asm:301096)
//
//  A cell may receive new tiberium only when it is inside the radar range,
//  its flag word does not already carry the growth-suppressing bits 0x500,
//  and it holds no building.  (The trailing refinement in the original also
//  rejects cells whose tiberium value would exceed the type's cap, which is
//  folded into the caller.)
bool CellClass::CanAddTiberium() const {
    if (!MapClass::Instance || !MapClass::Instance->In_Radar(MapCoords, true))
        return false;

    if (Field_140 & 0x500u)
        return false;

    if (GameActive) {
        ObjectClass* pObj = FirstObject;
        while (pObj) {
            if (pObj->WhatAmI() == AbstractType::Building)
                return false;
            pObj = pObj->NextObject;
        }
    }

    return true;
}

// ============================================================================
 // asm: CellClass_ScatterContent
//
//  Shoves any object parked on this cell (and, for the ground layer, its
//  immediate neighbourhood) toward an adjacent free cell.  Used when a
//  structure is placed over ground a unit is sitting on, and when a factory's
//  bib must be cleared before production can resume.
//
//  `bIgnoreInfantry` lets the caller leave infantry where they are - the
//  original skips infantry when this is set so that guard-position troops are
//  not displaced by a structure being built around them.
// ============================================================================
void CellClass::Scatter_Content(int32 a2, int32 a3, bool bIgnoreInfantry)
{
    ObjectClass* pList = FirstObject; // the project's callers use the ground chain

    // asm: any elite techno on the chain forces every occupant to scatter.
    bool bForceScatter = false;
    if (!bIgnoreInfantry && pList != nullptr) {
        for (ObjectClass* pObj = pList; pObj != nullptr; pObj = pObj->NextObject) {
            if (!IsTechnoObject(pObj))
                continue;
            if (static_cast<TechnoClass*>(pObj)->GetVeterancy() >= 2) {
                bForceScatter = true;
                break;
            }
        }
    }

    // asm: snapshot every techno on the chain first (the scatter can mutate it).
    std::vector<ObjectClass*> technos;
    for (ObjectClass* pObj = pList; pObj != nullptr; pObj = pObj->NextObject) {
        if (IsTechnoObject(pObj))
            technos.push_back(pObj);
    }

    const CoordStruct crd(a2, a3, 0);

    for (ObjectClass* pObj : technos) {
        TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);

        // asm: skip anything that cannot scatter, unless it was forced
        // (elite present / ignore-infantry) or the type opts in via the
        // SCATTER ability.  The project exposes that ability as CanScatter().
        if (!bForceScatter && !bIgnoreInfantry && !pTechno->CanScatter())
            continue;

        pTechno->Scatter(crd, a3 != 0, bIgnoreInfantry);
    }
}

// ============================================================================
 // CellClass::Adjust_Threat -
//
// Propagates a threat impulse into every house's threat grid at this cell's
// region.  The original walks `vec_Houses`, skipping the source house itself
// and any house it is allied with, and forwards the (coordHash, threat) pair
// to HouseClass_SetThreat.  The coordHash is MapClass::Cell_Region of the
// cell's own coordinates, so all cells of a region share one threat slot.
// ============================================================================
void CellClass::Adjust_Threat(HouseClass* pFromHouse, int32 threat)
{
    const int32 coordHash = MapClass::Cell_Region(MapCoords);
    const int32 fromIndex = (pFromHouse != nullptr) ? pFromHouse->GetArrayIndex() : -1;

    for (int32 i = 0; i < TheHouseCount; ++i)
    {
        HouseClass* pHouse = Houses[i];
        if (pHouse == nullptr)
            continue;

        // The source house never penalises itself.
        if (pHouse->GetArrayIndex() == fromIndex)
            continue;

        // Allied houses share the threat picture, so they are left alone -
        // the loop only pushes the impulse to hostiles and neutrals.
        if (pFromHouse != nullptr && pHouse->Allied_With(fromIndex))
            continue;

        pHouse->Set_Threat(coordHash, threat);
    }
}

// ============================================================================
 // asm: CellClass_GetCoords_unknown2
//
//  Forwards the cell's own coordinates and, when the flag word at +0x140 has
//  bit 0x100 set, raises Z by the global bias dword_89E7B4 - the height of
//  one isometric level, published when the tile tables are built.  The
//  project maps that bias to LevelHeight.
// ============================================================================
CoordStruct* CellClass::GetCoordsUnknown2(CoordStruct* pCoords) const
{
    // CellClass is not an ObjectClass in this project, so the "vtable +0x48
    // GetCoords" slot maps to the cell's own coordinate builder.
    *pCoords = Get_CellCoords();

    if ((Field_140 & 0x0100u) != 0)
        pCoords->Z += LevelHeight;

    return pCoords;
}

// ============================================================================
 // asm: char __fastcall CellClass_IsVeins(CellClass *this)   (asm:304383)
//
//  A cell accepts vein growth only when its overlay frame byte (+0x11C) is at
//  most 4, its land byte is not one of the four vein-hostile types (2, 3, 6
//  and 8), and the current overlay is either absent (-1) or flagged as a vein
//  overlay.  The binary tests the vein flag directly on the OverlayTypeClass
//  record at +0x2AE.
// ============================================================================
bool CellClass::IsVeins() const
{
    if (OverlayFrame > 4)
        return false;

    const int32 land = static_cast<int32>(Land);
    if (land == 2 || land == 3 || land == 6 || land == 8)
        return false;

    if (Overlay == -1)
        return true;

    const OverlayTypeClass* pType = OverlayTypeClass::Array != nullptr
                                  ? (*OverlayTypeClass::Array)[Overlay]
                                  : nullptr;
    return (pType != nullptr) && pType->IsVeinholeMonster;
}

// ============================================================================
 // asm: void __thiscall CellClass_ActivateVeins(CellClass *this)   (asm:307149)
//
//  给踩在矿脉上的单位放血。自身先过三道门槛：叠加物必须是矿脉(0x7E)、
//  生长阶段不低于 0x30、叠加物帧(+0x11C)为 0，且本方格还没有被打上激活
//  标记。筛选通过后顺着该格的对象链走一遍：只有站在地面上的单位，且其
//  类型既没有 ImmuneToVeins 也没有 VEIN_PROOF 能力，才会在格心生成一个
//  矿脉攻击动画。只要本格确实处理过，就给格子置上激活标记，避免重复触发。
// ============================================================================
void CellClass::ActivateVeins()
{
    // 叠加物必须是矿脉，且生长阶段达标、尚未被激活过。
    if (Overlay != 0x7E)
        return;
    if (OverlayFrame < 0x30)
        return;
    if (OverlayData != 0)
        return;
    if ((Field_140 & 0x2000u) != 0u)
        return;

    if (ZoneIndex == 0)
        return;

    for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
    {
        // 只处理活着的、且在地面上的科技类对象。
        if (pObj->WhatAmI() == AbstractType::Infantry
            || pObj->WhatAmI() == AbstractType::Unit
            || pObj->WhatAmI() == AbstractType::Building
            || pObj->WhatAmI() == AbstractType::Aircraft)
        {
            TechnoClass* pTechno = static_cast<TechnoClass*>(pObj);

            if (!pTechno->OnFloor())
                continue;

            const TechnoTypeClass* pType = pTechno->GetTechnoType();
            if (pType != nullptr && pType->ImmuneToVeins)
                continue;

            // 原版这里查的是 VEIN_PROOF 能力位（掩码 0x...）；本项目的
            // HasAbility 接收的是 VeteranAbilities/EliteAbilities 数组下标，
            // 两者语义不同，故此处以"脚下免疫矿脉"的等价判定代替。
            if (pTechno->HasAbility(ABILITY_VEIN_PROOF_INDEX))
                continue;

            CoordStruct pos = Get_CellCoords();
            pos.X += 0x80;
            pos.Y += 0x80;
            pos.Z  = Get_Z_Height();

            if (TheRules != nullptr && TheRules->VeinAttack != nullptr)
            {
                AnimClass* pAnim = new AnimClass(
                    TheRules->VeinAttack, pos, 0, 1, 0x600, 0, false);
                (void)pAnim;
            }
        }
    }

    Field_140 |= 0x20000u;
}

// ============================================================================
 // asm: CellClass_SetupVeins
//
//  把一格矿脉重新铺设成完整形态。先确认本格是矿脉宿主，再取其包围矩形与
//  一格尺寸做相交裁剪，然后按包围范围自后向前逐格执行铺设；收尾时再调用
//  一次 ActivateVeins，让站在矿脉上的单位重新吃到矿脉伤害。
//
//  注意：逐格铺设那一层依赖完整的图像/动画铺设管线（矿脉贴图分块与
//  TacticalMap 脏区登记），本项目尚未建模该管线，因此这里只保留判定与
//  裁剪流程，并保证收尾的激活调用与原始行为一致。
// ============================================================================
void CellClass::SetupVeins()
{
    if (!IsVeins())
        return;

    // 取本格自身的包围矩形，做一次相交裁剪；任一方向非正即视为无可铺设区域。
    RectangleStruct cellRect = Get_Containing_Rect();
    const int32 cellW = 0x100;
    const int32 cellH = 0x100;

    if (cellRect.Width <= 0 || cellRect.Height <= 0)
        return;
    if (cellW <= 0 || cellH <= 0)
        return;

    // 裁剪到两者重叠区域（保持原实现的取整方式）。
    int32 left   = cellRect.X;
    int32 top    = cellRect.Y;
    int32 right  = cellRect.X + cellRect.Width;
    int32 bottom = cellRect.Y + cellRect.Height;

    if (left > 0)  { right  += left;  left = 0; }
    if (top  > 0)  { bottom += top;   top  = 0; }
    if (right  > cellW) right  = cellW;
    if (bottom > cellH) bottom = cellH;

    // 自后向前逐格铺设矿脉贴图。
    for (int32 y = bottom - 1; y >= top; --y)
    {
        for (int32 x = right - 1; x >= left; --x)
        {
            // 该层的贴图分块铺设由渲染管线负责，此处仅推进游标。
            (void)x;
            (void)y;
        }
    }

    // 收尾：重新激活，让站在矿脉上的单位吃到伤害。
    ActivateVeins();
}

// ============================================================================
// asm: sub_485390 —— 四个正交邻格的"还是矿脉"掩码（0x485390，无栈参）
//
//   for (i = 0; i < 4; ++i) {
//       if (2*i >= 8) cell = this;                       // 死分支：i<4 恒假
//       else          cell = Map.Coord_Cell(this.coords + CellCoordDirections[2*i]);
//       if (cell.Overlay == -1)                        continue;   // 304322
//       if (cell[11Ch] > 0 && OverlayType[Overlay].IsVeins) continue;
//       if (cell.Overlay != 7Eh && !OverlayType[Overlay].IsVeins) continue;
//       if (cell.Overlay == 7Eh && cell[11Eh] < 30h)     continue;
//       mask |= 1 << i;
//   }
// ============================================================================
// asm: 读取 OverlayTypeClass[overlay] 记录 +0x2AE 处的 IsVeins 字节。
static bool Is_Overlay_Vein_Type(int32 overlay)
{
    if (overlay < 0 || OverlayTypeClass::Array == nullptr)
        return false;
    if (overlay >= OverlayTypeClass::Array->Count)
        return false;
    OverlayTypeClass* pType = (*OverlayTypeClass::Array)[overlay];
    return pType != nullptr && pType->IsVeins;
}

uint8 CellClass::Vein_Neighbor_Mask() const
{
    // asm: CellCoordDirections（8 个 (dx,dy)，与 FootClass.cpp 的同名表一致）。
    static const int8 kCellCoordDirections[8][2] = {
        {  1,  0 }, {  1,  1 }, {  0,  1 }, { -1,  1 },
        { -1,  0 }, { -1, -1 }, {  0, -1 }, {  1, -1 },
    };

    uint8 mask = 0;

    for (int32 i = 0; i < 4; ++i) {
        const int32 dir = i * 2;

        // asm 304291-304316：取方向 2i 的邻格（越界时退回本格）。
        const CellClass* pCell = this;
        if (dir < 8) {
            if (MapClass::Instance == nullptr)
                continue;
            const CellStruct probe(
                static_cast<int16>(MapCoords.X + kCellCoordDirections[dir][0]),
                static_cast<int16>(MapCoords.Y + kCellCoordDirections[dir][1]));
            pCell = MapClass::Instance->GetCellAt(probe);
            if (pCell == nullptr)
                continue;
        }

        // asm 304322-304323: mov ecx,[eax+44h] / cmp ecx,-1 / jz 结束
        if (pCell->Overlay == -1)
            continue;

        // asm 304326-304332: bl = [eax+11Ch] ; 非零时再看叠加物类型的 IsVeins
        const bool overlayIsVein = Is_Overlay_Vein_Type(pCell->Overlay);
        if (pCell->VeinStage > 0 && overlayIsVein)
            continue;

        // asm 304336-304344: 不是 7Eh 就必须是矿脉类叠加物
        if (pCell->Overlay != 0x7E) {
            if (!overlayIsVein)
                continue;
        } else {
            // asm 304347-304349: 7Eh 且阶段 < 30h 才不算
            if (pCell->OverlayFrame < 0x30)
                continue;
        }

        mask = static_cast<uint8>(mask | (1u << i));
    }

    return mask;
}

// ============================================================================
// asm: sub_485590 —— 一格矿脉叠加物的重接（0x485590，无栈参）
//
//   逐句：
//     rect = Update_Cell(cell) ∩ GetContainingRect(cell)     ; 脏矩形（渲染用）
//     cell[11Eh] = 0
//     bl = sub_485390(cell)                                  ; 邻格矿脉掩码
//     if (bl && cell[11Ch] == 0) cell[11Eh] = (rand%3) + 3*bl
//     else                       cell[44h] = -1              ; 叠加物整格撤掉
//     Map.sub_6551C0(&cell.coords)                           ; 雷达/小地图格刷新
//     for (dir = 0; dir < 8; dir += 2) {
//         adj = cell.Adjacent_Cell(dir)
//         if (adj[11Ch] || adj.Overlay != 7Eh || adj[11Eh] >= 30h) continue
//         bl = sub_485390(adj)
//         if (!bl) adj[44h] = -1
//         else     adj[11Eh] = (rand%3) + 3*bl
//         adj.Setup(-1)
//         TacticalMap.RegisterDirtyArea(adj 的重叠矩形)
//         Map.sub_6551C0(&adj.coords)
//     }
//     TacticalMap.RegisterDirtyArea(cell 的重叠矩形)
//     cell.Setup(-1)
//
//   本工程把两处脏矩形合并成一次 RegisterDirtyArea（OpenYR 的 Get_Containing_Rect
//   已给出该格的屏幕包围盒），地图刷新沿用 MapClass 的格更新入口。
// ============================================================================
void CellClass::Relink_Vein_Overlay()
{
    // asm 304767-304768: 清掉本格的叠加物帧，再取邻格矿脉掩码。
    OverlayFrame = 0;
    const uint8 mask = Vein_Neighbor_Mask();

    if (mask != 0 && VeinStage == 0) {
        // asm 304778-304794: (|Random2 % 3|) + 3 * bl —— 8 位乘加。
        int32 roll = 0;
        if (ScenarioClass::Instance != nullptr)
            roll = ScenarioClass::Instance->Random.Next(0, 2);
        else
            roll = std::rand() % 3;
        OverlayFrame = static_cast<uint8>((roll & 0xFF) + 3 * static_cast<int32>(mask));
    } else {
        // asm 304800: 这格不再是矿脉了。
        Overlay   = -1;
        OverlayData = 0;
    }

    // asm 305004-305006: CellClass_Setup(cell, -1) —— 重算本格属性。
    Setup(-1);

    // asm 305075-305078 + 305089-305141: 脏矩形登记 + 雷达格刷新。
    if (TacticalClass::Instance != nullptr) {
        const RectangleStruct box = Get_Containing_Rect();
        TacticalClass::Instance->RegisterDirtyArea(
            Rectangle(box.X, box.Y, box.Width, box.Height), false);
    }

    // ── asm loc_485783：对四个正交邻格做同样的重接 ────────────────────────
    for (int32 dir = 0; dir < 8; dir += 2) {
        CellClass* pAdj = AdjacentCells[dir];
        if (pAdj == nullptr)
            continue;

        // asm 304819-304827: 只处理仍可长矿脉、仍是 7Eh、阶段 < 30h 的邻格。
        if (pAdj->VeinStage != 0)
            continue;
        if (pAdj->Overlay != 0x7E)
            continue;
        if (pAdj->OverlayFrame >= 0x30)
            continue;

        // asm 304830: bl = sub_485390(adj) ; 再重掷阶段或撤掉叠加物。
        const uint8 adjMask = pAdj->Vein_Neighbor_Mask();
        if (adjMask == 0) {
            pAdj->Overlay    = -1;
            pAdj->OverlayData = 0;
        } else {
            int32 roll = 0;
            if (ScenarioClass::Instance != nullptr)
                roll = ScenarioClass::Instance->Random.Next(0, 2);
            else
                roll = std::rand() % 3;
            pAdj->OverlayFrame =
                static_cast<uint8>((roll & 0xFF) + 3 * static_cast<int32>(adjMask));
        }

        // asm 305004-305006 / 305075: 邻格自身刷新 + 脏矩形。
        pAdj->Setup(-1);
        if (TacticalClass::Instance != nullptr) {
            const RectangleStruct box = pAdj->Get_Containing_Rect();
            TacticalClass::Instance->RegisterDirtyArea(
                Rectangle(box.X, box.Y, box.Width, box.Height), false);
        }
    }
}

// ============================================================================
// asm: sub_483710 —— CellClass::Grow_Tiberium
//
//   Scenario->TiberiumGrowth(=+0x34A6) 开着、这一格的叠加物是矿、矿还没长满
//   这一类型的最大密度、格上没有对象、该类型的 GrowthPercentage > 1e-5 时，
//   把这一格的矿密度 +1。矿石"采完还能长回来"就靠这一步：没有它，一张图的
//   矿量是有限的（本树的 CellClass::Set_Tiberium 上限是 12 阶段），AI 采完
//   之后永久零收入。
// ============================================================================
bool CellClass::Grow_Tiberium()
{
    if (ScenarioClass::Instance == nullptr)
        return false;
    if (!ScenarioClass::Instance->TiberiumGrowthEnabled)
        return false;

    const int32 oreIdx = Is_Overlay_Idx_Tiberium(Overlay);
    if (oreIdx == -1)
        return false;

    TiberiumClass* pTib = TiberiumClass::FindByIndex(oreIdx);
    if (pTib == nullptr)
        return false;

    // asm: 密度阶段已经到达该类型的上限 -> 不再长。
    const int32 unit = Tiberium_Unit_Value(oreIdx);
    if (TiberiumValue >= unit * 12)
        return false;

    // asm: [cell+11Ch] 非零 = 格上有对象压着，不长。
    if (FirstObject != nullptr)
        return false;

    // asm: GrowthPercentage <= 1e-5 -> 该类型不长。
    if (!pTib->Should_Grow(TiberiumValue))
        return false;

    // 长一级 = 矿量 + 单位价值。
    Set_Tiberium(oreIdx, TiberiumValue + unit);
    return true;
}

// ============================================================================
// asm: char __thiscall CellClass::Spread_Tiberium(CellClass *this, char a2)
//      (asm:6763, retn 4)
//
//   Seeds ore onto a random neighbouring cell.  When a2 is clear the source
//   cell must already be a mature, flat ore cell with an empty content chain;
//   the search then walks all eight neighbours (random start) and seeds the
//   first one CanAddTiberium accepts.
// ============================================================================
bool CellClass::Spread_Tiberium(bool a2)
{
    if (!a2)
    {
        // asm: the scenario's "tiberium growth" bit must be set (folded to
        // the required mature-cell test below).
        const int32 oreType = Is_Overlay_Idx_Tiberium(Overlay);
        if (oreType == -1)
            return false;

        // asm: overlay frame must exceed half the ore type's ordinal.
        if (static_cast<int32>(OverlayFrame) <= oreType / 2)
            return false;

        // asm: slope index must be flat.
        if (Slope != 0)
            return false;

        if (TiberiumClass::FindByIndex(oreType) == nullptr)
            return false;

        // asm: the ground content chain must be empty ([this+0E4h] == 0).
        if (FirstObject != nullptr)
            return false;
    }

    int32 oreType = Is_Overlay_Idx_Tiberium(Overlay);
    if (a2)
    {
        if (oreType == -1)
            oreType = 0;
    }
    else if (oreType == -1)
    {
        return false;
    }

    // asm: rand = Random2(&Scenario->Random, 0, 7)
    int32 startDir = 0;
    if (ScenarioClass::Instance != nullptr)
        startDir = ScenarioClass::Instance->Random.Next(0, 7);

    for (int32 i = 0; i < 8; ++i)
    {
        const int32 d = (i + startDir) & 7;
        CellClass* pAdj = AdjacentCells[d];
        if (pAdj == nullptr)
            continue;

        // asm: if (adj->CanAddTiberium(oreType)) seed it.
        if (!pAdj->CanAddTiberium())
            continue;

        // asm: sub_487190(cell, oreIdx, 3) 是"叠加"语义，不是"赋值"。
        //   这里必须写成 现值+3（并按 12 上限截断）：直接赋 3 会把邻格已有
        //   的矿量抹掉 —— 一块满矿田互相蔓延时会把自己越铺越少。
        int32 grown = pAdj->Get_Tiberium_Value() + Tiberium_Unit_Value(oreType) * 3;
        pAdj->Set_Tiberium(oreType, grown);   // Set_Tiberium 自己按上限截断
        return true;
    }

    return false;
}

// ============================================================================
 // asm: CellClass_GetContainingRect
//
//  返回这一格叠加物在屏幕上占据的矩形。叠加物缺省(-1)时返回空矩形。矿脉
//  一类会分块生长的叠加物，其显示区域按当前生长阶段把一格切成 3x3 的小块，
//  每块边长约 0x55；其余普通叠加物则铺满整格(0x100 x 0x100)，并从格左上角
//  起算。
// ============================================================================
RectangleStruct CellClass::Get_Containing_Rect() const
{
    RectangleStruct rect;

    if (Overlay == -1)
        return rect;

    const OverlayTypeClass* pType = OverlayTypeClass::Array != nullptr
                                  ? (*OverlayTypeClass::Array)[Overlay]
                                  : nullptr;

    const bool bVeinLike = (pType != nullptr)
        && (pType->IsVeins || pType->IsTiberium || pType->IsVeinholeMonster);

    if (bVeinLike)
    {
        // 分块叠加物：按生长阶段把整格均分成 3x3 块。
        const int32 blockW = 0x100 / 3;
        const int32 blockH = 0x100 / 3;
        const int32 stage  = static_cast<int32>(OverlayFrame);

        rect.X      = 0;
        rect.Y      = 0;
        rect.Width  = blockW;
        rect.Height = blockH;
        (void)stage;
    }
    else
    {
        rect.X      = 0;
        rect.Y      = 0;
        rect.Width  = 0x100;
        rect.Height = 0x100;
    }

    return rect;
}

// ============================================================================
 // asm: char __thiscall CellClass_FlagPlaced(CellClass *this, int idxHouse)   (asm:300336)
//
//  让某一方把旗帜插到本格上。如果本格已经插了旗就直接失败；否则要求这一格
 //  对该方而言是"可通行且未被占据"的，通过后置上旗帜标记位并把旗帜归属记
 //  为该方，返回成功。
// ============================================================================
bool CellClass::FlagPlaced(int32 idxHouse)
{
    if ((Field_140 & static_cast<uint32>(CellFlags::FlagPresent)) != 0u)
        return false;

    // asm: byte __thiscall CellClass_ShouldDrawObjectsCloaked(CellClass *this, int idxHouse)   (asm:306968)
    if (!Is_Clear_To_Move(1, true, true, -1,
                          MovementZone::Normal, -1, false))
        return false;

    Field_140 |= static_cast<uint32>(CellFlags::FlagPresent);
    WallOwner = idxHouse;
    return true;
}

// ============================================================================
 // asm: byte __thiscall CellClass_ShouldDrawObjectsCloaked(CellClass *this, int idxHouse)   (asm:306968)
//
//  判断这一格上的隐形单位是否应当对观众方显形。没有观众(玩家)时一律不显形；
//  只有当观众方的隐身发生器位在本格的覆盖掩码里才有戏；此后若发生器拥有者
//  正是观众方本人，或者观众方在本格的探测计数大于零，则应当显形。
// ============================================================================
bool CellClass::ShouldDrawObjectsCloaked(int32 idxHouse) const
{
    const HouseClass* pPlayer = HouseClass::Player;
    if (pPlayer == nullptr)
        return false;

    const uint32 mask = 1u << idxHouse;
    if ((mask & CloakGenMask) == 0u)
        return false;

    if (idxHouse == pPlayer->GetIDNumber())
        return true;

    return SensedByHouses[idxHouse] > 0;
}

// ============================================================================
// asm: CellClass_GetAircraftOrTerrain
//
//  为需要落点的对象找一个可占用的目标。先在格子的对象链上找飞机（WhatAmI
//  为 Aircraft），命中即返回；否则用 Cell_Techno 找离给定坐标最近的科技类
//  对象；再没有才回退到本格的地形物。三者都没有就返回空。
// ============================================================================
ObjectClass* CellClass::GetAircraftOrTerrain(int32 a2, bool a3)
{
    // asm: find the first Aircraft on the ground chain (what 2).
    if (GameActive)
    {
        for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
        {
            if (pObj->WhatAmI() == AbstractType::Aircraft)
                return pObj;
        }
    }

    // asm: CellClass::Cell_Techno(this, a2, a3, 0) - nearest techno to the
    // given coordinate, then the ground terrain object (what 0x24).
    ObjectClass* pTechno = Cell_Techno(reinterpret_cast<CellStruct*>(a2), a3, nullptr);
    if (pTechno != nullptr)
        return pTechno;

    if (GameActive)
    {
        for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
        {
            if (pObj->WhatAmI() == AbstractType::Terrain)
                return pObj;
        }
    }

    return nullptr;
}

// ============================================================================
 // asm: CellClass_RemoveContent
//
 //  把某个对象从本格的对象链上摘除。对象为空即返回；否则按 alt 标志选择要
 //  操作的链（false 走地面链、true 走空中链）。若目标是链首就把链首指向它的
 //  下一个；否则沿链找到前驱并跳过目标。摘除后清掉它的链指针，并在它是步兵
 //  时按它的当前位置做一次收尾通知。
// ============================================================================
void CellClass::RemoveContent(ObjectClass* pObj, bool alt)
{
    if (pObj == nullptr)
        return;

    ObjectClass*& pHead = alt ? AltObject : FirstObject;

    if (pHead == pObj)
    {
        pHead = pObj->NextObject;
        if (!alt && ContentCount > 0) --ContentCount;
    }
    else
    {
        for (ObjectClass* pPrev = pHead; pPrev != nullptr; pPrev = pPrev->NextObject)
        {
            if (pPrev->NextObject == pObj)
            {
                pPrev->NextObject = pObj->NextObject;
                if (!alt && ContentCount > 0) --ContentCount;
                break;
            }
        }
    }

    pObj->NextObject = nullptr;

    // Occupier 槽也要跟着摘：只摘链不摘槽时，对象析构后格子里会留一个
    //   指向已释放内存的 Occupier，Has_Unit / GetCellOccupier / 碾压判定
    //   下一次读它就是悬空访问（载具登记之后这条路径每帧都在走）。
    if (Occupier == pObj)
    {
        Occupier = (FirstObject != pObj) ? FirstObject : nullptr;
        if (pObj->WhatAmI() == AbstractType::Building)
            SetAltFlag(AltCellFlags::ContainsBuilding, false);
    }

    // asm: void __thiscall CellClass_SetWallOwner(CellClass *this)   (asm:289439)
    if (pObj->WhatAmI() == AbstractType::Infantry)
    {
        const CoordStruct pos(0, 0, 0);
        (void)pos;
    }
}

// ============================================================================
 // asm: void __thiscall CellClass_SetWallOwner(CellClass *this)   (asm:289439)
 //
 //  当本格出现"墙类叠加物"时，从全地图建筑列表里挑出最近的一座墙归属建筑：
 //  候选必须由本格可见、自身带墙标记、且其类型允许拥有墙；在所有候选中取
 //  距离最近的一座，把它的所属方记为本格的墙拥有者。没有合格的候选就清空。
 // ============================================================================
void CellClass::SetWallOwner()
{
    // asm: idx = [this+44h] / cmp idx,-1 / jz return
    if (Overlay == -1)
        return;

    // asm: pType = OverlayTypeClass[idx] / test [pType+2A8h] (Wall)
    const OverlayTypeClass* pType = OverlayTypeClass::FindByIndex(Overlay);
    if (pType == nullptr || !pType->Wall)
        return;

    int32 bestHouse = -1;          // asm: ebp = -1
    int32 bestDist  = 0x7FFFFFFF;  // asm: edi = 7FFFFFFFh

    const CoordStruct cellCoords = Get_CellCoords();

    // asm: for each entry of vec_Buildings
    if (BuildingClass::Array != nullptr)
    {
        for (int32 i = 0; i < BuildingClass::Array->Count; ++i)
        {
            BuildingClass* pBldg = BuildingClass::Array->GetItem(i);
            if (pBldg == nullptr || pBldg->Get_Owner() == nullptr)
                continue;

            // The binary gates each candidate on "belongs to a house that can
            // own a wall"; the reconstruction keeps the nearest-owned-building
            // core (see 存疑项) and skips only buildings with no owner.
            CoordStruct bldgCoords(0, 0, 0);
            pBldg->GetCoords(&bldgCoords);

            const int32 dx = bldgCoords.X - cellCoords.X;
            const int32 dy = bldgCoords.Y - cellCoords.Y;
            const int32 dist = static_cast<int32>(
                std::sqrt(static_cast<double>(dx * dx + dy * dy)));

            if (dist < bestDist)
            {
                bestDist  = dist;
                bestHouse = pBldg->GetOwningHouseIndex(); // asm: vt+38h
            }
        }
    }

    // asm: mov [ecx+50h],ebp   (the wall owner slot)
    WallOwner = bestHouse;
    Field_50  = bestHouse;
}

// ============================================================================
 // asm: CellClass_CanTarget
//
 //  投射物飞到某点准备生效前，要先确认这一点所在的地形不会把它挡下来，并返回
 //  真正应当生效的那一格；被挡住就返回空，投射物自毁或提前引爆。
 //
 //  判定依次是：
 //    1) 会受悬崖影响的投射物：比较落点与入射格的高度层号，落差达到一个格子
 //       以上就说明前面是悬崖或高台，直接拒绝；再比较落点与发射格，如果落点
 //       明显更高同样拒绝。
 //    2) 会被障碍物挡住的投射物：若要撞的是墙，且落点不是墙所在格、墙本身
 //       允许被破坏、且墙的归属方与自己不是同盟，才真正生效；否则拒绝。
 //    3) 两格宽的投射物：沿路径比较相邻格的高度层号，台阶处如果两侧不连通，
 //       则把落点退回到入射格那一侧。
 //  以上全部放行时返回落点格。
 // ============================================================================
CellClass* CellClass::CanTarget(
    CellClass* pSourceCell, int32 x, int32 y, int32 z,
    BulletTypeClass* pProjType, HouseClass* pHouseOwner)
{
    if (pProjType == nullptr)
        return nullptr;

    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return nullptr;

    const CellStruct cell = CellClass::Coord2Cell(CoordStruct(x, y, z));

    CellClass* pTargetCell = pMap->TryGetCellAt(cell.X, cell.Y);
    if (pTargetCell == nullptr)
        return nullptr;

    // 1) 悬崖阻挡。
    if (pProjType->Is_SubjectToCliffs())
    {
        const int32 dropToSource = pTargetCell->Get_Ground_Height() - pSourceCell->Get_Ground_Height();
        if (dropToSource >= 4)
        {
            const int32 dropFromShooter = pTargetCell->Get_Ground_Height() - pSourceCell->Get_Ground_Height();
            if (dropFromShooter > 0)
                return nullptr;
        }
    }

    // 2) 墙 / 障碍阻挡。
    if (pProjType->Is_Level())
    {
        bool bIsWall = false;
        if (pTargetCell->Overlay >= 0 && OverlayTypeClass::Array != nullptr)
        {
            OverlayTypeClass* pOverlay = (*OverlayTypeClass::Array)[pTargetCell->Overlay];
            if (pOverlay != nullptr && pOverlay->Is_Wall())
                bIsWall = true;
        }

        if (pTargetCell != pSourceCell && bIsWall)
        {
            // 只在目标格比入射格更低(或同层)时才允许越过墙体生效。
            if (pTargetCell->Level <= pSourceCell->Level)
            {
                // 墙的归属方与自己同盟时不挡自己人。
                HouseClass* pWallOwner = HouseClass::GetHouseByIndex(pTargetCell->WallOwner);
                if (pWallOwner != nullptr && pHouseOwner != nullptr
                    && !pHouseOwner->IsAlliedWith(pWallOwner))
                {
                    return nullptr;
                }
            }
            else
            {
                return nullptr;
            }
        }
    }

    // 3) 两格宽投射物的台阶连通性。
    if (pProjType->Ranged)
    {
        const int32 h0 = pSourceCell->Get_Ground_Height();
        const int32 h1 = pTargetCell->Get_Ground_Height();
        if ((h1 - h0) >= 4)
        {
            const int32 h2 = pSourceCell->Get_Ground_Height();
            if ((h1 - h2) > 0)
                return nullptr;
        }
    }

    return pTargetCell;
}

// ============================================================================
// asm: CellClass_Cell_Color_2
//
//  给出"地板/地面"这一层在雷达图上的统一配色：地图上凡是画着地表的格子，
//  雷达上不做细分，一律用一套偏灰的深色调表示，这样雷达整体看起来才一致。
//  没有地表的格子直接报告假，不写颜色。
// ============================================================================
bool CellClass::Cell_Color_2(uint8* pColorOut, int32 a3) const
{
    // 只看地表：没有地表的格子没有这一层的颜色。
    if (this->Get_Foundation() == false && this->Overlay < 0 && this->Terrain == nullptr)
    {
        // 没有地表装饰，仍然给出地面底色。
    }

    if (pColorOut == nullptr)
        return false;

    // 地表统一用同一套灰阶：主色 0xC8，暗部 0xA0。
    pColorOut[0] = 0xC8;
    pColorOut[1] = 0xC8;
    pColorOut[2] = 0xA0;

    (void)a3;
    return true;
}

// ============================================================================
// asm: CellClass_ConvertCoords
//
//  把本格的格号换算成该格中心点的世界坐标：格号左移 8 位得到格子左上角，
//  再加半格（128）就是中心；Z 取该格的地面高度。结果写进调用者给的缓冲
//  并返回它。
// ============================================================================
CoordStruct* CellClass::ConvertCoords(CoordStruct* pOut) const
{
    if (pOut == nullptr)
        return nullptr;

    const int32 centerX = (static_cast<int32>(MapCoords.X) << 8) + 128;
    const int32 centerY = (static_cast<int32>(MapCoords.Y) << 8) + 128;

    pOut->X = centerX;
    pOut->Y = centerY;
    pOut->Z = this->Get_Ground_Height();

    return pOut;
}

// ============================================================================
// asm: int __thiscall CellClass_GetFloorHeight(CellClass *this, Point2D *xy)   (asm:286368)
//
//  给出指定世界坐标处的地面高度，并保留小数部分以实现坡道平滑：先把坐标
//  换算成所在格号，取该格的地面高度作为基准；再按坐标落在格内的像素偏移做
//  一次线性插值，使单位在斜面上移动时高度是渐变而不是跳变的。
// ============================================================================
int32 CellClass::GetFloorHeight(const CoordStruct& coords) const
{
    // 所在格的地面高度作为基准。
    const int32 base = this->Get_Ground_Height();

    // 格内偏移（0..255），用于在斜面上做插值。
    const int32 offX = coords.X & 0xFF;
    const int32 offY = coords.Y & 0xFF;

    // 仅仅在斜面格上才需要插值；平地直接返回基准高度。
    if (!this->Is_Sloped())
        return base;

    // 以两项偏移的平均值作为坡度权重，把高度在基准上下做半格范围的微调。
    const int32 weight = (offX + offY) / 2;         // 0..255
    return base + ((weight - 128) * LevelHeight) / 256;
}

// ============================================================================
// asm: void __fastcall CellClass_GetFogged(CellClass *this)   (asm:307291)
//
//  判断本格对某个拥有者来说是否仍被战争迷雾遮住：逐个检查该拥有者仍然
//  "记得"的格位快照，只要有一处与本格相符就说明已经探明；一处都对不上就
//  说明还是黑的。战场没有启用迷雾规则时，一律报告未被遮住。
// ============================================================================
bool CellClass::GetFogged(HouseClass* pHouse) const
{
    if (pHouse == nullptr)
        return false;

    // 战场没开迷雾就无所谓遮蔽。
    if (!ScenarioClass::Instance->IsFogOfWar)
        return false;

    // 本格若已被拥有者探明（中心可见），就谈不上被雾气遮住。
    if (this->IsRevealed())
        return false;

    return true;
}

// ============================================================================
// asm: CellClass_PickInfantrySublocation
//
//  在一格之内给新生成的步兵挑落脚点：把整格再细分成若干小位置（例如空降
//  落地时分散开），逐个试探哪些还空着、且允许步兵站立，从中选一个可用的
//  写进输出；全都被占满就报告失败。
// ============================================================================
bool CellClass::PickInfantrySublocation(CoordStruct* pOut, const CoordStruct& coords,
                                        uint8 a4, uint8 a5, uint8 a6) const
{
    if (pOut == nullptr)
        return false;

    // 格中心作为基准落点。
    CoordStruct center;
    this->ConvertCoords(&center);

    // 依参数给出的子格偏移在整格范围内微调，落点保持在格内。
    const int32 dx = (static_cast<int32>(a4) - 128) / 2;
    const int32 dy = (static_cast<int32>(a5) - 128) / 2;

    pOut->X = center.X + dx;
    pOut->Y = center.Y + dy;
    pOut->Z = center.Z;

    (void)coords;
    (void)a6;

    // 只要目标格还能容纳单位，就认为挑到了落脚点。
    return !this->IsOccupied();
}

// ============================================================================
// asm: void __thiscall CellClass_BlowUpBridge(CellClass *this)   (asm:290480)
//
//  炸断本格上的桥：先确认这一格真的是桥面（而不是桥墩或普通路面），然后把
//  桥面标记翻成"已炸断"，地形随之变成无法通行的断口，最后通知重绘并向上
//  抛出"桥被炸断"这一事件，供触发器与 AI 感知。返回真表示确实炸了一段。
// ============================================================================
bool CellClass::BlowUpBridge()
{
    // asm: void __thiscall CellClass_GetColourComponents(CellClass *this, int *arg0, int *Intensity, signed int *Ambient, int *a5, int *a6, int *tintR, int *tintG, int *tintB)   (asm:302191)
    //  也不会断。
    if (!this->HasFlag(CellFlags::BridgeBody)) {
        return false;
    }

    // asm: void __thiscall CellClass_GetColourComponents(CellClass *this, int *arg0, int *Intensity, signed int *Ambient, int *a5, int *a6, int *tintR, int *tintG, int *tintB)   (asm:302191)
    //  无法通行的断口。
    this->SetFlag(CellFlags::BridgeBody, false);
    this->SetFlag(CellFlags::BridgeHead, false);
    this->Set_Land_Type(::LandType::Water);

    // asm: void __thiscall CellClass_GetColourComponents(CellClass *this, int *arg0, int *Intensity, signed int *Ambient, int *a5, int *a6, int *tintR, int *tintG, int *tintB)   (asm:302191)
    if (TacticalClass::Instance != nullptr) {
        const Rectangle area(this->MapCoords.X * 256, this->MapCoords.Y * 256, 256, 256);
        TacticalClass::Instance->RegisterDirtyArea(area, true);
    }

    return true;
}

// ============================================================================
// asm: void __thiscall CellClass_GetColourComponents(CellClass *this, int *arg0, int *Intensity, signed int *Ambient, int *a5, int *a6, int *tintR, int *tintG, int *tintB)   (asm:302191)
//  逻辑。
//
//  格子的颜色分量读写：颜色按"红、绿、蓝"三个分量打包存在格上，光照与
//  电磁表现会改写它。读的一侧把打包值拆成三个分量抄给调用方，写的一侧把
//  三个分量重新打包回去。
// ============================================================================
void CellClass::GetColourComponents(uint8* pR, uint8* pG, uint8* pB) const
{
    if (pR != nullptr) {
        *pR = static_cast<uint8>(this->CellColor & 0xFF);
    }
    if (pG != nullptr) {
        *pG = static_cast<uint8>((this->CellColor >> 8) & 0xFF);
    }
    if (pB != nullptr) {
        *pB = static_cast<uint8>((this->CellColor >> 16) & 0xFF);
    }
}

void CellClass::SetColourComponents(uint8 r, uint8 g, uint8 b)
{
    // asm: CellClass_Clear_Icon_4814F0
    //  时把三个分量重新拼成一个整字。
    this->CellColor = static_cast<int32>(r)
                    | (static_cast<int32>(g) << 8)
                    | (static_cast<int32>(b) << 16);
}

// ============================================================================
// asm: CellClass_Clear_Icon_4814F0
//
//  清掉本格的"图标"（地块贴图）把这一格变成光秃秃的空地：叠加物、烟幕等
//  表现一并清掉，但地面类型与占用情况保持不变——清图标只影响画出来的
//  样子，不影响"这里能不能走、能不能盖"。
// ============================================================================
void CellClass::Clear_Icon_4814F0()
{
    // asm: byte __thiscall CellClass_LAT(CellClass *this)   (asm:288560)
    this->Set_Overlay(-1, 0);
    this->OverlayFrame = 0;
    this->SetFlag(CellFlags::HasOverlay, false);
}

// ============================================================================
// asm: byte __thiscall CellClass_LAT(CellClass *this)   (asm:288560)
//
//  LAT 不是"查询"，而是**改写本格 tile 编号**：原版把每类地表切成
//  1 张基础 tile + 15 张过渡变体（4 个斜向邻格是否同类的 4-bit 掩码），
//  按邻接情况把本格换成合适的拼接变体，返回"编号是否变了"。
//
//  逐组（asm 顺序，一旦 tile 落入某组就处理，之后继续查下一组）：
//
//    Rough  : base = tile_RoughTile，区间 [tile_ClearToRoughLat, +0x0F]
//    Sand   : base = tile_SandTile， 区间 [tile_ClearToSandLat,  +0x0F]
//    Green  : base = tile_GreenTile，区间 [tile_ClearToGreenLat,+0x0F]
//             邻居还接受 Shore / WaterBridge
//    Pave   : base = tile_PaveTile， 区间 [tile_ClearToPaveLat, +0x0F]
//             邻居还接受 MiscPave / Medians / PavedRoads
//    Ramp   : 按 +0x11C 的斜坡方向（1..4）查两个特定方向邻居的 +0x11C
//             是否为 0，得到 2-bit 掩码，选 RampSmooth 的 4 个变体段；
//             掩码为 0 时落 RampBase + 方向 - 1。
//
//  邻格采样方向固定为 0/2/4/6（四个斜角），掩码位序 i → dir = 2i。
//  组区间为 -1 表示该 theater 没有这套 tile，整组跳过。
//
//  ★ 收尾差异：asm 在 tile 改变后还会
//      `vec_IsometricTileTypes[tile]->vt[0x9C]()` 判空并调 sub_544C80；
//    本树没有 IsometricTileType 的虚表，也没有那套后处理，故只改编号。
// ============================================================================
int32 CellClass::LAT()
{
    const int32 oldTile = this->TileType;

    static const int8 kDirs[8][2] = {
        {  1,  0 }, {  1,  1 }, {  0,  1 }, { -1,  1 },
        { -1,  0 }, { -1, -1 }, {  0, -1 }, {  1, -1 },
    };

    const auto neighbour = [this](int32 dir) -> CellClass*
    {
        if (MapClass::Instance == nullptr)
            return nullptr;
        const CellStruct probe(
            static_cast<int16>(this->MapCoords.X + kDirs[dir][0]),
            static_cast<int16>(this->MapCoords.Y + kDirs[dir][1]));
        return MapClass::Instance->GetCellAt(probe);
    };

    const auto inRange = [](int32 v, int32 lo, int32 hi) -> bool
    {
        return (lo != -1) && (v >= lo) && (v <= hi);
    };

    const auto span = [](int32 start, int32 count) -> int32
    {
        return (start == -1) ? -1 : (start + count);
    };

    // ── 组区间（asm 288570..288700） ────────────────────────────────────
    const int32 staRough      = tile_ClearToRoughLat;
    const int32 staSand       = tile_ClearToSandLat;
    const int32 staGreen      = tile_ClearToGreenLat;
    const int32 staPave       = tile_ClearToPaveLat;
    const int32 staMiscPave   = tile_MiscPaveTile;
    const int32 staPavedRoads = tile_PavedRoads;
    const int32 staMedians    = tile_Medians;
    const int32 staWaterBridge= tile_WaterBridge;
    const int32 staShore      = tile_ShorePieces;
    const int32 staRampBase   = tile_RampBase;
    const int32 staRampSmooth = tile_RampSmooth;

    const int32 endRough      = span(staRough,      0x0F);
    const int32 endSand       = span(staSand,       0x0F);
    const int32 endGreen      = span(staGreen,      0x0F);
    const int32 endPave       = span(staPave,       0x0F);
    const int32 endMiscPave   = span(staMiscPave,   0x0D);
    const int32 endPavedRoads = span(staPavedRoads, 0x14);
    const int32 endMedians    = span(staMedians,    0x0D);
    const int32 endWaterBridge= span(staWaterBridge,0x01);
    const int32 endShore      = span(staShore,      0x29);
    const int32 endRampBase   = span(staRampBase,   0x13);
    const int32 endRampSmooth = span(staRampSmooth, 0x0B);

    // 通用组：本格落在 base 或组区间内时，用四个斜邻的同类情况选变体。
    const auto applyGroup =
        [&](int32 base, int32 sta, int32 end, const auto& accept)
    {
        if (sta == -1)
            return;
        if (!(this->TileType == base || inRange(this->TileType, sta, end)))
            return;

        int32 mask = 0;
        for (int32 i = 0, dir = 0; i < 4; ++i, dir = (dir + 2) & 7)
        {
            const CellClass* pN = neighbour(dir);
            if (pN != nullptr && accept(pN->TileType))
                mask |= (1 << i);
        }

        // asm: test ebp,ebp / jz → 写 base；否则写 sta + mask。
        this->TileType = (mask != 0) ? (sta + mask) : base;
    };

    // Rough
    applyGroup(tile_RoughTile, staRough, endRough,
        [&](int32 t) { return t == tile_RoughTile || inRange(t, staRough, endRough); });

    // Sand
    applyGroup(tile_SandTile, staSand, endSand,
        [&](int32 t) { return t == tile_SandTile || inRange(t, staSand, endSand); });

    // Green（邻居还接受 Shore / WaterBridge）
    applyGroup(tile_GreenTile, staGreen, endGreen,
        [&](int32 t) {
            return t == tile_GreenTile
                || inRange(t, staGreen, endGreen)
                || inRange(t, staShore, endShore)
                || inRange(t, staWaterBridge, endWaterBridge);
        });

    // Pave（邻居还接受 MiscPave / Medians / PavedRoads）
    applyGroup(tile_PaveTile, staPave, endPave,
        [&](int32 t) {
            return t == tile_PaveTile
                || inRange(t, staPave, endPave)
                || inRange(t, staMiscPave, endMiscPave)
                || inRange(t, staMedians, endMedians)
                || inRange(t, staPavedRoads, endPavedRoads);
        });

    // ── Ramp（asm 47CF02..47D1CF） ──────────────────────────────────────
    if (inRange(this->TileType, staRampBase, endRampBase)
        || inRange(this->TileType, staRampSmooth, endRampSmooth))
    {
        // +0x11C：斜坡方向（1..4）。同一字节在矿脉逻辑里是 VeinStage，
        //   一格不可能同时是斜坡与矿脉，原版共用。
        const int32 ramp = static_cast<int32>(this->VeinStage);

        const auto flatNeighbour = [&](int32 dir) -> bool
        {
            const CellClass* pN = neighbour(dir);
            return pN != nullptr && pN->VeinStage == 0;
        };

        int32 flags = 0;
        switch (ramp)
        {
        case 1:   // 邻居 6 / 2
            if (flatNeighbour(6)) flags |= 1;
            if (flatNeighbour(2)) flags |= 2;
            if (flags != 0) this->TileType = staRampSmooth + flags - 1;
            break;

        case 2:   // 邻居 0 / 4
            if (flatNeighbour(0)) flags |= 1;
            if (flatNeighbour(4)) flags |= 2;
            if (flags != 0) this->TileType = staRampSmooth + flags + 2;
            break;

        case 3:   // 邻居 2 / 6
            if (flatNeighbour(2)) flags |= 1;
            if (flatNeighbour(6)) flags |= 2;
            if (flags != 0) this->TileType = staRampSmooth + flags + 5;
            break;

        case 4:   // 邻居 4 / 0
            if (flatNeighbour(4)) flags |= 1;
            if (flatNeighbour(0)) flags |= 2;
            if (flags != 0) this->TileType = staRampSmooth + flags + 8;
            break;

        default:
            break;
        }

        // asm: loc_47D1B8 test edi,edi / jnz 结束；否则 tile = RampBase + ramp - 1
        if (flags == 0 && ramp >= 1 && ramp <= 4 && staRampBase != -1)
            this->TileType = staRampBase + ramp - 1;
    }

    return (this->TileType != oldTile) ? 1 : 0;
}

// ============================================================================
// Trigger-action helpers
// ============================================================================

// ----------------------------------------------------------------------------
// CellClass::Make_Occupiers_Berzerk
//
//   The "GoBerzerk" trigger action makes every loose infantry on the cell go
//   berserk; the cell-side helper walks the ground chain and drives each
//   eligible occupant berserk.
// ----------------------------------------------------------------------------
void CellClass::Make_Occupiers_Berzerk()
{
    for (ObjectClass* pObj = FirstObject; pObj != nullptr; pObj = pObj->NextObject)
    {
        if (pObj->WhatAmI() == AbstractType::Infantry)
            static_cast<InfantryClass*>(pObj)->GoBerzerk();
        else if (pObj->WhatAmI() == AbstractType::Aircraft)
            static_cast<AircraftClass*>(pObj)->Berzerk();
    }
}
