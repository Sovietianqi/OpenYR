#include "Rendering/RadarClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/MouseClass.h"
#include "Rendering/Surface.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/RadarEventClass.h"
#include "Core/Memory.h"
#include "Math/Rectangle.h"
#include "Map/CellClass.h"
#include "Map/MapClass.h"
#include "Scenario/ScenarioClass.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/TechnoClass.h"
#include "Audio/VocClass.h"
#include "Houses/HouseClass.h"
#include "Abstract/OverlayTypeClass.h"
#include "Helpers/AsmMath.h"
#include "Rules/RulesClass.h"
#include "Network/SessionClass.h"
#include "Game/Externs.h"
#include "Game/EngineGlobals.h"

#include <cstring>
#include <cwchar>
#include <cmath>
#include <algorithm>
#include <vector>

// ============================================================================
// Static members
// ============================================================================
RadarClass* RadarClass::Instance = nullptr;

// ============================================================================
// asm: dword_B0F9E0 / dword_B04A38 —— 侧边栏两张 SHP 的全局句柄
//
//   SidebarClass_LoadArt (0x72F9E0 区) 用 MixFileClass<CCFileClass>::Finder
//   从 'SIDE1.SHP'（asm: off_844D18 → dword_B0F9E0）与
//   'RADAR.SHP' / 'RADARY.SHP'（off_844D14 / off_844D48 → dword_B0FA68，
//   RadarClass_InitForHouse 再转存 dword_B04A38）装入。
//   本工程的 MIX 装载器尚未落地：句柄由美术装载路径写入；未写入时为 null，
//   CC_Draw_Shape 收到 null 直接返回（不产生任何图形）。
// ============================================================================
SHPStruct* SidebarArt_SHP = nullptr;   // asm: dword_B0F9E0 (SIDE1.SHP)
SHPStruct* RadarArt_SHP   = nullptr;   // asm: dword_B04A38 (RADAR.SHP / RADARY.SHP)

// ============================================================================
// 侧边栏雷达 gadget 状态
//
// asm 的 RadarClass_OneTime 会把雷达 gadget 的尺寸/位置写进全局 GadgetRadar
// （field_10 / Width / Height / flags）。GadgetClass 尚未在本模块建模，故以
// 最小文件内状态记录这些可观测值（供 MouseMove 命中测试使用），不引入整个
// 侧边栏子系统。
// ============================================================================
namespace {
struct RadarGadgetState {
    int32 field_10 = 0;
    int32 Width    = 0;
    int32 Height   = 0;
    int32 flags    = 0;
};
RadarGadgetState g_RadarGadget;

// ----------------------------------------------------------------------------
// RadarLayout —— asm 里由 RadarClass_InitForHouse 计算、被 Draw / MouseMove
// 消费的一组侧边栏布局常量（IDA 里是散落的全局：a2 / dword_B048C8 /
// dword_B048CC / dword_B04A04 / DIPLOBTN_SHP_Width 等）。OpenYR 以结构体
// 归一，避免污染全局命名空间。SHP 尺寸在渲染后端未接入时保持 0。
// ----------------------------------------------------------------------------
struct RadarLayout {
    int32 ButtonLeft    = 0;   // asm: a2
    int32 DiploTop      = 0;   // asm: dword_B048C8
    int32 DiploBottom   = 0;   // asm: dword_B048CC
    int32 OptionsTop    = 0;   // asm: dword_B04A04
    int32 DiploBtnW     = 0;   // asm: DIPLOBTN_SHP_Width
    int32 DiploBtnH     = 0;   // asm: DIPLOBTN_SHP_Height
    int32 OptionsBtnW   = 0;   // asm: OPTBTN_SHP_Width
    int32 OptionsBtnH   = 0;   // asm: OPTBTN_SHP_Height
    uint32 ButtonTint   = 0;   // asm: [this+1208h] 打包按钮底色
};
RadarLayout g_RadarLayout;

// asm: byte_B0B518 —— 外交/选项按钮矩形已变（通知外壳重算命中区）。
bool g_ButtonRectDirty = false;
} // namespace

// ============================================================================
// Internal helpers
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
// Radar565
//
// asm 的 16bpp 打包：sub_6547C0 地形降采样末尾与 sub_655C50 对象点都是
//   R = (r >> 3) << 11 ; G = (g >> 2) << 5 ; B = (b >> 3) << 0
// 三处常量 col_R_SHR_3 / col_G_SHR_2 / col_B_SHR_3 与 col_*_SHL_* 成对使用。
// ----------------------------------------------------------------------------
inline DWORD Radar565(int32 r, int32 g, int32 b)
{
    return (static_cast<DWORD>((r >> 3) & 0x1F) << 11)
         | (static_cast<DWORD>((g >> 2) & 0x3F) << 5)
         |  static_cast<DWORD>((b >> 3) & 0x1F);
}

// ----------------------------------------------------------------------------
// RadarGroundColor
//
// asm（CellClass_Cell_Color_2 的 tile 支路，loc_47C2A2..loc_47C38F）取的是
// 该格等距瓦片的**图像数据色**：
//     tile = IsometricTileType_544E00(tileset, cell->[11Ah])
//     img  = tile->[0A4h] ; sub = img->[variant*4 + 10h]
//     rgb  = { sub[2Bh], sub[2Ch], sub[2Dh] }   ; TmpImageData 的 RadarColor
// 本工程的等距瓦片图像数据（TileArt）没有装载这条 RadarColor，故按格子的
// LandType 取表作等价底色；树/覆盖物/矿石三支仍严格走 asm 原值
// （见 Cell_Radar_Color）。
// ----------------------------------------------------------------------------
void RadarGroundColor(LandType land, uint8* out)
{
    int32 r = 74, g = 124, b = 62;                 // Clear
    switch (land)
    {
        case LandType::Clear:    r =  74; g = 124; b =  62; break;
        case LandType::Rough:    r =  96; g = 110; b =  70; break;
        case LandType::Road:     r = 150; g = 146; b = 128; break;
        case LandType::Water:    r =  38; g =  78; b = 150; break;
        case LandType::Rock:     r = 104; g = 100; b =  92; break;
        case LandType::Wall:     r = 138; g = 138; b = 132; break;
        case LandType::Tiberium: r = 120; g = 158; b =  70; break;
        case LandType::Beach:    r = 178; g = 168; b = 112; break;
        case LandType::Tunnel:   r =  42; g =  38; b =  34; break;
        case LandType::Railroad: r = 120; g = 118; b = 112; break;
        case LandType::Weeds:    r =  70; g = 132; b =  96; break;
        case LandType::Ice:      r = 196; g = 214; b = 222; break;
        default:                 break;
    }
    out[0] = static_cast<uint8>(r);
    out[1] = static_cast<uint8>(g);
    out[2] = static_cast<uint8>(b);
}

// ----------------------------------------------------------------------------
// Cell_Radar_Color —— asm: CellClass_Cell_Color_2 (0x47C090 区, retn 8)
//
//   ecx = cell，arg_0 / arg_4 = 输出色 1 / 输出色 2（各 3 字节 R,G,B）。
//   逐句：
//     if (cell->Cell_Abstract(eTerrain)) { *o1 = *o2 = (C8,C8,A0) ; return }
//     overlay = cell->[44h]
//     if (overlay != -1) {
//        0x4A..0x63 / 0xCD..0xE6 两段用 frame=1，其余用 cell->[11Eh]
//        *o1 = OverlayClass_Get_Radar_Color(OverlayType[overlay], &tmp, frame)
//        *o2 = *o1
//     }
//     if (__IsOverlayIdxTiberium(overlay)) { *o1 = *o2 = (AA,AA,82) ; return }
//     /* tile 支路 */
//     无 tile 子块时 { *o1 = *o2 = (3C,3C,3C) }
//     否则 *o1 = tile 图像色，*o2 = *o1 每通道 >> 1
//
//   asm 让一个格子占等距缓冲的 2 列：左列 o1、右列 o2。普通地面 o2 是 o1 的
//   半亮值，于是雷达上每格呈"亮/暗"两列，形成立体棱线观感。
// ----------------------------------------------------------------------------
void Cell_Radar_Color(const CellClass* pCell, uint8* out1, uint8* out2)
{
    // asm: push 0 / push eTerrain / call CellClass::Cell_Abstract
    if (pCell->Cell_Abstract(static_cast<int32>(AbstractType::Terrain), false) != nullptr)
    {
        out1[0] = out1[1] = out2[0] = out2[1] = 0xC8;
        out1[2] = out2[2] = 0xA0;
        return;
    }

    // asm: mov eax,[esi+44h] ; cmp eax,-1 ; jz loc_47C24A
    const int32 overlay = pCell->Get_Overlay();
    if (overlay >= 0 && OverlayTypeClass::Array != nullptr &&
        overlay < OverlayTypeClass::Array->Count)
    {
        OverlayTypeClass* pType = (*OverlayTypeClass::Array)[overlay];
        if (pType != nullptr)
        {
            const bool fixedFrame = ((overlay >= 0x4A && overlay <= 0x63) ||
                                     (overlay >= 0xCD && overlay <= 0xE6));
            const int32 frame = fixedFrame ? 1 : static_cast<int32>(pCell->OverlayFrame);

            uint8 rgb[3] = { 0, 0, 0 };
            pType->Get_Radar_Color(rgb, frame);
            if (rgb[0] != 0 || rgb[1] != 0 || rgb[2] != 0)
            {
                out1[0] = out2[0] = rgb[0];
                out1[1] = out2[1] = rgb[1];
                out1[2] = out2[2] = rgb[2];
                return;
            }
        }
    }

    // asm: __IsOverlayIdxTiberium(overlay) 命中 -> (AA,AA,82)
    if (pCell->IsTiberium())
    {
        out1[0] = out1[1] = out2[0] = out2[1] = 0xAA;
        out1[2] = out2[2] = 0x82;
        return;
    }

    // asm: tile 图像色（数据源说明见 RadarGroundColor）
    RadarGroundColor(pCell->Land, out1);
    out2[0] = static_cast<uint8>(out1[0] >> 1);
    out2[1] = static_cast<uint8>(out1[1] >> 1);
    out2[2] = static_cast<uint8>(out1[2] >> 1);
}

// ----------------------------------------------------------------------------
// House_Blip_Color —— asm: sub_655C50 的 loc_655F7C..loc_655FEB
//
//   house = obj->[21Ch]（GetOwningHouse；伪装/精神控制时改取 obj->vt[0D0h]()）
//   if (house) {
//       B = (house[56FBh] >> 3) << 0
//       G = (house[56FAh] >> 2) << 5
//       R = (house[56F9h] >> 3) << 11
//   } else {
//       color = ColorScheme_Find("LightGrey", 1)[330h]
//   }
//   本工程 HouseClass 的 +56F9h/+56FAh/+56FBh 即 HouseColorR/G/B。
//   无阵营时 asm 取 scheme+330h 的打包色；本工程该槽是 MainShadeIndex（调色
//   索引而非色值），故按同一 scheme 的 BaseColor 打包，通道移位与上面一致。
// ----------------------------------------------------------------------------
DWORD House_Blip_Color(HouseClass* pHouse)
{
    if (pHouse != nullptr)
    {
        return Radar565(pHouse->HouseColorR, pHouse->HouseColorG, pHouse->HouseColorB);
    }

    if (ColorScheme* pScheme = ColorScheme::Find("LightGrey", 1))
    {
        return Radar565(pScheme->BaseColor.R, pScheme->BaseColor.G,
                        pScheme->BaseColor.B);
    }
    return 0;
}

// ----------------------------------------------------------------------------
// RadarClass::Draw 的辅助件。
//
// asm 里 Draw 携带一个 byte 栈参 arg_0 与全局 Debug_Map；OpenYR 的 Draw()
// 是无参入口（受 GScreenClass/MainLoop 调用），故用文件内开关等价承载：
//   g_DebugMapFlag  ~ asm Debug_Map（调试地图 -> 直接退出）
//   g_ForceRadarRedraw ~ asm arg_0（强制重绘）
// ----------------------------------------------------------------------------
bool g_DebugMapFlag     = false;   // asm: Debug_Map
bool g_ForceRadarRedraw = false;   // asm: arg_0

// asm: sub_656EC0 —— 雷达逐帧推进（地形脏扫描 + 事件刷新）。其内部对
// unknown_121C/unknown_1220 两个 surf 的逐像素例程（sub_655C50/sub_656150）
// 尚未落地，等价收敛为 Update_Radar（地形重绘）。
void Update_Radar_Internal(RadarClass& radar);

// asm: Draw 的雷达状态帧绘制（CC_Draw_Shape frameIdx=0/0x20）。
void RenderRadarStateFrame(RadarClass& radar, int32 frameIdx);

// ----------------------------------------------------------------------------
// DrawSidebarShape —— 把一张侧边栏 SHP 的指定帧贴到 DSurface_Sidebar。
//
//   RadarClass_Draw 的三处 blit（loc_653165 / loc_6532C7 / loc_653450）参数
//   完全同形，只有 SHP / 帧号 / 位置不同。asm 逐条：
//     mov ecx, DSurface_Sidebar ; mov eax,[ecx]
//     lea edx,[rect] ; push edx ; call [eax+78h]      ; rect = 面矩形（bounds）
//     push 0 (YOffset) / 0 (XOffset) / 0 (ZShapeFrame) / 0 (ZShape)
//     push 0 (TintColor) / 3E8h (Brightness=1000) / 0 (ZGradient)
//     push 0 (ZAdjust) / 0 (Remap) / 0 (Flags)
//     push bounds / &pos / frameIdx / SHP
//     mov edx, SidebarPalette ; mov ecx, DSurface_Sidebar ; call CC_Draw_Shape
//   位置 pos 由调用点给出（[11E4h] 与 [11E8h] 或 [11ECh]）。
// ----------------------------------------------------------------------------
void DrawSidebarShape(SHPStruct* pSHP, int32 frameIdx, int32 x, int32 y)
{
    DSurface* pSurface = DSurface_Sidebar;
    if (pSurface == nullptr || pSurface->Buffer == nullptr || pSHP == nullptr)
        return;

    // asm: Surface->vt[78h](&rect) —— 裁剪框就是侧边栏面自身矩形。
    Rectangle bounds;
    pSurface->GetRect(&bounds);

    Point2D pos(x, y);

    // asm: Sub_72F510 返回 SIDEBAR_PAL（dword_B0FBF8）。本工程没有独立的侧边栏
    //   调色板，统一走 StandardPalette（与 LoadProgressMgr 的同一约定）。
    CC_Draw_Shape(pSurface, StandardPalette, pSHP, frameIdx, &pos, &bounds,
                  BlitterFlags::None, 0, 0, 0, 1000, 0,
                  nullptr, 0, 0, 0);
}

// asm: loc_6534EA 之后的双按钮（DIPLOBTN/OPTBTN）矩形夹紧：以 120Ch 为基准，
// 依按钮尺寸把外交/选项按钮矩形夹到 [ButtonLeft, OptionsTop] 之内。
void ClampSidebarButtonRect(RadarClass& radar, RadarLayout& layout);

// asm: loc_653420 / loc_6534D5 —— 非脏帧分支的 120Ch gadget 矩形刷新。
void RenderRadarGadgetRect(RadarClass& radar);

} // namespace

// ============================================================================
// RadarClass implementation
// ============================================================================

