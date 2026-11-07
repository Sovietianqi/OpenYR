#include "LoadProgressMgr.h"
#include "Externs.h"
#include "MPTeams.h"
#include "../Rendering/ConvertClass.h"
#include "../Rendering/Surface.h"
#include "../FileFormats/BitText.h"
#include "../FileFormats/PCX.h"
#include "../FileFormats/PCXLoader.h"
#include "../FileFormats/SHP.h"
#include "../UI/StartSpotClass.h"
#include "../Core/Memory.h"

#include <cmath>
#include <cstring>

// ============================================================================
// LoadProgressMgr singleton (asm: LoadProgressMgr global pointer)
// ============================================================================
static LoadProgressMgr* s_pLoadProgressMgr = nullptr;

// ============================================================================
// Release_Surface -- internal helper for the asm's "push 1 / call dword ptr
// [eax]" idiom: every one of the +0x04 / +0x08 / +0x60 slots is a COM-style
// object whose vtable slot 0 takes the single argument 1.
// ============================================================================
namespace {
void Release_Surface(void* pObj)
{
    if (pObj == nullptr)
        return;
    // asm: mov ecx,eax / push 1 / call dword ptr [eax]
    typedef void (*ReleaseFn)(void* /*this*/, int32 /*one*/);
    void** vtable = *reinterpret_cast<void***>(pObj);
    if (vtable != nullptr && vtable[0] != nullptr)
        reinterpret_cast<ReleaseFn>(vtable[0])(pObj, 1);
}
}

// ============================================================================
// Free_Buffer -- the asm's "call FreeBuffer" on a buffer slot.  These are the
// SHP art blobs the original news up; a plain scalar delete matches
// "delete ptr" semantics.
// ============================================================================
namespace {
void Free_Buffer(void* pBuffer)
{
    if (pBuffer == nullptr)
        return;
    typedef uint8 ByteArray;
    GameDelete(reinterpret_cast<ByteArray*>(pBuffer));
}
}

// ============================================================================
// LoadProgressMgr_FreeData
//
// asm 真实入口 0x554370（AsmComplex3 原标注 0x554330 有误，该地址在 asm
// 中根本不存在）。asm 原文签名：
//   ; void __fastcall LoadProgressMgr_FreeData(LoadProgressManagerClass *this)
//
//   cmp byte ptr [esi+50h], bl / jz  short loc_5543A3
//   mov eax,[esi+44h] / test eax,eax / jz short loc_5543A3
//   call FreeBuffer                        ; delete [esi+44h]
// loc_5543A3:
//   mov [esi+44h], ebx
//   cmp byte ptr [esi+51h], bl / jz  short loc_5543BD
//   mov eax,[esi+48h] / test eax,eax / jz short loc_5543BD
//   call FreeBuffer
// loc_5543BD:
//   mov [esi+48h], ebx
//   cmp byte ptr [esi+52h], bl / jz  short loc_5543D7
//   mov eax,[esi+4Ch] / test eax,eax / jz short loc_5543D7
//   call FreeBuffer
// loc_5543D7:
//   mov [esi+4Ch], ebx
//   call sub_72B460                        ; renderer scratch teardown
//   call sub_72B870                        ; renderer scratch teardown
//   mov eax,[esi+4] / test eax,eax / jz  short loc_55440B
//   push 1 / mov ecx,eax / call dword ptr [eax]    ; virtual[vt+0](1)
// loc_55440B:
//   mov [esi+4], ebx
//   mov eax,[esi+8] / test eax,eax / jz  short loc_554427
//   push 1 / mov ecx,eax / call dword ptr [eax]
// loc_554427:
//   mov [esi+8], ebx
//   mov eax,[esi+60h] / test eax,eax / jz  short loc_55443F
//   push 1 / mov ecx,eax / call dword ptr [eax]
// loc_55443F:
//   mov [esi+60h], ebx
//   retn
//
// 槽对 {valid-flag, buffer} 的偏移与 asm struct 逐字节吻合：
//   [+50]/[+44]  TitleBarSHP      [+51]/[+48]  LoadBarSHP
//   [+52]/[+4C]  BackgroundSHP
// ============================================================================
void LoadProgressMgr::FreeData()
{
    // asm: cmp byte ptr [esi+50h],bl / jz loc_5543A3
    //      mov eax,[esi+44h] / test eax,eax / jz loc_5543A3
    if (TitleBarSHP_loaded && TitleBarSHP != nullptr)
        Free_Buffer(TitleBarSHP);                // asm: call FreeBuffer
    TitleBarSHP = nullptr;                       // asm: mov [esi+44h],ebx

    // asm: cmp byte ptr [esi+51h],bl / jz loc_5543BD
    if (LoadBarSHP_loaded && LoadBarSHP != nullptr)
        Free_Buffer(LoadBarSHP);
    LoadBarSHP = nullptr;                        // asm: mov [esi+48h],ebx

    // asm: cmp byte ptr [esi+52h],bl / jz loc_5543D7
    if (BackgroundSHP_loaded && BackgroundSHP != nullptr)
        Free_Buffer(BackgroundSHP);
    BackgroundSHP = nullptr;                     // asm: mov [esi+4Ch],ebx

    // asm: call sub_72B460 / call sub_72B870
    // 渲染侧 scratch 清理（DirectDraw 全局过程，见 Rendering/DDrawClass）。
    // 铁律①：宁可留白，不写错——此处保留 asm 调用点，不臆造实现。

    // asm: mov eax,[esi+4] / test / jz / push 1 / call dword ptr [eax]
    if (Stream0 != nullptr) {
        Release_Surface(Stream0);                // asm: virtual[vt+0](1)
        Stream0 = nullptr;                       // asm: mov [esi+4],ebx
    }

    // asm: mov eax,[esi+8] / test / jz / push 1 / call dword ptr [eax]
    if (Stream1 != nullptr) {
        Release_Surface(Stream1);
        Stream1 = nullptr;                       // asm: mov [esi+8],ebx
    }

    // asm: mov eax,[esi+60h] / test / jz / push 1 / call dword ptr [eax]
    if (field_60 != nullptr) {
        Release_Surface(field_60);
        field_60 = nullptr;                      // asm: mov [esi+60h],ebx
    }
}

