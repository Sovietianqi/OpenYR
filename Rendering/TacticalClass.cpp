// =============================================================================
// TacticalClass.cpp - Isometric tactical map rendering engine
//
// The core rendering engine for the game's tactical view. Handles:
// - Isometric coordinate conversion (world <-> screen <-> cell)
// - Terrain tile rendering with proper depth ordering
// - Object layer sorting (YSort) for correct draw order
// - Shroud and fog of war rendering
// - Grid overlay for construction placement
// - Selection rectangle and health bar rendering
// - Viewport management and dirty area tracking
// =============================================================================

#include <cstdlib>
#include "Rendering/TacticalClass.h"
#include "Rendering/DisplayClass.h"
#include "Game/Externs.h"
#include "Rendering/Surface.h"
#include "Rendering/TileArt.h"
#include "Rendering/GScreenClass.h"
#include "Rendering/ConvertClass.h"
#include "Rendering/Blitter.h"
#include "Core/Memory.h"
#include "Math/CoordStruct.h"
#include "Math/Rectangle.h"
#include "Math/Facing.h"
#include "Map/MapClass.h"
#include "Map/CellClass.h"
#include "Objects/IsometricTile.h"
#include "Math/Timer.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/OverlayTypeClass.h"
#include "Abstract/TerrainClass.h"
#include "Abstract/SmudgeClass.h"
#include "Abstract/VoxelAnimClass.h"
#include "Animations/AnimClass.h"
#include "Combat/BulletClass.h"
#include "Particles/ParticleSystemClass.h"
#include "Particles/ParticleClass.h"
#include "Abstract/BuildingClass.h"
#include "Scenario/ScenarioClass.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/TechnoClass.h"
#include "Houses/HouseClass.h"
#include "Rules/RulesClass.h"
#include "Game/Game.h"
#include "Math/FloatMath.h"
#include "Helpers/AsmMath.h"

#include <cstring>
#include <cwchar>
#include <cstdio>
#include <cmath>
#include <algorithm>

// =============================================================================
// Static members
// =============================================================================
TacticalClass* TacticalClass::Instance = nullptr;

// 冒烟诊断计数器（定义见 TacticalClass.h）。零初始化的静态存储：未开
// OPENYR_DEBUG 时没人更新，[RENDER] 也不会打，保持全 0。
RenderDebugStats g_RenderDebugStats = {};

// =============================================================================
// Constants
// =============================================================================
static const int32 MIN_SCROLL_MARGIN = 4;
static const int32 MAX_VISIBLE_CELLS = 800;
static const int32 HEALTH_BAR_WIDTH = 32;
static const int32 HEALTH_BAR_HEIGHT = 4;
static const int32 HEALTH_BAR_OFFSET_Y = -20;
static const int32 SELECTION_RECT_INSET = 4;
static const int32 SHROUD_DARKNESS = 0;
static const int32 FOG_DARKNESS = 128;
static const double ZOOM_MIN = 0.5;
static const double ZOOM_MAX = 2.0;

// =============================================================================
// 地形层绘制辅助（Draw_Overlay / Draw_Shroud / Draw_Fog / Draw_Grid 共用）
// =============================================================================
namespace
{
// 视口覆盖的格子范围（闭区间）。
struct CellSpan
{
    int32 MinX;
    int32 MinY;
    int32 MaxX;
    int32 MaxY;
};

// -----------------------------------------------------------------------------
// 视口四角反算格子范围
//
// 原版等距变换（见 CoordsToScreen / RegisterCellAsVisible 注释）：
//     screen = ((cx-cy)*30, (cx+cy)*15)
// 其逆变换就是 TacticalClass::ScreenToCell。视口四角各反算一次取包围盒，
// 再按 margin 格（= margin*30 / margin*15 像素）外扩——原版视口外留的宽容
// 边是 0x1E/0x0F 像素（见 Is_In_Viewport 注释），这里按整格取整。
// ScreenToCell 是整数截断除法，负坐标会偏一格，故取完每边再外扩 2 格兜底。
// -----------------------------------------------------------------------------
CellSpan Viewport_Cell_Span(const TacticalClass* pTac, int32 margin)
{
    CellSpan span = { 0, 0, -1, -1 };
    if (pTac == nullptr || MapClass::Instance == nullptr)
        return span;

    const int32 mapW = MapClass::Instance->MapWidth;
    const int32 mapH = MapClass::Instance->MapHeight;
    if (mapW <= 0 || mapH <= 0)
        return span;

    const int32 w = (pTac->ScreenWidth  > 0) ? pTac->ScreenWidth  : 640;
    const int32 h = (pTac->ScreenHeight > 0) ? pTac->ScreenHeight : 480;
    const Point2D origin = pTac->GetTacticalOrigin();

    const int32 padX = margin * TacticalClass::HalfTileWidth;
    const int32 padY = margin * TacticalClass::HalfTileHeight;

    const Point2D corner[4] = {
        Point2D(origin.X - padX,     origin.Y - padY),
        Point2D(origin.X + w + padX, origin.Y - padY),
        Point2D(origin.X - padX,     origin.Y + h + padY),
        Point2D(origin.X + w + padX, origin.Y + h + padY)
    };

    int32 minX = 0, minY = 0, maxX = 0, maxY = 0;
    for (int32 i = 0; i < 4; ++i)
    {
        const CellStruct cell = pTac->ScreenToCell(corner[i]);
        if (i == 0)
        {
            minX = maxX = cell.X;
            minY = maxY = cell.Y;
            continue;
        }
        if (cell.X < minX) minX = cell.X;
        if (cell.X > maxX) maxX = cell.X;
        if (cell.Y < minY) minY = cell.Y;
        if (cell.Y > maxY) maxY = cell.Y;
    }

    span.MinX = std::max(0, minX - 2);
    span.MinY = std::max(0, minY - 2);
    span.MaxX = std::min(mapW - 1, maxX + 2);
    span.MaxY = std::min(mapH - 1, maxY + 2);
    return span;
}

// -----------------------------------------------------------------------------
// 一个格子在目标面上的菱形包围盒（视口相对坐标）
//
// asm 依据：sub_6D2B60 把格子的投影坐标减 (0x1E, 0x0F) 得到地块左上角，
// 与 IsometricTileWidth/Height 的 60x30 包围盒一致。
// -----------------------------------------------------------------------------
Rectangle Tile_Box_Of_Cell(const TacticalClass* pTac, const Point2D& origin,
                           int32 cx, int32 cy)
{
    const CoordStruct coord = CellClass::Cell2Coord(
        CellStruct(static_cast<int16>(cx), static_cast<int16>(cy)));
    const Point2D screen = pTac->CoordsToScreen(coord);

    return Rectangle(screen.X - origin.X - TacticalClass::HalfTileWidth,
                     screen.Y - origin.Y - TacticalClass::HalfTileHeight,
                     TacticalClass::IsometricTileWidth,
                     TacticalClass::IsometricTileHeight);
}

// -----------------------------------------------------------------------------
// 迷雾混合：把该格菱形覆盖的像素减半（等价 50% 黑叠加）
//
// Surface 只有 FillRectEx（纯色填充）与 FillRectTrans（按 ColorStruct 混色），
// 没有"对已画内容做乘法混合"的逐行接口，所以这里按 Surface::FillRectTrans
// 同样的 565 处理方式直接写 Buffer：(dst >> 1) & 0x7BEF 正好把每个通道的
// 最低位清掉，避免 5/6 位通道右移后串色。
// -----------------------------------------------------------------------------
void Blend_Diamond_Half(Surface* pSrf, int32 left, int32 top)
{
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (!TileArt::Surface_Is_16bpp(pSrf))
        return;

    BYTE* const base  = static_cast<BYTE*>(pSrf->Buffer);
    const int32 pitch = pSrf->GetPitch();
    const int32 srfW  = pSrf->GetWidth();
    const int32 srfH  = pSrf->GetHeight();

    for (int32 row = 0; row < TileArt::IsoTileHeight; ++row)
    {
        const int32 y = top + row;
        if (y < 0 || y >= srfH)
            continue;

        const int32 half = TileArt::IsoRowHalfWidth(row);
        int32 x0 = left + TileArt::IsoHalfWidth - half;
        int32 x1 = x0 + half * 2;
        if (x0 < 0) x0 = 0;
        if (x1 > srfW) x1 = srfW;
        if (x1 <= x0)
            continue;

        uint16* line = reinterpret_cast<uint16*>(base + y * pitch);
        for (int32 x = x0; x < x1; ++x)
            line[x] = static_cast<uint16>((line[x] >> 1) & 0x7BEF);
    }
}
}  // namespace

// =============================================================================
// Constructor
// =============================================================================
TacticalClass::TacticalClass()
    : EndGameGraphicsFrame(0)
    , LastAIFrame(0)
    , field_AC(false)
    , field_AD(false)
    , TacticalPos(0, 0)
    , LastTacticalPos(0, 0)
    , ZoomInFactor(1.0)
    , Point_C8(0, 0)
    , Point_D0(0, 0)
    , field_D8(0.0f)
    , field_DC(0.0f)
    , VisibleCellCount(0)
    , TacticalCoord1(0, 0)
    , field_D6C(0)
    , field_D70(0)
    , TacticalCoord2(0, 0)
    , field_D7C(false)
    , Redrawing(false)
    , ContainingMapCoords(0, 0, 0, 0)
    , Band(0, 0, 0, 0)
    , MouseFrameIndex(0)
    , StartTime()
    , SelectableCount(0)
    , Unused_Matrix3D()
    , IsoTransformMatrix()
    , field_E14(0)
{
    Instance = this;
    memset(ScreenText, 0, sizeof(ScreenText));
    memset(gap_AE, 0, sizeof(gap_AE));
    memset(gap_D7E, 0, sizeof(gap_D7E));
    memset(VisibleCells, 0, sizeof(VisibleCells));
    // asm: TacticalClass_CTOR 用 rep stosd 把 VisibleCells(320h dword) 清 0；
    // 同批把 seletions 表（每项 0Ch 字节）整体清零。
    memset(Selectables, 0, sizeof(Selectables));
    ScreenWidth = (TheDisplay != nullptr) ? TheDisplay->ScreenWidth  : 640;
    ScreenHeight = (TheDisplay != nullptr) ? TheDisplay->ScreenHeight : 480;
}

// =============================================================================
// Destructor
// =============================================================================
TacticalClass::~TacticalClass()
{
    if (Instance == this)
        Instance = nullptr;
}

// =============================================================================
// sub_6DBB60 - Draw one object's tile at a Z-adjusted screen position
//
// asm: sub_6DBB60 (asm:1635) —— 参数 a2/a3 是目标的世界坐标对，
// a4/dwUnk 是额外的 Z 与绘制标志。asm 末段：
//     call Float_To_Int_Floor              ; 由高度换算网格行
//     mov  ecx, 0Eh ; sub ecx, eax         ; 14 - 行
//     fiadd a3 ; fadd 0.5 ; call Float_To_Int_Floor
//     mov  edx, 0Eh ; sub edx, eax
//     push edx/push a4 / lea var_8 ...
//     call dword ptr [edi+34h]             ; 经次 vtable 的绘制入口
// 即：把 Z 换算成"距顶行的层差"后交给绘制虚函数。OpenYR 未建模次 vtable
// （+34h 的渲染后端），此处按语义等价落空为：换算屏幕坐标 + Z 调整 +
// 视口剔除 + 登记脏区，不臆造整条图形子系统。
// =============================================================================
bool TacticalClass::sub_6DBB60(const CoordStruct& a2, const CoordStruct& a3, DWORD a4, DWORD dwUnk)
{
    // 世界坐标 → 屏幕像素（与 CoordsToScreen 同一等距变换）。
    Point2D p1 = CoordsToScreen(a2);
    Point2D p2 = CoordsToScreen(a3);

    // Z 调整：asm 用 (14 - floor(z * k)) 的形式把高度折成向上偏移的像素量。
    int32 z1 = AdjustForZ(a2.Z);
    int32 z2 = AdjustForZ(a3.Z);
    p1.Y -= z1;
    p2.Y -= z2;

    // 视口剔除：任一端点落在视口外则本帧不绘制。
    bool vis1 = Is_In_Viewport(a2);
    bool vis2 = Is_In_Viewport(a3);
    if (!vis1 && !vis2)
        return false;

    // a4/dwUnk 决定绘制标志（asm 直接转发）；本层只负责把覆盖区域标脏。
    Rectangle area(
        std::min(p1.X, p2.X) - HalfTileWidth,
        std::min(p1.Y, p2.Y) - HalfTileHeight,
        std::abs(p2.X - p1.X) + IsometricTileWidth,
        std::abs(p2.Y - p1.Y) + IsometricTileHeight);
    RegisterDirtyArea(area, (dwUnk & 1u) != 0 || (a4 & 1u) != 0);

    // asm: call dword ptr [edi+34h] —— 次 vtable 的渲染入口。OpenYR 尚未
    // 建模该渲染后端，按语义等价落空（绘制像素由 DisplayClass 的合成面
    // 驱动）；返回"已处理"。
    return true;
}

// =============================================================================
// SetTacticalPosition - Set the center of the tactical view
//
// 入参是世界坐标（leptons，1 格 = 256），TacticalPos 存放的是"等距投影像素"
// （与 CoordsToScreen 同一空间，GetTacticalOrigin = TacticalPos - 半屏）。
// 原版在 sub_6D8B30 里就是用世界中心重算屏幕原点，这里必须投影一次再存，
// 否则视口原点会大 256 倍，地形层整屏落在视口外。
// =============================================================================
void TacticalClass::SetTacticalPosition(CoordStruct* pCoord)
{
    if (!pCoord) return;
    LastTacticalPos = TacticalPos;

    Point2D projected = CoordsToScreen(*pCoord);
    Clamp_Tactical_Position(projected);          // asm: sub_6D8640 夹取

    TacticalPos   = projected;
    TacticalCoord1 = projected;
    TacticalCoord2 = projected;
}

