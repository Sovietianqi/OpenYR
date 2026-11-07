#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/Surface.h"
#include "Containers/DynamicVectorClass.h"
#include "Math/Rectangle.h"
#include "Math/Timer.h"

// Forward declarations
class HouseClass;

// ============================================================================
// asm: dword_B0F9E0 / dword_B04A38 —— 侧边栏底板与雷达面板两张 SHP 的全局句柄。
//   SidebarClass_LoadArt 从 'SIDE1.SHP' / 'RADAR.SHP'(或 'RADARY.SHP') 装入；
//   本工程的 MIX 装载器尚未落地，未装载时为 null。
// ============================================================================
extern SHPStruct* SidebarArt_SHP;
extern SHPStruct* RadarArt_SHP;

// ============================================================================
// RadarClass - Radar/minimap rendering
//
// Renders the minimap in the corner of the screen. Shows terrain,
// unit positions, and building foundations. Supports zooming and
// clicking to navigate the tactical view.
// ============================================================================
class NOVTABLE RadarClass : public DisplayClass
{
public:
    static RadarClass* Instance;

    RadarClass();
    virtual ~RadarClass();

    // 雷达 128x128 离屏面（asm: 逐像素雷达更新的源面）。
    Surface* Get_Radar_Surface() const { return RadarSurface; }

    // MapClass methods
    virtual void CreateEmptyMap(
        const Rectangle& pMapRect, bool reuse, int8 nLevel, bool bUnk2);
    virtual void SetVisibleRect(const Rectangle& mapRect);

    // DisplayClass overrides
    virtual MouseCursorType GetLastMouseCursor() override;

    // RadarClass virtual methods
    virtual void DisposeOfArt();
    virtual void* vt_entry_CC(void* out_pUnk, Point2D* pPoint);
    virtual void vt_entry_D0(DWORD dwUnk);
    virtual void Init_For_House();

    // Non-virtual methods
    void Init_Radar();
    void Draw();
    void Update();

    // ========================================================================
    // 雷达小地图绘制（引擎光栅，逐像素写画布局部坐标）
    //
    //   asm 依据（全部已按条落地）：
    //     * RadarClass_OneTime 写入 +11E4h=0、+11E8h=10h、+11ECh=30h、
    //       +11F4h=31h、+11F8h/1200h=8Ch(140)、+11FCh/1204h=6Ch(108)；
    //     * RadarClass_Draw 的 loc_65336D 把 +120Ch 矩形写成
    //       (X=11E4h, Y=11ECh, W=num_168=168, H=6Eh=110)；
    //     * sub_654490 / sub_654650 求等距包围盒、[1488h] 缩放与 [149Ch] 面矩形；
    //     * sub_654EA0 逐格铺 CellClass_Cell_Color_2 的两色（每格 2 列）；
    //     * sub_6547C0 把包围盒缓冲盒式降采样成 16bpp 地形面（asm 1220h）；
    //     * sub_655C50 逐像素复合对象点（阵营色）；
    //     * sub_656EC0 的 14DCh 视野框：中心由 rect_mouseclass_vp 的屏幕中心
    //       经 TacticalMap→Coord_Cell 投影，宽=floor(2W*F/60+1)、高=floor(2H*F/30)，
    //       夹进 [149Ch,149Ch+14A4h] 后**两次** vt[58h] 描边（外框再扩 1 像素），
    //       颜色取 [1208h]（InitForHouse 由 stru_B0FA1C 打包的 RGB565）。
    //   因此雷达外框是侧边栏局部 (0,48,168,110)，内图区是 (16,49,140,108)。
    //
    //   frame 传入的是"内图区"矩形（屏幕绝对坐标），函数自行裁剪。
    // ========================================================================
    void Draw_Mini_Map(Surface* pSrf, const Rectangle& frame);

    // ========================================================================
    // asm: sub_654EA0 + sub_6547C0 —— 重建雷达地形缓存。
    //   逐格取 CellClass_Cell_Color_2 的两色铺进包围盒 RGB 缓冲（asm 123Ch），
    //   再按 [1488h] 盒式降采样成 16bpp 地形面（asm 1220h）。
    //   同时把 [1490h]/[1498h]/[149Ch..14A8h] 写回本类字段。
    // ========================================================================
    void Render_Terrain();
    int32 Last_Mini_Map_Objects() const { return LastMiniMapObjects; }