// ============================================================================
// LoadProgressMgr_DTOR2  (asm: LoadProgressMgr_DTOR2, 0x552AA0)
//
//   mov ecx, LoadProgressMgr
//   test ecx, ecx
//   jz  short loc_552ABD        ; no singleton -> just null the pointer
//   call LoadProgressMgr_FreeData
//   push eax ; call delete
// loc_552ABD:
//   mov LoadProgressMgr, 0
// ============================================================================
void LoadProgressMgr::DTOR2()
{
    if (s_pLoadProgressMgr != nullptr) {
        s_pLoadProgressMgr->FreeData();          // asm: call LoadProgressMgr_FreeData
        GameDelete(s_pLoadProgressMgr);          // asm: call delete
    }
    s_pLoadProgressMgr = nullptr;                // asm: mov LoadProgressMgr, 0
}

// ============================================================================
// ProgressScreenClass_GetLoadTextColor (asm: 0x642B30)
//
//   mov eax, [ecx+80h]           ; side flag
//   mov edx, 1                   ; shade count
//   test eax, eax
//   jnz short loc_642B49
//   mov ecx, offset aAlliedload  ; "AlliedLoad"
//   jmp ColorScheme_Find
// loc_642B49:
//   mov ecx, offset aSovietload  ; "SovietLoad"
//   jmp ColorScheme_Find
//
// 归属修正：+0x80 落在 LoadProgressMgr（sizeof=0x64）之外，实为
// ProgressScreenClass（sizeof=0x84）的字段。
// ============================================================================
void* ProgressScreenClass::Get_Load_Text_Color(ProgressScreenClass* pScreen)
{
    const char* pName = "AlliedLoad";
    if (pScreen != nullptr && pScreen->SideFlag != 0)
        pName = "SovietLoad";                    // asm: jnz loc_642B49

    ColorScheme* pScheme = ColorScheme::Find(pName, 1);  // asm: jmp ColorScheme_Find
    return static_cast<void*>(pScheme);
}

// ============================================================================
// Screen_GetTitleBarSHP  (asm: Screen_GetTitleBarSHP, 0x72B2F0)
//
//   eax = str_ttlbr640_shp
//   cmp ecx, 280h / jz ret        ; 640-pixel screen keeps the small bar
//   eax = str_ttlbr800_shp        ; otherwise the wide variant
// ============================================================================
const char* Screen_GetTitleBarSHP(int32 width)
{
    if (width == 0x280)
        return "ttlbr640.shp";                   // asm: str_ttlbr640_shp
    return "ttlbr800.shp";                       // asm: str_ttlbr800_shp
}