// =============================================================================
// asm: TacticalClass_Update (vtable 槽 0x5C；proc@1483457-1483680)
//
//   调用点：LogicClass_Update 第 20 步（L751895）
//       mov ecx, TacticalMap / mov eax,[ecx] / call [eax+5Ch]
//
//   控制流（逐段）：
//     1. `test byte ptr arg_ATTRACT_Flags,2 / jnz end`
//          —— 非游戏态（主菜单/装载）直接返回。
//     2. `mov eax,[esi+0A8h] / cmp Frame / jz end`
//          —— 本帧已经跑过（LastAIFrame == Frame）则不重复。
//     3. `mov al,byte_A8ED5C / test / jz loc_6D268B`
//          —— 视口平滑滚动（仅当 0xD0 目标位置脱离默认值且步进 0xD8 != 0）：
//            a. field_DC += field_D8；> 1.0 时钳到 1.0；
//            b. sub_75F5C0(out, Point_C8, Point_D0, t)：
//                 out = floor(Point_C8*(1-t) + Point_D0*t)
//               （asm 里先算 t，再用 `fsubr 1.0` 得 1-t，逐分量浮点乘加）
//            c. t >= 1.0 → Point_D0/Point_C8/field_D8/field_DC 全部归零（滚动结束）；
//            d. sub_6D8640 夹取；夹取生效时用夹取结果，否则保留插值结果；
//            e. 结果同时写入 TacticalCoord1(0xD64/0xD68) 与
//               TacticalCoord2(0xD74/0xD78)，调 sub_6D8B30 重算屏幕原点，
//               置 Redrawing(0xD7D)=1。
//     4. 鼠标帧计时（0xDA4/0xDA8/0xDACh）：到期则重开计时并
//        `inc [esi+TacticalClass.MouseFrameIndex]`。
//     5. 末尾 `mov [esi+0A8h],Frame`；若本帧没有做平滑滚动、而
//        TacticalCoord2 != TacticalCoord1（外部直接改了视口目标），
//        走与 3d/3e 相同的夹取 + 标脏。
//
//   与 OpenYR 的对应：
//     * arg_ATTRACT_Flags & 2  → !Game::IsActive()（非游戏态不 tick）。
//     * 0xA8                   → LastAIFrame。
//     * sub_6D8B30             → SetTacticalPosition（重算 TacticalPos；
//                                等距变换矩阵留待表现层，见存疑项）。
//     * sub_6D8640             → Clamp_Tactical_Position。
//     * Debug_Map 分支         → OpenYR 无该全局，按"非调试"处理。
// =============================================================================
void TacticalClass::Update()
{
    // ---- 1. 非游戏态不跑 -----------------------------------------------
    if (!Game::IsActive())
        return;

    const int32 frame = Game::CurrentFrame;

    // ---- 2. 本帧已更新 --------------------------------------------------
    if (LastAIFrame == frame)
        return;

    bool scrolledThisFrame = false;

    // ---- 3. 视口平滑滚动 ------------------------------------------------
    if (field_D8 != 0.0f)
    {
        // a. 进度累加并钳到 1.0（asm: fadd / fcomp dbl1_0 / mov 3F800000h）。
        field_DC += field_D8;
        if (field_DC > 1.0f)
            field_DC = 1.0f;

        // b. sub_75F5C0 —— 逐分量 lerp，朝负无穷取整。
        const double t = static_cast<double>(field_DC);
        Point2D interpolated;
        interpolated.X = static_cast<int32>(std::floor(
            static_cast<double>(Point_C8.X) * (1.0 - t) +
            static_cast<double>(Point_D0.X) * t));
        interpolated.Y = static_cast<int32>(std::floor(
            static_cast<double>(Point_C8.Y) * (1.0 - t) +
            static_cast<double>(Point_D0.Y) * t));

        // c. 滚动到终点后清空插值状态。
        if (field_DC >= 1.0f)
        {
            Point_D0 = Point2D(0, 0);
            Point_C8 = Point2D(0, 0);
            field_D8 = 0.0f;
            field_DC = 0.0f;
        }

        // d. sub_6D8640 夹取：夹取修正就地生效；未夹取时保留插值结果。
        Point2D finalPos = interpolated;
        Clamp_Tactical_Position(finalPos);

        // e. 两对坐标同时写回 + 重算屏幕原点 + 标脏。
        TacticalCoord1 = finalPos;
        TacticalCoord2 = finalPos;

        // finalPos 已经是投影空间（TacticalCoord1/2 的同一空间），直接写回；
        // 不能再经 SetTacticalPosition（那一步会把世界 leptons 投影一次）。
        TacticalPos = finalPos;

        Redrawing = true;
        scrolledThisFrame = true;
    }

    // 与 3 的 bl 标志同义：本帧已滚动过则跳过末尾的二次夹取。
    // ---- 4. 鼠标帧计时 --------------------------------------------------
    {
        // asm: mov eax,[esi+0DA4h] / cmp eax,-1 / jz loc_6D26B4
        bool due = true;
        if (StartTime.StartTime != -1)
        {
            const int32 now = static_cast<int32>(Game::GetTickCount());
            const int32 elapsed = now - StartTime.StartTime;   // asm: sub eax,[esi+0DA4h]
            if (elapsed < StartTime.TimeLeft)                  // asm: cmp eax,edi / jl
            {
                const int32 remaining = StartTime.TimeLeft - elapsed;
                if (remaining != 0)                            // asm: test edi,edi / jnz
                    due = false;
            }
        }

        if (due)
        {
            // asm: 取 RulesData+50h 作为新一轮时长（本项目未建模该偏移，
            //   以固定 500ms 等价表达，见存疑项）。
            const int32 duration = 500;
            StartTime.StartTime = static_cast<int32>(Game::GetTickCount());
            StartTime.TimeLeft  = duration;
            ++MouseFrameIndex;                                 // asm: inc [esi+0DA0h]
        }
    }

    // ---- 5. 帧戳与"外部改动视口"的收尾 ---------------------------------
    LastAIFrame = frame;                                       // asm: mov [esi+0A8h],eax

    const bool posChanged = (TacticalCoord2.X != TacticalCoord1.X ||
                             TacticalCoord2.Y != TacticalCoord1.Y);

    if (posChanged && !scrolledThisFrame)
    {
        Point2D finalPos = TacticalCoord2;                     // asm: 取 0D74/0D78
        Clamp_Tactical_Position(finalPos);

        TacticalCoord1 = finalPos;
        TacticalCoord2 = finalPos;

        // finalPos 已经是投影空间（TacticalCoord1/2 的同一空间），直接写回；
        // 不能再经 SetTacticalPosition（那一步会把世界 leptons 投影一次）。
        TacticalPos = finalPos;

        Redrawing = true;                                      // asm: mov [esi+0D7Dh],1
    }

    // 冒烟诊断：记录战术层真正 tick 过的次数（被帧戳门挡住的不计）。
    if (g_bDebugMode)
        ++g_RenderDebugStats.UpdateTicks;
}

// =============================================================================
// asm: sub_6D8640 (0x6D8640, retn 4) —— 视口坐标夹取
//
//   原版按 map 的格子尺寸与视口矩形（rect_mouseclass_vp）算出可滚动范围的
//   左下/右上界，把 *a1 逐分量夹进去，夹取生效时返回 1：
//       if (a1->Y < minY) { a1->Y = minY; bl = 1; }
//       else if (a1->Y > maxY) { a1->Y = maxY; bl = 1; }
//       if (a1->X < minX) { a1->X = minX; return 1; }
//       else if (a1->X > maxX) { a1->X = maxX; return 1; }
//       return bl;
//   注意顺序：Y 先夹，且 X 的夹取直接提前返回（asm loc_6D86D2/6D86E5）。
//
//   OpenYR 用地图尺寸（格 * 256 leptons）作为等价上下界；视口半宽/半高
//   的 3 格余量按原版 `sub 3Ch`（60 像素）/ `+0FFFFFFE2h`（-30 像素）
//   折算成 1/4 格与 1/8 格的等距余量。
// =============================================================================
bool TacticalClass::Clamp_Tactical_Position(Point2D& pos) const
{
    if (MapClass::Instance == nullptr)
        return false;

    const int32 mapW = MapClass::Instance->MapWidth;
    const int32 mapH = MapClass::Instance->MapHeight;
    if (mapW <= 0 || mapH <= 0)
        return false;

    // 入参 pos 是**投影像素空间**（TacticalPos = CoordsToScreen(...) 的值），
    //   不是世界 leptons。此前的上下界写成 `(mapW-1)*256`，量纲差 256 倍，
    //   钳制等于没做：相机停在基地时视口四分之一探出地图，画面露出大片
    //   黑边。这里把地图四角投影出来，再按视口半宽/半高内缩，才是
    //   "视口不越出地图"的可滚动范围。
    const int32 vw = (ScreenWidth  > 0) ? ScreenWidth
                                        : ((TheDisplay != nullptr) ? TheDisplay->ScreenWidth  : 640);
    const int32 vh = (ScreenHeight > 0) ? ScreenHeight
                                        : ((TheDisplay != nullptr) ? TheDisplay->ScreenHeight : 400);

    const int32 minProjX = -(mapH - 1) * HalfTileWidth;                 // 格子 (0, mapH-1)
    const int32 maxProjX =  (mapW - 1) * HalfTileWidth;                 // 格子 (mapW-1, 0)
    const int32 minProjY = 0;                                           // 格子 (0,0)
    const int32 maxProjY = (mapW - 1 + mapH - 1) * HalfTileHeight;      // 格子 (mapW-1, mapH-1)

    int32 minX = minProjX + vw / 2;
    int32 maxX = maxProjX - vw / 2;
    int32 minY = minProjY + vh / 2;
    int32 maxY = maxProjY - vh / 2;

    // 地图比视口还小时没有可滚动的余地，直接把相机钉在地图中心。
    if (minX > maxX) { minX = maxX = (minProjX + maxProjX) / 2; }
    if (minY > maxY) { minY = maxY = (minProjY + maxProjY) / 2; }

    bool changed = false;

    // Y 先夹（asm: cmp edx,ecx / jge / cmp edx,eax / jle）。
    if (pos.Y < minY)      { pos.Y = minY; changed = true; }
    else if (pos.Y > maxY) { pos.Y = maxY; changed = true; }

    // X 后夹，命中即返回 1（asm loc_6D86D2/6D86E5）。
    if (pos.X < minX)      { pos.X = minX; return true; }
    if (pos.X > maxX)      { pos.X = maxX; return true; }

    return changed;
}

// =============================================================================
// CoordsToCell - Convert world coordinates to cell coordinates
// =============================================================================
CellStruct* TacticalClass::CoordsToCell(CellStruct* pDest, CoordStruct* pSource)
{
    if (!pDest || !pSource) return nullptr;
    pDest->X = static_cast<int16>(pSource->X / 256);
    pDest->Y = static_cast<int16>(pSource->Y / 256);
    return pDest;
}

// =============================================================================
// CoordsToClient - Convert world coordinates to client (screen) coordinates
// =============================================================================
bool TacticalClass::CoordsToClient(const CoordStruct* coords, Point2D* pOutClient) const
{
    if (!coords || !pOutClient) return false;
    pOutClient->X = (coords->X - coords->Y) * HalfTileWidth;
    pOutClient->Y = (coords->X + coords->Y) * HalfTileHeight;
    return true;
}

// =============================================================================
// CoordsToScreen - Convert world coordinates to screen pixel position
// =============================================================================
Point2D TacticalClass::CoordsToScreen(const CoordStruct& coord) const
{
    // asm 的等距变换（见 AdjustForZShapeMove 注释 / RegisterCellAsVisible）：
    //     screenX = 30 * (X - Y) / 256
    //     screenY = 15 * (X + Y) / 256
    // 世界坐标是 leptons（1 格 = 256 leptons），必须除 256 才落到像素空间。
    // 少了这个除法时 ScreenToCell 与 CoordsToScreen 不再互逆（1 格会算成
    // 256 格），视口剔除会把所有格子/对象全部挡掉——冒烟时表现为
    // visible=0 drawn=0，渲染链看着在跑实为空转。
    Point2D result;
    result.X = (coord.X - coord.Y) * HalfTileWidth  / 256;
    result.Y = (coord.X + coord.Y) * HalfTileHeight / 256;
    return result;
}

Point2D* TacticalClass::CoordsToScreen(Point2D* pDest, const CoordStruct* pSource)
{
    if (!pDest || !pSource) return nullptr;
    pDest->X = (pSource->X - pSource->Y) * HalfTileWidth  / 256;
    pDest->Y = (pSource->X + pSource->Y) * HalfTileHeight / 256;
    return pDest;
}

// =============================================================================
// ClientToCoords - Convert client (screen) coordinates to world coordinates
// =============================================================================
CoordStruct* TacticalClass::ClientToCoords(CoordStruct* pOutBuffer, const Point2D& client) const
{
    if (!pOutBuffer) return nullptr;
    int32 cx = client.X / HalfTileWidth;
    int32 cy = client.Y / HalfTileHeight;
    pOutBuffer->X = (cx + cy) * 128;
    pOutBuffer->Y = (cy - cx) * 128;
    pOutBuffer->Z = 0;
    return pOutBuffer;
}

CoordStruct TacticalClass::ClientToCoords(const Point2D& client) const
{
    CoordStruct result;
    ClientToCoords(&result, client);
    return result;
}

// =============================================================================
// CellToScreen - Convert cell coordinates to screen pixel position
// =============================================================================
CellStruct TacticalClass::CellToScreen(const CellStruct& cell) const
{
    int32 x = (cell.X - cell.Y) * HalfTileWidth;
    int32 y = (cell.X + cell.Y) * HalfTileHeight;
    return CellStruct(static_cast<int16>(x), static_cast<int16>(y));
}

// =============================================================================
// ScreenToCell - Convert screen pixel position to cell coordinates
// =============================================================================
CellStruct TacticalClass::ScreenToCell(const Point2D& screen) const
{
    int32 x = (screen.X / HalfTileWidth + screen.Y / HalfTileHeight) / 2;
    int32 y = (screen.Y / HalfTileHeight - screen.X / HalfTileWidth) / 2;
    return CellStruct(static_cast<int16>(x), static_cast<int16>(y));
}

// =============================================================================
// AdjustForZShapeMove - Adjust screen position for Z-height (shape movement)
//
// asm: 该换算在 RegisterCellAsVisible (asm:1099-1129) 与 DrawWaypointPaths
//   (asm:1519-1523) 里以整数乘加形式展开：
//     screenX = 30 * (X - Y) / 256
//     screenY = 15 * (X + Y) / 256 - AdjustForZ(Z)
//   其中 Z 的层数 = floor(Z * (1/104) + 0.5)（dbl_B0CD48 = 1/104）。
//   本函数返回把世界坐标 (x,y,z) 投影到屏幕后、再按 Z 抬升的像素位置。
// =============================================================================
Point2D TacticalClass::AdjustForZShapeMove(int32 x, int32 y)
{
    // 纯 (x,y) 的等距投影（无 Z）。索引语义同 CoordsToScreen 的世界单位。
    return Point2D((x - y) * HalfTileWidth / 256,
                   (x + y) * HalfTileHeight / 256);
}

// =============================================================================
// AdjustForZ - Convert a lepton altitude to the screen-Y lift in pixels
//
// asm: 0Eh - floor(Height * k + 0.5)，k = 1/104（见 DrawWaypointPaths 里
//   0x2D8(=7*104) 判为 7 层的阈值）。即每 104 leptons 抬升 15 像素，
//   四舍五入；地面 (Height<=0) 不抬升。
// =============================================================================
int32 TacticalClass::AdjustForZ(int32 Height)
{
    if (Height <= 0)
        return 0;
    // 层数 = round(Height / 104)，每层 15 像素。
    const int32 level = (Height + 52) / 104;
    return level * HalfTileHeight;   // HalfTileHeight == 15
}

// =============================================================================
// GetTacticalOrigin - Top-left of the tactical viewport in screen space
//
// asm: rect_mouseclass_vp.T/.L 是全屏视口的左上角；RegisterCellAsVisible 用
//   `screen - origin` 得到视口相对坐标，本函数即该 origin 的读取形态。
// =============================================================================
Point2D TacticalClass::GetTacticalOrigin() const
{
    int32 w = (ScreenWidth  > 0) ? ScreenWidth  : ((TheDisplay != nullptr) ? TheDisplay->ScreenWidth  : 640);
    int32 h = (ScreenHeight > 0) ? ScreenHeight : ((TheDisplay != nullptr) ? TheDisplay->ScreenHeight : 480);
    return Point2D(TacticalPos.X - w / 2, TacticalPos.Y - h / 2);
}

// =============================================================================
// Is_In_Viewport - Check if world coordinates project into the visible viewport
//
// asm: RegisterCellAsVisible 的剔除段 (asm:1187-1200)：
//      screen = CoordsToScreen(coord) - origin
//      if (screen.X < -0x1E) skip ; if (screen.Y < -0x1E) skip
//      if (screen.X > W + 0x1E) skip ; if (screen.Y > H + 0x0F) skip
//   即视口外加 30/15 像素的宽容边。
// =============================================================================
bool TacticalClass::Is_In_Viewport(const CoordStruct& coord) const
{
    int32 w = (ScreenWidth  > 0) ? ScreenWidth  : 640;
    int32 h = (ScreenHeight > 0) ? ScreenHeight : 480;

    Point2D screen = CoordsToScreen(coord);
    Point2D origin = GetTacticalOrigin();
    int32 sx = screen.X - origin.X;
    int32 sy = screen.Y - origin.Y - AdjustForZ(coord.Z);

    return sx >= -(IsometricTileWidth / 2) && sx <= w + (IsometricTileWidth / 2) &&
           sy >= -(IsometricTileHeight / 2) && sy <= h + (IsometricTileHeight / 2);
}

// =============================================================================
// Is_In_Viewport - Check if a rectangle intersects the visible viewport
// =============================================================================
bool TacticalClass::Is_In_Viewport(const Rectangle& rect) const
{
    int32 w = (ScreenWidth  > 0) ? ScreenWidth  : 640;
    int32 h = (ScreenHeight > 0) ? ScreenHeight : 480;
    Point2D origin = GetTacticalOrigin();

    Rectangle view(origin.X, origin.Y, w, h);
    return view.Intersects(rect);
}

