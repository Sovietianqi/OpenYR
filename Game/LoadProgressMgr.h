#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Math/Rectangle.h"

class DSurface;                 // asm: the "+0x4 / +0x8 / +0x60" slots are
class DDrawSurface;             //     surface-like objects released via vt[0]
class Surface;                  // DSurface_Hidden / 图标面（Rendering/Surface.h）
class SHPStruct;                // asm +0x54 someSHP
class StartSpotClass;           // asm +0x50 PlayerStartSpot
class ColorScheme;              // Rendering/ConvertClass.h
class ConvertClass;             // Rendering/ConvertClass.h

// ============================================================================
// LoadProgressMgr - scenario loading progress overlay singleton
//
// asm struct: LoadProgressManagerClass  (sizeof = 0x64)
//
//   +0x00  ...                       (vtable / head, not modelled here)
//   +0x04  Stream0                   surface-like object, released via vt[0]
//   +0x08  Stream1                   surface-like object, released via vt[0]
//   +0x44  TitleBarSHP               "ttlbr640.shp" / "ttlbr800.shp"
//   +0x48  LoadBarSHP                "spldbrs.shp" / "spldbrl.shp"
//   +0x4C  BackgroundSHP             background art
//   +0x50  TitleBarSHP_loaded        byte flag: +0x44 holds live art
//   +0x51  LoadBarSHP_loaded         byte flag: +0x48 holds live art
//   +0x52  BackgroundSHP_loaded      byte flag: +0x4C holds live art
//   +0x60  field_60                  surface-like object, released via vt[0]
//
// asm: LoadProgressMgr_DTOR2 (0x552AA0) frees the singleton through
// LoadProgressMgr_FreeData and nulls the global pointer.
// ============================================================================
class LoadProgressMgr {
public:
    LoadProgressMgr() noexcept
        : Stream0(nullptr), Stream1(nullptr),
          TitleBarSHP(nullptr), LoadBarSHP(nullptr), BackgroundSHP(nullptr),
          TitleBarSHP_loaded(false), LoadBarSHP_loaded(false),
          BackgroundSHP_loaded(false), field_60(nullptr) {}

    // asm: LoadProgressMgr_FreeData (真实入口 0x554370)
    //   void __fastcall LoadProgressMgr_FreeData(LoadProgressManagerClass *this)
    //   （AsmComplex3 原标注 0x554330 有误，该地址在 asm 中不存在）
    void FreeData();

    // asm: LoadProgressMgr_DTOR2 (0x552AA0)
    //   ecx = LoadProgressMgr ; test ; jz skip
    //   call LoadProgressMgr_FreeData ; delete
    // skip:
    //   mov LoadProgressMgr, 0
    static void DTOR2();

public:
    // asm +0x04 / +0x08 / +0x60 -- surface-like slots torn down in FreeData
    void* Stream0;
    void* Stream1;

    // asm +0x44 / +0x48 / +0x4C -- the held SHP art
    void* TitleBarSHP;
    void* LoadBarSHP;
    void* BackgroundSHP;

    // asm +0x50 / +0x51 / +0x52 -- "art is live" byte flags
    bool  TitleBarSHP_loaded;
    bool  LoadBarSHP_loaded;
    bool  BackgroundSHP_loaded;

    // asm +0x60
    void* field_60;
};

// ============================================================================
// ProgressScreenClass -- asm sizeof = 0x84
//
// The "AlliedLoad" / "SovietLoad" colour scheme lookup belongs to THIS class,
// not to LoadProgressMgr: asm reads [ecx+0x80] as the side flag, which is
// outside LoadProgressMgr's 0x64-byte box.
//
// asm: ProgressScreenClass_GetLoadTextColor (0x642B30)
// ============================================================================
class ProgressScreenClass {
public:
    // asm: ProgressScreenClass_GetLoadTextColor (0x642B30)
    //   eax = [ecx+80h] ; edx = 1 ; test ; jz allied
    //   ecx = "SovietLoad" ; jmp ColorScheme_Find
    // allied:
    //   ecx = "AlliedLoad" ; jmp ColorScheme_Find
    static void* Get_Load_Text_Color(ProgressScreenClass* pScreen);

    // ------------------------------------------------------------------------
    // asm: sub_643E90 (0x643E90, retn 4: idx) -> double
    //   返回第 idx 段进度的完成比例：[idx*8+8] / [+48h]；
    //   idx >= 段数(+61h) 时返回 1.0（[+48h]/[+48h]）。
    // ------------------------------------------------------------------------
    double Get_Progress(int32 index) const;

    // ------------------------------------------------------------------------
    // asm: sub_643C50 (0x643C50, retn 14h: idx, double step, double unk)
    //   把第 idx 段写成 "总进度 * 0.01 * step"，超过总进度即夹到总进度；
    //   随后重算总完成百分比并驱动重绘（原版走 LoadManager 通知 + WM_PAINT，
    //   没有窗口句柄时改调 sub_643AE0 直接重绘）。
    // ------------------------------------------------------------------------
    void Set_Progress(int32 index, double step, double unk);