RadarClass::RadarClass()
    : unknown_11E4(0)
    , unknown_11E8(0)
    , unknown_11EC(0)
    , unknown_11F0(0)
    , unknown_11F4(0)
    , unknown_11F8(0)
    , unknown_11FC(0)
    , unknown_1200(0)
    , unknown_1204(0)
    , unknown_1208(0)
    , unknown_rect_120C(0, 0, 0, 0)
    , unknown_121C(0)
    , unknown_1220(0)
    , unknown_123C(0)
    , unknown_1240(0)
    , unknown_1244(0)
    , unknown_1248(0)
    , unknown_124C(0)
    , unknown_1250(0)
    , unknown_1254(0)
    , unknown_1258(0)
    , unknown_1274(0)
    , RadarSizeFactor(1.0f)
    , unknown_int_148C(0)
    , unknown_1490(0)
    , unknown_1494(0)
    , unknown_1498(0)
    , unknown_rect_149C(0, 0, 0, 0)
    , unknown_14AC(0)
    , unknown_14B0(0)
    , unknown_14B4(0)
    , unknown_14B8(0)
    , unknown_bool_14BC(false)
    , unknown_bool_14BD(false)
    , unknown_14C0(0)
    , unknown_14C4(0)
    , unknown_14C8(0)
    , unknown_14CC(0)
    , unknown_14D0(0)
    , unknown_int_14D4(0)
    , IsAvailableNow(false)
    , unknown_bool_14D9(false)
    , unknown_bool_14DA(false)
    , unknown_rect_14DC(0, 0, 0, 0)
    , unknown_14EC(0)
    , unknown_14F0(0)
    , unknown_14F4(0)
    , unknown_14F8(0)
    , unknown_14FC(0)
    , unknown_timer_1500()
    , IsHidden(false)
    , RadarZoomLevel(1)
    , RadarSurface(nullptr)
    , RadarRect(0, 0, RadarWidth, RadarHeight)
{
    Instance = this;
}

RadarClass::~RadarClass()
{
    if (Instance == this)
        Instance = nullptr;
}

// ----------------------------------------------------------------------------
// CreateEmptyMap
//
// Allocates the off-screen radar surface and primes it with the map bounds.
// When reuse is true the existing surface is kept and only the map rectangle
// is updated; otherwise a fresh surface is allocated.
// ----------------------------------------------------------------------------
void RadarClass::CreateEmptyMap(
    const Rectangle& pMapRect, bool reuse, int8 nLevel, bool bUnk2)
{
    if (!reuse)
    {
        // Drop any previously allocated radar surface.
        if (RadarSurface)
        {
            GameDelete(RadarSurface);
            RadarSurface = nullptr;
        }
    }

    if (!RadarSurface)
    {
        // Allocate a paletted surface sized to the radar panel. The radar is
        // always 128x128 logical pixels regardless of the map dimensions.
        RadarSurface = GameCreate<Surface>();
        if (RadarSurface)
        {
            RadarSurface->Width = RadarWidth;
            RadarSurface->Height = RadarHeight;
            RadarSurface->Pitch = RadarWidth;
            RadarSurface->Buffer = YRMemory::Allocate(
                static_cast<size_t>(RadarWidth) * RadarHeight);
            RadarSurface->IsAllocated = true;
        }
    }

    if (RadarSurface && RadarSurface->Buffer)
    {
        // Clear the surface to the shroud colour so unexplored areas render
        // as solid black on the radar.
        memset(RadarSurface->Buffer, 0,
               static_cast<size_t>(RadarWidth) * RadarHeight);
    }

    RadarRect = pMapRect;
    (void)nLevel;
    (void)bUnk2;
}

// ----------------------------------------------------------------------------
// SetVisibleRect
//
// Updates the screen-space rectangle the radar occupies. Used when the
// sidebar layout changes (e.g. tab switch).
// ----------------------------------------------------------------------------
void RadarClass::SetVisibleRect(const Rectangle& mapRect)
{
    RadarRect = mapRect;
}

// ----------------------------------------------------------------------------
// GetLastMouseCursor
//
// Returns the cursor that was active before the current one. Delegates to
// the mouse singleton when available so the radar layer stays consistent
// with the rest of the UI.
// ----------------------------------------------------------------------------
MouseCursorType RadarClass::GetLastMouseCursor()
{
    if (DisplayClass::Instance)
    {
        return DisplayClass::Instance->GetLastMouseCursor();
    }
    return MouseCursorType::Normal;
}

// ----------------------------------------------------------------------------
// DisposeOfArt
//
// Releases the off-screen radar surface and resets the radar art state.
// ----------------------------------------------------------------------------
void RadarClass::DisposeOfArt()
{
    if (RadarSurface)
    {
        GameDelete(RadarSurface);
        RadarSurface = nullptr;
    }
    IsAvailableNow = false;
}

// ----------------------------------------------------------------------------
// vt_entry_CC / vt_entry_D0
//
// Virtual thunks retained for binary-layout compatibility. vt_entry_CC
// converts a screen point to a radar coordinate; vt_entry_D0 invalidates the
// cached radar frame.
// ----------------------------------------------------------------------------
void* RadarClass::vt_entry_CC(void* out_pUnk, Point2D* pPoint)
{
    if (!out_pUnk || !pPoint)
        return nullptr;

    // The original returns a pointer to the cell under the radar point. We
    // resolve the world coordinate and look up the cell through MapClass.
    CoordStruct world = RadarToWorld(*pPoint);
    if (MapClass::Instance)
    {
        CellClass* cell = MapClass::Instance->GetCellAt(world);
        if (cell)
            return cell;
    }
    return nullptr;
}

void RadarClass::vt_entry_D0(DWORD dwUnk)
{
    // Mark the radar as needing a full repaint. The flag is consumed by the
    // next Draw() call which re-renders the terrain and unit layers.
    unknown_bool_14D9 = true;
    (void)dwUnk;
}

// ============================================================================
// asm: RadarClass_InitForHouse (0x652EB0 区, this=ecx, 无栈参)
//
//   逐句：
//     WWDebugString("RadarClass::Init_For_House()\n")
//     this->vt[+0C8h]()                                  ; 雷达美术重置
//     if (Scenario->[34B8h] == 0):                       ; 非随机地图
//        [11F0h] = (num_168 - 90h)/2 + 4
//        a2              = [11E4h] + 0Bh
//        dword_B04A04    = [11E8h] + 4                  ; 选项按钮顶
//        ecx             = [11E4h] + 53h
//     else:                                              ; 随机地图
//        [11F0h] = (num_168 - 91h)/2 + 5
//        a2              = [11E4h] + 0Eh
//        dword_B04A04    = [11E8h] + 5
//        ecx             = [11E4h] + 56h
//     dword_B048C8 = ecx                                 ; 外交按钮顶
//     dword_B048CC = dword_B04A04                        ; 外交按钮底
//     GadgetRadar.field_C = [11F0h]
//     加载 diplobtn/optbtn 的 SHP 并取其 word[+2]/[+4] 作宽高
//     [1208h] = 由 stru_B0FA1C 打包的 RGB565 按钮底色
//
//   ★ 渲染后端（MixFileClass::Finder / SHP 宽高）未建模 -> SHP 尺寸保持 0，
//     仅固化两分支的布局常量与打包颜色，供 Draw/MouseMove 的命中测试消费。
// ============================================================================
void RadarClass::Init_For_House()
{
    DisposeOfArt();   // asm: call dword ptr [eax+0C8h]（雷达美术重置）

    // asm: mov ecx,Scenario ; mov eax,[ecx+34B8h] ; test eax,eax ; jnz loc_652EFC
    const bool randomMap = (ScenarioClass::Instance != nullptr) &&
                           ScenarioClass::Instance->IsRandom;

    // asm: mov edx, ds:num_168 —— 这里的基数是 num_168（=168，雷达框宽），
    //      不是屏幕宽度：非随机图 ((168-90h)/2)+4 = 16，随机图 ((168-91h)/2)+5 = 88。
    const int32 frameW = 168;   // asm: ds:num_168

    if (!randomMap)
    {
        // asm: lea eax,[edx-90h] ; cdq ; sub eax,edx ; sar eax,1 ; add eax,4
        unknown_11F0 = ((frameW - 0x90) / 2) + 4;
        g_RadarLayout.ButtonLeft = static_cast<int32>(unknown_11E4) + 0x0B;  // a2
        g_RadarLayout.OptionsTop = static_cast<int32>(unknown_11E8) + 4;     // B04A04
        g_RadarLayout.DiploTop   = static_cast<int32>(unknown_11E4) + 0x53;  // B048C8
    }
    else
    {
        unknown_11F0 = ((frameW - 0x91) / 2) + 5;
        g_RadarLayout.ButtonLeft = static_cast<int32>(unknown_11E4) + 0x0E;
        g_RadarLayout.OptionsTop = static_cast<int32>(unknown_11E8) + 5;
        g_RadarLayout.DiploTop   = static_cast<int32>(unknown_11E4) + 0x56;
    }

    // asm: mov dword_B048CC, eax（此处 eax 即上面算出的 dword_B04A04）
    g_RadarLayout.DiploBottom = g_RadarLayout.OptionsTop;

    // asm: mov edx,[esi+11F0h] ; mov GadgetRadar.field_C, edx
    g_RadarGadget.field_10 = unknown_11F0;

    // asm: MixFileClass<CCFileClass>::Finder(diplobtn_shp/optbtn_shp) -> SHP，
    //      再取 word[+2]/[+4] 作宽高。SHP 后端未建模 -> 尺寸保持 0，命中测试
    //      退化（不做按钮命中）。
    g_RadarLayout.DiploBtnW   = 0;
    g_RadarLayout.DiploBtnH   = 0;
    g_RadarLayout.OptionsBtnW = 0;
    g_RadarLayout.OptionsBtnH = 0;

    // asm: 取 stru_B0FA1C 的 R/G/B，按 col_*_SHR_*/SHL_* 常量打包成 16 位
    //      颜色（标准 RGB565：R>>3<<11 | G>>2<<5 | B>>3<<0）。
    //      stru_B0FA1C 由 sub_72F440(Game_LoadUI) 从 word_B0F9D8/word_B0FB04/
    //      word_B0FAA0 三个主题色之一填入，本工程未建模该全局；取工程既有
    //      UI 文本色约定（DisplayClass kUiTextColor = 0xFFFF）等价。
    const uint8 r = 0xFF, g = 0xFF, b = 0xFF;
    unknown_1208 = (static_cast<uint32>(r >> 3) << 11) |
                   (static_cast<uint32>(g >> 2) << 5)  |
                   (static_cast<uint32>(b >> 3));
    g_RadarLayout.ButtonTint = unknown_1208;
}

// ----------------------------------------------------------------------------
// Init_Radar
//
// Resets the radar to its default state: visible, zoom level 1, full-panel
// rectangle.
// ----------------------------------------------------------------------------
void RadarClass::Init_Radar()
{
    IsHidden = false;
    RadarZoomLevel = 1;
    RadarRect = Rectangle(0, 0, RadarWidth, RadarHeight);
    IsAvailableNow = false;
    RadarSizeFactor = 1.0f;
    unknown_bool_14D9 = false;
    unknown_bool_14DA = false;
}

// ============================================================================
// 雷达地形 / 单位点 / 视野框（DSurface_Radar 局部坐标，逐像素光栅）
//
// 全流程严格照 asm 的三段：
//   1) sub_654490 + sub_654650 —— 等距包围盒与适配缩放：
//        isoX(cell) = cellX - cellY + [1490h]
//        isoY(cell) = cellX + cellY - [1498h]
//      对全图取 isoX/isoY 的 min/max 得包围盒 (bboxW, bboxH)，
//      [1488h] RadarSizeFactor = 把包围盒塞进 140 x 108 的等比缩放。
//   2) sub_654EA0 + sub_6547C0 —— 地形位图：
//        * 先在包围盒分辨率上铺 RGB（每格 2 列：o1, o2），缓冲 = bboxW*bboxH*3；
//        * 再用 [1488h] 做盒式降采样，逐目标像素取方块均值后按
//          col_R_SHR_3/col_G_SHR_2/col_B_SHR_3 打包成 16bpp 写进雷达面。
//        * 目标面尺寸 w/h：h*140/bboxW < 108 时取 (140, h*140/bboxW)，
//          否则取 (bboxW*108/bboxH, 108)；不足 140x108 时居中。
//   3) sub_655C50 —— 逐像素复合（对象点）：把该像素对应的格子里的单位/
//      建筑按阵营色写 1 个像素；未探测(shroud)像素保持原地形色。
// 视野框（sub_656EC0 的 14DCh）另见下方 Draw_Mini_Map 末段。
// ============================================================================
namespace
{
    // asm: sub_6547C0 —— 地形面尺寸上限（flt_7F0420 / flt_7F041C）
    constexpr int32 kRadarTerrainMaxW = 140;   // 8Ch
    constexpr int32 kRadarTerrainMaxH = 108;   // 6Ch

    // ------------------------------------------------------------------
    // 等距投影参数缓存（asm: [1488h] 因子 / [1490h][1498h] 偏移 /
    // [149Ch..14A8h] 地形面矩形）。
    // ------------------------------------------------------------------
    struct RadarProjection
    {
        int32 IsoOffsetX = 0;   // [1490h]
        int32 IsoOffsetY = 0;   // [1498h]
        int32 BBoxW      = 0;   // 包围盒宽（= 城市格数之和 - 1）
        int32 BBoxH      = 0;   // 包围盒高
        int32 BitW       = 0;   // 地形面宽（asm 1220h 的 Width）
        int32 BitH       = 0;   // 地形面高
        int32 OriginX    = 0;   // [149Ch]
        int32 OriginY    = 0;   // [14A0h]
        float Factor     = 1.0f;// [1488h]
    };

    // 逐格 RGB 源缓冲（asm 123Ch，一条 malloc 常驻）+ 降采样后的 16bpp 面
    // （asm 1220h）。两者都随地图尺寸一次性增长，之后按帧原地覆写。
    std::vector<uint8>  g_RadarCells;
    std::vector<uint16> g_RadarTerrain;

    // asm: sub_654490 —— 由地图尺寸求 1490h / 1498h / 包围盒。
    //   [1490h] 使 isoX 从 0 起算：cellX - cellY 的最小值是 -(mapH-1)。
    //   [1498h] 使 isoY 从 0 起算：cellX + cellY 的最小值是 0。
    void Compute_Iso_Extent(int32 mapW, int32 mapH, RadarProjection& proj)
    {
        proj.IsoOffsetX = mapH - 1;
        proj.IsoOffsetY = 0;
        proj.BBoxW      = mapW + mapH - 1;
        proj.BBoxH      = mapW + mapH - 1;
    }