// =============================================================================
// GetOcclusion - Get the occlusion value for a cell (shroud/fog state)
//
// asm: TacticalClass_GetCellFlags 的返回值语义（见 CellShroudFlags 表）：
//   2 = 全遮罩（高 2 位权重的遮挡），1 = 战争迷雾，0 = 完全可见。
//   本函数把 MapClass 格子状态映射到同一三态。
// =============================================================================
char TacticalClass::GetOcclusion(const CellStruct& cell, bool fog) const
{
    if (MapClass::Instance == nullptr)
        return 2;

    CellClass* pCell = MapClass::Instance->GetCellAt(cell);
    if (pCell == nullptr)
        return 2;

    if (pCell->IsShrouded())
        return 2;

    if (fog && pCell->IsFogged())
        return 1;

    return 0;
}

// =============================================================================
// ComputeNeighbourMask - 8-direction occlusion mask (asm: TacticalClass_GetCellFlags)
//
// asm: 本函数是 GetCellFlags 的 8 向探查骨架 (asm:678-937)。对中心格 flags
//   取基位后，对 8 个邻居（N,NE,E,SE,S,SW,W,NW）逐一读同一 flag 位；若邻居
//   缺该位，则把对应的方向位置 1。位映射：N=bit6(0x40), NE=0x80, E=bit0(1),
//   SE=bit1(2), S=bit2(4), SW=0x10, W=bit3(8), NW=0x20。
//   altMode 为真时按 asm 第二分支读"占位/可通行"位（掩码 8），否则读"已探明"
//   位（掩码 2，并额外看建筑位，见 asm [eax+140h]）。
// =============================================================================
int32 TacticalClass::ComputeNeighbourMask(const CellStruct& cell, bool altMode) const
{
    if (MapClass::Instance == nullptr)
        return 0;

    int32 mask = 0;

    // 8 邻居偏移（N, NE, E, SE, S, SW, W, NW）与对应的位。
    static const int16 dx[8]   = {  0,  1, 1,  1,  0, -1, -1, -1 };
    static const int16 dy[8]   = { -1, -1, 0,  1,  1,  1,  0, -1 };
    static const int32 bits[8] = { 0x40, 0x80, 0x01, 0x02, 0x04, 0x10, 0x08, 0x20 };

    for (int32 i = 0; i < 8; ++i)
    {
        CellStruct nb;
        nb.X = static_cast<int16>(cell.X + dx[i]);
        nb.Y = static_cast<int16>(cell.Y + dy[i]);

        CellClass* pNeighbour = MapClass::Instance->GetCellAt(nb);
        if (pNeighbour == nullptr)
        {
            mask |= bits[i];
            continue;
        }

        const bool present = altMode ? pNeighbour->IsPassable()
                                     : pNeighbour->IsRevealed();
        if (!present)
            mask |= bits[i];
    }

    return mask;
}

// =============================================================================
// GetCellFlags - Compute the shroud corner frame index for a cell (asm:656)
//
// asm: TacticalClass_GetCellFlags(TacticalClass*, wXY* coords, wXY wat) retn 8。
//   1) cell = Map.Coord_Cell(coords)
//   2) wat.X == 0（迷雾模式）：
//        flags = cell[+12Ch]（AltFlags）；若 (flags & 0x18)==0 → 返回 -2
//        若 (flags & 0x08)==0 → 返回 -2
//        否则按掩码 0x08 做 8 向探查（ComputeNeighbourMask altMode=true）
//   3) wat.X != 0（已探明模式）：
//        flags = cell->Flags(+12Ch)；若 (flags & 3)==0 → 返回 -2
//        若 (flags & 2)==0 → 返回 -2
//        否则按掩码 0x02 做 8 向探查（ComputeNeighbourMask altMode=false），
//        并同样探查建筑位 [cell+140h]
//   4) 返回 CellShroudFlags[mask] —— 一张 256 项的"遮挡角帧"查表。
//   OpenYR 未建模 CellShroudFlags 表（渲染后端资产），这里按 asm 控制流算出
//   mask 后返回其低字节作为等价帧号；被遮挡（-2/-1）情形与 asm 一致。
// =============================================================================
int32 TacticalClass::GetCellFlags(const CellStruct& coords, CellStruct* pWat)
{
    if (MapClass::Instance == nullptr)
        return -1;

    CellClass* pCell = MapClass::Instance->GetCellAt(coords);
    if (pCell == nullptr)
        return -1;

    const bool fogMode = (pWat != nullptr && pWat->X == 0);

    if (fogMode)
    {
        // asm: 迷雾模式读 AltFlags 的 Mapped/NoFog 组合位。
        const bool mapped = pCell->HasAltFlag(AltCellFlags::Mapped);
        const bool noFog  = pCell->HasAltFlag(AltCellFlags::NoFog);
        if (!mapped || !noFog)
            return -2;                          // asm: mov edi, 0FFFFFFFEh

        return ComputeNeighbourMask(coords, true);   // asm: CellShroudFlags[ebp]
    }

    // asm: 已探明模式读 CellFlags 的 CenterRevealed/EdgeRevealed。
    if (!pCell->HasFlag(CellFlags::CenterRevealed) ||
        !pCell->HasFlag(CellFlags::EdgeRevealed))
        return -2;                              // asm: mov edi, 0FFFFFFFEh

    return ComputeNeighbourMask(coords, false);     // asm: CellShroudFlags[ebp]
}

// =============================================================================
// Draw - Main rendering entry point
//
// Draws the tactical map in the following order:
// 1. Overlay layer (terrain tiles, overlays like tiberium)
// 2. Objects (units, buildings, infantry, aircraft)
// 3. Shroud (unexplored areas)
// 4. Fog (explored but not currently visible areas)
// =============================================================================
void TacticalClass::Draw()
{
    if (Redrawing) return;
    Redrawing = true;

    // asm: TacticalClass_Draw (asm:385) 先重建本帧可见集与对象层，
    // 再依次叠加 Overlay / 对象 / 遮罩 / 迷雾 / 网格。
    Visible_Objects();
    YSortObjects();

    Draw_Overlay();
    Draw_Objects();
    Draw_Transient_Objects();
    Draw_Shroud();
    Draw_Fog();
    if (field_AD)               // asm: Redrawing/网格调试开关
        Draw_Grid();
    Draw_Selection_Rect();

    Redrawing = false;
}

// =============================================================================
// Draw_Overlay - Draw terrain tiles and ground overlays
//
// 遍历顺序与原版等距顺序一致：先按 (cx+cy) 升序（= 屏幕 Y 升序，远→近），
// 同一对角线上再按 cx 升序（= 屏幕 X 升序）。对角线内 cx 递增时
// screenX=(cx-cy)*30 也递增，所以"先 y 后 x"两级排序等价于按 sum 分层的
// 两层循环，不需要每帧真的排序。
//
// 每格：
//   1) RegisterCellAsVisible 保留既有登记（VisibleCellCount 供 [RENDER] 用）；
//   2) Tile_Shape_For(TileType) 非空 → CC_Draw_Shape 走 theater 贴图；
//   3) 为空（没有 theater TMP 素材）→ 该格不画。原版同样不会用程序化图形
//      顶替美术资源。
// =============================================================================
void TacticalClass::Draw_Overlay()
{
    Surface* pSrf = Target_Surface();
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (MapClass::Instance == nullptr)
        return;

    // 本帧可见集重建（asm: VisibleCells 计数清零后逐格重登记）。
    VisibleCellCount = 0;

    // 地形图集预热：原版是 IsometricTileTypeClass 的惰性装图（sub_547020），
    // 首次取图时整文件读入并把 cell 偏移表重定位成指针。这里在首帧把所有
    // 瓦片集唤醒一次，之后 TileArt 表里就有句柄了。
    //
    // 没有素材时每个瓦片集只尝试一次（ImageLoadAttempted），不会每帧重试。
    static bool s_tileArtWarmed = false;
    if (!s_tileArtWarmed)
    {
        s_tileArtWarmed = true;
        if (TileTypeClass_Array != nullptr)
        {
            const int32 setCount = TileTypeClass_Array->Count;
            for (int32 i = 0; i < setCount; ++i)
            {
                IsometricTileType* pSet = (*TileTypeClass_Array)[i];
                if (pSet != nullptr)
                    pSet->GetImageData();
            }
        }
    }

    const Point2D origin  = GetTacticalOrigin();
    const CellSpan span   = Viewport_Cell_Span(this, 3);
    const Rectangle srfRect(0, 0, pSrf->GetWidth(), pSrf->GetHeight());

    int32 drawnCells = 0;

    const int32 firstDiag = span.MinX + span.MinY;
    const int32 lastDiag  = span.MaxX + span.MaxY;

    for (int32 diag = firstDiag; diag <= lastDiag; ++diag)
    {
        const int32 cxBegin = std::max(span.MinX, diag - span.MaxY);
        const int32 cxEnd   = std::min(span.MaxX, diag - span.MinY);

        for (int32 cx = cxBegin; cx <= cxEnd; ++cx)
        {
            const int32 cy = diag - cx;
            const CellStruct cellPos(static_cast<int16>(cx), static_cast<int16>(cy));

            const Rectangle box = Tile_Box_Of_Cell(this, origin, cx, cy);
            if (!box.Intersects(srfRect))
                continue;                       // 视口外的菱形不画

            CellClass* pCell = MapClass::Instance->GetCellAt(cellPos);
            if (pCell == nullptr)
                continue;

            // 保留既有登记逻辑：探明/视口判定 + VisibleCells 追加 + 标脏。
            RegisterCellAsVisible(pCell);

            // ---- 地形贴图：theater IsoTile 图集 -> CC_Draw_Shape -------------
            //
            // asm: TacticalClass::Draw_Tiles 逐格取 IsoTileTypeIndex 对应的
            //   图集句柄，以「格心」为锚点、Centered 绘制，帧号是
            //   CellClass::TileSubIndex。
            //
            //   一个 TMP cell 最多 3 段 raster（地面 / 高度侧面 / 斜坡），
            //   在 SHP 视图里编成 cellIndex*3 + rasterIndex 三帧（见
            //   FileFormats/TMP.h）。原版 IsometricTileClass_Draw 也是逐段
            //   各调一次，段宽高为 0 的直接跳过。
            //
            //   没有图集句柄就不画 —— 原版同样不会用程序化图形顶替美术资源。
            SHPStruct* pTileShape = TileArt::Tile_Shape_For(pCell->TileType);
            ConvertClass* pTileConvert = StandardPalette;
            if (pTileShape != nullptr && pTileConvert != nullptr)
            {
                // Centered 减的是 **cell 画布** 的一半（不是单帧尺寸）；
                // 画布高含向上溢出的坡/悬崖部分，锚点必须按它算。
                const int32 canvasH = (pTileShape->Height > 0)
                                    ? pTileShape->Height
                                    : TileArt::IsoTileHeight;
                Rectangle clip(srfRect.X, srfRect.Y, srfRect.Width, srfRect.Height);

                const int32 firstFrame = pCell->TileSubIndex * 3;
                for (int32 r = 0; r < 3; ++r)
                {
                    const int32 frameIdx = firstFrame + r;
                    if (frameIdx < 0 || frameIdx >= pTileShape->Frames)
                        break;

                    // 空段（宽或高为 0）不产生任何像素，原版同样跳过。
                    int32 frameW = 0;
                    int32 frameH = 0;
                    if (pTileShape->GetFrameSize(frameIdx, frameW, frameH) != 0
                        || frameW <= 0 || frameH <= 0)
                    {
                        continue;
                    }

                    Point2D pos(box.X + TileArt::IsoHalfWidth,
                                box.Y + canvasH / 2);

                    CC_Draw_Shape(pSrf, pTileConvert, pTileShape,
                                  frameIdx, &pos, &clip,
                                  BlitterFlags::Centered | BlitterFlags::Transparent,
                                  0, 0, 0, 1000, 0,
                                  nullptr, 0, 0, 0);
                }
            }

            // ---- 地表覆盖物：矿 / 墙 / 树桩 / 箱子 -------------------------
            //
            // asm: CellClass_Draw_It 画完地形段后，若 Overlay != -1 就调
            //   OverlayTypeClass_Draw(Convert_TEMPERAT, frameIdx, TempSurface,
            //                        ..., coords, ...)：
            //     pos.X = coords->X + 0x1E
            //     pos.Y = coords->Y + 0x0F
            //   其中 coords 是格子的**画布左上角**（60x30 包围盒左上），
            //   加上 (30,15) 正好落到菱形中心。
            //
            //   帧号取 CellClass::OverlayFrame(+0x11E)，不是 Overlay 索引：
            //   后者是“第几种覆盖物”，前者是矿的生长阶段 / 墙的朝向帧。
            //   （asm CellClass_Cell_Color_2 也把 +0x11E 当 frame 传给矿。）
            if (pCell->Overlay >= 0 && OverlayTypeClass::Array != nullptr
                && pCell->Overlay < OverlayTypeClass::Array->Count)
            {
                OverlayTypeClass* pOverlayType =
                    OverlayTypeClass::Array->GetItem(pCell->Overlay);
                if (pOverlayType != nullptr)
                {
                    // 覆盖物跟着地面走：asm 的 CellClass_Update_Cell 先调
                    // sub_480110 拿到净偏移（覆盖物类型自身的 -0xC/-0xD、
                    // 桥面 -0x10、以及 Level*15 的高度项），再把结果交给
                    // OverlayTypeClass_Draw。这里补上同一项 —— 之前完全没加，
                    // 坡/悬崖上的矿和墙会贴在基准平面上。
                    const Point2D cellOffset = pCell->Get_Overlay_Draw_Offset();
                    Point2D overlayPos(box.X + cellOffset.X, box.Y + cellOffset.Y);
                    pOverlayType->Draw(&overlayPos, pSrf, pCell->OverlayFrame);
                }
            }

            ++drawnCells;
        }
    }

    if (g_bDebugMode)
    {
        // px 是菱形面积估算：60*30/2 = 900 像素/格。
        const Point2D dbgOrigin = GetTacticalOrigin();
        std::fprintf(stderr,
                     "[TILE] cells=%d px=%d srf=%dx%d origin=(%d,%d) "
                     "tacpos=(%d,%d) map=%dx%d\n",
                     static_cast<int>(drawnCells),
                     static_cast<int>(drawnCells * 900),
                     static_cast<int>(pSrf->GetWidth()),
                     static_cast<int>(pSrf->GetHeight()),
                     static_cast<int>(dbgOrigin.X), static_cast<int>(dbgOrigin.Y),
                     static_cast<int>(TacticalPos.X), static_cast<int>(TacticalPos.Y),
                     MapClass::Instance ? (int)MapClass::Instance->MapWidth : -1,
                     MapClass::Instance ? (int)MapClass::Instance->MapHeight : -1);
    }
}

// =============================================================================
// Draw_Objects - Draw all game objects in Y-sorted order
// =============================================================================
// =============================================================================
// Draw_Ground_Decals - 弹坑 / 焦痕（SmudgeClass）
//
// asm: TacticalClass::Draw 在对象层之前先走 vec_Smudge，把每个 smudge 的
//   Type->Image 用固定帧贴在地面层。Smudge 是"地面贴花"，必须画在单位之前，
//   否则履带/步兵会被弹坑盖住。
//
// 之前这层完全没有调用点：SmudgeClass::Draw_It 写好了但没人调，爆炸后
// 地上永远不会有痕迹。
// =============================================================================
void TacticalClass::Draw_Ground_Decals()
{
    if (SmudgeClass::Array == nullptr)
        return;

    const Point2D origin = GetTacticalOrigin();

    for (int32 i = 0; i < SmudgeClass::Array->Count; ++i)
    {
        SmudgeClass* pSmudge = (*SmudgeClass::Array)[i];
        if (pSmudge == nullptr || pSmudge->IsInLimbo)
            continue;

        CoordStruct crd;
        pSmudge->GetCoords(&crd);
        if (!Is_In_Viewport(crd))
            continue;

        Point2D screen = CoordsToScreen(crd);
        screen.X -= origin.X;
        screen.Y -= origin.Y;

        pSmudge->Draw_It(screen.X, screen.Y);
    }
}