    // RadarClass_OneTime 的布局常量（见上述 asm 地址）。
    static constexpr int32 RadarLocalX   = 0;
    static constexpr int32 RadarLocalY   = 48;   // 11ECh
    static constexpr int32 RadarFrameW   = 168;  // num_168
    static constexpr int32 RadarFrameH   = 110;  // 6Eh
    static constexpr int32 RadarMapX     = 16;   // 11E8h
    static constexpr int32 RadarMapY     = 49;   // 11F4h
    static constexpr int32 RadarMapW     = 140;  // 11F8h
    static constexpr int32 RadarMapH     = 108;  // 11FCh

    // ========================================================================
    // asm: RadarClass_MouseMove (0x653810 区, this=ecx, retn 8)
    //
    //   先透传给 DisplayClass_MouseMove(elementID, mouseCoords)，再做雷达侧的
    //   命中测试：光标落在外交部（DIPLOBTN）或选项（OPTBTN）按钮矩形内时，
    //   覆盖鼠标指针为雷达专用光标（MouseClass_SetPointer(0,0)）。
    //   按钮矩形由 Init_For_House 算出的布局常量（a2/dword_B04A04 等）决定。
    // ========================================================================
    void MouseMove(int32* elementID, Point2D* mouseCoords);
    void Click_Render();    bool IsRadarHidden() const;
    void ToggleRadar();
    Point2D WorldToRadar(const CoordStruct& world) const;
    CoordStruct RadarToWorld(const Point2D& radar) const;

    // ========================================================================
    // asm: RadarClass_UpdteRadar? (0x657D20)   —— 雷达地形重绘
    //
    //   符号名带 `?`（IDA 不确定），故按行为命名 Update_Radar。
    //   this = ecx（RadarClass*，全局 Map）；无栈参数 -> __thiscall。
    //   调用者：MapClass::Shroud_The_Map / MapClass_ClearShroud 等 5+ 处，
    //           均以 ecx = offset Map 调用。
    //
    //   注意：与现有 RadarClass::Update()（事件计时器衰减）**语义不同**，
    //         不是同一函数，故另立名。
    //
    //   —— 语义（asm 逐句）：
    //     if (hWndParent == 0):                       ; headless 分支
    //       B = [this+0x1220] ; A = [this+0x121C]
    //       r0 = B->vt[+78h](B, &rect0, 0, 1)         ; Lock 类
    //       r1 = A->vt[+78h](A, &rect1, B, r0)
    //       A->vt[+08h](A, r1)
    //       sub_656150(this)
    //       return
    //     w = A->vt[+7Ch](A) ; h = A->vt[+80h](A)     ; 宽度/高度
    //     for (y = 0 ; y < h ; ++y)
    //       for (x = 0 ; x < w ; ++x)
    //         { point = {x, y} ; sub_655C50(this, &point) }
    //
    //   字段对应：asm +0x121C / +0x1220 即本类 unknown_121C / unknown_1220。
    //
    //   铁律①：asm 批注把 vtable 槽位（+78h/+08h/+7Ch/+80h）误当成命名函数；
    //     它们是 surf 对象的虚槽。sub_656150 / sub_655C50 尚未落地，
    //     此处只固化已确证的 headless/窗口两分支与遍历形状。
    // ========================================================================
    void Update_Radar();

    // ========================================================================
    // asm: RadarClass_CreateMap (0x6DA8B0 区, retn 10h)
    //
    //   eax = [esp+a5] / edx = [esp+a4]
    //   push a5 / push a4
    //   eax = [esp+m.X] / edx = [esp+dest]
    //   push m(2 dwords) / push dest
    //   call MapClass::Set_Map_Dimensions / retn 10h
    //
    //   纯参数转发：把地图尺寸交给 MapClass 的尺寸入口。★ 名字级缺口。
    //   OpenYR 里 MapClass::Set_Map_Dimensions 未单独落地，故此处直接把
    //   尺寸写入 MapClass 并分配格子（语义等价，注释留痕）。
    // ========================================================================
    static void CreateMap(int32 dest, int16 mX, int16 mY, uint8 a4, uint8 a5);