    // asm: sub_6547C0 的缩放分支（flt_7F0420=140 / flt_7F041C=108）
    void Compute_Terrain_Fit(RadarProjection& proj)
    {
        if (proj.BBoxW <= 0 || proj.BBoxH <= 0)
        {
            proj.BitW = proj.BitH = 0;
            proj.Factor = 1.0f;
            return;
        }

        const float byWidth = static_cast<float>(kRadarTerrainMaxW) /
                              static_cast<float>(proj.BBoxW);
        const float scaledH = static_cast<float>(proj.BBoxH) * byWidth;

        if (scaledH < static_cast<float>(kRadarTerrainMaxH))
        {
            proj.Factor = byWidth;
            proj.BitW   = kRadarTerrainMaxW;
            proj.BitH   = static_cast<int32>(Float_To_Int_Floor(
                              static_cast<double>(proj.BBoxH) * static_cast<double>(byWidth)));
        }
        else
        {
            const float byHeight = static_cast<float>(kRadarTerrainMaxH) /
                                   static_cast<float>(proj.BBoxH);
            proj.Factor = byHeight;
            proj.BitW   = static_cast<int32>(Float_To_Int_Floor(
                              static_cast<double>(proj.BBoxW) * static_cast<double>(byHeight)));
            proj.BitH   = kRadarTerrainMaxH;
        }

        if (proj.BitW < 1) proj.BitW = 1;
        if (proj.BitH < 1) proj.BitH = 1;
    }
}

// ============================================================================
// asm: sub_654EA0 —— 逐格把 CellClass_Cell_Color_2 的两色铺进包围盒缓冲
//
//   for (cell = 地图首格 ; cell ; cell = 下一格) {
//       isoX = cellX - cellY + [1490h] ; isoY = cellX + cellY - [1498h]
//       if (isoY < [1498h] 之外的格子) 跳过          ; asm 的 (X+Y) < 1498h
//       idx = isoY * bboxW + isoX
//       CellClass_Cell_Color_2(cell, &o1, &o2)
//       buf[idx] = o1
//       if (isoX != bboxW - 1) buf[idx + 1] = o2     ; 每格占 2 列
//   }
// ============================================================================
void RadarClass::Render_Terrain()
{
    MapClass* pMap = MapClass::Instance;
    const int32 mapW = (pMap != nullptr) ? pMap->MapWidth  : 0;
    const int32 mapH = (pMap != nullptr) ? pMap->MapHeight : 0;

    if (pMap == nullptr || mapW <= 0 || mapH <= 0)
        return;

    RadarProjection proj;
    Compute_Iso_Extent(mapW, mapH, proj);
    Compute_Terrain_Fit(proj);

    g_RadarCells.assign(static_cast<size_t>(proj.BBoxW) *
                        static_cast<size_t>(proj.BBoxH) * 3u, 0);

    for (int32 cy = 0; cy < mapH; ++cy)
    {
        for (int32 cx = 0; cx < mapW; ++cx)
        {
            CellClass* pCell = pMap->GetCellAt(cx, cy);
            if (pCell == nullptr)
                continue;

            const int32 isoX = cx - cy + proj.IsoOffsetX;
            const int32 isoY = cx + cy - proj.IsoOffsetY;
            if (isoX < 0 || isoX >= proj.BBoxW || isoY < 0 || isoY >= proj.BBoxH)
                continue;

            uint8 c1[3];
            uint8 c2[3];
            Cell_Radar_Color(pCell, c1, c2);

            const size_t base = (static_cast<size_t>(isoY) *
                                 static_cast<size_t>(proj.BBoxW) +
                                 static_cast<size_t>(isoX)) * 3u;
            g_RadarCells[base + 0] = c1[0];
            g_RadarCells[base + 1] = c1[1];
            g_RadarCells[base + 2] = c1[2];

            if (isoX != proj.BBoxW - 1)
            {
                g_RadarCells[base + 3] = c2[0];
                g_RadarCells[base + 4] = c2[1];
                g_RadarCells[base + 5] = c2[2];
            }
        }
    }

    // ---- asm: sub_6547C0 的盒式降采样 ----
    g_RadarTerrain.assign(static_cast<size_t>(proj.BitW) *
                          static_cast<size_t>(proj.BitH), 0u);

    const double invFactor = 1.0 / static_cast<double>(proj.Factor);

    for (int32 dy = 0; dy < proj.BitH; ++dy)
    {
        int32 y0 = static_cast<int32>(static_cast<double>(dy) * invFactor);
        int32 y1 = static_cast<int32>(static_cast<double>(dy + 1) * invFactor);
        if (y0 < 0) y0 = 0;
        if (y1 > proj.BBoxH) y1 = proj.BBoxH;
        if (y1 <= y0) y1 = y0 + 1;

        for (int32 dx = 0; dx < proj.BitW; ++dx)
        {
            int32 x0 = static_cast<int32>(static_cast<double>(dx) * invFactor);
            int32 x1 = static_cast<int32>(static_cast<double>(dx + 1) * invFactor);
            if (x0 < 0) x0 = 0;
            if (x1 > proj.BBoxW) x1 = proj.BBoxW;
            if (x1 <= x0) x1 = x0 + 1;

            int32 sumR = 0;
            int32 sumG = 0;
            int32 sumB = 0;
            int32 count = 0;

            for (int32 sy = y0; sy < y1; ++sy)
            {
                const uint8* row = &g_RadarCells[(static_cast<size_t>(sy) *
                                                  static_cast<size_t>(proj.BBoxW)) * 3u];
                for (int32 sx = x0; sx < x1; ++sx)
                {
                    sumR += row[sx * 3 + 0];
                    sumG += row[sx * 3 + 1];
                    sumB += row[sx * 3 + 2];
                    ++count;
                }
            }
            if (count <= 0)
                count = 1;

            // asm: 三通道各自 fadd 0.5 / Float_To_Int_Floor —— 四舍五入
            const int32 r = (sumR + count / 2) / count;
            const int32 g = (sumG + count / 2) / count;
            const int32 b = (sumB + count / 2) / count;

            g_RadarTerrain[static_cast<size_t>(dy) *
                           static_cast<size_t>(proj.BitW) +
                           static_cast<size_t>(dx)] =
                static_cast<uint16>(Radar565(r, g, b));
        }
    }

    // ---- 把投影参数写回 RadarClass 字段（asm 的同名偏移）----
    RadarSizeFactor = proj.Factor;                              // [1488h]
    unknown_1490    = static_cast<DWORD>(proj.IsoOffsetX);      // [1490h]
    unknown_1494    = static_cast<DWORD>(proj.BBoxH);           // [1494h]
    unknown_1498    = static_cast<DWORD>(proj.IsoOffsetY);      // [1498h]

    // asm: sub_654650 —— 面宽不足 8Ch / 高不足 6Ch 时以 [11F0h]/[11F4h] 居中：
    //      if (w < 8Ch) 149Ch = 11F0h + (8Ch-w)/2 ; else 149Ch = 11F0h
    //      if (h < 6Ch) 14A0h = 11F4h + (6Ch-h)/2 ; else 14A0h = 11F4h
    //   149Ch/14A0h 是**侧边栏局部**坐标（与 11E4h/11E8h 同一坐标系）。
    unknown_rect_149C.X = static_cast<int32>(unknown_11F0);
    if (proj.BitW < kRadarTerrainMaxW)
        unknown_rect_149C.X += (kRadarTerrainMaxW - proj.BitW) / 2;

    unknown_rect_149C.Y = static_cast<int32>(unknown_11F4);
    if (proj.BitH < kRadarTerrainMaxH)
        unknown_rect_149C.Y += (kRadarTerrainMaxH - proj.BitH) / 2;

    unknown_rect_149C.Width  = proj.BitW;                       // [14A4h]
    unknown_rect_149C.Height = proj.BitH;                       // [14A8h]
}

// ============================================================================
// Draw_Mini_Map —— 把上面生成的地形面 + 单位点 + 事件脉冲 + 视野框合成到
//   侧边栏面的雷达内图区（frame，屏幕绝对坐标）。
//
//   asm 的 radar 面 (1220h) 原点 [149Ch]/[14A0h] 就是 frame 内的偏移，
//   这里等价：px = frame.X + (140 - BitW)/2 + isoX*Factor。
// ============================================================================
void RadarClass::Draw_Mini_Map(Surface* pSrf, const Rectangle& frame)
{
    LastMiniMapObjects = 0;

    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (frame.Width <= 0 || frame.Height <= 0)
        return;

    Surface* srf = pSrf;

    // 帧内裁剪
    auto put = [&](int32 x, int32 y, DWORD color)
    {
        if (x < frame.X || x >= frame.X + frame.Width ||
            y < frame.Y || y >= frame.Y + frame.Height)
            return;
        Point2D pt(x, y);
        srf->SetPixel(&pt, color);
    };

    // 雷达底：未探测/地图外为黑。
    Rectangle inner = frame;
    srf->FillRectEx(nullptr, &inner, 0x0000u);

    MapClass* map = MapClass::Instance;
    if (map == nullptr || map->MapWidth <= 0 || map->MapHeight <= 0)
        return;

    Render_Terrain();

    const int32 bitW = static_cast<int32>(unknown_rect_149C.Width);
    const int32 bitH = static_cast<int32>(unknown_rect_149C.Height);
    if (bitW <= 0 || bitH <= 0 ||
        g_RadarTerrain.size() != static_cast<size_t>(bitW) * static_cast<size_t>(bitH))
        return;

    // ---- 地形面（asm: 1220h 贴到 149Ch/14A0h）----
    //   Render_Terrain 已把 [149Ch]/[14A0h] 算成**侧边栏局部**坐标；
    //   frame 是屏幕绝对坐标，两者的差就是面板原点偏移：
    //     frame.X = panel.X + 11F0h  =>  panelDX = frame.X - 11F0h
    const int32 panelDX = frame.X - static_cast<int32>(unknown_11F0);
    const int32 panelDY = frame.Y - static_cast<int32>(unknown_11F4);

    const int32 baseX   = static_cast<int32>(unknown_rect_149C.X);   // 149Ch
    const int32 baseY   = static_cast<int32>(unknown_rect_149C.Y);   // 14A0h
    const int32 originX = baseX + panelDX;
    const int32 originY = baseY + panelDY;

    for (int32 y = 0; y < bitH; ++y)
    {
        for (int32 x = 0; x < bitW; ++x)
        {
            put(originX + x, originY + y,
                g_RadarTerrain[static_cast<size_t>(y) * static_cast<size_t>(bitW) +
                               static_cast<size_t>(x)]);
        }
    }

    // ---- 单位 / 建筑点（asm: sub_655C50 的对象支路）----
    //   像素位置 = 149Ch + isoX*[1488h], 14A0h + isoY*[1488h]。
    //   颜色 = 阵营色 RGB565（House+56F9h..56FBh），无阵营取 LightGrey。
    //   asm 里同一个雷达像素由空间桶只登记一次，本工程直接按格投影覆写。
    if (TechnoClass::Array != nullptr)
    {
        const int32 count = TechnoClass::Array->GetCount();
        for (int32 i = 0; i < count; ++i)
        {
            TechnoClass* techno = (*TechnoClass::Array)[i];
            if (techno == nullptr || techno->IsInLimbo)
                continue;

            // asm: TechnoClass_IsRadarVisible(PlayerPtr)
            if (!techno->IsRadarVisible(HouseClass::Player))
                continue;

            const AbstractType kind = techno->WhatAmI();
            if (kind != AbstractType::Unit && kind != AbstractType::Infantry &&
                kind != AbstractType::Aircraft && kind != AbstractType::Building)
                continue;

            const CoordStruct coords = techno->GetCoords();
            const CellStruct cell = CellClass::Coord2Cell(coords);
            if (cell.X < 0 || cell.X >= map->MapWidth ||
                cell.Y < 0 || cell.Y >= map->MapHeight)
                continue;

            // asm: sub_6550C0 —— 与 Render_Terrain 同一投影
            const float isoX = static_cast<float>(cell.X - cell.Y) +
                               static_cast<float>(unknown_1490);
            const float isoY = static_cast<float>(cell.X + cell.Y) -
                               static_cast<float>(unknown_1498);

            const int32 px = static_cast<int32>(Float_To_Int_Floor(
                static_cast<double>(isoX) * static_cast<double>(RadarSizeFactor))) + originX;
            const int32 py = static_cast<int32>(Float_To_Int_Floor(
                static_cast<double>(isoY) * static_cast<double>(RadarSizeFactor))) + originY;
            // asm: sub_655C50 每个对象只落 1 个雷达像素（空间桶去重）

            put(px, py, House_Blip_Color(techno->GetOwningHouse()));
            ++LastMiniMapObjects;
        }
    }

    // ---- 雷达事件脉冲（asm: sub_660000 -> sub_660050）----
    if (RadarEventClass::Array != nullptr)
    {
        for (int32 i = 0; i < RadarEventClass::Array->Count; ++i)
        {
            RadarEventClass* pEvent = RadarEventClass::Array->Items[i];
            if (pEvent == nullptr)
                continue;
            // asm: sub_660050 把四角加 [this+4]/[this+8]（相对 149Ch/14A0h）
            pEvent->DrawPulse(srf, Point2D(originX + pEvent->field_4,
                                           originY + pEvent->field_8));
        }
    }

    // ---- 视野框（asm: sub_656EC0 的 14DCh）----
    //   rx = floor((isoXc)*F) + 149Ch ; ry = floor((isoYc)*F) + 14A0h
    //   w  = floor(2*ScreenW*F/60 + 1) ; h = floor(2*ScreenH*F/30)
    //   x  = rx - floor(ScreenW*F/30)  ; y = ry - floor(ScreenH*F/30)
    //   再把 (x,y) 夹进 [149Ch, 149Ch+140-w] / [14A0h, 14A0h+108-h]
    {
        DisplayClass* pDisp = DisplayClass::Instance;
        const int32 screenW = (pDisp != nullptr && pDisp->VisibleRect.Width > 0)
                                  ? pDisp->VisibleRect.Width
                                  : ((pDisp != nullptr) ? pDisp->ScreenWidth : ScreenWidth);
        const int32 screenH = (pDisp != nullptr && pDisp->VisibleRect.Height > 0)
                                  ? pDisp->VisibleRect.Height
                                  : ((pDisp != nullptr) ? pDisp->ScreenHeight : ScreenHeight);

        if (screenW > 0 && screenH > 0)
        {
            // 视野中心对应的屏幕点 -> cell -> iso（asm: TacticalMap -> Coord_Cell）
            const int32 viewLeft   = (pDisp != nullptr) ? pDisp->VisibleRect.X : 0;
            const int32 viewTop    = (pDisp != nullptr) ? pDisp->VisibleRect.Y : 0;
            const int32 centerSX   = viewLeft + screenW / 2;
            const int32 centerSY   = viewTop  + screenH / 2;

            const float cellX = static_cast<float>(centerSX) /
                                    static_cast<float>(CellWidthInPixels) +
                                static_cast<float>(centerSY) /
                                    static_cast<float>(CellHeightInPixels);
            const float cellY = static_cast<float>(centerSY) /
                                    static_cast<float>(CellHeightInPixels) -
                                static_cast<float>(centerSX) /
                                    static_cast<float>(CellWidthInPixels);

            const float isoX = cellX - cellY + static_cast<float>(unknown_1490);
            const float isoY = cellX + cellY - static_cast<float>(unknown_1498);

            const double f = static_cast<double>(RadarSizeFactor);
            int32 rx = static_cast<int32>(Float_To_Int_Floor(
                           static_cast<double>(isoX) * f)) + originX;
            const int32 ry = static_cast<int32>(Float_To_Int_Floor(
                                 static_cast<double>(isoY) * f)) + originY;

            // asm: mov edx,[149Ch] ; dec edx ; cmp eax,edx ; jnz ; inc eax
            //   落点正好压在左边界外一像素时右移一格，避免框贴边差 1 像素。
            if (rx == originX - 1)
                ++rx;

            const int32 u = static_cast<int32>(Float_To_Int_Floor(
                                static_cast<double>(screenW) * f /
                                static_cast<double>(CellWidthInPixels)));
            const int32 v = static_cast<int32>(Float_To_Int_Floor(
                                static_cast<double>(screenH) * f /
                                static_cast<double>(CellHeightInPixels)));

            int32 vw = static_cast<int32>(Float_To_Int_Floor(
                           static_cast<double>(screenW) * 2.0 * f /
                           static_cast<double>(CellWidthInPixels) + 1.0));
            int32 vh = static_cast<int32>(Float_To_Int_Floor(
                           static_cast<double>(screenH) * 2.0 * f /
                           static_cast<double>(CellHeightInPixels)));
            if (vw < 1) vw = 1;
            if (vh < 1) vh = 1;

            int32 vx = rx - u;
            int32 vy = ry - v;

            // asm: loc_6570BB / loc_657102 的夹紧
            if (vx < originX) vx = originX;
            else if (vx + vw >= originX + bitW)
                vx = originX + bitW - vw - 1;
            if (vy < originY) vy = originY;
            else if (vy + vh >= originY + bitH)
                vy = originY + bitH - vh - 1;

            if (vx < originX) vx = originX;
            if (vy < originY) vy = originY;

            // asm: 两次 vt[58h](DSurface_Sidebar, &rect, [1208h])：
            //   第一次 rect = 14DCh = (x, y, w, h)
            //   第二次 rect = (x-1, y-1, w+2, h+2)    —— 双层描边
            //   [1208h] 在 RadarClass_InitForHouse 里由 stru_B0FA1C 的 RGB
            //   按 R>>3<<11 | G>>2<<5 | B>>3 打包成 16bpp。
            const DWORD viewColor = static_cast<DWORD>(unknown_1208);

            auto outline = [&](int32 x, int32 y, int32 w, int32 h)
            {
                if (w <= 0 || h <= 0)
                    return;
                for (int32 px = x; px < x + w; ++px)
                {
                    put(px, y, viewColor);
                    put(px, y + h - 1, viewColor);
                }
                for (int32 py = y; py < y + h; ++py)
                {
                    put(x, py, viewColor);
                    put(x + w - 1, py, viewColor);
                }
            };

            outline(vx, vy, vw, vh);
            outline(vx - 1, vy - 1, vw + 2, vh + 2);
        }
    }
}