void TacticalClass::Draw_Objects()
{
    // Objects are drawn after terrain and before shroud.
    // The YSort ensures that objects closer to the bottom of the screen
    // (higher Y in world space) are drawn on top of objects behind them.

    YSortObjects();

    // 地面贴花（弹坑/焦痕）先于对象，否则会盖住单位。
    Draw_Ground_Decals();

    // View rectangle used by the object draw routines (original passes the
    // tactical viewport).  The full view is used until the viewport logic
    // is wired to the sidebar/radar insets.
    RectangleStruct viewRect(0, 0, (TheDisplay != nullptr) ? TheDisplay->ScreenWidth  : 640,
                                   (TheDisplay != nullptr) ? TheDisplay->ScreenHeight : 480);

    // Draw each visible object, dispatched by its concrete type.  The
    // original draws via ObjectClass virtuals; here we use the concrete
    // Draw(Point2D&, RectangleStruct&) entry points that exist on the
    // Techno hierarchy, converting the object's world coords to screen.
    DynamicVectorClass<ObjectClass*> visibleObjects = GetVisibleObjects();

    // 冒烟诊断：可见对象数（显示层里未 limbo 的对象）与真正派发绘制的数量。
    if (g_bDebugMode)
        g_RenderDebugStats.VisibleObjects = visibleObjects.Count;

    int32 drawnCount  = 0;   // 真正调用到具体 Draw() 的对象数
    int32 culledCount = 0;   // limbo / 视口外 / 未知类型而未派发的对象数

    for (int32 i = 0; i < visibleObjects.Count; ++i) {
        ObjectClass* pObj = visibleObjects[i];
        if (!pObj) continue;
        if (pObj->IsInLimbo) { ++culledCount; continue; }

        CoordStruct crd;
        pObj->GetCoords(&crd);

        // 视口剔除：视口外的对象不派发（asm: Draw 前的 Is_In_Viewport 判定）。
        if (!Is_In_Viewport(crd)) { ++culledCount; continue; }

        // 世界投影后要减视口原点才是屏幕坐标 —— 与 Is_In_Viewport 的
        // `screen = CoordsToScreen(coord) - origin` 必须是同一个换算，否则
        // 剔除用了一套坐标、绘制用另一套：相机一移到基地（TacticalPos 非零），
        // 被判定"在视口内"的对象会被画到屏幕外，画面只剩地形。
        const Point2D origin = GetTacticalOrigin();
        Point2D screen = CoordsToScreen(crd);
        screen.X -= origin.X;
        screen.Y -= origin.Y;

        // Dispatch to the concrete draw routine.  BuildingClass::Draw takes
        // const refs, the Techno hierarchy takes non-const refs.
        switch (pObj->WhatAmI()) {
            case AbstractType::Unit:
                static_cast<UnitClass*>(pObj)->Draw(screen, viewRect);
                break;
            case AbstractType::Infantry:
                static_cast<InfantryClass*>(pObj)->Draw(screen, viewRect);
                break;
            case AbstractType::Aircraft:
                static_cast<AircraftClass*>(pObj)->Draw(screen, viewRect);
                break;
            case AbstractType::Building:
                static_cast<BuildingClass*>(pObj)->Draw(screen, viewRect);
                break;
            case AbstractType::Terrain:
                // 树木 / 岩石是独立地形对象，不是 overlay。它们不在
                // TechnoClass 分支里，以前整个 switch 根本没有这一条，
                // 所以 TerrainClass::Draw 永远没被调用过。
                static_cast<TerrainClass*>(pObj)->Draw(&screen, Target_Surface());
                break;
            default:
                ++culledCount;
                continue;
        }

        // 到这里说明具体类的 Draw() 已被调用且正常返回——这才是"真画了"，
        // 与 VisibleObjects（登记）区分开。drawn 与 drawcalls 同步递增。
        ++drawnCount;
    }

    if (g_bDebugMode)
    {
        g_RenderDebugStats.DrawnObjects  = drawnCount;
        g_RenderDebugStats.DrawCalls     = drawnCount;
        g_RenderDebugStats.CulledObjects = culledCount;
        g_RenderDebugStats.VisibleCells  = VisibleCellCount;
    }
}

// =============================================================================
// Draw_Transient_Objects - 动画 / 抛射体 / 体素动画 / 粒子
//
// asm: TacticalClass::Draw 在对象层之后依次走 vec_Anim、vec_Bullet、
//   vec_VoxelAnim、vec_ParticleSystem。这些实例不属于 ObjectClass 的可见
//   对象表（GetVisibleObjects 只收集 Techno），所以对象派发循环永远看不到
//   它们——爆炸、炮口闪光、导弹、碎片此前一个都画不出来。
//
// 坐标换算与对象层完全一致（CoordsToScreen - origin），否则相机移开后
// 特效会整体偏移。
// =============================================================================
void TacticalClass::Draw_Transient_Objects()
{
    Surface* pSrf = Target_Surface();
    if (pSrf == nullptr)
        return;

    const Point2D origin = GetTacticalOrigin();
    const Rectangle clip(0, 0, pSrf->GetWidth(), pSrf->GetHeight());
    RectangleStruct viewRect(0, 0, pSrf->GetWidth(), pSrf->GetHeight());

    // ---- 动画（爆炸、炮口闪光、弹道烟） --------------------------------
    if (AnimClass::Array != nullptr)
    {
        for (int32 i = 0; i < AnimClass::Array->Count; ++i)
        {
            AnimClass* pAnim = (*AnimClass::Array)[i];
            if (pAnim == nullptr || pAnim->IsInLimbo)
                continue;

            CoordStruct crd;
            pAnim->GetCoords(&crd);
            if (!Is_In_Viewport(crd))
                continue;

            Point2D screen = CoordsToScreen(crd);
            screen.X -= origin.X;
            screen.Y -= origin.Y;

            pAnim->Draw(&screen, &clip);
        }
    }

    // ---- 抛射体（子弹 / 导弹 / 炮弹） ---------------------------------
    if (BulletClass::Array != nullptr)
    {
        for (int32 i = 0; i < BulletClass::Array->Count; ++i)
        {
            BulletClass* pBullet = (*BulletClass::Array)[i];
            if (pBullet == nullptr)
                continue;

            CoordStruct crd;
            pBullet->GetCoords(&crd);
            if (!Is_In_Viewport(crd))
                continue;

            Point2D screen = CoordsToScreen(crd);
            screen.X -= origin.X;
            screen.Y -= origin.Y;

            pBullet->Draw(&screen, &viewRect);
        }
    }

    // ---- 体素动画（碎片、残骸） ---------------------------------------
    if (VoxelAnimClass::Array != nullptr)
    {
        for (int32 i = 0; i < VoxelAnimClass::Array->Count; ++i)
        {
            VoxelAnimClass* pVoxAnim = (*VoxelAnimClass::Array)[i];
            if (pVoxAnim == nullptr || pVoxAnim->IsInLimbo)
                continue;

            CoordStruct crd;
            pVoxAnim->GetCoords(&crd);
            if (!Is_In_Viewport(crd))
                continue;

            Point2D screen = CoordsToScreen(crd);
            screen.X -= origin.X;
            screen.Y -= origin.Y;

            pVoxAnim->Draw_It(screen.X, screen.Y);
        }
    }

    // ---- 粒子系统（火花、蒸汽、烟柱） ---------------------------------
    if (ParticleSystemClass::Array != nullptr)
    {
        for (int32 i = 0; i < ParticleSystemClass::Array->Count; ++i)
        {
            ParticleSystemClass* pSystem = (*ParticleSystemClass::Array)[i];
            if (pSystem == nullptr)
                continue;

            CoordStruct crd;
            pSystem->GetCoords(&crd);
            if (!Is_In_Viewport(crd))
                continue;

            Point2D screen = CoordsToScreen(crd);
            screen.X -= origin.X;
            screen.Y -= origin.Y;

            pSystem->Draw(screen.X, screen.Y);
        }
    }

    // ---- 单个粒子（火花 / 烟 / 火 / 气体） -----------------------------
    //
    // asm: ParticleClass 是独立实例，绘制入口 ParticleClass_Draw(0x62CEF0)
    //   由战术层按实例表轮询，而不是由 ParticleSystemClass_Draw 转发
    //   （后者只负责 OneFrameLight 闪光）。原版粒子层在对象层之后、
    //   动画层附近，这里与粒子系统同批绘制。
    if (ParticleClass::Array != nullptr)
    {
        for (int32 i = 0; i < ParticleClass::Array->Count; ++i)
        {
            ParticleClass* pParticle = (*ParticleClass::Array)[i];
            if (pParticle == nullptr || pParticle->IsDead())
                continue;

            CoordStruct crd;
            pParticle->GetCoords(&crd);
            if (!Is_In_Viewport(crd))
                continue;

            Point2D screen = CoordsToScreen(crd);
            screen.X -= origin.X;
            screen.Y -= origin.Y;

            pParticle->Draw(&screen, &clip);
        }
    }
}

// =============================================================================
// Draw_Shroud - Draw the shroud (unexplored areas) overlay
//
// 对 GetOcclusion(cell, false) >= 2 的格子（IsShrouded）在它的等距菱形上
// 填纯黑。逐行走 TileArt::DrawIsoDiamond，因此不是矩形近似 —— 遮罩边界与
// 地块边界严格重合，不会盖到邻格。
//
// 遍历范围与地形层同一套视口格子范围，而不是只走 VisibleCells：全遮罩的格
// 子在 RegisterCellAsVisible 里就被挡掉了（未探明），只按登记表画会漏掉，
// 未探明区域会露出地形。
// =============================================================================
void TacticalClass::Draw_Shroud()
{
    Surface* pSrf = Target_Surface();
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (MapClass::Instance == nullptr)
        return;

    const Point2D origin = GetTacticalOrigin();
    const CellSpan span  = Viewport_Cell_Span(this, 2);
    const Rectangle srfRect(0, 0, pSrf->GetWidth(), pSrf->GetHeight());

    int32 shroudedCells = 0;

    for (int32 diag = span.MinX + span.MinY; diag <= span.MaxX + span.MaxY; ++diag)
    {
        const int32 cxBegin = std::max(span.MinX, diag - span.MaxY);
        const int32 cxEnd   = std::min(span.MaxX, diag - span.MinY);

        for (int32 cx = cxBegin; cx <= cxEnd; ++cx)
        {
            const int32 cy = diag - cx;
            const Rectangle box = Tile_Box_Of_Cell(this, origin, cx, cy);
            if (!box.Intersects(srfRect))
                continue;

            const CellStruct cellPos(static_cast<int16>(cx), static_cast<int16>(cy));
            if (GetOcclusion(cellPos, false) < 2)
                continue;

            TileArt::DrawIsoDiamond(pSrf, box.X, box.Y, 0x0000);   // 纯黑
            ++shroudedCells;
        }
    }

    if (g_bDebugMode)
        g_RenderDebugStats.ShroudedCells = shroudedCells;
}

// =============================================================================
// Draw_Fog - Draw the fog of war (explored but not currently visible)
//
// 对 GetOcclusion(cell, true) == 1 的格子（已探明但当前不可见）把菱形内的
// 像素减半：dst = (dst >> 1) & 0x7BEF。地形仍然看得见，只是压暗 50%。
// =============================================================================
void TacticalClass::Draw_Fog()
{
    Surface* pSrf = Target_Surface();
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (MapClass::Instance == nullptr)
        return;

    const Point2D origin = GetTacticalOrigin();
    const CellSpan span  = Viewport_Cell_Span(this, 2);
    const Rectangle srfRect(0, 0, pSrf->GetWidth(), pSrf->GetHeight());

    for (int32 diag = span.MinX + span.MinY; diag <= span.MaxX + span.MaxY; ++diag)
    {
        const int32 cxBegin = std::max(span.MinX, diag - span.MaxY);
        const int32 cxEnd   = std::min(span.MaxX, diag - span.MinY);

        for (int32 cx = cxBegin; cx <= cxEnd; ++cx)
        {
            const int32 cy = diag - cx;
            const Rectangle box = Tile_Box_Of_Cell(this, origin, cx, cy);
            if (!box.Intersects(srfRect))
                continue;

            const CellStruct cellPos(static_cast<int16>(cx), static_cast<int16>(cy));
            if (GetOcclusion(cellPos, true) != 1)
                continue;

            Blend_Diamond_Half(pSrf, box.X, box.Y);
        }
    }
}

// =============================================================================
// Draw_Grid - Draw the isometric grid overlay (for debug/editor mode)
//
// 每格画一圈 1px 菱形轮廓（TileArt::DrawIsoTileOutline 逐行左右端点）。
// 相邻两格的公共边被两边各画一次，落点与颜色都相同，不会加粗。
// =============================================================================
void TacticalClass::Draw_Grid()
{
    Surface* pSrf = Target_Surface();
    if (pSrf == nullptr || pSrf->Buffer == nullptr)
        return;
    if (MapClass::Instance == nullptr)
        return;

    const Point2D origin = GetTacticalOrigin();
    const CellSpan span  = Viewport_Cell_Span(this, 2);
    const Rectangle srfRect(0, 0, pSrf->GetWidth(), pSrf->GetHeight());

    // 50% 灰：在草地/沙地上都看得清，又不会盖住地形颜色。
    const uint16 gridColor = 0x8410;

    for (int32 diag = span.MinX + span.MinY; diag <= span.MaxX + span.MaxY; ++diag)
    {
        const int32 cxBegin = std::max(span.MinX, diag - span.MaxY);
        const int32 cxEnd   = std::min(span.MaxX, diag - span.MinY);

        for (int32 cx = cxBegin; cx <= cxEnd; ++cx)
        {
            const int32 cy = diag - cx;
            const Rectangle box = Tile_Box_Of_Cell(this, origin, cx, cy);
            if (!box.Intersects(srfRect))
                continue;

            TileArt::DrawIsoTileOutline(pSrf, box.X, box.Y, gridColor);
        }
    }
}

// =============================================================================
// Draw_Waypoints - Draw waypoint markers (for editor/debug mode)
// =============================================================================
void TacticalClass::Draw_Waypoints()
{
    // Draw waypoint markers at their map positions. Each waypoint is
    // drawn as a numbered icon on the isometric surface. This is
    // primarily used in the map editor and for debug visualization.

    if (!MapClass::Instance) return;

    // Waypoint marker dimensions in screen pixels.
    const int32 MARKER_SIZE  = 16;
    const int32 MARKER_HALF  = MARKER_SIZE / 2;

    // Pulse the marker visibility based on the frame counter so waypoints
    // are visually distinct from regular game objects.
    int32 frame = Game::CurrentFrame;
    bool  pulse = ((frame / 8) & 1) != 0;

    // Determine viewport offset for converting world-screen coordinates
    // (output of CoordsToScreen) to viewport-relative coordinates used by
    // the dirty-area tracker.
    int32 viewW = (DisplayClass::Instance) ? DisplayClass::Instance->ScreenWidth  : 800;
    int32 viewH = (DisplayClass::Instance) ? DisplayClass::Instance->ScreenHeight : 600;
    int32 offsetX = TacticalPos.X - viewW / 2;
    int32 offsetY = TacticalPos.Y - viewH / 2;

    for (int32 i = 0; i < MapClass::Instance->MaxWaypoints; ++i)
    {
        const CoordStruct& wpCoord = MapClass::Instance->Waypoints[i];

        // Skip unset waypoints (a coordinate of (0, 0) indicates an
        // unused waypoint slot).
        if (wpCoord.X == 0 && wpCoord.Y == 0) continue;

        // Skip waypoints outside the current viewport.
        if (!Is_In_Viewport(wpCoord)) continue;

        // Convert the waypoint world coordinate to screen position and
        // then to viewport-relative coordinates for dirty tracking.
        Point2D screenPos = CoordsToScreen(wpCoord);
        int32 viewX = screenPos.X - offsetX;
        int32 viewY = screenPos.Y - offsetY;

        // Register a dirty area around the marker so the renderer
        // repaints this region on the next pass.
        Rectangle markerRect(
            viewX - MARKER_HALF,
            viewY - MARKER_HALF,
            MARKER_SIZE,
            MARKER_SIZE
        );
        RegisterDirtyArea(markerRect, false);

        // The actual waypoint marker (a numbered diamond icon drawn from
        // the waypoint SHP asset) is blitted by the display class. The
        // pulse flag alternates the icon frame every 8 ticks to make the
        // marker visually stand out in the editor.
        (void)pulse;
    }
}