    // ========================================================================
    // asm: RadarClass_Diplomacy_DialogFunc (retn 10h) —— 外交屏对话框过程。
    //
    //   stdcall(hDlg, uMsg, wParam, lParam)：
    //     先 Dialog_HandleGeneralMessages(hDlg,uMsg,wParam,lParam)，非零即返回；
    //     uMsg==0x111(WM_COMMAND) 走盟约/聊天/杂项命令分派（写 byte_A8D108 掩码
    //       与 dword_A8D110 会话开关）；控件 0x423 / 1743h..1749h /
    //       1750h..1753h,1762h..1764h / 0x730；
    //     uMsg==0x113(WM_TIMER) 走 sub_658640 重画全部行 + InvalidateRect；
    //     uMsg==0x497(初始化) 用 ControlList_Diplomacy_* 表填 8 个名字/击杀静态
    //       与 7 行盟约/聊天复选框（受 vec_Houses / Rules allies / country /
    //       mute-SW 影响）。
    //   Win32 调用在 Windows 构建下直调 user32，其它平台退化为"无控件/0"
    //   （与 Game/GameOptionsClass.cpp 的 Dlg_Get_Item / Dlg_Send_Msg 同一约定）。
    // ========================================================================
    static int32 Diplomacy_DialogFunc(void* hDlg, uint32 uMsg, uint32 wParam, int32 lParam);

    // ========================================================================
    // asm: RadarClass_OneTime (this=ecx, 无栈参)
    //   初始化雷达 gadget 的矩形/尺寸与 EVA 语音索引，并调用 DisplayClass_OneTime。
    // ========================================================================
    void One_Time();

    // ========================================================================
    // asm: RadarClass_InitClear (this=ecx, 无栈参)
    //   调用 DisplayClass_InitClear，随后清雷达绘制状态并重置两个 DVC。
    // ========================================================================
    void Init_Clear();

    // ========================================================================
    // asm: RadarClass::Radar_Activate (retn 8) —— 雷达/战术地图激活状态机。
    //   a2 = 目标状态(0..4)，a3 = playSound 标志。
    // ========================================================================
    void Radar_Activate(int32 a2, uint8 a3);

    // ========================================================================
    // asm: RadarClass_Play_Movie —— 雷达开机时播放的过场动画（BINK）。
    // ========================================================================
    void Play_Movie();

    // ========================================================================
    // 雷达事件（asm RadarEventClass_Add）—— 在雷达图上登记一次脉冲事件。
    // ========================================================================
    void AddEvent(int32 type, const CellStruct& cell);

    // ========================================================================
    // asm: sub_65FDD0 —— 逐帧推进所有雷达事件。
    // ========================================================================
    void UpdateEvents();

    // Properties
    static constexpr int32 RadarWidth = 128;
    static constexpr int32 RadarHeight = 128;
    static constexpr int32 MaxZoomLevel = 4;
    static constexpr int32 MinZoomLevel = 1;

    DWORD unknown_11E4;   // asm: [this+11E4h] 侧边栏/雷达左基准（Init_For_House 布局）
    DWORD unknown_11E8;
    DWORD unknown_11EC;
    DWORD unknown_11F0;
    DWORD unknown_11F4;
    DWORD unknown_11F8;
    DWORD unknown_11FC;
    DWORD unknown_1200;
    DWORD unknown_1204;
    DWORD unknown_1208;
    Rectangle unknown_rect_120C;
    DWORD unknown_121C;
    DWORD unknown_1220;
    DynamicVectorClass<CellStruct> unknown_cells_1124;
    DWORD unknown_123C;
    DWORD unknown_1240;
    DWORD unknown_1244;
    DWORD unknown_1248;
    DWORD unknown_124C;
    DWORD unknown_1250;
    DWORD unknown_1254;
    DWORD unknown_1258;
    DynamicVectorClass<Point2D> unknown_points_125C;
    DWORD unknown_1274;
    DynamicVectorClass<Point2D> FoundationTypePixels[22];
    float RadarSizeFactor;
    int32 unknown_int_148C;
    DWORD unknown_1490;
    DWORD unknown_1494;
    DWORD unknown_1498;
    Rectangle unknown_rect_149C;
    DWORD unknown_14AC;
    DWORD unknown_14B0;
    DWORD unknown_14B4;
    DWORD unknown_14B8;
    bool unknown_bool_14BC;
    bool unknown_bool_14BD;
    DWORD unknown_14C0;
    DWORD unknown_14C4;
    DWORD unknown_14C8;
    DWORD unknown_14CC;
    DWORD unknown_14D0;
    int32 unknown_int_14D4;
    bool IsAvailableNow;
    bool unknown_bool_14D9;
    bool unknown_bool_14DA;
    Rectangle unknown_rect_14DC;
    DWORD unknown_14EC;
    DWORD unknown_14F0;
    DWORD unknown_14F4;
    DWORD unknown_14F8;
    DWORD unknown_14FC;
    CDTimerClass unknown_timer_1500;

private:
    bool IsHidden;
    int32 RadarZoomLevel;
    Surface* RadarSurface;
    Rectangle RadarRect;
    int32 LastMiniMapObjects = 0;
};