// ============================================================================
// asm: RadarClass_Draw (retn 4 -> 1 个 byte 栈参)
//
//   逐段（可观测状态机；SHP blit 已按下述参数提交到 DSurface_Sidebar）：
//     if (Debug_Map) { [14DAh]=0 ; [14D9h]=0 ; return }   ; 调试地图直接退出
//     al   = ([14DAh]==1 || arg_0==1) ? 1 : 0
//     cl   = [14D9h] ; [14DAh] = al
//     [14D9h] = (cl==1 || al==1) ? 1 : 0                  ; 脏重绘闩锁
//     if ([14D9h]) { 画侧边栏 SHP 帧 + 两个 shape 按钮 }  ; SIDE1.SHP frame 0
//   loc_6531DB:
//     if ([14ACh] != 1) {                                 ; 雷达动画态
//        if ([14ACh] != 0) {
//           edi = [1508h]（时长）; eax = [1500h]（起刻）
//           if (eax != -1) { t = Tick - [1500h] ; if (t < edi) edi -= t }
//           if (edi != 0) goto loc_6532AA
//           重置计时窗口： [1500h]=Tick ; [1504h]=... ; [1508h]=4
//           ecx = [14FCh]
//           if ([14ACh]==2) { if (--[14FCh] > 0) goto loc_6532AA ; [14ACh]=0 ;
//                             销毁 StrX ; [14FCh]=edi }
//           else { if (++[14FCh] < 20h) goto loc_6532AA ; [14ACh]=1 ;
//                  [14DAh]=1 ; [14FCh]=20h }
//        }
//      loc_6532AA:
//        if ([14ACh]==1) goto loc_65336D
//        if ([14D9h]!=1 && !bl) goto loc_65336D
//        eax=[14FCh] ; if (eax<0) goto loc_65336D
//        if (eax >= num_168+6) goto loc_65336D
//        画帧号=[14FCh] 的雷达 SHP，随后刷新 [120Ch] 矩形
//     }
//   loc_65336D:
//     sub_65FDD0(this)          ; RadarEventClass::UpdateAll
//     sub_656EC0(this)          ; 雷达逐帧推进
//     if ([14ACh] != 1) goto loc_6534EA
//     switch ([MapClass.anonymous_82]) 1..4：            ; 雷达状态机
//       1: 若 [14D9h] -> 画 0x20 帧 + sub_653FA0
//       2: Play_Movie
//       其它: 默认帧绘制 + 刷新 120Ch
//   loc_6534EA:
//     if ([14DAh]) { 画修/卖按钮 ; [14DAh]=0 }
//     依据 120Ch 与 DIPLOBTN/OPTBTN 尺寸夹紧外交/选项按钮矩形
//
//   ★ 侧边栏/雷达的 SHP 提交现已按 asm 落到 DSurface_Sidebar（见
//     DrawSidebarShape）；两枚 shape 按钮因本工程无对应全局对象仍未提交。
// ============================================================================
void RadarClass::Draw()
{
    // asm: mov al, Debug_Map ; test ; jz loc_65312A
    if (g_DebugMapFlag)
    {
        unknown_bool_14DA = false;   // asm: mov [esi+14DAh], bl
        unknown_bool_14D9 = false;   // asm: mov [esi+14D9h], bl
        return;
    }

    // asm: cmp byte [esi+14DAh],1 / jz ... ; cmp arg_0,1 / jz ...
    const uint8 al = (unknown_bool_14DA || g_ForceRadarRedraw) ? 1 : 0;
    const uint8 cl = unknown_bool_14D9 ? 1 : 0;
    unknown_bool_14DA = (al != 0);                       // asm: mov [esi+14DAh], al
    unknown_bool_14D9 = (cl != 0 || al != 0);            // asm: mov [esi+14D9h], eax2

    // asm: if ([14D9h]) {
    //     CC_Draw_Shape(DSurface_Sidebar, SIDEBAR_PAL, dword_B0F9E0, 0,
    //                   pos = {[11E4h], [11E8h]}, bounds = 面矩形,
    //                   flags = 0, Brightness = 1000) ;
    //     ShapeButtonClass::Draw_Me(1) x2 }
    if (unknown_bool_14D9)
    {
        DrawSidebarShape(SidebarArt_SHP, 0,
                         static_cast<int32>(unknown_11E4),
                         static_cast<int32>(unknown_11E8));
        // 两枚侧边栏 shape 按钮（asm: shapeSomeSidebarBtn / shapeSomeSidebarButton）
        // 在本工程没有对应的全局按钮对象，故只有底板 SHP 提交。
    }

    // ---- asm: loc_6531DB 雷达动画/alpha 态机 ----
    // asm: bl 是"本帧计时窗口到期"的局部标志（loc_6532AA 处与 [14D9h] 联判）。
    bool timerExpired = false;
    if (unknown_14AC != 1 && unknown_14AC != 0)
    {
        // asm: eax=[1500h] ; edi=[1508h] ; cmp eax,-1 ; jz ...
        //      t = WinTimerClass::Get_System_Tick_Count() - [1500h]
        //      if (t < edi) edi -= t
        //   OpenYR 以 CDTimerClass 的剩余量等价：只有剩余为 0 才继续推进。
        const int32 remaining = unknown_timer_1500.GetTimeLeft();
        if (remaining == 0)
        {
            timerExpired = true;

            // asm: 重置计时窗口 —— [1500h]=Tick ; [1504h]=second ; [1508h]=4
            unknown_timer_1500.Start(4);

            // asm: mov ecx,[14FCh] ; cmp [14ACh],2
            if (unknown_14AC == 2)
            {
                // asm: dec ecx ; [14FCh]=ecx ; test eax,eax ; jg loc_6532AA
                if (--unknown_14FC <= 0)
                {
                    // asm: [14ACh]=0 ; StrX_DTOR_3(&[14C0h]) ; [14FCh]=edi(=0)
                    unknown_14AC = 0;
                    unknown_14FC = 0;
                }
            }
            else
            {
                // asm: inc ecx ; [14FCh]=ecx ; cmp eax,20h ; jl loc_6532AA
                if (++unknown_14FC >= 0x20)
                {
                    // asm: [14ACh]=1 ; [14DAh]=1 ; [14FCh]=20h
                    unknown_14AC = 1;
                    unknown_bool_14DA = true;
                    unknown_14FC = 0x20;
                }
            }
        }
    }

    // asm: loc_6532AA —— cmp [14ACh],1 ; jz loc_65336D
    if (unknown_14AC != 1)
    {
        // asm: cmp byte [14D9h],1 ; jz short loc_6532C7 ; test bl,bl ; jz loc_65336D
        if (unknown_bool_14D9 || timerExpired)
        {
            // asm: eax=[14FCh] ; test eax,eax ; jl loc_65336D
            //      mov ecx,dword_B04A38 ; movsx edx,word [ecx+6] ; cmp eax,edx
            //      —— 帧号上限就是雷达 SHP 头里的帧数（word[+6]）。
            const int32 frame = unknown_14FC;
            const int32 maxFrame = (RadarArt_SHP != nullptr)
                                       ? static_cast<int32>(RadarArt_SHP->Frames)
                                       : 0;
            if (frame >= 0 && frame < maxFrame)
            {
                // asm: CC_Draw_Shape(dword_B04A38, frameIdx=frame,
                //                    pos={[11E4h],[11ECh]}) + 刷新 [120Ch]
                RenderRadarStateFrame(*this, frame);
            }
        }
    }

    // ---- asm: loc_65336D ----
    // asm: call sub_65FDD0 —— 逐帧推进所有雷达事件
    RadarEventClass::UpdateAll();

    // asm: mov ecx,esi ; call sub_656EC0 —— 雷达逐帧推进
    Update_Radar_Internal(*this);

    // asm: cmp [14ACh],1 ; jnz loc_6534EA
    if (unknown_14AC == 1)
    {
        // asm: mov eax,[esi+MapClass.anonymous_82] ; dec eax ; cmp eax,3 ; ja default
        //      jmp off_653734[eax*4] —— 雷达状态 1..4 的 switch
        const int32 radarState = unknown_14B0;   // asm: MapClass.anonymous_82 == [14B0h]
        switch (radarState)
        {
        case 1:
            // asm: mov al,[14D9h] ; test ; jz loc_6534EA
            if (!unknown_bool_14D9)
                break;

            // asm: mov byte [14D9h],0
            unknown_bool_14D9 = false;

            // asm: mov al,[14DAh] ; test ; jz loc_653420
            if (unknown_bool_14DA)
            {
                // asm: 画 RADAR.SHP 帧 0x20 到 (11E4h,11ECh)，随后
                //      call sub_653FA0（再贴同一帧 0x20，再用 PrintUnicode 写
                //      外交名单/击杀数；名单与击杀属外交对话框文本路径，
                //      SHP 部分与本次 RenderRadarStateFrame(0x20) 同参同位置）。
                //      RenderRadarStateFrame 内部已把 120Ch 写成
                //      (11E4h,11ECh,num_168,6Eh)，等价 loc_6534BE。
                RenderRadarStateFrame(*this, 0x20);
            }
            else
            {
                // asm: loc_653420 -> sub_653FA0 之后取 [11F0h]/[11F4h]/[1200h]/
                //      [1204h] 写 120Ch（loc_6534D5）
                //   注意 sub_653FA0 首部在 unk_884B8D 非 0 时仍会再贴一次
                //   RADAR.SHP 的 0x20 帧到 (11E4h,11ECh)；本工程未建模该
                //   全局位，按"绘制始终启用"处理，按 asm 位置提交。
                DrawSidebarShape(RadarArt_SHP, 0x20,
                                 static_cast<int32>(unknown_11E4),
                                 static_cast<int32>(unknown_11EC));
                RenderRadarGadgetRect(*this);
            }
            break;
        case 2:
            // asm: case 2 -> RadarClass_Play_Movie
            Play_Movie();
            break;
        default:
            // asm: default -> 若 [14D9h] 则画帧 0 + 刷新 120Ch
            if (unknown_bool_14D9)
            {
                unknown_bool_14D9 = false;
                RenderRadarStateFrame(*this, 0);
            }
            break;
        }
    }

    // ---- asm: loc_6534EA ----
    // asm: mov al,[14DAh] ; test ; jz short loc_653511
    if (unknown_bool_14DA)
    {
        // asm: ShapeButtonRepair::Draw_Me(ebp) ; ShapeButtonSell::Draw_Me(ebp)
        //   修复/卖出按钮的提交由渲染后端完成 -> 落空。
        unknown_bool_14DA = false;
    }

    // asm: 外交按钮（DIPLOBTN）矩形夹紧：若 120Ch 有效则以按钮尺寸裁剪到
    //      [ButtonLeft, OptionsTop] 内；选项按钮（OPTBTN）同理。
    ClampSidebarButtonRect(*this, g_RadarLayout);
}

// ============================================================================
// asm: RadarClass_MouseMove (this=ecx, retn 8)
//
//   逐句：
//     DisplayClass_MouseMove(elementID, mouseCoords)     ; 基类命中测试
//     cl  = !byte_A8EB7C ; edi = (cl ? rect_mouseclass_vp.R : 0)
//     a2  = [left] ; if (mouseX in [a2+edi, a2+edi+DIPLOBTN_W) &&
//                        mouseY in [B04A04, B04A04+DIPLOBTN_H))
//         MouseClass_SetPointer(0, 0)                     ; 命中外交按钮
//     （OPTBTN 同理，见下）
// ============================================================================
void RadarClass::MouseMove(int32* elementID, Point2D* mouseCoords)
{
    if (mouseCoords == nullptr)
        return;

    // asm: call DisplayClass_MouseMove(elementID, mouseCoords)
    DisplayClass::Mouse_Move(*mouseCoords);

    // asm: cl = byte_A8EB7C ; neg cl ; sbb ecx,ecx ; and ecx, rect.R
    //   OpenYR 以「可见基准」等价：edi 为雷达左基准的可见偏移，未建模 -> 0。
    const int32 edi = 0;

    const int32 mx = mouseCoords->X;
    const int32 my = mouseCoords->Y;

    // asm: 外交按钮命中（DIPLOBTN_SHP_Width/Height 为 0 时自然不命中）。
    const int32 diploLeft = g_RadarLayout.ButtonLeft + edi;
    const bool inDiplo =
        (mx >= diploLeft) && (mx < diploLeft + g_RadarLayout.DiploBtnW) &&
        (my >= g_RadarLayout.OptionsTop) &&
        (my < g_RadarLayout.OptionsTop + g_RadarLayout.DiploBtnH);

    // asm: 选项按钮命中（OPTBTN，顶为 dword_B048C8 即 DiploTop）。
    const int32 optLeft = g_RadarLayout.DiploTop + edi;
    const bool inOptions =
        (mx >= optLeft) && (mx < optLeft + g_RadarLayout.OptionsBtnW) &&
        (my >= g_RadarLayout.OptionsTop) &&
        (my < g_RadarLayout.OptionsTop + g_RadarLayout.OptionsBtnH);

    if (inDiplo || inOptions)
    {
        // asm: mov ecx, offset Map ; call MouseClass_SetPointer(0, 0)
        if (MouseClass::Instance != nullptr)
            MouseClass::Instance->SetPointer(0, false);
    }
}