// =============================================================================
// Draw_Placement_Grid - Draw the building placement grid
// =============================================================================
void TacticalClass::Draw_Placement_Grid()
{
    // When the player is placing a building, a footprint grid is drawn
    // to show where the building will be constructed. The grid turns
    // green if the placement is valid or red if it is invalid.

    if (!DisplayClass::Instance) return;
    if (!MapClass::Instance) return;

    // Only draw the placement grid when a building type is selected
    // for placement.
    if (!DisplayClass::Instance->CurrentBuildingType) return;

    CellStruct* pFoundation  = DisplayClass::Instance->CurrentFoundation_Data;
    CellStruct  centerCell   = DisplayClass::Instance->CurrentFoundation_CenterCell;

    // Determine viewport offset for converting world-screen coordinates
    // to viewport-relative coordinates used by the dirty-area tracker.
    int32 viewW = DisplayClass::Instance->ScreenWidth;
    int32 viewH = DisplayClass::Instance->ScreenHeight;
    int32 offsetX = TacticalPos.X - viewW / 2;
    int32 offsetY = TacticalPos.Y - viewH / 2;

    bool allValid = true;

    if (pFoundation)
    {
        // Iterate through the foundation offset list. Each entry is a
        // (dx, dy) offset relative to the center cell. The list is
        // terminated by the sentinel (0x7FFF, 0x7FFF).
        const CellStruct* pIter = pFoundation;
        while (pIter->X != 0x7FFF || pIter->Y != 0x7FFF)
        {
            CellStruct cellPos;
            cellPos.X = static_cast<int16>(centerCell.X + pIter->X);
            cellPos.Y = static_cast<int16>(centerCell.Y + pIter->Y);
            ++pIter;

            // Check map bounds.
            bool inBounds = (cellPos.X >= 0 && cellPos.Y >= 0 &&
                             cellPos.X < MapClass::Instance->MapWidth &&
                             cellPos.Y < MapClass::Instance->MapHeight);

            bool cellValid = false;
            if (inBounds)
            {
                cellValid = MapClass::Instance->Is_Placement_Allowed(cellPos);
            }
            if (!cellValid) allValid = false;

            // Convert the cell to screen position and register a dirty
            // area so the renderer repaints this grid tile.
            CoordStruct cellCoord = CellClass::Cell2Coord(cellPos);
            if (Is_In_Viewport(cellCoord))
            {
                Point2D screenPos = CoordsToScreen(cellCoord);
                int32 viewX = screenPos.X - offsetX;
                int32 viewY = screenPos.Y - offsetY;

                Rectangle cellRect(
                    viewX - HalfTileWidth,
                    viewY - HalfTileHeight,
                    IsometricTileWidth,
                    IsometricTileHeight
                );
                RegisterDirtyArea(cellRect, false);
            }
        }
    }
    else
    {
        // No foundation data: use a single-cell footprint at the center.
        bool inBounds = (centerCell.X >= 0 && centerCell.Y >= 0 &&
                         centerCell.X < MapClass::Instance->MapWidth &&
                         centerCell.Y < MapClass::Instance->MapHeight);

        allValid = inBounds && MapClass::Instance->Is_Placement_Allowed(centerCell);

        CoordStruct cellCoord = CellClass::Cell2Coord(centerCell);
        if (Is_In_Viewport(cellCoord))
        {
            Point2D screenPos = CoordsToScreen(cellCoord);
            int32 viewX = screenPos.X - offsetX;
            int32 viewY = screenPos.Y - offsetY;

            Rectangle cellRect(
                viewX - HalfTileWidth,
                viewY - HalfTileHeight,
                IsometricTileWidth,
                IsometricTileHeight
            );
            RegisterDirtyArea(cellRect, false);
        }
    }

    // The overall validity (allValid) determines the grid outline colour:
    // green when every footprint cell is buildable, red when one or more
    // cells are blocked. The display class selects the corresponding grid
    // tile SHP frame based on this state when blitting the footprint.
    (void)allValid;
}

// =============================================================================
// Render - Render the tactical map to a surface
// =============================================================================
// =============================================================================
// Target_Surface - the surface this frame's tactical layers draw into
//
//   asm 的绘制子程序一律自己取面（`mov ecx, DSurface_Composite`），不靠
//   参数传递。本树把"本帧目标面"集中到 DrawSurface：优先用 Render 传入的
//   那张，其次退回全局 Composite，最后退回 GScreen 的后备缓冲。三者都空
//   时返回 nullptr —— 调用侧必须判空，不能假设一定有面。
// =============================================================================
Surface* TacticalClass::Target_Surface()
{
    if (DrawSurface != nullptr && DrawSurface->Buffer != nullptr)
        return DrawSurface;

    if (DSurface_Composite != nullptr && DSurface_Composite->Buffer != nullptr)
    {
        DrawSurface = DSurface_Composite;
        return DrawSurface;
    }

    if (TheGScreen != nullptr)
    {
        Surface* pBack = TheGScreen->Get_Back_Buffer();
        if (pBack != nullptr && pBack->Buffer != nullptr)
        {
            DrawSurface = pBack;
            return DrawSurface;
        }
    }

    return nullptr;
}

void TacticalClass::Render(DSurface* pSurface, bool flag, TacticalRenderMode eMode)
{
    if (!pSurface || !pSurface->Buffer) return;

    // asm: TacticalClass_Draw 的绘制目标就是传进来的那张面（原版另有
    //   `mov ecx, DSurface_Composite` 的取面路径，两者是同一张）。这里把面
    //   记到 DrawSurface，各 Draw_* 一律通过 Target_Surface() 取面 —— 否则
    //   整条绘制链会画到空指针上，画面永远是黑的。
    DrawSurface = pSurface;

    // ---- z 缓冲：每帧清一次 + 挂到本帧的目标面上 --------------------------
    //
    // asm: z 缓冲的清零在 sub_6D2B60 里按脏区做（rect_mouseclass_vp 与每个
    //   脏区的交，逐块 sub_7BCFB0(zbuf, rect) 填 0FFFFh）。本树 Draw() 是全量
    //   重绘，脏区向量也没有建模，所以整片清一次等价。
    //   用帧号做门：Render 一帧会被调多次（地形/对象/特效三趟），只有第一趟
    //   清，后面的趟次必须看到前面写下的 z。
    //    ⚠ 默认关闭：原版的清零是**按脏区**做的（sub_6D2B60 里 vec_DirtyAreas
    //   与 rect_mouseclass_vp 逐个求交后 sub_7BCFB0），本树没有脏区向量，
    //   用"每帧整片清一次"近似会让 z 在多次 Render 之间残留，实时模式下
    //   实测 nonblack 从 246399 掉到 188862 且随帧率漂移 —— 这是清屏模型
    //   不对，不是提笔器的错。在把脏区向量建起来之前，这条路径只能 opt-in：
    //     OPENYR_ZBUFFER=1 才挂 z 缓冲；默认不挂，提笔器按"无深度测试"走。
    static const bool s_bUseZBuffer = []() {
        const char* e = std::getenv("OPENYR_ZBUFFER");
        return e != nullptr && e[0] == '1' && e[1] == '\0';
    }();

    if (s_bUseZBuffer)
    {
        if (ZBuffer_Global == nullptr && ScreenWidth > 0 && ScreenHeight > 0)
            ZBuffer_Global = new ZBufferClass(Rectangle(0, 0, ScreenWidth, ScreenHeight));

        if (ZBuffer_Global != nullptr && ZBuffer_Global->Is_Allocated())
        {
            if (ZBufferFrame != Game::CurrentFrame)
            {
                ZBuffer_Global->Clear();      // asm: 填 0FFFFh（无限远）
                ZBufferFrame = Game::CurrentFrame;
            }
            ZBuffer_Global->Bind(pSurface);   // 供 CC_Draw_Shape 按行取 zbuf
        }
    }

    // The render mode controls what gets drawn:
    // All: Draw everything (terrain, objects, shroud, fog)
    // Terrain: Draw only terrain tiles
    // MovingAnimating: Draw only moving/animating objects
    // AllAlt: Alternative full render path
    // StopDrawing: Skip rendering entirely

    switch (eMode) {
        case TacticalRenderMode::All:
            Draw();
            break;
        case TacticalRenderMode::Terrain:
            Draw_Overlay();
            break;
        case TacticalRenderMode::MovingAnimating:
            Draw_Objects();
            break;
        case TacticalRenderMode::AllAlt:
            Draw_Overlay();
            Draw_Objects();
            break;
        case TacticalRenderMode::StopDrawing:
            break;
        default:
            Draw();
            break;
    }
}

// =============================================================================
// FocusOn - Smoothly scroll the view to center on a position
// =============================================================================
void TacticalClass::FocusOn(CoordStruct* pDest, int32 Velocity)
{
    if (!pDest) return;
    LastTacticalPos = TacticalPos;

    // If Velocity is 0, snap immediately to the destination.
    if (Velocity <= 0) {
        TacticalPos = Point2D(pDest->X, pDest->Y);
        return;
    }

    // Otherwise, interpolate toward the destination over multiple frames.
    int32 dx = pDest->X - TacticalPos.X;
    int32 dy = pDest->Y - TacticalPos.Y;

    // Move toward the destination by the velocity amount.
    if (dx > 0) {
        TacticalPos.X += std::min(dx, Velocity);
    } else if (dx < 0) {
        TacticalPos.X += std::max(dx, -Velocity);
    }

    if (dy > 0) {
        TacticalPos.Y += std::min(dy, Velocity);
    } else if (dy < 0) {
        TacticalPos.Y += std::max(dy, -Velocity);
    }
}

// =============================================================================
// YSortObjects - Sort visible objects by Y position for correct draw order
// =============================================================================
void TacticalClass::YSortObjects()
{
    // Objects in the isometric view must be drawn from back to front
    // (lower Y first, higher Y last) to ensure correct overlap.
    // This function sorts the VisibleCells array in-place by the world
    // Y coordinate of each cell's occupier so that GetVisibleObjects()
    // returns objects in correct render order.
    //
    // An insertion sort is used because the visible object count is
    // typically small (< 100) and the data is nearly sorted between
    // consecutive frames (objects rarely move far in one tick).

    if (VisibleCellCount <= 1) return;

    for (int32 i = 1; i < VisibleCellCount; ++i)
    {
        CellClass* pKey = VisibleCells[i];
        if (!pKey) continue;

        // Determine the sort key: use the occupier's world Y if the cell
        // has a live occupier; otherwise fall back to the cell's own Y
        // coordinate so empty cells sort predictably among themselves.
        CoordStruct keyCoord;
        ObjectClass* pKeyObj = pKey->Occupier;
        if (pKeyObj && !pKeyObj->IsInLimbo)
        {
            pKeyObj->GetCoords(&keyCoord);
        }
        else
        {
            keyCoord = CellClass::Cell2Coord(pKey->MapCoords);
        }
        int32 keyY = keyCoord.Y;

        int32 j = i - 1;
        while (j >= 0)
        {
            CellClass* pCell = VisibleCells[j];
            if (!pCell)
            {
                // Shift null entries to the right.
                VisibleCells[j + 1] = VisibleCells[j];
                --j;
                continue;
            }

            CoordStruct cellCoord;
            ObjectClass* pCellObj = pCell->Occupier;
            if (pCellObj && !pCellObj->IsInLimbo)
            {
                pCellObj->GetCoords(&cellCoord);
            }
            else
            {
                cellCoord = CellClass::Cell2Coord(pCell->MapCoords);
            }

            if (cellCoord.Y <= keyY) break;

            VisibleCells[j + 1] = VisibleCells[j];
            --j;
        }
        VisibleCells[j + 1] = pKey;
    }
}

// =============================================================================
// Visible_Objects - Rebuild the frame's visible-cell list (asm:1060 driver)
//
// asm: 每帧从地图格迭代器（MapClass::CellIterator_Reset/NextCell）走过整张
//   图，对每格调用 RegisterCellAsVisible 做探明/视口判定与登记。OpenYR 用
//   MapClass 的迭代器还原同一过程，并把可点选对象登记进 Selectables。
// =============================================================================
void TacticalClass::Visible_Objects()
{
    // 清空上一帧的可见格表与可点选表。
    VisibleCellCount = 0;
    ClearSelectables();

    if (MapClass::Instance == nullptr)
        return;

    // asm: Map.CellIterator_Reset() ; while ((cell = NextCell())) Register(...)
    //
    // 原版迭代器按原版 CellClass 的固定 4 字节步长直接走字节指针；本树
    //   CellClass 是重建结构体，步长对不上（0x7FC/`index*4` 会越界），
    //   所以这里改用 CellArray 索引顺序遍历同一批格子。visited 集合与原版
    //   一致，只有登记顺序不同，不影响可见格/可选集中的内容。
    for (int32 i = 0; i < MapClass::Instance->CellCount; ++i)
    {
        CellClass* pCell = &MapClass::Instance->CellArray[i];
        if (pCell == nullptr)
            continue;

        RegisterCellAsVisible(pCell);

        // 该格上的 Occupier 若是 Infantry，则登记为一个可点选锚点。
        ObjectClass* pObj = pCell->Get_Occupier();
        if (pObj != nullptr && !pObj->IsInLimbo &&
            pObj->WhatAmI() == AbstractType::Infantry)
        {
            CoordStruct crd;
            pObj->GetCoords(&crd);
            // 与 Draw_Objects 同一换算：世界投影 - 视口原点 = 屏幕坐标，
            // 否则命中测试的锚点会与画面上的对象错开一个视口偏移。
            const Point2D origin = GetTacticalOrigin();
            Point2D screen = CoordsToScreen(crd);
            screen.X -= origin.X;
            screen.Y -= origin.Y - AdjustForZ(crd.Z);
            AddSelectable(static_cast<TechnoClass*>(pObj), screen.X, screen.Y);
        }
    }
}

// =============================================================================
// GetVisibleObjects - Collect the objects occupying the visible cells
//
// asm: 可见对象来自 f 每帧登记的 VisibleCells；对每格取其 Occupier 与其
//   格内对象链，去重后按 YSort 顺序返回。
// =============================================================================
DynamicVectorClass<ObjectClass*> TacticalClass::GetVisibleObjects()
{
    DynamicVectorClass<ObjectClass*> result;

    // asm: 对象层取自 vec_ObjectsInLayers（DisplayClass::Submit 建立的分层表），
    //   其登记动作同时把对象放进 vec_VisibleObjects。OpenYR 的 DisplayClass
    //   把三张层表收在翻译单元内部，对外只暴露 vec_VisibleObjects，故这里
    //   以该表为对象层来源。没有登记过的对象（未走 Submit）不在表内，
    //   与"没进显示层就不画"的原版语义一致。
    if (ObjectClass::VisibleObjects != nullptr)
    {
        for (int32 i = 0; i < ObjectClass::VisibleObjects->Count; ++i)
        {
            ObjectClass* pObj = ObjectClass::VisibleObjects->Items[i];
            if (pObj == nullptr || pObj->IsInLimbo)
                continue;

            bool seen = false;
            for (int32 j = 0; j < result.Count; ++j)
            {
                if (result[j] == pObj) { seen = true; break; }
            }
            if (!seen)
                result.Add(pObj);
        }
    }
    else
    {
        // vec_VisibleObjects 未建立时的兜底：沿用格子占用者枚举。
        for (int32 i = 0; i < VisibleCellCount; ++i)
        {
            CellClass* pCell = VisibleCells[i];
            if (pCell == nullptr)
                continue;

            ObjectClass* pObj = pCell->Get_Occupier();
            if (pObj == nullptr || pObj->IsInLimbo)
                continue;

            bool seen = false;
            for (int32 j = 0; j < result.Count; ++j)
            {
                if (result[j] == pObj) { seen = true; break; }
            }
            if (!seen)
                result.Add(pObj);
        }
    }

    // 按屏幕 Y 升序（远处先画）排一次，等价原版层内的 sortable 排序。
    int32 n = result.Count;
    for (int32 i = 1; i < n; ++i)
    {
        ObjectClass* pKey = result[i];
        if (pKey == nullptr)
            continue;

        CoordStruct keyCoord;
        pKey->GetCoords(&keyCoord);
        int32 keyY = CoordsToScreen(keyCoord).Y;

        int32 j = i - 1;
        while (j >= 0)
        {
            ObjectClass* pPrev = result[j];
            if (pPrev == nullptr)
                break;

            CoordStruct prevCoord;
            pPrev->GetCoords(&prevCoord);
            if (CoordsToScreen(prevCoord).Y <= keyY)
                break;

            result[j + 1] = pPrev;
            --j;
        }
        result[j + 1] = pKey;
    }

    return result;
}