    // ------------------------------------------------------------------------
    // asm: ProgressScreenClass_DrawPlayerSpot (0x6436D0, retn 14h)
    //
    //   __thiscall(ProgressScreenClass *this, ColorSchemeClass *Color,
    //              wchar_t *Name, int32 *a4, int houseIdx, int a6)
    //
    //   a4 是栈上一个 {x, y} 的双 dword（原版直接把 [a4] / [a4+4] 当坐标读）；
    //   a6 非零时整条"图标 + 名字"都画，等于 0 只画一段进度条。
    // ------------------------------------------------------------------------
    void DrawPlayerSpot(ColorScheme* pColor, const wchar_t* pName,
                        const int32* pPos, int32 houseIdx, bool bDrawIcon);

    // ------------------------------------------------------------------------
    // asm: sub_642EF0 (retn 10h) —— "图标 + 15 个 W + 图形框" 的整块尺寸
    //   *pWidth  = rect.W + iconW + textW + 1Ah + 0Ah
    //   *pHeight = max(rect.H + 6, iconH) ; max(.., 字体行高) ; +4 ; *= repeat
    // ------------------------------------------------------------------------
    void Measure_Spot(int32* pWidth, int32* pHeight, int32 repeat,
                      bool bWithIcon) const;

    // asm: sub_642E00 (retn 8) —— 图形框尺寸 +6
    void Measure_Spot_Size(int32* pWidth, int32* pHeight) const;

    // asm: sub_642E40 (retn 8) —— 0 号国家图标尺寸（无图标则双 0）
    void Measure_Icon(int32* pWidth, int32* pHeight) const;

    // asm: sub_642E80 (__fastcall: ecx/edx) —— 15 个 'W' 的宽度 + 字体行高
    void Measure_Font_Cell(int32* pWidth, int32* pHeight) const;

    // ------------------------------------------------------------------------
    // asm: sub_643400 (retn 18h) —— 进度条本体
    //   (this, x, y, double progress, Surface *DSurface_Hidden, ColorScheme*)
    //   先按配色把整条填成实心，再把 someSHP 按 progress 缩放贴上去。
    // ------------------------------------------------------------------------
    void Draw_Spot_Bar(int32 x, int32 y, double progress, Surface* pSurface,
                       ColorScheme* pColor);

    // ------------------------------------------------------------------------
    // asm: sub_643670 (retn 14h) —— 玩家名
    //   PrintUnicode(DSurface_Hidden, ..) 把名字画在条形右侧；
    //   xOffset == -1 时改用 "15 个 W" 的宽度做偏移。
    // ------------------------------------------------------------------------
    void Draw_Spot_Label(int32 x, int32 y, const wchar_t* pText,
                         ColorScheme* pColor, int32 xOffset);

    // ------------------------------------------------------------------------
    // asm: Get_House_PCX_Icon (0x4E3570, __fastcall: ecx = country)
    //   -2 随机 / 0..9 / -3 (0xFFFFFFFD) 各自对应一张国家图标 PCX，
    //   由全局 PCX 表按名字取面；未命中返回 0。
    // ------------------------------------------------------------------------
    static Surface* Get_House_Icon(int32 country);

    // asm +0x00 .. +0x03
    uint8  Unknown00[0x04];
    // asm +0x04 —— LoadManager 回指（进度变化时上报）
    void*  LoadManager;
    // asm +0x08 .. +0x47 —— 8 段进度（每段 8 字节 double）
    double Progress[8];
    // asm +0x48 —— 总进度（也是 Get_Progress 的除数）
    double MainProgress;
    // asm +0x50 —— 起始点表（多人）；本函数不读，但结构里必须在位
    StartSpotClass* PlayerStartSpot;
    // asm +0x54 —— 进度槽的图形（someSHP）
    SHPStruct* SpotSHP;
    // asm +0x58 .. +0x5B
    uint8  Unknown58[0x04];
    // asm +0x5C
    int32  field_5C;
    // asm +0x60 —— 允许通知 LoadManager 的标志
    uint8  NotifyEnabled;
    // asm +0x61 —— 总人数 / 段数（sub_643E90 也把它当段数用）
    uint8  TotalPlayers;
    // asm +0x62 .. +0x63
    uint8  Unknown62[0x02];
    // asm +0x64 —— 窗口句柄（WM_PAINT 的目标）
    void*  Hwnd;
    // asm +0x68 .. +0x6F
    uint8  Unknown68[0x08];
    // asm +0x70
    uint8  field_70;
    // asm +0x71 —— 非零时进度条先按配色铺一层实心底
    uint8  BarFillEnabled;
    // asm +0x72 .. +0x73
    uint8  Unknown72[0x02];
    // asm +0x74 —— 调色板覆盖（非空时盖过配色方案里的那张）
    ConvertClass* PaletteOverride;
    // asm +0x78 —— 名字横向偏移覆盖；0FFFFFFFFh = 用字体宽度
    int32  LabelOffset;
    // asm +0x7C
    int32  field_7C;
    int32  SideFlag;                   // asm +0x80: 0 = Allied, non-zero = Soviet

private:
    // asm: sub_69E7E0 —— SpotSHP 第 frame 帧的 {X,Y,W,H}；
    // 帧号越界或图形缺失时回落到 someOtherRect（全 0）。
    Rectangle Get_Spot_Rect(int32 frame) const;
};

// asm: ProgressScreen —— 全局进度屏对象
extern ProgressScreenClass ProgressScreen;


// asm globals: str_ttlbr640_shp / str_ttlbr800_shp and the progress-bar
// counterparts -- selected by the screen width (0x280 = 640).
const char* Screen_GetTitleBarSHP(int32 width);
const char* Screen_GetProgressBarSHP(int32 width);