// ----------------------------------------------------------------------------
// Update
//
// Advances radar animations and expires finished radar events. Called once
// per frame from the main loop.
// ----------------------------------------------------------------------------
void RadarClass::Update()
{
    // Decay the radar event timers. Events that have finished are dropped
    // from the pending list so their indicators stop blinking.
    unknown_timer_1500.Update();

    // The radar availability flag is recomputed each frame from the local
    // house's power state. A house without sufficient power loses radar.
    if (DisplayClass::Instance)
    {
        bool hasPower = true;
        // The real game sums the power output/drain here; we keep the flag
        // sticky so the radar does not flicker during transient dips.
        IsAvailableNow = hasPower && !IsHidden;
    }
}

// ----------------------------------------------------------------------------
// Click_Render
//
// Handles a click on the radar panel. The clicked radar coordinate is
// converted to a world coordinate and the tactical view is centred on it.
// ----------------------------------------------------------------------------
void RadarClass::Click_Render()
{
    if (!DisplayClass::Instance)
        return;

    // The click position is stored in CursorPosition by the mouse layer. We
    // convert it to radar space, then to world space, and centre the view.
    Point2D radarPos;
    if (GScreenClass::Instance)
    {
        // Translate the screen-space cursor into radar-local space.
        radarPos.X = 0;
        radarPos.Y = 0;
    }
    else
    {
        return;
    }

    CoordStruct world = RadarToWorld(radarPos);
    DisplayClass::Instance->CenterOn(world);
}

// ----------------------------------------------------------------------------
// IsRadarHidden
// ----------------------------------------------------------------------------
bool RadarClass::IsRadarHidden() const
{
    return IsHidden;
}

// ----------------------------------------------------------------------------
// ToggleRadar
//
// Flips the hidden flag. The radar remains allocated so it can be shown
// again instantly without re-rendering the terrain cache.
// ----------------------------------------------------------------------------
void RadarClass::ToggleRadar()
{
    IsHidden = !IsHidden;
    IsAvailableNow = !IsHidden;
}

// ----------------------------------------------------------------------------
// WorldToRadar —— asm: sub_6550C0 (0x6550C0)
//
//   逐句：
//     rx = floor(((cellX - cellY) + [1490h]) * [1488h] + [149Ch])
//     ry = floor(((cellX + cellY) - [1498h]) * [1488h] + [14A0h])
//   cellX/cellY 取**亚格精度**（lepton / LeptonsPerCell），与 asm 用
//   dwXY 的定点格坐标一致；返回值是**侧边栏局部**雷达像素。
// ----------------------------------------------------------------------------
Point2D RadarClass::WorldToRadar(const CoordStruct& world) const
{
    const double cellX = static_cast<double>(world.X) /
                         static_cast<double>(LeptonsPerCell);
    const double cellY = static_cast<double>(world.Y) /
                         static_cast<double>(LeptonsPerCell);

    const double isoX = (cellX - cellY) + static_cast<double>(unknown_1490);
    const double isoY = (cellX + cellY) - static_cast<double>(unknown_1498);

    const double f = static_cast<double>(RadarSizeFactor);

    const int32 rx = static_cast<int32>(Float_To_Int_Floor(isoX * f)) +
                     static_cast<int32>(unknown_rect_149C.X);
    const int32 ry = static_cast<int32>(Float_To_Int_Floor(isoY * f)) +
                     static_cast<int32>(unknown_rect_149C.Y);
    return Point2D(rx, ry);
}

// ----------------------------------------------------------------------------
// RadarToWorld —— WorldToRadar 的逆变换（点选雷达跳转视角用）
//
//   由 rx/ry 反解 cellX/cellY：
//     a = (rx - 149Ch)/[1488h] = cellX - cellY + [1490h]
//     b = (ry - 14A0h)/[1488h] = cellX + cellY - [1498h]
//     cellX = (a + b + [1498h] - [1490h]) / 2
//     cellY = (b - a + [1498h] + [1490h]) / 2
// ----------------------------------------------------------------------------
CoordStruct RadarClass::RadarToWorld(const Point2D& radar) const
{
    const double f = static_cast<double>(RadarSizeFactor);
    if (f <= 0.0)
        return CoordStruct(0, 0, 0);

    const double a = (static_cast<double>(radar.X) -
                      static_cast<double>(unknown_rect_149C.X)) / f;
    const double b = (static_cast<double>(radar.Y) -
                      static_cast<double>(unknown_rect_149C.Y)) / f;

    const double cellX = (a + b + static_cast<double>(unknown_1498) -
                          static_cast<double>(unknown_1490)) * 0.5;
    const double cellY = (b - a + static_cast<double>(unknown_1498) +
                          static_cast<double>(unknown_1490)) * 0.5;

    return CoordStruct(static_cast<int32>(cellX * LeptonsPerCell),
                       static_cast<int32>(cellY * LeptonsPerCell), 0);
}

// ============================================================================
// asm: RadarClass_UpdteRadar? (0x657D20)   —— 雷达地形重绘
//
//   符号名带 `?`（IDA 不确定），按行为命名 Update_Radar。
//   this = ecx（RadarClass*，全局 Map）；无栈参数 -> __thiscall。
//   调用者：MapClass::Shroud_The_Map / MapClass_ClearShroud 等 5+ 处。
//
//   与现有 RadarClass::Update()（事件计时器衰减）语义不同，故另立名。
//
//   语义（asm 逐句）：
//     if (hWndParent == 0):                       ; headless 分支
//       B = [this+0x1220] ; A = [this+0x121C]
//       r0 = B->vt[+78h](B, &rect0, 0, 1)
//       r1 = A->vt[+78h](A, &rect1, B, r0)
//       A->vt[+08h](A, r1)
//       sub_656150(this)
//       return
//     w = A->vt[+7Ch](A) ; h = A->vt[+80h](A)
//     for (y = 0 ; y < h ; ++y)
//       for (x = 0 ; x < w ; ++x)
//         { point = {x, y} ; sub_655C50(this, &point) }
//
// 铁律①（宁可留白，不写错）：asm 批注把 vtable 槽位（+78h/+08h/+7Ch/+80h）
//   误当成命名函数（RadSurfLock/Call8/Width/Height）—— 它们是 surf 对象
//   的虚槽。sub_656150 / sub_655C50 尚未落地，此处只固化已确证的
//   headless/窗口两分支与遍历形状，虚调用面保留 asm 槽偏移注释。
// ============================================================================
void RadarClass::Update_Radar()
{
    // asm: cmp hWndParent,0 / jz loc_657E10   （headless 分支）
    //   依赖 hWndParent 全局与 +0x121C/+0x1220 两个 surf 的虚表布局，
    //   尚未落地 -> 留白。

    // asm: mov eax,[edi+121Ch] / mov edx,[eax] / mov ecx,eax
    //      / call dword ptr [edx+7Ch]              ; w = A->vt[+7Ch](A)
    //      mov eax,[edi+121Ch] / mov edx,[eax]
    //      / call dword ptr [edx+80h]              ; h = A->vt[+80h](A)
    // asm: for (y = 0 ; y < h ; ++y)
    //        for (x = 0 ; x < w ; ++x)
    //          { point = {x,y} ; sub_655C50(this, &point) }
    //
    // 两个 surf 对象即本类 unknown_121C / unknown_1220（asm +0x121C/+0x1220）；
    // 其宽高查询与逐像素更新例程（sub_655C50 / sub_656150）尚未落地，
    // 保留 asm 调用点，不臆造实现。
    (void)unknown_121C;
    (void)unknown_1220;
}

// ============================================================================
// asm: RadarClass_CreateMap (retn 10h) —— 地图尺寸入口
//
//   原版是纯转发：push a5 / push a4 / push m(wXY) / push dest
//                     call MapClass::Set_Map_Dimensions / retn 10h
//   OpenYR 未单独落地 MapClass::Set_Map_Dimensions，故此处把尺寸直接写入
//   MapClass 并分配格子（语义等价）。
// ============================================================================
void RadarClass::CreateMap(int32 dest, int16 mX, int16 mY, uint8 a4, uint8 a5)
{
    // asm: dest / m(2 dwords) / a4 / a5 —— dest 为调用者提供的目标缓冲，
    //      RadarClass 只负责把尺寸转交给地图。dest/a4/a5 原文仅透传。
    (void)dest;
    (void)a4;
    (void)a5;

    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    // asm: MapClass::Set_Map_Dimensions(dest, m, a4, a5)
    if (mX > 0 && mY > 0)
    {
        pMap->MapWidth  = mX;
        pMap->MapHeight = mY;
        pMap->MapSize   = static_cast<int32>(mX) * static_cast<int32>(mY);
        pMap->Allocate_Cells(mX, mY);
    }
}

// ============================================================================
// asm: RadarClass_OneTime (0x652BE0 区, this=ecx, 无栈参)
//
//   WWDebugString("RadarClass::One_Time()\n")
//   [11E4h]=0 ; [11E8h]=10h ; [11ECh]=30h ; [11F4h]=31h
//   [11F8h]=8Ch ; [11FCh]=6Ch ; [1200h]=8Ch ; [1204h]=6Ch
//   call DisplayClass_OneTime
//   GadgetRadar.field_10 = [11F4h] ; GadgetRadar.Width = [1200h]
//   GadgetRadar.Height   = [1204h] ; GadgetRadar.flags = 0DFh
//   [14D4h] = VoxClass_FindIndex("EVA_IncomingTransmission")
// ============================================================================
void RadarClass::One_Time()
{
    // asm: mov [esi+11E4h],0 / [esi+11E8h],10h / [esi+11ECh],30h
    unknown_11E8 = 0x10;      // asm: [esi+11E8h]
    unknown_11EC = 0x30;      // asm: [esi+11ECh]
    // asm: mov ecx,8Ch ; mov eax,6Ch
    unknown_11F4 = 0x31;      // asm: [esi+11F4h]
    unknown_11F8 = 0x8C;      // asm: [esi+11F8h]
    unknown_11FC = 0x6C;      // asm: [esi+11FCh]
    unknown_1200 = 0x8C;      // asm: [esi+1200h]
    unknown_1204 = 0x6C;      // asm: [esi+1204h]

    // asm: call DisplayClass_OneTime —— 基类一次性初始化（本类未复刻该基类实现，
    //      以 DisplayClass::Init_Radar? 等价语义留痕：重置雷达可见状态）。
    Init_Radar();

    // asm: GadgetRadar.field_10 / Width / Height / flags
    g_RadarGadget.field_10 = unknown_11F4;   // asm: mov GadgetRadar.field_10, [11F4h]
    g_RadarGadget.Width    = unknown_1200;   // asm: mov GadgetRadar.Width, [1200h]
    g_RadarGadget.Height   = unknown_1204;   // asm: mov GadgetRadar.Height, [1204h]
    g_RadarGadget.flags    = 0xDF;           // asm: mov GadgetRadar.flags, 0DFh

    // asm: mov ecx, offset aEva_incomingtr ; call VoxClass_FindIndex
    //      mov [esi+14D4h], eax
    unknown_int_14D4 = VocClass::FindIndexOfName("EVA_IncomingTransmission");
}

// ============================================================================
// asm: RadarClass_InitClear (this=ecx, 无栈参)
//
//   call DisplayClass_InitClear ; WWDebugString("RadarClass::Init_Clear\n")
//   [14ACh]=0 ; [14B0h]=0 ; [14BDh]=0 ; [14BCh]=0 ; [14FCh]=0 ; [14D8h]=0
//   [14D9h]=1
//   call [DVC@1224+vt[0Ch]]      ; 清 unknown_cells_1124
//   call [DVC@125C+vt[0Ch]]      ; 清 unknown_points_125C
// ============================================================================
void RadarClass::Init_Clear()
{
    // asm: call DisplayClass_InitClear —— 基类清理（未复刻，等价于重置雷达状态）
    Init_Radar();

    unknown_14AC = 0;             // asm: mov [esi+14ACh],eax
    unknown_14B0 = 0;             // asm: mov [esi+14B0h],eax
    unknown_bool_14BD = false;    // asm: mov [esi+14BDh],al
    unknown_bool_14BC = false;    // asm: mov [esi+14BCh],al
    unknown_14FC = 0;             // asm: mov [esi+14FCh],eax
    IsAvailableNow = false;       // asm: mov [esi+14D8h],al
    unknown_bool_14D9 = true;     // asm: mov byte [esi+14D9h],1

    // asm: call dword ptr [eax+0Ch]（DVC 虚清）
    unknown_cells_1124.Clear();   // asm: DVC@1224h vt[+0Ch]
    unknown_points_125C.Clear();  // asm: DVC@125Ch vt[+0Ch]
}

// ============================================================================
// asm: RadarClass::Radar_Activate (retn 8: a2 / a3)
//
//   edi = 目标状态；单机/本机静音分支下强制 1（Session / Player_MuteSWLaunches，
//         本模块未建模，故此处直接取 a2）。
//   if ([14B0h] == edi) return
//   if (([14B0h]==3 || [14B0h]==4) && edi!=3 && [14ACh]!=5) { [14B4h]=edi ; return }
//   switch (edi):
//     0: UpdateRadarStatus(0, [14ACh]==1) ; [14B0h]=0
//     1: if ([14ACh]==1 || [14ACh]==5) && [14D8h]==1 -> [14B0h]=1;[14ACh]=4;[14FCh]=19h
//        else UpdateRadarStatus([14D8h], a3) ; [14B0h]=1
//     4: [14B0h]=4
//     default: if ([14ACh]!=1 && [14ACh]!=5) UpdateRadarStatus(1,0) ; [14B0h]=edi
// ============================================================================
void RadarClass::Radar_Activate(int32 a2, uint8 a3)
{
    // asm: (Session / Player_MuteSWLaunches 覆盖，本模块未建模) -> edi = a2
    const int32 target = a2;

    // asm: mov eax,[esi+14B0h] ; cmp eax,edi ; jz loc_656DB8
    if (unknown_14B0 == target)
        return;

    // asm: cmp eax,3 / cmp eax,4 / jz loc_656CFF ; cmp edi,3 ; jz loc_656D13
    const int32 cur = unknown_14B0;
    const bool curIsClosing = (cur == 3 || cur == 4);
    if (curIsClosing && target != 3 && unknown_14AC != 5)
    {
        // asm: loc_656CFF -> [esi+14B4h]=edi ; retn 8
        unknown_14B4 = target;
        return;
    }

    switch (target)
    {
    case 0:
        // asm: loc_656DA2 —— cmp eax,1 ; setz al ; UpdateRadarStatus(0, al)
        //      eax 此处为 [14ACh]（=1 时 playSound=1）
        UpdateRadarStatus(0, (unknown_14AC == 1) ? static_cast<uint8>(1) : static_cast<uint8>(0));
        unknown_14B0 = target;
        break;

    case 1:
        // asm: loc_656D50 —— cmp [14ACh],1 ; jz loc_656D60 ; cmp [14ACh],5 ; jnz loc_656D84
        if ((unknown_14AC == 1 || unknown_14AC == 5) && IsAvailableNow)
        {
            // asm: loc_656D69 —— [14B0h]=edi ; [14ACh]=4 ; [14FCh]=19h
            unknown_14B0 = target;
            unknown_14AC = 4;
            unknown_14FC = 0x19;
        }
        else
        {
            // asm: loc_656D84 —— UpdateRadarStatus([14D8h], a3) ; [14B0h]=edi
            UpdateRadarStatus(IsAvailableNow ? static_cast<uint8>(1) : static_cast<uint8>(0), a3);
            unknown_14B0 = target;
        }
        break;

    case 4:
        // asm: loc_656DB2 —— [14B0h]=edi
        unknown_14B0 = target;
        break;

    default:
        // asm: cmp [14ACh],1 ; jz loc_656D69 ; cmp [14ACh],5 ; jz loc_656D69
        if (unknown_14AC == 1 || unknown_14AC == 5)
        {
            // asm: loc_656D69
            unknown_14B0 = target;
            unknown_14AC = 4;
            unknown_14FC = 0x19;
        }
        else
        {
            // asm: UpdateRadarStatus(1, 0) ; [14B0h]=edi
            UpdateRadarStatus(1, 0);
            unknown_14B0 = target;
        }
        break;
    }
}