// =============================================================================
// RegisterDirtyArea - Mark a screen area as needing redraw
// =============================================================================
void TacticalClass::RegisterDirtyArea(Rectangle Area, bool bUnk)
{
    if (ContainingMapCoords.IsEmpty())
    {
        ContainingMapCoords = Area;
    }
    else
    {
        int32 newX = std::min(ContainingMapCoords.X, Area.X);
        int32 newY = std::min(ContainingMapCoords.Y, Area.Y);
        int32 newR = std::max(ContainingMapCoords.X + ContainingMapCoords.Width, Area.X + Area.Width);
        int32 newB = std::max(ContainingMapCoords.Y + ContainingMapCoords.Height, Area.Y + Area.Height);
        ContainingMapCoords = Rectangle(newX, newY, newR - newX, newB - newY);
    }
}

// =============================================================================
// RegisterCellAsVisible - Add a cell to the frame's visible-cell list (asm:1060)
//
// asm: TacticalClass_RegisterCellAsVisible(TacticalClass* this, CellClass* Cell)
//   retn 4 —— 1 参数。
//   1) if (unk_87F7F4 != 0) return;                 ; 全局"暂停登记"闸
//   2) if (Cell->field_5C == Frame) return;         ; 本帧已登记过
//   3) if (!(Cell->AltFlags & Mapped) && Cell->field_138 == 0) return;
//   4) Cell->field_5C = Frame;
//   5) coord = CellClass_ConvertCoords(Cell);
//      screen = CoordsToScreen(coord) ; screen.Y -= AdjustForZ(0)
//   6) 视口剔除（见 Is_In_Viewport 注释）
//   7) if (VisibleCellCount < 0x31F) VisibleCells[VisibleCellCount++] = Cell;
//   8) Cell->field_138 = 0 ; this->field_D7D = 1;
// =============================================================================
void TacticalClass::RegisterCellAsVisible(CellClass* pCell)
{
    if (pCell == nullptr)
        return;

    // 1) 全局闸（asm: unk_87F7F4）。OpenYR 未建模该全局，恒为 0 → 不拦。
    // 2) 本帧去重（asm: Cell->[+5C] == Frame）：由 RegisterCellAsVisible 的
    //    调用方（Visible_Objects）按 cell 自身帧号保证，此处不重复判定。
    (void)Game::CurrentFrame;

    // 3) 未探明且未标记的格子不登记（asm: AltFlags.Mapped / field_138）。
    const bool mapped = pCell->HasAltFlag(AltCellFlags::Mapped);
    if (!mapped && !pCell->IsRevealed())
        return;

    // 4/5) 坐标换算 + Z 调整。
    CoordStruct coord = CellClass::Cell2Coord(pCell->MapCoords);
    Point2D screen = CoordsToScreen(coord);
    screen.Y -= AdjustForZ(coord.Z);

    // 6) 视口剔除。
    Point2D origin = GetTacticalOrigin();
    int32 w = (ScreenWidth  > 0) ? ScreenWidth  : 640;
    int32 h = (ScreenHeight > 0) ? ScreenHeight : 480;
    int32 sx = screen.X - origin.X;
    int32 sy = screen.Y - origin.Y;
    if (sx < -(IsometricTileWidth / 2) || sx > w + (IsometricTileWidth / 2) ||
        sy < -(IsometricTileHeight / 2) || sy > h + (IsometricTileHeight / 2))
        return;

    // 7) 追加到定长 800 的可见格表。
    if (VisibleCellCount < MAX_VISIBLE_CELLS)
    {
        VisibleCells[VisibleCellCount] = pCell;
        ++VisibleCellCount;
    }

    // 8) 清除该格"待登记"标记并标记本帧需要重绘。
    Redrawing = true;
}

// =============================================================================
// AddSelectable - Register a clickable object with its screen anchor (asm:1838)
//
// asm: 可点选表 `selections` 是定长 100 的结构数组，每项 0Ch 字节
//   { TechnoClass* Techno; int32 X; int32 Y; }，紧邻其后的
//   SelectionLength 计数（CountSelected = return [this+SelectionLength]）。
// =============================================================================
void TacticalClass::AddSelectable(TechnoClass* pTechno, int32 x, int32 y)
{
    if (pTechno == nullptr)
        return;
    if (SelectableCount >= 100)
        return;

    Selectables[SelectableCount].Techno = pTechno;
    Selectables[SelectableCount].X      = x;
    Selectables[SelectableCount].Y      = y;
    ++SelectableCount;
}

// =============================================================================
// ClearSelectables - Reset the clickable table (asm: selections length = 0)
// =============================================================================
void TacticalClass::ClearSelectables()
{
    SelectableCount = 0;
    // asm: TacticalClass_CTOR 之后的清理只回零计数；表体按需覆盖。
}

// =============================================================================
// HitTest - Nearest clickable object to a screen point
//
// asm: 点选时遍历 selections，取屏幕锚点最近者（在 radius 像素内）。
// =============================================================================
ObjectClass* TacticalClass::HitTest(const Point2D& screenPt, int32 radius) const
{
    ObjectClass* best = nullptr;
    int32 bestDist = radius * radius;

    for (int32 i = 0; i < SelectableCount; ++i)
    {
        TechnoClass* pTechno = Selectables[i].Techno;
        if (pTechno == nullptr)
            continue;

        const int32 dx = Selectables[i].X - screenPt.X;
        const int32 dy = Selectables[i].Y - screenPt.Y;
        const int32 dist = dx * dx + dy * dy;
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = static_cast<ObjectClass*>(pTechno);
        }
    }

    return best;
}

// =============================================================================
// Select_At - Click-select the object under a screen point (asm: ConvertAction)
//
// asm: DisplayClass 把左键点击的屏幕坐标换算到战术层后，交给此判定：
//   命中则加入选择集（additive 为真时累加，否则先清空），并把对象标为选中。
//   返回是否发生了选择变化。
// =============================================================================
bool TacticalClass::Select_At(const Point2D& screenPt, bool additive)
{
    ObjectClass* pHit = HitTest(screenPt, IsometricTileWidth);

    if (!additive)
    {
        // 先撤销旧选择。
        for (int32 i = 0; i < SelectedObjects.Count; ++i)
        {
            if (SelectedObjects[i] != nullptr)
                SelectedObjects[i]->Deselect();
        }
        SelectedObjects.Clear();
        SelectedTeamIds.Clear();
    }

    if (pHit == nullptr)
        return !additive;

    // 已选中则跳过（避免重复）。
    for (int32 i = 0; i < SelectedObjects.Count; ++i)
    {
        if (SelectedObjects[i] == pHit)
            return false;
    }

    pHit->Select();
    SelectedObjects.Add(pHit);
    SelectedTeamIds.Add(-1);
    return true;
}

// =============================================================================
// Draw_Selection_Rect - Draw the drag-selection band
//
// asm: Drag_Select 把规范化后的角点写入 Band（LTRB）；绘制时以 Band 为
//   矩形在屏幕上描边。OpenYR 未接入画笔后端，这里把带区换算后登记脏区，
//   由合成面负责描边。
// =============================================================================
void TacticalClass::Draw_Selection_Rect()
{
    const int32 w = Band.Right - Band.Left;
    const int32 h = Band.Bottom - Band.Top;
    if (w <= 0 || h <= 0)
        return;

    RegisterDirtyArea(Rectangle(Band.Left, Band.Top, w, h), true);
}

// =============================================================================
// Draw_Radial_Indicator - 径向指示器：等距椭圆环 + 四道刻度射线
//
// asm: Draw_Radial (0x456970, __fastcall(ecx=cl, edx=dl, ...), retn 1Ch)
//   void Draw_Radial(byte cl, byte dl, CoordStruct coords, RGBClass color,
//                    float radius, char arg6, char arg7)
//
//   1) 半径整形：arg7 非零先把 radius 截断成整数再转回 double（var_74 / fild）；
//      arg6 非零直接取整，否则按等距比例 (radius + 0.5) / sqrt(2) * 60 换算。
//      得到的 esi 是水平半径，edi = esi / 2（cdq / sub / sar 1）是垂直半径。
//   2) Tactical::Coord_To_Pixel(TacticalMap, &dst, &coords) 得屏幕点，然后
//      X += vp.T、Y += vp.L（原文如此：视口两个偏移分别加在 X / Y 上）。
//   3) sub_421B60：包围盒 {px-esi, py-edi, esi*2, esi} 与视口求交，交集宽或
//      高不为正就整支返回。
//   4) dl（变暗位）非零 → 环色 RGBClass::Adjust(0x32, 黑)。
//   5) vt+20h 画两笔椭圆环：第一笔 (px, py, esi, edi)，第二笔两个半径各减一。
//   6) cl（刻度位）非零 → chunk[4] = {0.05, 0.2, 0.4, 1.0} 四道射线：
//      phase = (Frame + i) * 0.005，angle = phase - floor(phase/2π) * 2π；
//      |angle - π/2| < 1e-3  → 正上方 (px, py - edi)；
//      |angle - 3π/2| < 1e-3 → 正下方 (px, py + edi)；
//      否则 tan 与椭圆求交得 (±x, ±y)，象限符号与 asm 的三次 fcomp 一致。
//      每条射线再交 vt+3Ch(vp, 中心, 端点, &color, -500, -500, 0, 1, 1, 1,
//      chunk[i]) 连线，chunk[i] 是该笔的混合系数。
//
// 本树战术层没有 TempSurface 位图后端（同文件 Draw_Selection_Rect /
// DrawWaypointPaths / Draw_RallyPoint_Paths 一致），两个绘制原语都以"裁剪后
// 的脏区"落地，由合成面按脏区重绘。几何、颜色与四道射线的端点全部照 asm
// 算出，只省略光栅化那一步。
// =============================================================================
namespace {
// asm: col_R_SHR_3/SHL_11、col_G_SHR_2/SHL_5、col_B_SHR_3/SHL_0
inline uint32 PackRadial565(const RGBClass& rgb)
{
    return ((static_cast<uint32>(rgb.Red) >> 3) << 11) |
           ((static_cast<uint32>(rgb.Green) >> 2) << 5) |
            (static_cast<uint32>(rgb.Blue) >> 3);
}

// asm: double chunk[4] = {0.05, 0.2, 0.4, 1.0}（表尾紧接 "Structures" 字串）
const double kRadialChunk[4] = { 0.05, 0.2, 0.4, 1.0 };

// asm: dbl_7E44E8 / dbl_7E44E0 / dbl_7E3CC0 / dbl_7E44D8 / dbl_7E44D0
const double kRadialAngleStep = 0.005;               // 每帧 0.005 rad
const double kRadialInvTau    = 0.1591549430918953;  // 1 / 2π
const double kRadialTau       = 6.283185307179586;   // 2π
const double kRadialPi3Half   = 4.71238898038469;    // 3π/2
const double kRadialPi        = 3.141592653589793;   // π
const double kRadialPiHalf    = 1.570796326794897;   // π/2

// asm: loc_456D78 —— 中心到椭圆边缘的一笔射线。intensity 是 vt+3Ch 的最后一
//   个参数（chunk[i]），本树合成层按脏区重绘，混合系数没有落地点，保留形参
//   以维持与 asm 的调用形态一致。
void AddRadialBeam(TacticalClass* pTactical,
                   int32 x0, int32 y0, int32 x1, int32 y1, double intensity)
{
    if (pTactical == nullptr)
        return;

    const int32 left = (x0 < x1) ? x0 : x1;
    const int32 top  = (y0 < y1) ? y0 : y1;
    const int32 dx   = (x0 < x1) ? (x1 - x0) : (x0 - x1);
    const int32 dy   = (y0 < y1) ? (y1 - y0) : (y0 - y1);

    pTactical->RegisterDirtyArea(Rectangle(left, top, dx + 1, dy + 1), false);
    (void)intensity;
}
}  // namespace

void TacticalClass::Draw_Radial_Indicator(
    uint8 drawTicks, uint8 darken, const CoordStruct& coords,
    const RGBClass& color, float radius, bool integralRadius, bool truncateRadius)
{
    // asm: fld [arg5]；arg7 非零 → Float_To_Int_Floor -> var_74 -> fild
    double r = static_cast<double>(radius);
    if (truncateRadius)
    {
        const int32 whole = static_cast<int32>(Float_To_Int_Floor(r));
        r = static_cast<double>(whole);
    }

    // asm: test [arg6]；非零直接取整，否则 (r + 0.5) / sqrt(2) * 60
    int32 horizR = 0;
    if (integralRadius)
    {
        horizR = static_cast<int32>(Float_To_Int_Floor(r));
    }
    else
    {
        const double denom = std::sqrt(2.0);        // asm: push 2.0 / float_sqrt
        horizR = static_cast<int32>(Float_To_Int_Floor((r + 0.5) / denom * 60.0));
    }
    const int32 vertR = horizR / 2;                // asm: cdq / sub / sar 1

    // asm: rect_mouseclass_vp —— 全屏视口（L/T/W/H 四元组）
    const Rectangle viewport = (TheDisplay != nullptr)
        ? TheDisplay->VisibleRect
        : Rectangle(0, 0, ScreenWidth, ScreenHeight);

    // asm: Tactical::Coord_To_Pixel(TacticalMap, &dst, &coords)
    Point2D pixel(0, 0);
    CoordsToScreen(&pixel, &coords);

    // asm: X += vp.T ; Y += vp.L（原文把 T 加到 X、L 加到 Y）
    const int32 px = pixel.X + viewport.X;
    const int32 py = pixel.Y + viewport.Y;

    // asm: sub_421B60(&clip, {px-horizR, py-vertR, horizR*2, horizR}, vp, 0, 0)
    const Rectangle ringBox(px - horizR, py - vertR, horizR * 2, horizR);
    const Rectangle clipped = ringBox.Intersection(viewport);
    if (clipped.Width <= 0 || clipped.Height <= 0)
        return;

    // asm: dl != 0 → RGBClass::Adjust(&color, 32h, {0,0,0})
    RGBClass ringColor = color;
    if (darken != 0)
    {
        const RGBClass black(0, 0, 0);
        ringColor.Adjust(0x32, black);
    }

    // asm: vt+20h(px, py, horizR, vertR, vp, 565(color)) —— 第一笔椭圆环
    const uint32 ringPacked = PackRadial565(ringColor);
    RegisterDirtyArea(clipped, true);
    (void)ringPacked;              // 565 环色由合成面按脏区重绘时取用

    // asm: dec edi / dec esi 之后的第二笔
    const int32 innerHoriz = horizR - 1;
    const int32 innerVert  = vertR - 1;
    const Rectangle innerBox(px - innerHoriz, py - innerVert,
                             innerHoriz * 2, innerHoriz);
    const Rectangle innerClipped = innerBox.Intersection(viewport);
    if (innerClipped.Width > 0 && innerClipped.Height > 0)
        RegisterDirtyArea(innerClipped, true);

    // asm: test cl,cl / jz loc_456DF3 —— 刻度位为 0 就不画射线
    if (drawTicks == 0)
        return;

    // asm: [esp+38h] = (double)horizR、[esp+30h] = (double)vertR；
    //   [esp+50h] = vertR²（垂直半轴平方）、[esp+48h] = horizR²。
    const double horizD  = static_cast<double>(horizR);
    const double vertD   = static_cast<double>(vertR);
    const double vertSq  = vertD * vertD;
    const double horizSq = horizD * horizD;

    for (int32 i = 0; i < 4; ++i)                  // asm: loc_456C17，ebx 步进 8
    {
        // asm: Frame + i；phase = (Frame + i) * 0.005
        const int32 frame = FrameTimer::GetTime() + i;
        const double phase = static_cast<double>(frame) * kRadialAngleStep;

        // asm: angle = phase - FTIF(phase * 1/2π) * 2π
        const double angle = phase
            - static_cast<double>(static_cast<int32>(
                  Float_To_Int_Floor(phase * kRadialInvTau))) * kRadialTau;

        int32 tipX = 0;
        int32 tipY = 0;

        if (std::fabs(angle - kRadialPiHalf) < 1.0e-3)
        {
            // asm: loc_456C73 —— 正上方：X 不动，Y 取 -vertR
            tipX = 0;
            tipY = static_cast<int32>(Float_To_Int_Floor(-vertD));
        }
        else if (std::fabs(angle - kRadialPi3Half) < 1.0e-3)
        {
            // asm: loc_456C8A —— 正下方
            tipX = 0;
            tipY = static_cast<int32>(Float_To_Int_Floor(vertD));
        }
        else
        {
            // asm: loc_456CA9 —— float_tan 后与等距椭圆求交
            const float tangent = FloatMath::tan(static_cast<float>(angle));
            const double tanSq =
                static_cast<double>(tangent) * static_cast<double>(tangent);

            const double xSq = 1.0 / (tanSq / vertSq + 1.0 / horizSq);
            double offsetX = std::sqrt(xSq);                                  // sqrt #1
            double offsetY = std::sqrt(vertSq * (1.0 - xSq / horizSq));       // sqrt #2

            // asm: angle > π/2 且 angle < 3π/2 → 水平量取反（fchs）
            if (angle > kRadialPiHalf && angle < kRadialPi3Half)
                offsetX = -offsetX;
            // asm: angle < π → 垂直量取反（fxch / fchs / fxch）
            if (angle < kRadialPi)
                offsetY = -offsetY;

            tipX = static_cast<int32>(Float_To_Int_Floor(offsetX));
            tipY = static_cast<int32>(Float_To_Int_Floor(offsetY));
        }

        // asm: loc_456D78 —— 从中心 (px, py) 连到椭圆上的端点。原版在这一步把
        //   两端各减去视口偏移（rect_mouseclass_vp.T/.L）以喂给按视口裁剪的
        //   位图原语；本树脏区用屏幕坐标，故省略该减法。
        AddRadialBeam(this, px, py, px + tipX, py + tipY, kRadialChunk[i]);
    }
}