// ============================================================================
// Screen_GetProgressBarSHP  (asm: Screen_GetProgressBarSHP, 0x72B310)
//
//   eax = str_spldbrs_shp ; cmp ecx, 280h ; jz ret
//   eax = str_spldbrl_shp
// ============================================================================
const char* Screen_GetProgressBarSHP(int32 width)
{
    if (width == 0x280)
        return "spldbrs.shp";                    // asm: str_spldbrs_shp
    return "spldbrl.shp";                        // asm: str_spldbrl_shp
}
// ============================================================================
// asm: ProgressScreen 全局对象（原版数据段里直接定义的一个实例）
// ============================================================================
ProgressScreenClass ProgressScreen = {};

// ============================================================================
// asm: sub_643E90 (0x643E90, retn 4: arg_0 = 段号) -> double
//
//   edx = (signed char)[ecx+61h]          ; 段数
//   eax = arg_0
//   if (eax >= edx) { fld [ecx+48h] ; retn 4 }        ; 越界直接给总量（= 1.0）
//   fld [ecx+eax*8+8] ; fdiv [ecx+48h] ; retn 4
//
//   注意返回的是 double（不是 float），调用方先 fmul 100 再与 0..100 的
//   目标位置比较。
// ============================================================================
double ProgressScreenClass::Get_Progress(int32 index) const
{
    const int32 steps = static_cast<signed char>(TotalPlayers);
    if (index >= steps)
        return 1.0;                                  // asm: fld [ecx+48h] / 自身相除
    if (MainProgress == 0.0)
        return 0.0;                                  // asm 无此判断，防 0 除
    return Progress[index] / MainProgress;           // asm: fld [ecx+eax*8+8] / fdiv
}

// ============================================================================
// asm: sub_643C50 (0x643C50, retn 14h: arg0 = 段号, a3/a2 = 两个 double)
//
//   sum = 0 ; for (i = 0 ; i < (signed char)[+61h] ; ++i) sum += [+8+i*8]
//   sum = (sum / [+48h]) * 100 -> 取整（原版算完即丢，只用于重绘节流）
//   [idx*8+8] = [+48h] * 0.01 * a3
//   if ([idx*8+8] > [+48h]) { [+8+idx*8] = [+48h] ; [+0C+idx*8] = [+4Ch] }
//   if (a2 != [+idx*8+8]) {
//       总完成百分比 = (sum / [+48h]) * 100
//       if ([+64h] && [+60h]) LoadManager->vt[0](0x11AE, &pct)
//       if ([+64h]) SendMessage(hwnd, WM_PAINT, 0, 0) else sub_643AE0(this, a2)
//   }
// ============================================================================
void ProgressScreenClass::Set_Progress(int32 index, double step, double unk)
{
    (void)unk;

    const int32 steps = static_cast<signed char>(TotalPlayers);
    double sum = 0.0;                                // asm: fadd qword ptr [eax]
    for (int32 i = 0; i < steps; ++i)
        sum += Progress[i];

    if (index < 0 || index >= 8)
        return;

    const double oldValue = Progress[index];           // asm: fld [esi+ecx*8+8]（旧值留栈）
    Progress[index] = MainProgress * 1.0e-2 * step;    // asm: fmul num_0_01 / fmul a3
    if (Progress[index] > MainProgress)                // asm: fcomp [+48h] / test 41h
        Progress[index] = MainProgress;                // asm: mov eax,[esi+48h] / [..+8]

    // asm: fcomp [ecx*8+8] / test 40h / jnz 退出 —— 新旧相等就不重绘
    if (oldValue == Progress[index])
        return;

    // 降级点：原版此处的 LoadManager 通知（vt[0](0x11AE, &pct)）与
    //   SendMessage(WM_PAINT) 需要窗口句柄与 LoadProgressMgr 的私有通知口；
    //   本树两者都未建，重绘改由 NotifyEnabled 标志交给调用方（进度屏绘制
    //   每帧自行读取 Progress[]）。
    (void)sum;
    (void)Hwnd;
}

// ============================================================================
// asm: sub_69E7E0 的 .data 回落矩形（someOtherRect）
//
//   SHP 缺失或帧号越界时，原版把 someOtherRect 的 L/T/R/B 原样拷出，而
//   InitList_2617 把它整块清零，所以回落值就是 {0,0,0,0}。
// ============================================================================
static Rectangle s_SpotFallbackRect(0, 0, 0, 0);