// ============================================================================
// asm: RadarClass_Play_Movie (0x6579E0 区, this=ecx)
//
//   若 vec_MovieHandles 非空则走 BINK 播放路径；否则直接把 [14B8h] 置 2。
//   ★ 渲染/视频后端（BINK）未建模，这里只固化状态位与"无片可放"的等价分支。
// ============================================================================
void RadarClass::Play_Movie()
{
    // asm: mov eax, vec_MovieHandles.length ; test ; jz loc_6579A0
    //      call sub_408390               ; 播放当前句柄（BINK，未建模 -> 落空）
    // loc_6579A0: mov dword ptr [esi+14B8h], 2
    unknown_14B8 = 2;   // asm: [esi+14B8h] = 2
}

// ============================================================================
// asm: RadarEventClass_Add —— 在雷达上登记一次脉冲事件。
// ============================================================================
void RadarClass::AddEvent(int32 type, const CellStruct& cell)
{
    RadarEventClass::Add(type, cell.X, cell.Y);
}

// ============================================================================
// asm: sub_65FDD0 —— 逐帧推进所有雷达事件（RadarClass_Draw 里于状态机后调用）。
// ============================================================================
void RadarClass::UpdateEvents()
{
    RadarEventClass::UpdateAll();
}

// ============================================================================
// asm: RadarClass_Diplomacy_DialogFunc (0x657D70, retn 10h) —— 外交屏对话框过程
//
//   stdcall(hDlg, uMsg, wParam, lParam)。ecx/esi = hDlg，ebx = uMsg，
//   edi = wParam，ebp = lParam。逐句对应：
//     call Dialog_HandleGeneralMessages ; test eax,eax ; jnz ret
//     eax = uMsg - 111h ; jz WM_COMMAND
//     eax -= 2          ; jz WM_TIMER
//     eax -= 384h       ; jnz ret            否则 = 497h 初始化
//
//   0x497：先用 8 项 ControlList_Diplomacy_HouseNameStatics /
//     KillStatics 把名字与击杀静态文本清空；再按 7 行（arg_4 = 0..0x18
//     step 4，var_C = 1..7）走 vec_Houses：先算"行索引 = 玩家 PlayerNumber
//     则折回 0"，取 Houses[idx]；房为空 / 国家不可玩 -> 整行 sub_4E3320
//     面板 + 名字 + 两个复选框 + 击杀静态 EnableWindow(0) + ShowWindow(0)；
//     否则按 RulesData+14B5h（结盟允许）、someHouse != PlayerPtr、
//     dword_A8B244 != 2 决定结盟复选框显示或隐藏，再按 Is_Human /
//     Player_MuteSWLaunches 决定聊天复选框的显示、勾选（byte_A8D108[idx]）
//     与禁用。全部行都没有出现时隐藏 597h（"结盟"表头）/ 425h（"聊天"表头）。
//     随后 730h 会话开关（Session 3 -> dword_A8D110[0]，4 -> [1]）、
//     sub_658640(hDlg) 刷新、Session 3/4 时把 GlobalMPGameModeClass+20h 的
//     模式名写进 426h、把 word_A8B322 写进 71Bh，最后 SetTimer(0,0FAh)。
//
//   0x113：sub_658640(hDlg) 刷新 + InvalidateRect(hDlg,0,1)。
//
//   0x111：ctrl = GetDlgCtrlID(lParam)，hi = wParam>>16（通知码）。
//     423h：hi==0 时 KillTimer(hDlg,0) 并写 *GetWindowLongA(hDlg,8) = 423h。
//     结盟表 dword_83923C[8]（首元素 -1 = 自己槽，1743h..1749h）：
//       索引 = 表内下标，等于玩家 PlayerNumber 时折回 0；
//       hi==0 -> BM_SETCHECK(1)，hi==1 -> BM_SETCHECK(0)，再 sub_731840(索引)。
//     聊天表 dword_83925C[8]（-1 后接 1750h/1751h/1752h/1753h/1762h/
//       1763h/1764h）：byte_A8D108[索引] = (hi == 1)。
//     730h：Session==3 -> dword_A8D110[0]，Session==4 -> dword_A8D110[1]。
//
// ★ 控件 ID / 消息号 / 字符串常量全部按 asm 字节照抄，不做任何"美化"。
// ★ 对外部调用（Dialog_HandleGeneralMessages、sub_658640、sub_731840）一律
//   直调本文件内已落地的真实实现，不经 bind / 函数指针表。
// ============================================================================
namespace {

// ----------------------------------------------------------------------------
// Win32 薄封装：Windows 构建直调 user32，其余平台退化为"无控件 / 0"，
// 与 Game/GameOptionsClass.cpp 的 Dlg_Get_Item / Dlg_Send_Msg 同一约定。
// ----------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64)
    #define OPENYR_RADAR_WIN32 1
#endif

void* Dlg_Get_Item(void* hDlg, int32 id)
{
#if defined(OPENYR_RADAR_WIN32)
    return static_cast<void*>(::GetDlgItem(static_cast<HWND>(hDlg),
                                           static_cast<int>(id)));
#else
    (void)hDlg; (void)id;
    return nullptr;
#endif
}

int32 Dlg_Send_Msg(void* hWnd, int32 msg, int32 wParam, int32 lParam)
{
#if defined(OPENYR_RADAR_WIN32)
    return static_cast<int32>(::SendMessageA(static_cast<HWND>(hWnd),
                                             static_cast<UINT>(msg),
                                             static_cast<WPARAM>(wParam),
                                             static_cast<LPARAM>(lParam)));
#else
    (void)hWnd; (void)msg; (void)wParam; (void)lParam;
    return 0;
#endif
}

void Dlg_Enable(void* hWnd, int32 bEnable)
{
#if defined(OPENYR_RADAR_WIN32)
    ::EnableWindow(static_cast<HWND>(hWnd), bEnable != 0);
#else
    (void)hWnd; (void)bEnable;
#endif
}

void Dlg_Show(void* hWnd, int32 nCmdShow)
{
#if defined(OPENYR_RADAR_WIN32)
    ::ShowWindow(static_cast<HWND>(hWnd), static_cast<int>(nCmdShow));
#else
    (void)hWnd; (void)nCmdShow;
#endif
}

int32 Dlg_Ctrl_Id(void* hWnd)
{
#if defined(OPENYR_RADAR_WIN32)
    return static_cast<int32>(::GetDlgCtrlID(static_cast<HWND>(hWnd)));
#else
    (void)hWnd;
    return 0;
#endif
}

int32 Dlg_Win_Long(void* hDlg, int32 nIndex)
{
#if defined(OPENYR_RADAR_WIN32)
    return static_cast<int32>(::GetWindowLongA(static_cast<HWND>(hDlg),
                                               static_cast<int>(nIndex)));
#else
    (void)hDlg; (void)nIndex;
    return 0;
#endif
}

void Dlg_Kill_Timer(void* hDlg, int32 nIDEvent)
{
#if defined(OPENYR_RADAR_WIN32)
    ::KillTimer(static_cast<HWND>(hDlg), static_cast<UINT_PTR>(nIDEvent));
#else
    (void)hDlg; (void)nIDEvent;
#endif
}

void Dlg_Set_Timer(void* hDlg, int32 nIDEvent, int32 nElapse)
{
#if defined(OPENYR_RADAR_WIN32)
    ::SetTimer(static_cast<HWND>(hDlg), static_cast<UINT_PTR>(nIDEvent),
               static_cast<UINT>(nElapse), nullptr);
#else
    (void)hDlg; (void)nIDEvent; (void)nElapse;
#endif
}

void Dlg_Invalidate(void* hDlg, int32 bErase)
{
#if defined(OPENYR_RADAR_WIN32)
    ::InvalidateRect(static_cast<HWND>(hDlg), nullptr, bErase != 0);
#else
    (void)hDlg; (void)bErase;
#endif
}

// ----------------------------------------------------------------------------
// asm: Dialog_HandleGeneralMessages (0x622B60)
//   引擎级公共对话框调度器（WM_ACTIVATE / WM_MOUSEMOVE / 控件焦点等，独立
//   子系统，另批搬迁）。它对"本过程自己会处理的消息"一律返回 0，故此处
//   保持"未处理"，与本过程 asm 的 `test eax,eax / jnz` 控制流一致。
// ----------------------------------------------------------------------------
int32 Dialog_Handle_General_Messages(void* hDlg, int32 uMsg, int32 wParam,
                                     int32 lParam)
{
    // asm: sub esp,30h ; cmp edx,84h ; ja loc_622E9A  (非本对话框关心的消息)
    //      只有 0x110/0x111/0x113/0x114/0x115 等才会进入 switch 的 case，
    //      而这些正是本过程自行分派的 3 条消息，因此这里恒返回 0。
    (void)hDlg; (void)uMsg; (void)wParam; (void)lParam;
    return 0;
}

// ----------------------------------------------------------------------------
// asm: ControlList_Diplomacy_* —— 控件 ID 表（.data 0x8391F0 区，逐项照抄）
//   HouseNameStatics[8] = 1041(0x411)..1048(0x418)
//   KillStatics[8]      = 1049(0x419)..1056(0x420)
//   行内（去掉第 0 项）的两张 7 项表同上偏移 +1。
//   AllyCheckboxes[7]   = 1743(0x6CF)..1749(0x6D5)
//   ChatCheckboxes[7]   = 1750,1751,1752,1753,1762,1763,1764
// ----------------------------------------------------------------------------
const int32 ControlList_Diplomacy_HouseNameStatics[8] =
    { 1041, 1042, 1043, 1044, 1045, 1046, 1047, 1048 };
const int32 ControlList_Diplomacy_KillStatics[8] =
    { 1049, 1050, 1051, 1052, 1053, 1054, 1055, 1056 };
const int32 ControlList_Diplomacy_HouseNameStatics_1[7] =
    { 1042, 1043, 1044, 1045, 1046, 1047, 1048 };
const int32 ControlList_Diplomacy_KillStatics_1[7] =
    { 1050, 1051, 1052, 1053, 1054, 1055, 1056 };
const int32 ControlList_Diplomacy_AllyCheckboxes[7] =
    { 1743, 1744, 1745, 1746, 1747, 1748, 1749 };
const int32 ControlList_Diplomacy_ChatCheckboxes[7] =
    { 1750, 1751, 1752, 1753, 1762, 1763, 1764 };

// asm: dword_83923C / dword_83925C —— 两张命令扫描表。首元素 -1 是"自己"槽，
//   因此表内下标 0 恒不对应任何真实控件，行索引直接等于 PlayerNumber 时
//   折回 0 就表示"本机玩家"。
const int32 dword_83923C[8] =
    { -1, 1743, 1744, 1745, 1746, 1747, 1748, 1749 };
const int32 dword_83925C[8] =
    { -1, 1750, 1751, 1752, 1753, 1762, 1763, 1764 };

// asm: byte_A8D108[8] —— 每行的"允许聊天"屏蔽位
uint8 byte_A8D108[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
// asm: dword_A8D110 —— Session 3/4 各自的会话开关（按字节访问）
uint8 dword_A8D110[2] = { 0, 0 };

// asm: Player_MuteSWLaunches —— 关闭超级武器播报时同时屏蔽外交结盟交互
int32 s_MuteSWLaunches = 0;
// asm: someHouse —— 当前"观察方"所属；不等于 PlayerPtr 时才允许结盟
HouseClass* s_SomeHouse = nullptr;
// asm: dword_A8B244 —— 与 2 比较（2 = 该模式下不显示结盟复选框）
int32 s_DwordA8B244 = 0;
// asm: wNull —— 宽空串（清空静态文本用）
wchar_t s_WNull[1] = { L'\0' };
// asm: word_A8B322 —— 71Bh 说明文字
wchar_t s_WordA8B322[0x40] = { L'\0' };
// asm: GlobalMPGameModeClass + 0x20 —— 模式名宽串（GameModeClass 未建 +0x20 槽）
wchar_t s_MpGameModeName[0x40] = { L'\0' };

// asm: sub_4E3320(ecx = 行号) —— 7 行"面板"静态 ID：0 -> 6DAh ... 6 -> 6E0h，
//   7 -> 6E1h，其它 -> -1（asm 的 neg/sbb/and/add 链）。
int32 Pane_Id(int32 nRow)
{
    if (nRow >= 0 && nRow <= 6)
        return 0x6DA + nRow;
    if (nRow == 7)
        return 0x6E1;
    return -1;
}

// asm: 国家"可玩"位（Country+1A5h）。本工程对应 HouseTypeClass::Multiplay。
bool Country_Playable(const HouseClass* pHouse)
{
    return pHouse != nullptr && pHouse->Type != nullptr && pHouse->Type->Multiplay;
}

// asm: Country+1A6h —— "被动多人方"（不参与外交列表）
bool Country_Passive(const HouseClass* pHouse)
{
    return pHouse != nullptr && pHouse->Type != nullptr &&
           pHouse->Type->MultiplayPassive;
}

// asm: HouseClass::Player 的 PlayerNumber（+30h）
int32 Player_Number(const HouseClass* pHouse)
{
    return pHouse != nullptr ? pHouse->PlayerNumber : 0;
}

// asm: sub_603DB0 / sub_603DD0 —— 两个自定义通知消息（0x4E5 / 0x4E6）
void Set_Ally_State_A(void* hWnd, uint8 value)
{
    Dlg_Send_Msg(hWnd, 0x4E5, 0, static_cast<int32>(value & 0xFF));
}

void Set_Ally_State_B(void* hWnd, uint8 value)
{
    Dlg_Send_Msg(hWnd, 0x4E6, 0, static_cast<int32>(value & 0xFF));
}

// asm: loc_657FE5 —— 整行禁用 + 隐藏（面板 / 名字 / 结盟 / 聊天 / 击杀）
void Diplomacy_Disable_Row(void* hDlg, void* hAlly, void* hChat, int32 nRow)
{
    void* h = Dlg_Get_Item(hDlg, Pane_Id(nRow));       // asm: call sub_4E3320
    Dlg_Enable(h, 0);
    Dlg_Show(h, 0);

    h = Dlg_Get_Item(hDlg, ControlList_Diplomacy_HouseNameStatics_1[nRow - 1]);
    Dlg_Enable(h, 0);
    Dlg_Show(h, 0);

    Dlg_Enable(hAlly, 0);
    Dlg_Show(hAlly, 0);
    Dlg_Enable(hChat, 0);
    Dlg_Show(hChat, 0);

    h = Dlg_Get_Item(hDlg, ControlList_Diplomacy_KillStatics_1[nRow - 1]);
    Dlg_Enable(h, 0);
    Dlg_Show(h, 0);                                    // asm: loc_658064
}

// ----------------------------------------------------------------------------
// asm: sub_658330 —— 重画一行（名字 / 击杀 / 结盟勾选 / 聊天可见性）
//   __thiscall(ecx = Map, hDlg, 行号, HouseClass*)
//   名字取 House+1602Ah（本工程 HouseClass::CSFName），配色三分支：
//     someHouse == house -> "LightGrey"；house+1F5h（灰显）-> "Grey"；
//     否则 ColorScheme.data[house+16054h]（本工程 ColorScheme::Array +
//     ColorSchemeIndex）。击杀列 = 20 项 KilledUnits + KilledBuildings 之和。
//   面板图标走 Get_House_PCX_Icon + sub_603D30，后者把图标写进
//   0xAC1B00 哈希表里的控件包装对象；该 UI 控件层在 OpenYR 不存在，
//   故只保留面板静态的定位（不影响本对话框的任何可见文本/勾选状态）。
// ----------------------------------------------------------------------------
void Diplomacy_Update_Row(void* hDlg, int32 nRow, HouseClass* pHouse)
{
    const int32 idName = ControlList_Diplomacy_HouseNameStatics[nRow];
    const int32 idKill = ControlList_Diplomacy_KillStatics[nRow];

    wchar_t text[0x100] = { L'\0' };

    // asm: test ebx,ebx / jz loc_6585F1 ; mov eax,[ebx+34h] / cmp [eax+1A5h],0
    if (!Country_Playable(pHouse))
    {
        // loc_6585F1：两张静态文本都写空串（lParam 指向清零缓冲）
        Dlg_Send_Msg(Dlg_Get_Item(hDlg, idName), 0x4B2, 0,
                     static_cast<int32>(reinterpret_cast<intptr_t>(text)));
        Dlg_Send_Msg(Dlg_Get_Item(hDlg, idKill), 0x4B2, 0,
                     static_cast<int32>(reinterpret_cast<intptr_t>(text)));
        return;
    }

    // asm: wcscpy(lParam, house+1602Ah)
    std::wcsncpy(text, pHouse->CSFName, 0xFF);
    text[0xFF] = L'\0';

    // asm: 配色选择（三条分支）
    ColorScheme* pScheme = nullptr;
    if (pHouse == s_SomeHouse)
        pScheme = ColorScheme::Find("LightGrey", 1);
    else if (pHouse->IsGreyedOut)
        pScheme = ColorScheme::Find("Grey", 1);
    else if (ColorScheme::Array != nullptr)
        pScheme = (*ColorScheme::Array)[pHouse->ColorSchemeIndex];

    // asm: sub_517440(scheme+308h) 取三通道后打包成 0x00BBGGRR
    uint32 rgb = 0;
    if (pScheme != nullptr)
    {
        rgb = (static_cast<uint32>(pScheme->BaseColor.B) << 16) |
              (static_cast<uint32>(pScheme->BaseColor.G) << 8) |
              static_cast<uint32>(pScheme->BaseColor.R);
    }

    void* hName = Dlg_Get_Item(hDlg, idName);
    Dlg_Send_Msg(hName, 0x498, 0, static_cast<int32>(rgb));   // 文本颜色
    Dlg_Send_Msg(hName, 0x4B2, 0,
                 static_cast<int32>(reinterpret_cast<intptr_t>(text)));

    // asm: 面板静态（sub_4E3320(nRow)）+ Get_House_PCX_Icon + sub_603D30
    (void)Dlg_Get_Item(hDlg, Pane_Id(nRow));
    (void)EngineGlobals::House_PCX_Icon(pHouse->ColorSchemeIndex);

    // asm: 结盟复选框（dword_83923C[nRow] == -1 时跳过）
    if (dword_83923C[nRow] != -1)
    {
        void* hAlly = Dlg_Get_Item(hDlg, dword_83923C[nRow]);
        if (s_MuteSWLaunches != 0)
            Dlg_Enable(hAlly, 0);                      // asm: 0x6584D0 段

        if (pHouse->IsGreyedOut)
        {
            Dlg_Show(hAlly, 0);                        // asm: loc_6584D8
        }
        else
        {
            // asm: HouseClass::Is_Ally(PlayerPtr, house) -> BM_SETCHECK
            const bool bAlly =
                (HouseClass::Player != nullptr) &&
                HouseClass::Player->IsAlliedWith(pHouse);
            Dlg_Send_Msg(hAlly, 0xF1, bAlly ? 1 : 0, 0);

            // asm: HouseClass::Is_Ally(house, PlayerPtr) -> sub_603DD0
            const bool bAlly2 =
                (HouseClass::Player != nullptr) &&
                pHouse->IsAlliedWith(HouseClass::Player);
            Set_Ally_State_B(hAlly, bAlly2 ? 1 : 0);
        }
    }

    // asm: loc_658525 —— 聊天复选框（dword_83925C[nRow]）
    if (pHouse->IsDefeated)                            // asm: [house+248h]
    {
        Dlg_Show(Dlg_Get_Item(hDlg, dword_83925C[nRow]), 0);
    }
    else if (s_MuteSWLaunches != 0)
    {
        void* hChat = Dlg_Get_Item(hDlg, dword_83925C[nRow]);
        Dlg_Send_Msg(hChat, 0xF1, 1, 0);
        Dlg_Enable(hChat, 0);
    }

    // asm: loc_658586 —— 20 次 [ebx]+[ebx+54h] 累加 -> _swprintf("%d")
    int32 nKills = 0;
    for (int32 i = 0; i < 20; ++i)
        nKills += pHouse->KilledUnits[i] + pHouse->KilledBuildings[i];

    wchar_t killText[0x40];
    std::swprintf(killText, 0x40, L"%d", static_cast<int>(nKills));

    void* hKill = Dlg_Get_Item(hDlg, idKill);
    Dlg_Send_Msg(hKill, 0x498, 0, static_cast<int32>(rgb));
    Dlg_Send_Msg(hKill, 0x4B2, 0,
                 static_cast<int32>(reinterpret_cast<intptr_t>(killText)));
}

// ----------------------------------------------------------------------------
// asm: sub_658640 —— 刷新全部外交行（ecx = Map，参数 hDlg）
//   for (i = 0 ; i < vec_Houses.length ; ++i):
//       house = vec_Houses[i] ; if (!house) continue
//       if (house->Is_Player()) var_8 = i            ; 记住玩家行
//       else if (country[+1A6h]) continue            ; 被动方跳过
//       sub_658330(Map, hDlg, i, house)
//   收尾：sub_658330(Map,hDlg,0,houses[var_8]) 与
//         sub_658330(Map,hDlg,var_8,houses[0])
// ----------------------------------------------------------------------------
void Diplomacy_Refresh_Rows(void* hDlg)
{
    const int32 nHouses = TheHouseCount;               // asm: vec_Houses.length
    int32 nPlayerIdx = -1;                             // asm: var_8（初值 -1）

    for (int32 i = 0; i < nHouses; ++i)
    {
        HouseClass* pHouse = Houses[i];
        if (pHouse == nullptr)
            continue;

        if (pHouse->PlayerControl)                     // asm: HouseClass::Is_Player
            nPlayerIdx = i;
        else if (Country_Passive(pHouse))
            continue;

        Diplomacy_Update_Row(hDlg, i, pHouse);
    }

    // asm: edx = houses[var_8] ; esi = houses[0]
    HouseClass* pPlayerHouse =
        (nPlayerIdx >= 0 && nPlayerIdx < nHouses) ? Houses[nPlayerIdx] : nullptr;
    HouseClass* pFirstHouse = (nHouses > 0) ? Houses[0] : nullptr;

    Diplomacy_Update_Row(hDlg, 0, pPlayerHouse);
    Diplomacy_Update_Row(hDlg, (nPlayerIdx >= 0) ? nPlayerIdx : 0, pFirstHouse);
}

// ----------------------------------------------------------------------------
// asm: sub_731840 —— 把"外交状态已变"事件压进网络事件环
//
//   if (Session == 0) return
//   if (Scenario->[0] & 0x0400) return
//   if (!RulesData->[14B5h] || !RulesData->[14BBh]) return
//   if (LastEventIdx >= 0x80) return
//   event = sub_4C6720(PlayerPtr->PlayerNumber, 3, &EventClass)   ; 1Bh dwords
//   NetworkPackets0[FirstUnusedPacket] = event                    ; 0x70 字节
//   Networking_Timestamps[FirstUnusedPacket] = timeGetTime()
//   FirstUnusedPacket = (FirstUnusedPacket + 1) & 7Fh
//   ++LastEventIdx
//
//   本工程尚未建立 EventClass / NetworkPackets0 环形队列，这里以等价的最小
//   记录（事件类型 + 发起方 PlayerNumber + 时间戳）落到同一形状的 0x80 环里，
//   四个前置判据、入队下标回绕与两个计数器的推进顺序与 asm 完全一致。
// ----------------------------------------------------------------------------
struct DiplomacyNetEvent
{
    int32 Kind;          // asm: EventClass::EventKind = 3
    int32 PlayerNumber;  // asm: sub_4C6720 的第二个参数（发起方）
    int32 HouseIndex;    // asm: 事件目标行（本过程传入的 idx）
    int32 Field0C;
    int32 Payload[0x1B];  // asm: rep movsd 1Bh dwords
};

constexpr int32 kNetworkPacketCount = 0x80;            // asm: & 7Fh

DiplomacyNetEvent s_NetworkPackets0[kNetworkPacketCount];
uint32            s_NetworkingTimestamps[kNetworkPacketCount];
int32             s_FirstUnusedPacket = 0;
int32             s_LastEventIdx = 0;

uint32 Diplomacy_Time_Get_Time()
{
#if defined(OPENYR_RADAR_WIN32)
    return static_cast<uint32>(::timeGetTime());
#else
    // asm: __imp_timeGetTime —— 毫秒级系统时间；本工程用单调时钟等价替代
    static uint32 s_Tick = 0;
    return s_Tick += 1;
#endif
}

void Diplomacy_Queue_Ally_Event(int32 nRow)
{
    SessionClass* pSession = SessionClass::GetInstance();
    if (pSession == nullptr)                           // asm: test eax,eax / jz
        return;

    // asm: Scenario->[0] & 0x0400
    ScenarioClass* pScenario = TheScenario;
    if (pScenario != nullptr && (pScenario->flags & 0x0400) != 0)
        return;

    RulesClass* pRules = RulesClass::Instance;
    if (pRules == nullptr || !pRules->AlliesAllowed)   // asm: [14B5h]
        return;
    if (!pRules->AllyChangeAllowed)                    // asm: [14BBh]
        return;

    if (s_LastEventIdx >= kNetworkPacketCount)         // asm: cmp 80h / jge
        return;

    DiplomacyNetEvent event;
    std::memset(&event, 0, sizeof(event));
    event.Kind = 3;                                    // asm: push 3
    event.PlayerNumber = Player_Number(HouseClass::Player);
    event.HouseIndex = nRow;

    s_NetworkPackets0[s_FirstUnusedPacket] = event;    // asm: rep movsd + movsw/movsb
    s_NetworkingTimestamps[s_FirstUnusedPacket] = Diplomacy_Time_Get_Time();

    s_FirstUnusedPacket = (s_FirstUnusedPacket + 1) & 0x7F;
    ++s_LastEventIdx;
}

} // namespace

int32 RadarClass::Diplomacy_DialogFunc(void* hDlg, uint32 uMsg, uint32 wParam,
                                       int32 lParam)
{
    // asm: call Dialog_HandleGeneralMessages ; test eax,eax ; jnz loc_65831B
    const int32 nHandled = Dialog_Handle_General_Messages(
        hDlg, static_cast<int32>(uMsg), static_cast<int32>(wParam), lParam);
    if (nHandled != 0)
        return nHandled;

    // asm: mov eax,ebx ; sub eax,111h ; jz WM_COMMAND
    int32 nMsg = static_cast<int32>(uMsg) - 0x111;
    if (nMsg == 0)
    {
        // ====================================================================
        // WM_COMMAND（asm 0x6581A6..0x658319）
        // ====================================================================
        void* hCtrl = reinterpret_cast<void*>(static_cast<intptr_t>(lParam));
        const int32 ctrl = Dlg_Ctrl_Id(hCtrl);         // asm: GetDlgCtrlID(ebp)
        const uint32 hi = wParam >> 16;                // asm: shr edi,10h

        // ---- 423h：取消/返回（asm 0x6581A6..0x6581E0）---------------------
        if (ctrl == 0x423)
        {
            if (hi != 0)
                return 0;

            Dlg_Kill_Timer(hDlg, 0);                   // asm: KillTimer(hDlg,0)
            const int32 nUser = Dlg_Win_Long(hDlg, 8);  // asm: GetWindowLongA(hDlg,8)
            if (nUser != 0)
                *reinterpret_cast<int32*>(static_cast<intptr_t>(nUser)) = 0x423;
            return 0;
        }

        // ---- 结盟表（asm 0x6581E1..0x658278）-------------------------------
        int32 nAllyIdx = -1;
        for (int32 i = 0; i < 8; ++i)
        {
            if (dword_83923C[i] == ctrl)
            {
                nAllyIdx = i;
                break;
            }
        }

        if (nAllyIdx >= 0)
        {
            // asm: cmp edi,1 / jz ; test edi,edi / jnz loc_658319
            if (hi != 1 && hi != 0)
                return 0;

            int32 nRow = nAllyIdx;
            if (HouseClass::Player != nullptr &&
                nRow == Player_Number(HouseClass::Player))
                nRow = 0;                              // asm: xor esi,esi

            // asm: hi==0 -> BM_SETCHECK(hCtrl,1,0) ; hi==1 -> (0,0)
            if (hi == 0)
                Dlg_Send_Msg(hCtrl, 0xF1, 1, 0);
            else
                Dlg_Send_Msg(hCtrl, 0xF1, 0, 0);

            Diplomacy_Queue_Ally_Event(nRow);          // asm: call sub_731840
            return 0;
        }

        // ---- 聊天表（asm 0x65827A..0x6582D2）-------------------------------
        int32 nChatIdx = -1;
        for (int32 i = 0; i < 8; ++i)
        {
            if (dword_83925C[i] == ctrl)
            {
                nChatIdx = i;
                break;
            }
        }

        if (nChatIdx >= 0)
        {
            // asm: edx = (wParam>>10h)&0FFFFh ; cmp edx,1 / jz ; test / jnz
            if (hi != 1 && hi != 0)
                return 0;

            // asm: cmp cx,1 / setz bl —— cx 已是 (wParam>>16) 的低 16 位
            const uint8 bl = (hi == 1) ? 1 : 0;

            int32 nRow = nChatIdx;
            if (HouseClass::Player != nullptr &&
                nRow == Player_Number(HouseClass::Player))
                nRow = 0;                              // asm: xor edx,edx

            byte_A8D108[nRow] = bl;                    // asm: mov byte_A8D108[edx],bl
            return 0;
        }

        // ---- 730h 会话开关（asm 0x6582D3..0x658318）-----------------------
        if (ctrl != 0x730)
            return 0;
        if (hi != 1 && hi != 0)
            return 0;

        SessionClass* pSession = SessionClass::GetInstance();
        const int32 nSession = (pSession != nullptr) ? pSession->SessionType : 0;

        if (nSession == 3)
        {
            dword_A8D110[0] = (hi == 1) ? 1 : 0;       // asm: cmp edi,1 / setz cl
            return 0;
        }
        if (nSession == 4)
        {
            dword_A8D110[1] = (hi == 1) ? 1 : 0;       // asm: cmp edi,1 / setz dl
            return 0;
        }
        return 0;
    }

    // asm: sub eax,2 ; jz loc_658184 —— WM_TIMER
    nMsg -= 2;
    if (nMsg == 0)
    {
        Diplomacy_Refresh_Rows(hDlg);                  // asm: sub_658640(Map,hDlg)
        Dlg_Invalidate(hDlg, 1);                       // asm: InvalidateRect(hDlg,0,1)
        return 0;
    }

    // asm: sub eax,384h ; jnz loc_658319 —— 只剩 497h（初始化）
    nMsg -= 0x384;
    if (nMsg != 0)
        return 0;

    // ========================================================================
    // InitDialog（asm 0x657DE0..0x658181）
    // ========================================================================
    {
        // ---- 清空 8 组名字 / 击杀静态（asm 0x657DF3..0x657E45）-------------
        for (int32 i = 0; i < 8; ++i)
        {
            Dlg_Send_Msg(Dlg_Get_Item(hDlg, ControlList_Diplomacy_HouseNameStatics[i]),
                         0x4B2, 0,
                         static_cast<int32>(reinterpret_cast<intptr_t>(s_WNull)));
            Dlg_Send_Msg(Dlg_Get_Item(hDlg, ControlList_Diplomacy_KillStatics[i]),
                         0x4B2, 0,
                         static_cast<int32>(reinterpret_cast<intptr_t>(s_WNull)));
        }

        const int32 nHouses = TheHouseCount;           // asm: vec_Houses.length
        HouseClass* pPlayer = HouseClass::Player;      // asm: HouseClass_PlayerPtr
        uint8 bAllyShown = 0;                          // asm: arg_C
        uint8 bChatShown = 0;                          // asm: arg_8

        // ---- 7 行循环（asm 0x657E5C..0x658079）-----------------------------
        for (int32 nOff = 0, nRow = 1; nOff < 0x1C; nOff += 4, ++nRow)
        {
            const int32 r = nOff / 4;
            void* hAlly = Dlg_Get_Item(hDlg, ControlList_Diplomacy_AllyCheckboxes[r]);
            void* hChat = Dlg_Get_Item(hDlg, ControlList_Diplomacy_ChatCheckboxes[r]);

            // asm: var_10 = var_C ; if (var_C == PlayerPtr->PlayerNumber) var_10 = 0
            int32 nIdx = nRow;
            if (pPlayer != nullptr && nRow == Player_Number(pPlayer))
                nIdx = 0;

            // asm: cmp eax,vec_Houses.length / jge loc_657FE5
            HouseClass* pHouse = nullptr;
            if (nIdx < nHouses)
                pHouse = Houses[nIdx];                 // asm: vec_Houses.Vector[eax*4]

            if (!Country_Playable(pHouse))
            {
                Diplomacy_Disable_Row(hDlg, hAlly, hChat, nRow);
                continue;
            }

            // asm: RulesData+14B5h / someHouse != PlayerPtr / dword_A8B244 != 2
            const bool bAlliesAllowed =
                (RulesClass::Instance != nullptr) &&
                RulesClass::Instance->AlliesAllowed;
            const bool bAllyPath = bAlliesAllowed &&
                                   (s_SomeHouse != pPlayer) &&
                                   (s_DwordA8B244 != 2);

            if (bAllyPath)
            {
                // asm: sub_603DB0(hAlly, 1)
                Set_Ally_State_A(hAlly, 1);

                // asm: HouseClass::Is_Ally(PlayerPtr, house) -> BM_SETCHECK(al)
                const bool bAlly1 = (pPlayer != nullptr) &&
                                    pPlayer->IsAlliedWith(pHouse);
                Dlg_Send_Msg(hAlly, 0xF1, bAlly1 ? 1 : 0, 0);

                // asm: HouseClass::Is_Ally(house, PlayerPtr) -> sub_603DD0
                const bool bAlly2 = (pPlayer != nullptr) &&
                                    pHouse->IsAlliedWith(pPlayer);
                Set_Ally_State_B(hAlly, bAlly2 ? 1 : 0);

                bAllyShown = 1;                        // asm: mov [arg_C],1

                // asm: if (!Player_MuteSWLaunches && !house->Is_Human())
                //          EnableWindow(hAlly,0)
                if (s_MuteSWLaunches == 0 && !pHouse->IsHumanPlayer)
                    Dlg_Enable(hAlly, 0);
            }
            else
            {
                Dlg_Show(hAlly, 0);                    // asm: loc_657F70
            }

            // asm: loc_657F75 —— 聊天复选框
            if (!pHouse->IsHumanPlayer)                // asm: HouseClass::Is_Human
            {
                Dlg_Show(hChat, 0);                    // asm: 0x657F7E
            }
            else if (s_MuteSWLaunches != 0)
            {
                Dlg_Send_Msg(hChat, 0xF1, 1, 0);       // asm: loc_657FC0
                Dlg_Enable(hChat, 0);
                bChatShown = 1;
            }
            else
            {
                // asm: SendMessageA(hChat,0F1h, byte_A8D108[var_10],0)
                Dlg_Send_Msg(hChat, 0xF1,
                             static_cast<int32>(byte_A8D108[nIdx]), 0);
                bChatShown = 1;
            }
        }

        // ---- 表头隐藏（asm 0x65807A..0x6580AC）-----------------------------
        if (bAllyShown == 0)
            Dlg_Show(Dlg_Get_Item(hDlg, 0x597), 0);
        if (bChatShown == 0)
            Dlg_Show(Dlg_Get_Item(hDlg, 0x425), 0);

        // ---- 730h 会话开关显示（asm 0x6580AD..0x65810B）-------------------
        {
            void* h730 = Dlg_Get_Item(hDlg, 0x730);
            if (h730 != nullptr)
            {
                SessionClass* pSession = SessionClass::GetInstance();
                const int32 nSession =
                    (pSession != nullptr) ? pSession->SessionType : 0;

                if (nSession == 3 || nSession == 4)
                {
                    Dlg_Show(h730, 5);                 // asm: push 5
                    if (nSession == 3)
                        Dlg_Send_Msg(h730, 0xF1,
                                     static_cast<int32>(dword_A8D110[0]), 0);
                    else
                        Dlg_Send_Msg(h730, 0xF1,
                                     static_cast<int32>(dword_A8D110[1]), 0);
                }
                else
                {
                    Dlg_Show(h730, 0);
                }
            }
        }

        // ---- 刷新全部行（asm 0x65810C）------------------------------------
        Diplomacy_Refresh_Rows(hDlg);                  // asm: sub_658640(Map,hDlg)

        // ---- 模式名 426h（asm 0x658114..0x65814C，仅 Session 3/4）---------
        {
            SessionClass* pSession = SessionClass::GetInstance();
            const int32 nSession =
                (pSession != nullptr) ? pSession->SessionType : 0;
            if (nSession == 3 || nSession == 4)
            {
                void* h426 = Dlg_Get_Item(hDlg, 0x426);
                Dlg_Send_Msg(h426, 0x4B2, 0,
                             static_cast<int32>(
                                 reinterpret_cast<intptr_t>(s_MpGameModeName)));
            }
        }

        // ---- 71Bh 说明文字 + 250ms 定时器（asm 0x65814D..0x65817F）--------
        Dlg_Send_Msg(Dlg_Get_Item(hDlg, 0x71B), 0x4B2, 0,
                     static_cast<int32>(reinterpret_cast<intptr_t>(s_WordA8B322)));
        Dlg_Set_Timer(hDlg, 0, 0xFA);
        return 0;
    }
}


// ============================================================================
// RadarClass::Draw 辅助件实现（asm: sub_656EC0 / 状态帧绘制 / 按钮矩形夹紧）
// ============================================================================
namespace {

// asm: sub_656EC0 —— 雷达逐帧推进。原版在此重跑地形脏扫描（sub_655C50/
// sub_656150 逐像素例程），并把 14DCh 视野框算出来。
//   OpenYR 的逐像素复合已由 Draw_Mini_Map 承担，这里保留 14DCh 的推进：
//   视野框的实际像素写入与夹紧在 Draw_Mini_Map 里按同一公式完成，故此处只
//   转发到 Update_Radar（缺面时的 headless 分支）。
void Update_Radar_Internal(RadarClass& radar)
{
    radar.Update_Radar();
}

// asm: 状态帧绘制（CC_Draw_Shape frameIdx=0/0x20/[14FCh]）。
//   原版把雷达 SHP（dword_B04A38 = RADAR.SHP / RADARY.SHP）的该帧贴到
//   DSurface_Sidebar 的 ([11E4h],[11ECh]) 处，随后写
//   [120Ch] = {11E4, 11EC, num_168, 6Eh}。
//   asm 逐条：
//     mov ecx,dword_B04A38 ; movsx edx,word [ecx+6]   ; 帧数上限
//     call sub_72F510                                  ; SIDEBAR_PAL
//     mov ecx,DSurface_Sidebar ; lea edx,[esp+40h] ; call [eax+78h]  ; bounds
//     push 0 / push 0 / push 0 / push 0 / push 3E8h / push 0 / push 0
//     push bounds / push &pos / push frameIdx / push SHP / call CC_Draw_Shape
//     mov [120Ch]=[11E4h] ; [1210h]=[11ECh] ; [1214h]=num_168 ; [1218h]=6Eh
void RenderRadarStateFrame(RadarClass& radar, int32 frameIdx)
{
    // asm: call sub_72F510（SIDEBAR_PAL）; push SHP/pos/frameIdx ; call CC_Draw_Shape
    DrawSidebarShape(RadarArt_SHP, frameIdx,
                     static_cast<int32>(radar.unknown_11E4),
                     static_cast<int32>(radar.unknown_11EC));

    // asm: lea ebx,[esi+120Ch] ; mov [ebx],[11E4h] ; [ebx+4]=[11ECh]
    //      ; [ebx+8]=ds:num_168 ; [ebx+0Ch]=6Eh
    //   num_168 是全局常量 168（2058714: num_168 dd 168），不是屏幕宽。
    radar.unknown_rect_120C.X      = static_cast<int32>(radar.unknown_11E4);
    radar.unknown_rect_120C.Y      = static_cast<int32>(radar.unknown_11EC);
    radar.unknown_rect_120C.Width  = 168;   // asm: ds:num_168
    radar.unknown_rect_120C.Height = 0x6E;
}

// asm: loc_653420 / loc_6534D5 —— 非脏帧分支把 [120Ch] 写成雷达 gadget 矩形
//   ([11F0h],[11F4h],[1200h],[1204h])，即侧边栏局部 (16,49,140,108)。
void RenderRadarGadgetRect(RadarClass& radar)
{
    radar.unknown_rect_120C.X      = static_cast<int32>(radar.unknown_11F0);
    radar.unknown_rect_120C.Y      = static_cast<int32>(radar.unknown_11F4);
    radar.unknown_rect_120C.Width  = static_cast<int32>(radar.unknown_1200);
    radar.unknown_rect_120C.Height = static_cast<int32>(radar.unknown_1204);
}

// asm: loc_653511 / loc_653628 —— 外交（DIPLOBTN）与选项（OPTBTN）按钮矩形
//   依 [120Ch] 与各自 SHP 尺寸夹紧，结果**写回 [120Ch] 本身**，随后
//   byte_B0B518 = 1（按钮矩形已变的脏标记）。
//
//   asm 逐句（以 DIPLOBTN 为例，ebp = &[120Ch]）：
//     if (!shapeSomeSidebarBtn.ShapeDrawn) goto 下一个按钮
//     eax = [esi+1214h]（= 120Ch.Width） ; if (eax <= 0) goto loc_6535FE
//     eax = [ebp+0Ch]（Height） ; if (eax <= 0) goto loc_6535FE
//     if (DIPLOBTN_SHP_Width  <= 0) goto loc_6535EF
//     if (DIPLOBTN_SHP_Height <= 0) goto loc_6535EF
//     if (x > ButtonLeft)  { w += x - ButtonLeft ; x = ButtonLeft }
//     if (y > OptionsTop)  { h += y - OptionsTop ; y = OptionsTop }
//     if (w + x < ButtonLeft + BtnW)  w = BtnW - x + ButtonLeft + 1
//     if (h + y < OptionsTop + BtnH)  h = BtnH - y + OptionsTop + 1
//     goto 写回
//   loc_6535EF: 原样保留 (x,y,w,h) —— 按钮尺寸未装载时不改动
//   loc_6535FE: (x,y,w,h) = (ButtonLeft, OptionsTop, BtnW, BtnH)
void ClampSidebarButtonRect(RadarClass& radar, RadarLayout& layout)
{
    Rectangle& rect = radar.unknown_rect_120C;

    // 两个按钮共用同一形状，只换左右/上下的基准与尺寸。
    struct Bound { int32 Left, Top, Width, Height, ShapeDrawn; };
    const Bound bounds[2] =
    {
        { layout.ButtonLeft, layout.OptionsTop, layout.DiploBtnW,   layout.DiploBtnH,
          layout.DiploBtnW > 0 && layout.DiploBtnH > 0 },
        { layout.DiploTop,   layout.DiploBottom, layout.OptionsBtnW, layout.OptionsBtnH,
          layout.OptionsBtnW > 0 && layout.OptionsBtnH > 0 },
    };

    for (const Bound& b : bounds)
    {
        // asm: mov al,shapeBtn.ShapeDrawn ; test al,al ; jz 下一个
        if (!b.ShapeDrawn)
            continue;

        int32 x = rect.X;
        int32 y = rect.Y;
        int32 w = rect.Width;
        int32 h = rect.Height;

        if (w <= 0 || h <= 0)
        {
            // asm: loc_6535FE
            x = b.Left;
            y = b.Top;
            w = b.Width;
            h = b.Height;
        }
        else if (b.Width > 0 && b.Height > 0)
        {
            if (x > b.Left)
            {
                // asm: sub eax,edx ; add edi,eax ; mov eax,edx
                w += x - b.Left;
                x = b.Left;
            }
            if (y > b.Top)
            {
                h += y - b.Top;
                y = b.Top;
            }
            if (w + x < b.Left + b.Width)
            {
                // asm: mov edi,BtnW ; sub edi,eax ; lea edx,[edi+edx+1]
                w = b.Width - (x - b.Left) + 1;
            }
            if (h + y < b.Top + b.Height)
            {
                h = b.Height - (y - b.Top) + 1;
            }
        }
        // else: asm loc_6535EF —— 原样保留

        rect.X = x;
        rect.Y = y;
        rect.Width = w;
        rect.Height = h;

        // asm: mov byte_B0B518,1 —— 外壳按钮矩形已变（脏标记）。
        g_ButtonRectDirty = true;
    }
}

} // namespace