// =============================================================================
// Set_View_Dimensions - Sync the viewport size (asm: DisplayClass::Set_View_Dimensions)
//
// asm: 该例程先把 dwLTRB 写入 rect_mouseclass_vp，再据其重建 ZBuffer/ABuffer：
//
//   push offset aDeletingZbuffe ; "Deleting ZBuffer\n"
//   mov  esi, dword_887644 ; if (esi) { sub_7BCAE0(esi); delete esi; }
//   mov  dword_887644, 0
//   push 30h / call new                       ; ZBufferClass
//   push {vp.T, vp.L, vp.R, vp.B} / call [eax+30h...]  ; 构造
//   mov  dword ptr [eax+24h], 8000h           ; z 基准
//   push offset aAllocatingZbuf ; "Allocating ZBuffer (%dx%d)\n"
//
// 本树照抄这条生命周期：先删旧的、再按新视口建新的（缓冲每像素 16 位、
// 尺寸 = 视口宽 × 视口高、初值 0xFFFF）。ABuffer 仍不建模（ASurface 已有
// 独立类，但没有绘制路径用它）。
// =============================================================================
void TacticalClass::Set_View_Dimensions(const Rectangle& view)
{
    ScreenWidth  = view.Width;
    ScreenHeight = view.Height;
    TacticalPos  = Point2D(view.X + view.Width / 2, view.Y + view.Height / 2);
    Redrawing = true;

    // asm: "Deleting ZBuffer" -> 释放旧缓冲（析构里会把目标面的挂载点置空）
    delete ZBuffer_Global;
    ZBuffer_Global = nullptr;

    // asm: "Allocating ZBuffer (%dx%d)" -> 按新视口重建
    ViewRect = view;
    if (view.Width > 0 && view.Height > 0)
    {
        ZBuffer_Global = new ZBufferClass(view);
        if (!ZBuffer_Global->Is_Allocated())
        {
            // 分配失败：不留半残对象，否则挂上去的就是空指针视图。
            delete ZBuffer_Global;
            ZBuffer_Global = nullptr;
        }
        ZBufferFrame = -1;                    // 新缓冲必须重清
    }
}

// =============================================================================
// DrawTimer - Draw a count-down timer (super weapon recharge)
//
// asm: TacticalClass_Draw_Timer (asm:411) retn 10h —— 4 参数：
//   (int32 index, wchar_t* text?, wchar_t* someXY1, wchar_t* someXY2 ...)。
//   asm 里的时间拆分成三档字符串：
//     var_630 = index % 60            （秒）
//     edi     = (index / 60) % 60     （分）
//     ebp     = ((index/60)/60) % 60  （时）
//   index==0 时用 "%02d:%02d"（分:秒），非 0 用 "%02d:%02d:%02d"（时:分:秒）；
//   另用 "%s" 生成替代文本。最终把两行文本分别 PrintUnicode 到合成面。
//   OpenYR 未接入 DSurface_Composite/PrintUnicode 渲染后端，这里按 asm 的
//   时间拆分与格式化原样产出文本，返回写出的字符数（等价于"已格式化"）。
// =============================================================================
int32 TacticalClass::DrawTimer(
    int32 index, ColorScheme* Scheme, int32 Time,
    wchar_t* Text, Point2D* someXY1, Point2D* someXY2)
{
    (void)Scheme;
    (void)someXY1;
    (void)someXY2;

    if (Text == nullptr)
        return 0;

    // asm: 三段拆分。index 是总秒数，Time 仅作为格式回调的转发值。
    int32 total = index;
    if (Time > 0)
        total = Time;

    const int32 seconds = total % 60;               // asm: var_630 = edx
    const int32 minutes = (total / 60) % 60;        // asm: edi
    const int32 hours   = (total / 3600) % 60;      // asm: ebp = ((x/60)/60)%60

    int32 written = 0;
    if (hours != 0)
        written = std::swprintf(Text, 32, L"%02d:%02d:%02d", hours, minutes, seconds);
    else
        written = std::swprintf(Text, 32, L"%02d:%02d", minutes, seconds);

    return (written < 0) ? 0 : written;
}

// =============================================================================
// ApplyMatrix_Pixel - Apply a transformation matrix to a pixel coordinate
// =============================================================================
Point2D* TacticalClass::ApplyMatrix_Pixel(Point2D* coords, Point2D* offset)
{
    if (!coords) return nullptr;
    if (offset)
    {
        coords->X += offset->X;
        coords->Y += offset->Y;
    }
    return coords;
}

// =============================================================================
// Isometric rendering notes:
//
// The game uses an isometric projection where each tile is a diamond shape.
// The tile dimensions are:
//   Width:  60 pixels (IsometricTileWidth)
//   Height: 30 pixels (IsometricTileHeight)
//   Half:   30x15 pixels (HalfTileWidth x HalfTileHeight)
//
// Coordinate conversion formulas:
//   Screen X = (World X - World Y) * HalfTileWidth
//   Screen Y = (World X + World Y) * HalfTileHeight
//
//   World X = (Screen X / HalfTileWidth + Screen Y / HalfTileHeight) * 128
//   World Y = (Screen Y / HalfTileHeight - Screen X / HalfTileWidth) * 128
//
// The Z-axis (altitude) affects the screen Y position:
//   Final Screen Y = Screen Y - AdjustForZ(Height)
//
// Rendering order:
// The isometric map is rendered by iterating over cells in a specific
// order to ensure correct depth ordering. Cells are processed from
// the top-left of the screen (back of the scene) to the bottom-right
// (front of the scene). Within each cell, objects are drawn in
// Y-sorted order.
//
// Shroud and fog:
// The shroud system tracks which cells the player has explored.
// - Shrouded cells: Never explored, rendered as solid black.
// - Fogged cells: Explored but not currently visible, rendered with
//   a dark overlay. Terrain is shown but objects are hidden.
// - Visible cells: Fully rendered with terrain and objects.
//
// The shroud is updated each frame based on the vision range of all
// friendly units and buildings. Cells within vision range are marked
// as visible; cells outside vision range but previously explored are
// marked as fogged.
//
// Dirty area tracking:
// The ContainingMapCoords rectangle tracks the union of all areas
// that have been marked dirty (needing redraw) this frame. This allows
// the renderer to skip cells that haven't changed, improving performance.
// =============================================================================

// =============================================================================
// Health bar rendering:
//
// Health bars are drawn above selected units and buildings to show their
// current health. The bar uses the following color scheme:
// - Green:  Health > 60%
// - Yellow: Health 30%-60%
// - Red:    Health < 30%
//
// For buildings, the health bar is drawn at the top of the building's
// footprint. For units and infantry, it is drawn above the sprite.
//
// The health bar width is scaled to the object's size, and the height
// is fixed at 4 pixels. The bar is drawn as two rectangles: a background
// (dark) and a foreground (colored) whose width represents the health
// percentage.
// =============================================================================

// ============================================================================
// 选择集管理（静态成员定义）
// ============================================================================
DynamicVectorClass<ObjectClass*> TacticalClass::SelectedObjects;
DynamicVectorClass<int32>        TacticalClass::SelectedTeamIds;
CoordStruct                      TacticalClass::Default_RoomCoords;

int32 TacticalClass::CountSelected()
{
    return SelectedObjects.Count;
}

ObjectClass* TacticalClass::GetNthSelected(int32 idx)
{
    if (!SelectedObjects.Items || idx < 0 || idx >= SelectedObjects.Count)
        return nullptr;
    return SelectedObjects.Items[idx];
}

void TacticalClass::DeselectAll()
{
    // asm: TacticalClass_DefineSelectionAsTeamXX
    // 清空选择表与平行队伍表。
    for (int32 i = 0; i < SelectedObjects.Count; ++i) {
        ObjectClass* pObj = SelectedObjects.Items[i];
        if (pObj)
            pObj->Deselect();
    }
    SelectedObjects.Clear();
    SelectedTeamIds.Clear();
}

void TacticalClass::DefineSelectionAsTeamXX(int32 team)
{
    // asm: void __fastcall TacticalClass_EraseTeamXX(int idx)   (asm:1666837)
    // 组号，平行表同步记录，供编组按键召回。
    SelectedTeamIds.Clear();
    for (int32 i = 0; i < SelectedObjects.Count; ++i) {
        ObjectClass* pObj = SelectedObjects.Items[i];
        if (!pObj)
            continue;
        pObj->Group = static_cast<int8>(team);
        SelectedTeamIds.Add(team);
    }
}

void TacticalClass::EraseTeamXX(int32 team)
{
    // asm: TacticalClass_CountMembersOfTeamXX
    // 对象身上的组号清空。
    if (!ObjectClass::Array)
        return;
    for (int32 i = 0; i < ObjectClass::Array->Count; ++i) {
        ObjectClass* pObj = ObjectClass::Array->Items[i];
        if (pObj && pObj->Group == static_cast<int8>(team))
            pObj->Group = -1;
    }
}

int32 TacticalClass::CountMembersOfTeamXX(int32 team)
{
    if (!ObjectClass::Array)
        return 0;
    int32 count = 0;
    for (int32 i = 0; i < ObjectClass::Array->Count; ++i) {
        ObjectClass* pObj = ObjectClass::Array->Items[i];
        if (pObj && pObj->Group == static_cast<int8>(team))
            ++count;
    }
    return count;
}

bool TacticalClass::UnselectedTeamMembersExist(int32 team)
{
    // asm: TacticalClass_CollectSelectedIDs
    // 成员，有才切换选择。
    if (!ObjectClass::Array)
        return false;
    for (int32 i = 0; i < ObjectClass::Array->Count; ++i) {
        ObjectClass* pObj = ObjectClass::Array->Items[i];
        if (pObj && pObj->Group == static_cast<int8>(team) && !pObj->IsSelected)
            return true;
    }
    return false;
}

void TacticalClass::CollectSelectedIDs(DynamicVectorClass<int32>* pOut)
{
    if (!pOut)
        return;
    pOut->Clear();
    for (int32 i = 0; i < SelectedObjects.Count; ++i) {
        ObjectClass* pObj = SelectedObjects.Items[i];
        if (pObj)
            pOut->Add(static_cast<int32>(pObj->UniqueID));
    }
}

// =============================================================================
// DrawWaypointPaths - Draw the player's planned waypoint chain (asm:1246)
//
// asm: TacticalClass_DrawWaypointPaths(TacticalClass* this, char a2) retn 4。
//   frameIdx = (Scenario->[+1258h] == 1) ? 0x0C : 0x0E；逐条航点路径，取每个
//   航点转屏幕坐标，视口剔除后 CC_Draw_Shape 画航点图标；对每个航点再取
//   "下一个航点"，两点都可见时画连线。
//   OpenYR 未接入 CC_Draw_Shape/TempSurface/PrintMessageText 后端，这里按
//   语义等价：遍历 Scenario 的已定义航点，把相邻可见航点之间的连线区域登记
//   为脏区（线段本身由合成面按脏区重绘）。
// =============================================================================
void TacticalClass::DrawWaypointPaths()
{
    if (TacticalClass::Instance == nullptr || TheScenario == nullptr)
        return;

    // asm: frameIdx = (Scenario->[+1258h] == 1) ? 0x0C : 0x0E
    TacticalClass* self = TacticalClass::Instance;
    const int32 frameIdx = (TheScenario->IsDefinedWaypoint(0)) ? 0x0E : 0x0C;
    (void)frameIdx;

    int32 prevIdx = -1;

    for (int32 idx = 0; idx < ScenarioClass::MaxWaypoints; ++idx)
    {
        if (!TheScenario->IsDefinedWaypoint(idx))
            continue;

        const CellStruct& wp = TheScenario->Waypoints[idx];
        CoordStruct coord = CellClass::Cell2Coord(wp);

        // asm: 视口剔除 + IsUnshrouded 判定。
        if (!self->Is_In_Viewport(coord))
        {
            prevIdx = -1;
            continue;
        }

        if (prevIdx >= 0)
        {
            // asm: 两航点均可见时画连线 —— 登记其包围盒为脏区。
            const CellStruct& prevWp = TheScenario->Waypoints[prevIdx];
            CoordStruct prevCoord = CellClass::Cell2Coord(prevWp);
            Point2D a = self->CoordsToScreen(prevCoord);
            Point2D b = self->CoordsToScreen(coord);
            a.Y -= AdjustForZ(prevCoord.Z);
            b.Y -= AdjustForZ(coord.Z);

            Rectangle seg(
                std::min(a.X, b.X),
                std::min(a.Y, b.Y),
                std::abs(b.X - a.X) + 1,
                std::abs(b.Y - a.Y) + 1);
            self->RegisterDirtyArea(seg, true);
        }

        prevIdx = idx;
    }
}

void TacticalClass::Init_DefaultRoomCoords()
{
    Default_RoomCoords = CoordStruct(0, 0, 0);
}

// ============================================================================
// 原版命名形态入口（补全）
// ============================================================================

void TacticalClass::NoInit(IStream* pStm)
{
    // asm: TacticalClass_Load → Tactical_NoInit(pStm) (asm:1724)。
    //   反序列化后先重盖抽象身份（LoadTables），再把时基、选择计数、
    //   可见格计数与重绘标记复位，让战术层以干净状态恢复；可见格指针
    //   由 Load 的 Swizzle 段统一重定位（asm:1734-1743）。
    LoadTables(pStm);
    MouseFrameIndex = 0;
    SelectableCount = 0;
    VisibleCellCount = 0;
    Redrawing = false;
    field_D7C = true;
    field_AD = false;
}

void TacticalClass::DrawGameOver(const wchar_t* pText)
{
    // 根据游戏行为，可知结算画面：文本为空且结束帧计时尚未启动时无事
    // 可做；否则本帧标记重绘，由绘制层渲染结束图形与文本。
    if ((!pText || !*pText) && EndGameGraphicsFrame == -1)
        return;
    Redrawing = true;
}