// ============================================================================
// 国家图标面缓存（asm: Get_House_PCX_Icon 背后的全局 PCX 表）
//
//   下标 = country + 3，覆盖 -3..9；country == -1 原版没有任何对应条目，
//   直接落空。每格只尝试装载一次，失败保持 nullptr（与原版"取不到就返回
//   0"一致，不会每次重画都去碰磁盘）。
// ============================================================================
namespace {
Surface* s_HouseIcons[13]      = {nullptr};
bool     s_HouseIconTried[13]  = {false};

// asm: cdq / sub eax,edx / sar eax,1 —— 有符号除以 2 向零取整
inline int32 TruncDiv2(int32 v)
{
    return (v - (v < 0 ? 1 : 0)) >> 1;
}

// asm: sub_4355D0 —— R/G/B 三个通道压成 16bpp 565
inline uint16_t RGB_To_565(int32 r, int32 g, int32 b)
{
    return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// asm: sub_517440([scheme+308h], &idx) 取该色阶的 RGB，再走一次 565 压缩
inline uint16_t Scheme_Color16(const ColorScheme* pScheme)
{
    if (pScheme == nullptr)
        return 0;
    const ColorStruct& rgb = pScheme->BaseColor;
    return RGB_To_565(rgb.R, rgb.G, rgb.B);
}

// asm: sub_433ED0(font, text, 0, &h) 量一段文字 —— 逐字宽度求和。
// 调用点固定是 15 个 'W'（原版用 wstrcpy_version 把 "W" 反复拷进缓冲）。
inline int32 Measure_W_Run(BitTextClass* pFont, int32 count)
{
    if (pFont == nullptr)
        return 0;
    int32 width = 0;
    for (int32 i = 0; i < count; ++i)
        width += pFont->Get_Char_Width('W');
    return width;
}

// asm: 字体行高 [GameFont+1Ch] —— 本树由 BitTextClass::Get_Char_Height 给出
inline int32 Font_Line_Height(BitTextClass* pFont)
{
    return (pFont != nullptr) ? pFont->Get_Char_Height() : 0;
}

// ---------------------------------------------------------------------------
// asm: PCX_Draw(PCX, name, 0) —— 按名字取一张 PCX 并交出一块可绘制面
//
//   原版 PCX_Draw 把命中的 PCX 描述符（0x308 字节，末尾是调色板）复制到
//   全局缓冲后返回；这里用 PCX_LoadFile11 读文件，再按调色板把 8bpp 索引
//   展开成 16bpp 像素，得到真正能被 Draw_Confined_PCX 贴的面。
// ---------------------------------------------------------------------------
Surface* Load_PCX_Surface(const char* pName)
{
    PCXClass* pPCX = PCX_LoadFile11(pName);
    if (pPCX == nullptr)
        return nullptr;

    const int32 width  = pPCX->GetWidth();
    const int32 height = pPCX->GetHeight();
    const BYTE* pImage = pPCX->GetImage();
    const BYTE* pPal   = pPCX->GetPalette();

    if (width <= 0 || height <= 0 || pImage == nullptr || pPal == nullptr) {
        delete pPCX;
        return nullptr;
    }

    BSurface* pSurface = new BSurface();
    pSurface->Allocate(width, height);          // BytesPerPixel = 2

    uint16_t* pDst = static_cast<uint16_t*>(pSurface->Buffer);
    if (pDst != nullptr) {
        const int32 pixels = width * height;
        for (int32 i = 0; i < pixels; ++i) {
            const BYTE idx = pImage[i];
            pDst[i] = RGB_To_565(pPal[idx * 3 + 0],
                                 pPal[idx * 3 + 1],
                                 pPal[idx * 3 + 2]);
        }
    }

    delete pPCX;
    return pSurface;
}
}  // namespace

// ============================================================================
// asm: Get_House_PCX_Icon (0x4E3570)
//
//   xor eax,eax
//   cmp ecx,-2 -> "rani.pcx" ; test ecx,ecx -> "usai.pcx"
//   1 japi / 2 frai / 3 geri / 4 gbri / 5 djbi / 6 arbi / 7 lati / 8 rusi
//   0FFFFFFFDh(-3) obsi / 9 yrii ; 其余落 locret，eax 仍是 0
// ============================================================================
Surface* ProgressScreenClass::Get_House_Icon(int32 country)
{
    static const char* const kIconNames[13] = {
        "obsi.pcx",     // -3 : 观察者
        "rani.pcx",     // -2 : 随机
        nullptr,        // -1 : 原版没有条目
        "usai.pcx",     //  0
        "japi.pcx",     //  1
        "frai.pcx",     //  2
        "geri.pcx",     //  3
        "gbri.pcx",     //  4
        "djbi.pcx",     //  5
        "arbi.pcx",     //  6
        "lati.pcx",     //  7
        "rusi.pcx",     //  8
        "yrii.pcx",     //  9
    };

    // asm: 逐级 cmp 链，没有一条命中就落 locret（eax = 0）
    if (country < -3 || country > 9)
        return nullptr;

    const int32 slot = country + 3;
    if (kIconNames[slot] == nullptr)
        return nullptr;

    if (!s_HouseIconTried[slot]) {
        s_HouseIconTried[slot] = true;
        s_HouseIcons[slot]     = Load_PCX_Surface(kIconNames[slot]);
    }
    return s_HouseIcons[slot];
}

// ============================================================================
// asm: sub_69E7E0 —— 取 SpotSHP 第 frame 帧的 {X,Y,W,H}
//
//   movsx edx,[shp+6] (帧数) ; cmp frame,edx ; jnb 回落
//   SHPFrame 是 24 字节，+8 处是 {Left,Top,Width,Height} 四个 word
//   越界/缺图形时拷 someOtherRect（全 0）
// ============================================================================
Rectangle ProgressScreenClass::Get_Spot_Rect(int32 frame) const
{
    if (SpotSHP == nullptr)
        return s_SpotFallbackRect;                     // asm: loc_69E829
    if (frame < 0 || frame >= SpotSHP->Frames)
        return s_SpotFallbackRect;                     // asm: cmp ecx,edx / jnb

    return SpotSHP->GetFrameBounds(frame);
}

// ============================================================================
// asm: sub_642E40 (retn 8) —— 0 号国家图标的宽高
//
//   icon = Get_House_PCX_Icon(0)
//   if (!icon) { *a1 = 0 ; *a2 = 0 ; retn 8 }
//   *a1 = icon->vt[7Ch]()   ; 宽
//   *a2 = icon->vt[80h]()   ; 高
// ============================================================================
void ProgressScreenClass::Measure_Icon(int32* pWidth, int32* pHeight) const
{
    int32 width  = 0;
    int32 height = 0;

    Surface* pIcon = Get_House_Icon(0);
    if (pIcon != nullptr) {
        width  = pIcon->GetWidth();                    // asm: call [edx+7Ch]
        height = pIcon->GetHeight();                   // asm: call [edx+80h]
    }

    if (pWidth != nullptr)
        *pWidth = width;
    if (pHeight != nullptr)
        *pHeight = height;
}

// ============================================================================
// asm: sub_642E80 (__fastcall: ecx = *a1, edx = *a2) —— 字体单元格
//
//   font = DSurface_GetGameFont(46h)
//   缓冲 = "W" x 15 ; 宽 = GetTextRect(font, 缓冲, 0, &h)
//   *a1 = 宽 ; *a2 = [font+1Ch]（行高）
// ============================================================================
void ProgressScreenClass::Measure_Font_Cell(int32* pWidth, int32* pHeight) const
{
    BitTextClass* pFont = Get_GameFont();

    if (pWidth != nullptr)
        *pWidth = Measure_W_Run(pFont, 0x0F);          // asm: 15 个 'W'
    if (pHeight != nullptr)
        *pHeight = Font_Line_Height(pFont);            // asm: [ebx+1Ch]
}

// ============================================================================
// asm: sub_642E00 (retn 8) —— 图形框的宽高各加 6
//
//   帧矩形 = sub_69E7E0([this+54h], &rect, 0)
//   *a1 = rect.W + 6 ; *a2 = rect.H + 6
//   （rect 的 Left/Top 也读了出来，但没有任何出口用到，照抄为临时量。）
// ============================================================================
void ProgressScreenClass::Measure_Spot_Size(int32* pWidth, int32* pHeight) const
{
    const Rectangle frame = Get_Spot_Rect(0);

    if (pWidth != nullptr)
        *pWidth = frame.Width + 6;                     // asm: [ecx+8] + 6
    if (pHeight != nullptr)
        *pHeight = frame.Height + 6;                   // asm: [ecx+0Ch] + 6
}

// ============================================================================
// asm: sub_642EF0 (retn 10h: a1 = &宽, a2 = &高, a3 = 倍数, a4 = 带图标)
//
//   rect   = sub_69E7E0([this+54h], &rect, 0)
//   iconW/H = 0 ; if (a4) { icon = Get_House_PCX_Icon(0) ; 取宽高 }
//   font = GetGameFont ; textW = 15 个 W 的宽 ; lineH = [font+1Ch]
//   *a1 = textW + iconW + rect.W + 1Ah + 0Ah
//   h   = max(rect.H + 6, iconH) ; h = max(h, lineH) ; h += 4
//   if (a3 > 1) h *= a3 ; *a2 = h
// ============================================================================
void ProgressScreenClass::Measure_Spot(int32* pWidth, int32* pHeight,
                                       int32 repeat, bool bWithIcon) const
{
    const Rectangle frame = Get_Spot_Rect(0);          // asm: +0Ch 就是 H

    int32 iconW = 0;
    int32 iconH = 0;
    if (bWithIcon)
        Measure_Icon(&iconW, &iconH);

    BitTextClass* pFont = Get_GameFont();
    const int32 textW = Measure_W_Run(pFont, 0x0F);
    const int32 lineH = Font_Line_Height(pFont);

    if (pWidth != nullptr) {
        // asm: lea edx,[eax+ecx+1Ah] / add ebx,0Ah
        *pWidth = textW + iconW + frame.Width + 0x1A + 0x0A;
    }

    int32 height = frame.Height + 6;                   // asm: var_34
    if (height < iconH)
        height = iconH;                                // asm: cmp/jle
    if (height < lineH)
        height = lineH;                                // asm: cmp ebx / jg
    height += 4;                                       // asm: add edi,4
    if (repeat > 1)
        height *= repeat;                              // asm: imul ecx,edx

    if (pHeight != nullptr)
        *pHeight = height;
}

// ============================================================================
// asm: sub_643400 (retn 18h) —— 进度条本体
//
//   __thiscall(this, x, y, double progress, Surface *DSurface_Hidden, ColorScheme*)
//
//   rect = {x+3, y+3, shpW, shpH}
//   if (Color == 0) { ColorScheme_Find("DarkRed",1) ; palette = StandardPalette }
//   else            { palette = Color->[30Ch] }
//   if ([this+74h]) palette = [this+74h]                  ; 覆盖
//   if ([this+71h]) { rgb = sub_517440(Color+308h) ;
//                     Surface->vt[58h](&rect, rgb565) }   ; 实心底
//   w = floor(shpW * progress)
//   CC_Draw_Shape(Surface, palette, [this+54h], 0, &{x+3,y+3},
//                 &{x+3,y+3,w,shpH}, 400h, ...)
// ============================================================================
void ProgressScreenClass::Draw_Spot_Bar(int32 x, int32 y, double progress,
                                        Surface* pSurface, ColorScheme* pColor)
{
    const Rectangle frame = Get_Spot_Rect(0);

    Rectangle bar(x + 3, y + 3, frame.Width, frame.Height);

    ConvertClass* pPalette;
    ColorScheme*  pScheme;
    if (pColor == nullptr) {
        // asm: mov ecx,offset aDarkred / mov edx,1 / call ColorScheme_Find
        pScheme  = ColorScheme::Find("DarkRed", 1);
        pPalette = StandardPalette;                    // asm: mov edx,StandardPalette
    } else {
        pScheme  = pColor;
        pPalette = pColor->LightConvert;               // asm: mov ecx,[eax+30Ch]
    }

    // asm: mov ecx,[esi+74h] / test / jz —— 非空则整张覆盖
    if (PaletteOverride != nullptr)
        pPalette = PaletteOverride;

    if (pSurface == nullptr)
        return;

    // asm: mov cl,[esi+71h] / test / jz loc_64352C —— 实心底
    if (BarFillEnabled != 0) {
        const uint16_t color = Scheme_Color16(pScheme);
        pSurface->FillRect(&bar, color);               // asm: call [eax+58h]
    }

    // asm: fild [var_38](宽) / fmul progress / Float_To_Int_Floor
    const int32 scaledW = static_cast<int32>(
        std::floor(static_cast<double>(frame.Width) * progress));

    Rectangle drawRect(x + 3, y + 3, scaledW, frame.Height);
    Point2D   position(x + 3, y + 3);

    CC_Draw_Shape(pSurface, pPalette, SpotSHP, 0, &position, &drawRect,
                  static_cast<BlitterFlags>(0x400), 0, 0, 0, 0, 0,
                  nullptr, 0, 0, 0);                   // asm: CC_Draw_Shape ; 400h
}

// ============================================================================
// asm: sub_643670 (retn 14h) —— 玩家名
//
//   font = DSurface_GetGameFont(46h) ; textW = 15 个 W 的宽 ; lineH = [font+1Ch]
//   dx = (xOffset == -1) ? textW : xOffset
//   PrintUnicode(DSurface_Hidden, text, x + dx, y + lineH, Color, 0, 19h)
// ============================================================================
void ProgressScreenClass::Draw_Spot_Label(int32 x, int32 y,
                                          const wchar_t* pText,
                                          ColorScheme* pColor, int32 xOffset)
{
    BitTextClass* pFont  = Get_GameFont();
    const int32   textW  = Measure_W_Run(pFont, 0x0F);
    const int32   lineH  = Font_Line_Height(pFont);

    const int32 dx = (xOffset == -1) ? textW : xOffset; // asm: cmp edx,-1 / jz

    if (DSurface_Hidden == nullptr || pText == nullptr)
        return;

    Point2D out;
    const Rectangle bounds(0, 0, DSurface_Hidden->Width, DSurface_Hidden->Height);
    Fancy_Text_Print_Wide(out, pText, DSurface_Hidden, bounds,
                          Point2D(x + dx, y + lineH),
                          Scheme_Color16(pColor), 0,
                          static_cast<TextPrintType>(0x19));
}

// ============================================================================
// asm: ProgressScreenClass_DrawPlayerSpot (0x6436D0, retn 14h)
//
//   __thiscall(this, ColorScheme *Color, wchar_t *Name, int32 *a4,
//              int houseIdx, int a6)
//
//   total = movsx [this+61h]
//   a1 = (houseIdx >= total) ? qword [this+48h]            ; 回落 = 总进度本身
//                            : [this+8+idx*8] / [this+48h] ; fld/fdiv
//
//   之后是"量一次 → 取帧矩形 → 量图标 → 量字体 → 求名字宽度 → 再量一次"
//   的固定序列，最后 sub_643400 画条、sub_643670 画名字；a6 非零时中间插
//   一段国家图标（含 session 3/4 的 -3 覆盖规则）。
// ============================================================================
void ProgressScreenClass::DrawPlayerSpot(ColorScheme* pColor,
                                         const wchar_t* pName,
                                         const int32* pPos,
                                         int32 houseIdx, bool bDrawIcon)
{
    const bool bIcon = bDrawIcon;

    // asm: movsx ecx,[esi+ProgressScreenClass.TotalPlayers] / cmp eax,ecx / jl
    double progress;
    const int32 total = static_cast<signed char>(TotalPlayers);
    if (houseIdx >= total) {
        // asm: mov edx,[esi+48h] / mov eax,[esi+4Ch] / 写成 qword -> a1
        progress = MainProgress;
    } else {
        // asm: fld [esi+eax*8+8] / fdiv qword [esi+48h] / fstp a1
        progress = Progress[houseIdx] / MainProgress;
    }

    // asm: var_48 = [a4+4] ; var_44 = var_4C = 0
    //      sub_642EF0(this, &var_44, &var_4C, 1, a6)
    int32 measW = 0;
    int32 measH = 0;
    Measure_Spot(&measW, &measH, 1, bIcon);

    // asm: edi = var_4C*houseIdx + var_48
    const int32 centreY = measH * houseIdx + pPos[1];

    // asm: sub_69E7E0([esi+54h], &a1+8, 0) —— 第一次取帧矩形
    Rectangle frame = Get_Spot_Rect(0);
    const int32 frameW = frame.Width;                  // var_8
    const int32 frameH = frame.Height;                 // var_4

    // asm: var_4C = var_44 = 0 ; if (a6) sub_642E40(this,&var_4C,&var_44)
    int32 iconW = 0;
    int32 iconH = 0;
    if (bIcon)
        Measure_Icon(&iconW, &iconH);

    // asm: var_48 = var_40 = 0 ; sub_642E80(ecx=&var_48, edx=&var_40)
    int32 textW = 0;
    int32 lineH = 0;
    Measure_Font_Cell(&textW, &lineH);

    // asm: var_44 = var_48 + var_4C + var_8 + 1Ah + 0Ah ; [esi+78h] 覆盖
    int32 labelW = textW + iconW + frameW + 0x1A + 0x0A;
    if (LabelOffset != -1)
        labelW = LabelOffset;

    // asm: loc_643810 —— 第二次取帧矩形，var_40 = 0
    frame = Get_Spot_Rect(0);
    (void)frameH;

    // asm: a1+8 = [a4] ; a1+0Ch = [a4+4] ; sub_642EF0(this,&var_40,&var_48,1,a6)
    int32 rowW = 0;
    int32 rowH = 0;
    Measure_Spot(&rowW, &rowH, 1, bIcon);
    (void)rowW;

    // asm: var_3C = var_4C = 0 ; sub_642E00(this,&var_3C,&var_4C)
    int32 cellW = 0;
    int32 cellH = 0;
    Measure_Spot_Size(&cellW, &cellH);
    (void)cellW;

    // asm: a1+8 = [a4]+5 ; ebp = ((var_48 - var_4C) / 2) + edi
    const int32 barX       = pPos[0] + 5;
    const int32 barCentreY = TruncDiv2(rowH - cellH) + centreY;

    // asm: var_30 = [a4] + var_8 + 15h ; var_10 = var_30 ; var_C = [a4+4]
    int32 iconPosX = pPos[0] + frameW + 0x15;
    int32 iconPosY = pPos[1];
    int32 var2C    = pPos[1];                          // asm: var_2C（死写）

    if (bIcon) {
        // asm: var_3C = var_48 = 0 ; sub_642EF0 ; var_40 = var_4C = 0 ; sub_642E40
        int32 w4 = 0;
        int32 h4 = 0;
        Measure_Spot(&w4, &h4, 1, true);
        int32 iw4 = 0;
        int32 ih4 = 0;
        Measure_Icon(&iw4, &ih4);

        // asm: var_C = ((var_48 - var_4C) / 2) + edi
        iconPosY = TruncDiv2(h4 - ih4) + centreY;
    }

    // asm: loc_64391D: var_34 = var_2C（随后被 loc_6439AF 覆盖的死写）
    int32 labelY = var2C;

    // asm: var_38 = var_30 ; sub_642EF0(this,&var_3C,&var_48,1,a6)
    int32 labelX = iconPosX;
    int32 w5 = 0;
    int32 h5 = 0;
    Measure_Spot(&w5, &h5, 1, bIcon);
    (void)w5;

    // asm: var_40 = var_4C = 0 ; sub_642E80(&var_40, &var_4C)
    int32 fw = 0;
    int32 fh = 0;
    Measure_Font_Cell(&fw, &fh);
    (void)fw;
    h5 = TruncDiv2(h5 - fh);                           // asm: var_48 = (var_48-var_4C)/2

    if (bIcon) {
        // asm: xor ecx,ecx / call Get_House_PCX_Icon ; 宽取 vt+7Ch，高丢弃
        Surface* pBaseIcon = Get_House_Icon(0);
        int32 baseW = 0;
        if (pBaseIcon != nullptr) {
            baseW = pBaseIcon->GetWidth();
            (void)pBaseIcon->GetHeight();
        }
        labelX = baseW + iconPosX + 0x0A;              // asm: lea edx,[eax+ecx+0Ah]
    }
    labelY = h5 + centreY;                             // asm: loc_6439AF 之前的加

    // asm: sub_643400(this, a1+8, ebp, a1, a1+4, DSurface_Hidden, a2)
    Draw_Spot_Bar(barX, barCentreY, progress, DSurface_Hidden, pColor);

    if (bIcon) {
        // asm: mov edx,NameNodeTypes.data / mov ecx,[edx+edi*4]
        //      call StartSpotClass_GetCountry
        StartSpotClass** ppSpots =
            reinterpret_cast<StartSpotClass**>(NameNodeTypes_Data());
        StartSpotClass* pSpot =
            (ppSpots != nullptr) ? ppSpots[houseIdx] : nullptr;

        int32 country = (pSpot != nullptr) ? pSpot->Get_Country() : -2;

        // asm: Session == 3 || Session == 4 且 [spot+6Bh] == -1 -> 国家 = -3
        if (g_Session == 3 || g_Session == 4) {
            if (pSpot != nullptr && pSpot->CountryFlag == -1)
                country = -3;
        }

        Surface* pIcon = Get_House_Icon(country);
        if (pIcon != nullptr) {
            // asm: xor ecx,ecx / call Get_House_PCX_Icon —— 取 0 号图标尺寸
            Surface* pBase = Get_House_Icon(0);
            int32 w2 = 0;
            int32 h2 = 0;
            if (pBase != nullptr) {
                w2 = pBase->GetWidth();
                h2 = pBase->GetHeight();               // 这次的高度是留用的
            }

            // asm: sub_42E410(&a1+8, var_10, var_C, w2, h2) —— 摆图标矩形
            Rectangle iconRect(iconPosX, iconPosY, w2, h2);

            // asm: sub_4355D0(0FFh, 0, 0FFh) —— 洋红透明色
            const int32 colorKey = RGB_To_565(0xFF, 0, 0xFF);

            // asm: cmp DSurface_Hidden,ebx / jz ; sub_4BAFE0（面是否丢失）
            if (DSurface_Hidden != nullptr && DSurface_Hidden->CanLock()) {
                // asm: Draw_Confined_PCX(PCX, &rect, DSurface_Hidden, icon, key)
                DSurface_Hidden->Draw_Confined_PCX(&iconRect, pIcon,
                                                   static_cast<int16>(colorKey));
            }
        }
    }

    // asm: sub_643670(this, var_38, var_34, Spot, a2, [a4]-var_38+var_44-3)
    const int32 labelOffsetX = pPos[0] - labelX + labelW - 3;
    Draw_Spot_Label(labelX, labelY, pName, pColor, labelOffsetX);
}