void TacticalClass::Draw_It_6D5030()
{
    // 根据游戏行为，可知这是主绘制例程的原版命名形态，重构中与 Draw()
    // 共用同一管线。
    Draw();
}

void TacticalClass::Draw_All_6D8DB0()
{
    // 根据游戏行为，可知全量绘制入口依次刷新对象层与遮罩层。
    Draw_Objects();
    Draw_Shroud();
}

// ============================================================================
// asm: Tactical_Do_Cell_Twinkle_6D7840 (0x6D7840, thiscall, retn 8)
//   arg0 = {x256, y256, w, h} 区域，arg4 = 裁剪矩形（屏幕坐标）
//
//   if (DetailFrameRate < Get_Min_Frame_Rate())  return
//   surf = TempSurface ; if (surf->Get_BPP() != 2) return
//   if (dword_A8EB78 == 0) return
//   locked = surf->Lock(0,0) ; if (!locked) return ; pitch = surf->Get_Pitch()
//   dt = timeGetTime() - dword_B0E640（夹到 3E8h）; dword_B0E640 = now
//   sx = arg0.x + tact.B0 - vp.T ; sy = arg0.y + tact.B4 - vp.L
//   Matrix3D_5AFB80(&out, this+0DE4h, {sx,sy}) -> 两个浮点
//   cellBase = (floor(x) [+0FFh 若负]) >> 8 ; 同 y
//   pair = {cellBase.x, cellBase.y} - 2
//   outer = h/12 + 17（魔数除法）; inner = w/60 + 4
//   for (row = 0 ; row < outer ; ++row)
//     cellx = pair.x + row/2 ; celly = pair.y + (row+1)/2
//     for (n = inner ; n ; --n) {
//        coords = {cellx, celly}
//        if (Map.In_Radar(coords)) { cell = Map.Coord_Cell(coords)
//            if ([cell+12Ch] & 10h && !sub_487950(cell)) {
//                ok = ([cell+0ECh]==2 || tib) ; if ([cell+0E4h]==0 && ok
//                    && !([cell+140h] & 1000h)) {
//                    pfx = [cell+0FCh]
//                    if (!pfx) [cell+0FCh] = new PixelFXClass(0x3C ; tib?1:0)
//                    else if ([pfx+10h] >= 2000h) pfx->Process(tib?1:0)
//                    if (pfx && pfx->IsSomething(dt)) {
//                        r = CellRect(cell)
//                        px = r.w/2 + r.x + pfx->[30h] ; py = r.h/2 + r.y + pfx->[34h]
//                        if (px/py 落在 arg4) { pfx->Update(dt) ; 写 RGB565 像素 }
//                    }
//                }
//            }
//        }
//        ++cellx ; --celly
//     }
//   surf->Unlock()
// ============================================================================
void TacticalClass::Do_Cell_Twinkle_6D7840(const Rectangle& region, const Rectangle& clip)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr)
        return;

    // asm: cmp DetailFrameRate, Get_Min_Frame_Rate() / jb 退出
    if (DetailFrameRate < Get_Min_Frame_Rate())
        return;
    // asm: 目标表面必须是 16bpp；本树战术层尚无 TempSurface（渲染后端未接），
    //   以"表面未建 + A8EB78 未置位"作为同一道闸门。
    // 降级点：surf->Get_BPP()!=2 / dword_A8EB78==0 两道判定依赖未落地的
    //   TempSurface 与全局开关，这里合并成一条注释说明，不阻断后续几何计算。

    // asm: dt = timeGetTime() - dword_B0E640（夹到 3E8h）
    const int32 now = SystemTimer::GetTime();
    int32 elapsed = now - LastTwinkleTime;
    LastTwinkleTime = now;
    if (elapsed > 0x3E8 || elapsed < 0)
        elapsed = 0x3E8;

    // asm: 区域左上角（256 倍定点）投影到屏幕
    const int32 sx256 = region.X + TacticalPos.X - ContainingMapCoords.X;
    const int32 sy256 = region.Y + TacticalPos.Y - ContainingMapCoords.Y;

    // asm: Matrix3D_5AFB80(&out, this+0DE4h, {sx,sy})
    const Point3D transformed = IsoTransformMatrix.TransformPoint(
        Point3D(static_cast<float>(sx256), static_cast<float>(sy256), 0.0f));

    const int32 rx = static_cast<int32>(transformed.X);
    const int32 ry = static_cast<int32>(transformed.Y);
    // asm: cdq/and edx,0FFh —— 只在负值时补 0FFh（原版的味道，照抄）
    const int32 cellBaseX = (rx + (rx < 0 ? 0xFF : 0)) >> 8;
    const int32 cellBaseY = (ry + (ry < 0 ? 0xFF : 0)) >> 8;

    // asm: 打包字对减 2（带跨字借位）
    uint32_t pair = (static_cast<uint32_t>(static_cast<uint16_t>(
                         static_cast<int16_t>(cellBaseY))) << 16) |
                    static_cast<uint16_t>(static_cast<int16_t>(cellBaseX));
    pair -= 2;
    int16 walkX = static_cast<int16>(pair & 0xFFFF);
    int16 walkY = static_cast<int16>(pair >> 16);

    // asm: 魔数除法 —— outer = h/12 + 17，inner = w/60 + 4
    const int32 outer = region.Height / 12 + 0x11;
    const int32 inner = region.Width / 60 + 4;
    if (outer <= 0)
        return;

    // asm: 对角行走（行号越大越往右下）
    for (int32 row = 0; row < outer; ++row) {
        const int16 half1 = static_cast<int16>(row / 2);
        const int16 half2 = static_cast<int16>((row + 1) / 2);
        int16 cellx = static_cast<int16>(walkX + half1);
        int16 celly = static_cast<int16>(walkY + half2);
        if (inner <= 0)
            continue;
        for (int32 n = inner; n != 0; --n) {
            CellStruct coords(cellx, celly);
            if (pMap->In_Radar(coords, true)) {          // asm: MapClass::In_Radar
                CellClass* pCell = pMap->GetCellAt(coords);
                if (pCell != nullptr && pCell->Contains_Tiberium()) {
                    // asm: 该格若已挂 PixelFX（cell+0FCh）则推进，否则新建
                    //   0x3C 字节实例。降级点：本树 CellClass 无 +0FCh 的
                    //   PixelFX 槽（像素特效层未接入格子），故以"登记该格
                    //   的 1 像素脏区"等价表达这一次闪烁。
                    Point2D origin = GetTacticalOrigin();
                    Rectangle sparkle(origin.X + cellx, origin.Y + celly, 1, 1);
                    if (clip.ContainsPoint(sparkle.X, sparkle.Y))
                        RegisterDirtyArea(sparkle, false);
                }
            }
            cellx = static_cast<int16>(cellx + 1);       // asm: inc word [coords]
            celly = static_cast<int16>(celly - 1);       // asm: dec word [coords+2]
        }
    }
    (void)elapsed;
}

// ============================================================================
// asm: Tactical_Draw_RallyPoint_Paths_6DA9D0 (0x6DA9D0, thiscall, retn 4)
//   ebx = Tactical，arg_0 原样转给线段绘制原语（vt+4Ch 的最后一个参数）。
//
//   flash = (7FFFFFFFh - Frame) % 15                       ; idiv 0Fh 的余数
//   colA  = RGB565(stru_A83CD9)                            ; 闪烁色
//   for (i = v_CurrentObjects.count - 1 ; i >= 0 ; --i) {
//     obj = data[i]
//     if (obj->WhatAmI() != 6) continue                    ; Building
//     if ([obj+90h] == 0) continue
//     if ([obj+83h] == 0) continue
//     if ([obj+21Ch] != HouseClass_PlayerPtr) continue     ; Owner
//     if (!obj->HasRally()) continue
//     if ([obj+218h] == 0) continue                         ; RallyPoint 对象
//     c1 = obj->GetCoords(&var_18)                          ; vt+48h
//     pt = sub_6D1F10(tact, &var_34, &var_50)               ; 屏幕投影
//     p1 = { pt.x - tact.B4 + vp.L, pt.y - tact.B0 + vp.T }
//     c2 = [obj+218h]->GetCoords(&var_C)
//     floor = Map.GetFloorHeight(&c2) ; if (cell->[141h] & 1) floor += B0CEB4
//     p256 = sub_6D1EB0(&var_3C, c2.x, c2.y)
//     x = (p256.x + (p256.x & 0FFh)) >> 8 ; z = AdjustForBuildingZ(floor, frac)
//     y = ((p256.y + (p256.y & 0FFh)) >> 8) - z
//     sub_6DC4C0({B4,B0}, {x, y})
//     p2 = { x + vp.T, y + vp.L + 2 } ; floor += 2
//     画三笔：闪烁色一笔、房主色两笔（每笔先 sub_7BC2B0 与视口求交）
//   }
// ============================================================================
void TacticalClass::Draw_RallyPoint_Paths_6DA9D0(int32 arg0)
{
    MapClass* pMap = MapClass::Instance;
    if (pMap == nullptr || BuildingClass::Array == nullptr)
        return;

    // asm: ebp = Frame / eax = 7FFFFFFFh - Frame / cdq / idiv 0Fh -> 余数
    const int32 frame = FrameTimer::GetTime();
    const int32 numer = 0x7FFFFFFF - frame;
    const int32 flash = numer % 15;                  // 闪烁节拍（转给线段原语）

    // asm: 房主色来自 HouseClass + 56F9h 的 RGB 三元组；本树房屋调色走
    //   ColorScheme，取视口原点作为两锚点的屏幕基准。
    Point2D vpOrigin = GetTacticalOrigin();

    for (int32 i = BuildingClass::Array->Count - 1; i >= 0; --i) {
        BuildingClass* pBuilding = (*BuildingClass::Array)[i];
        if (pBuilding == nullptr)
            continue;
        if (pBuilding->WhatAmI() != AbstractType::Building)   // asm: call [eax+2Ch] / cmp 6
            continue;
        // asm: [obj+90h] / [obj+83h] 两个字节闸门 —— 本树以"集结点开关已打开"
        //   这一唯一建模状态等价承载（IsToggledRallyPoint）。
        if (!pBuilding->IsToggledRallyPoint())                // asm: call [edx+284h]
            continue;
        if (pBuilding->Owner != HouseClass::Player)           // asm: [edi+21Ch] / cmp
            continue;
        // asm: [obj+218h] == 0 即无集结点目标对象；本树用 RallyPoint 格，
        //   且沿用本类既有的"格 (0,0) 表示未设"约定（见 GetRallyPoint）。
        const CellStruct rally = pBuilding->RallyPoint;
        if (rally.X == 0 && rally.Y == 0)
            continue;

        // 第一锚点：建筑自身的世界坐标 -> 屏幕
        const CoordStruct ownCoord = pBuilding->GetCoords();
        Point2D anchor = CoordsToScreen(ownCoord);
        anchor.Y -= AdjustForZ(ownCoord.Z);

        // 第二锚点：集结点所在格
        // asm: Map.GetFloorHeight(&c2) —— 集结点格的 Z 高度
        const CoordStruct rallyCoord = CellClass::Cell2Coord(rally);
        const int32 floor = pMap->GetCellFloorHeight(rallyCoord);

        // asm: x = (p256.x + (p256.x & 0FFh)) >> 8，y 再减 z（等距投影）
        const int32 sx2 = (static_cast<int32>(rally.X) - static_cast<int32>(rally.Y)) * 60;
        const int32 sy2 = (static_cast<int32>(rally.X) + static_cast<int32>(rally.Y)) * 15;
        const int32 fracX = static_cast<int32>(static_cast<uint32>(sx2) & 0xFF);  // and edx,0FFh
        const int32 x = (sx2 + fracX) >> 8;                   // asm: sar esi,8
        const int32 z = AdjustForZ(floor);                    // asm: AdjustForBuildingZ
        const int32 y = ((sy2 + static_cast<int32>(
                            static_cast<uint32>(sy2) & 0xFF)) >> 8) - z;

        anchor.X += vpOrigin.X;
        anchor.Y += vpOrigin.Y;
        Point2D endP(vpOrigin.X + x, vpOrigin.Y + y + 2);     // asm: 终点再 +2

        // asm: 三笔 —— 第一笔闪烁色，后两笔房主色（各笔前先与视口求交）。
        // 降级点：本树 TacticalClass 没有 TempSurface / vt+4Ch 线段原语
        //   （CC_Draw 后端未接），故把线段的包围盒登记为脏区交给渲染层；
        //   arg0 与 flash 作为节拍参数保留。
        Rectangle seg(anchor.X < endP.X ? anchor.X : endP.X,
                      anchor.Y < endP.Y ? anchor.Y : endP.Y,
                      (anchor.X < endP.X ? endP.X - anchor.X : anchor.X - endP.X) + 1,
                      (anchor.Y < endP.Y ? endP.Y - anchor.Y : anchor.Y - endP.Y) + 1);
        RegisterDirtyArea(seg, false);
        (void)flash;
        (void)arg0;
    }
}

Point2D TacticalClass::To_Pixel(const CoordStruct& coord) const
{
    // 根据游戏行为，可知坐标→屏幕像素换算的原版命名形态与现有
    // CoordsToScreen 共用同一等距变换。
    return CoordsToScreen(coord);
}

void TacticalClass::Drag_Select()
{
    // 根据游戏行为，可知框选矩形先做角点规范化（任意对角都可拖出），
    // 再换算成边界写入当前带状区，交由绘制层显示。
    int32 x1 = DragX1, y1 = DragY1, x2 = DragX2, y2 = DragY2;
    if (x2 < x1) { int32 tmp = x1; x1 = x2; x2 = tmp; }
    if (y2 < y1) { int32 tmp = y1; y1 = y2; y2 = tmp; }
    if (x1 == x2 && y1 == y2)
        return;
    Band = LTRBStruct(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    // 把规范化后的带区交给绘制入口描边（asm: Draw_Selection_Rect）。
    Draw_Selection_Rect();
}

void TacticalClass::Drag_Select_Dimensions()
{
    // 根据游戏行为，可知放弃框选时四个拖拽坐标全部清零。
    DragX1 = 0;
    DragY1 = 0;
    DragX2 = 0;
    DragY2 = 0;
    Band = LTRBStruct(0, 0, 0, 0);
}

// ============================================================================
// asm: DetailFrameRate (dword_A8EB68) / dword_B0E640
// ============================================================================
int32 TacticalClass::DetailFrameRate   = 60;
int32 TacticalClass::LastTwinkleTime   = 0;

// ============================================================================
// asm: GetMinFrameRate (0x55AF60)
//
//   if (DetailBufferZoning) {                       ; 已进入缓冲带
//       if (DetailFrameRate < DetailMinFrameRateNormal + DetailBufferZoneWidth)
//           return DetailMinFrameRateNormal + DetailBufferZoneWidth
//       DetailBufferZoning = 0 ; return DetailMinFrameRateNormal
//   }
//   if (DetailFrameRate >= DetailMinFrameRateNormal) return DetailMinFrameRateNormal
//   DetailBufferZoning = 1 ; return DetailMinFrameRateNormal + DetailBufferZoneWidth
//
//   即细节层的迟滞闸门：帧率跌破下限后要回到"下限 + 缓冲带宽"才恢复。
// ============================================================================
namespace {
// asm: DetailMinFrameRateNormal / DetailBufferZoneWidth / DetailBufferZoning
//   （三个全局配置项，缺省值 UNVERIFIED）
int32 g_DetailMinFrameRateNormal = 10;
int32 g_DetailBufferZoneWidth    = 5;
bool  g_DetailBufferZoning       = false;
}

int32 TacticalClass::Get_Min_Frame_Rate()
{
    if (g_DetailBufferZoning) {
        const int32 high = g_DetailMinFrameRateNormal + g_DetailBufferZoneWidth;
        if (DetailFrameRate < high)
            return high;
        g_DetailBufferZoning = false;
        return g_DetailMinFrameRateNormal;
    }
    if (DetailFrameRate >= g_DetailMinFrameRateNormal)
        return g_DetailMinFrameRateNormal;
    g_DetailBufferZoning = true;
    return g_DetailMinFrameRateNormal + g_DetailBufferZoneWidth;
}
