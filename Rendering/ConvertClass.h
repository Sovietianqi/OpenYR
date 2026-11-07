#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"
#include "Rendering/PaletteClass.h"
#include "Rendering/Blitter.h"
#include "FileFormats/SHP.h"

#include <cstring>

// Forward declarations
class Blitter;
class RLEBlitter;

// ============================================================================
// ConvertClass - Palette conversion and blitter management
//
// Manages color palette conversion tables and selects appropriate blitters
// based on rendering flags. Each ConvertClass instance holds a set of
// pre-configured blitters for different rendering modes.
// ============================================================================

class ConvertClass
{
public:
    // Global array of all ConvertClass instances
    static DynamicVectorClass<ConvertClass*>* Array;

    // Constructor
    ConvertClass(
        const BytePalette& palette,
        const BytePalette& eightBitPalette,
        DSurface* pSurface,
        size_t shadeCount,
        bool skipBlitters);

    // ========================================================================
    // 真实调色板构造（asm: ConvertClass::ConvertClass 0x48E740）
    //
    //   asm 签名：(this, Palette *Pal1, Palette *Pal2, BSurface *Surface,
    //              size_t len, char a6)
    //
    //   Pal1 是 768 字节的 RGB 三元组（0..63 的 VGA 6bit，PaletteClass 内部
    //   已展开成 0..255）。构造逐索引 i 调 PaletteClass::Closest_Color 得到
    //   8bit 索引表（+0x174），再按 len 组色阶用 Palette_FillBuffer 铺出
    //   16bpp 表（+0x170）—— 即“第 g 组整体亮度 = g/(len-1)”。
    //
    //   旧签名收的是 BytePalette（256 字节索引表），把索引当成了颜色，
    //   导入真实 .pal 后颜色必然全错；新签名才是原版语义。
    // ========================================================================
    ConvertClass(
        const PaletteClass& palette,
        const BytePalette& remapTable,
        DSurface* pSurface,
        size_t shadeCount,
        bool skipBlitters);

    // 源调色板（24bit RGB）。LightConvertClass / ColorScheme 都以它为基底。
    const PaletteClass& Get_Palette() const { return Palette; }
    void Set_Palette(const PaletteClass& pal);

    // 无 .pal 时的兜底调色板（中性灰阶 + VGA 前 16 色）。
    static PaletteClass MakeDefaultPalette();

    virtual ~ConvertClass();

    // Select a plain (non-RLE) blitter based on flags
    Blitter* SelectPlainBlitter(BlitterFlags flags) const;

    // Select an RLE blitter based on flags
    RLEBlitter* SelectRLEBlitter(BlitterFlags flags) const;

    // asm 0x490B9F..0x490E40 的分支树，返回 asm 的槽号 0..49。
    // 单独暴露是为了能对拍：槽号是 asm 的原始语义，槽号对了选择就对了。
    static int32 DecodeAsmBlitterSlot(uint32 flags);

    // Find or allocate a ConvertClass by filename
    static ConvertClass* FindOrAllocate(const char* pFilename);

    // ========================================================================
    // asm: ConvertClass_CTOR_FromFilename (0x626380, __fastcall: ecx = 文件名)
    //
    //   懒建的调色板集合（PaletteCollection）+ 三段式大写名索引探查：
    //     ① 首次探查命中 → 直接返回缓存的转换器；
    //     ② 未命中 → 名字集合 FindOrAlloc 一个槽位、登记 0x104 字节的名字
    //        条目（{char[0x100]; void* 槽位}）；
    //     ③ 再探查一次，仍未命中就把 .PAL 载入集合（sub_628DD0 读 + 
    //        sub_628D10 提交），最后第三次探查决定返回值（命中返回转换器，
    //        否则返回 0）。
    //   原版的索引是 palette_calculator 的哈希桶（idx % [calc+8]、0x104 步长
    //   链表）；本树没有该计算器子系统，以等价的线性名表替代（见 .cpp 注释）。
    // ========================================================================
    static ConvertClass* From_Filename(const char* pFname);

    // Create a ConvertClass from a palette file
    static void CreateFromFile(
        const char* pFilename, BytePalette*& pPalette, ConvertClass*& pDestination);

    // ========================================================================
    // asm: MSAnim_Load_Palette_5CB590 (0x5CB590)
    //
    //   MSAnim_Load_Palette_5CB590 proc near
    //   ; asm 注释写 "__thiscall(wchar_t **ecx0)" —— **该注释是误导**，
    //     函数体 ecx 是一个栈上 CCFileClass 对象，实参是字符串指针。
    //     按实际调用形态取 (const char* pName)。
    //   地址 0x5CB590 真实存在；符号名带 _5CB590 后缀（IDA 消歧）。
    //   调用者：GraphicMenuAnimItem_CTOR_4F2F10（返回值存入 [esi+24h]）。
    //
    //   —— 语义（asm 逐句）：
    //     if (mystrcmp(name, 0)) return 0             ; 名字守卫
    //     CCFileClass file(name) ; if (!file.Is_Available(0)) return 0
    //     buf = file.Read_File() ; if (!buf)          return 0
    //     for (i = 0 ; i < 0x100 ; ++i):              ; 256 组 RGB
    //       XXX_PAL_3[3*i+0] = buf[3*i+0] << 2        ; 6bit -> 8bit
    //       XXX_PAL_3[3*i+1] = buf[3*i+1] << 2
    //       XXX_PAL_3[3*i+2] = buf[3*i+2] << 2
    //     delete buf
    //     mem = new BYTE[0x188]                       ; sizeof ConvertClass
    //     if (mem) mem = ConvertClass(mem, &XXX_PAL_3, &XXX_PAL_3,
    //                                 DSurface_Primary, 1, 0)
    //     return mem
    //
    // 归属修正：这不是 MSAnim 的方法（名字里的 MSAnim 只是调用场景），
    //   本质是 ConvertClass 的工厂方法。
    //
    // 铁律①：XXX_PAL_3（0x300 字节原始调色板）与 DSurface_Primary 全局
    //   尚未落地，此处只固化名字守卫、读取流程与 6bit->8bit 展开，
    //   ConvertClass 构造暂以现有构造签名对接。
    // ========================================================================
    static ConvertClass* LoadFromPaletteFile(const char* pName);

protected:
    explicit ConvertClass(int) {} // noinit constructor

    // Build blitter tables
    void BuildBlitters(DSurface* pSurface, bool skipBlitters);

    // Build translucency masks from pixel format
    void BuildTranslucencyMasks();

public:
    int32 BytesPerPixel;
    Blitter* Blitters[50];
    RLEBlitter* RLEBlitters[39];
    int32 ShadeCount;
    void* FullColorData;
    void* PaletteData;
    void* ByteColorData;
    DWORD CurrentZRemap;
    uint16 HalfTranslucencyMask;
    uint16 QuatTranslucencyMask;

    // 源调色板（每索引 24bit RGB）。原版是构造入参 Pal1 的副本，
    // LightConvertClass 建明暗表时从这里取真实颜色。
    PaletteClass Palette;
};

// ============================================================================
// asm: StandardPalette (全局 .data 指针)
//
//   进度条底色的默认调色板：当调用方没有给出配色方案时，sub_643400 直接
//   `mov edx, StandardPalette` 取它。定义在 ConvertClass.cpp。
// ============================================================================
extern ConvertClass* StandardPalette;

// ============================================================================
// LightConvertClass - Brightness/color conversion with lighting
//
// Manages brightness-adjusted color palettes for unit rendering.
// Each LightConvertClass represents a specific tint/brightness combination.
// ============================================================================

class LightConvertClass : public ConvertClass
{
public:
    // Global array of LightConvertClass instances
    static DynamicVectorClass<LightConvertClass*>* Array;

    // Constructor
    LightConvertClass(
        BytePalette* palette1,
        BytePalette* palette2,
        Surface* pSurface,
        int32 colorR,
        int32 colorG,
        int32 colorB,
        bool skipBlitters,
        BYTE* pBuffer,
        size_t shadeCount);

    virtual ~LightConvertClass();

    // Update the color values
    virtual void UpdateColors(int32 red, int32 green, int32 blue, bool tinted);

    // Initialize a LightConvertClass with given RGB values
    static LightConvertClass* InitLightConvert(int32 red, int32 green, int32 blue);

    // Draw the light convert effect to a surface
    void DrawIt(Surface* pSurface, Rectangle rect, int32 flags = 0);

    // Get the color at a given index
    DWORD GetColor(int32 index) const;

    // Apply the light conversion to a buffer
    void Apply(BYTE* buffer, int32 length) const;

    // ------------------------------------------------------------------------
    // SelectBlitter0 / SelectBlitter1 —— asm 0x490B10 / 0x490E4F
    //
    // 两个函数结构相同：`mov eax,[esi+8] / test / jz AllocAllTheFuckingBlitters`，
    // 然后一串位测试直接 `mov eax,[esi+偏移] / retn 4`。下面是从 asm 逐分支
    // 读出的完整对照表（ushort 主表，`[esi+8]` 是 Blitters[0]，步长 4；
    // `[esi+0D0h]` 起是 RLEBlitters[0]）。表中「槽」= (偏移-8)/4。
    //
    //   ── 先判 ZRead (flags & 0x10) ──────────────────────────────────────
    //   ZWrite(0x4000) + Alpha(0x800)        -> 46
    //   ZWrite          + 无 Alpha           -> 20
    //   无ZWrite + 0x3000(ZGradient) + Alpha -> 37
    //   无ZWrite + 0x3000          + 无Alpha -> 10
    //   无ZWrite + 无0x3000        + Alpha   -> 26
    //   无ZWrite + 无0x3000        + 无Alpha ->  3
    //
    //   ── 无 ZRead：先按 flags&6 分三档半透明 ─────────────────────────────
    //   flags&6 == 0（不透明）:
    //     Darken(0x1):  ZWrite+Alpha 45 / ZWrite 18 / ZGrad+Alpha 36 /
    //                   ZGrad 8 / Alpha 24 / 其它 0
    //     非Darken:      ZWrite+Alpha 45 / ZWrite 18 / ZGrad+Alpha 36 /
    //                   ZGrad 8 / 0x8000 30 / 0x100 31 / 0x40 32 /
    //                   Alpha 25 / 其它 1
    //   flags&6 == 2 (Lucent25):
    //     ZWrite+Alpha 49 / ZWrite 23 / ZGrad&Warp(0x8)+Alpha 43 /
    //     ZGrad&Warp 16 / ZGrad+Alpha 40 / ZGrad 13 / Alpha 29 / 其它 6
    //   flags&6 == 4 (Lucent50):
    //     ZWrite+Alpha 48 / ZWrite 22 / ZGrad&Warp+Alpha 42 / ZGrad&Warp 15 /
    //     ZGrad+Alpha 39 / ZGrad 12 / Alpha 28 / 0x10000 34 / 0x20000 33 /
    //     其它 5
    //   flags&6 == 6 (Lucent75):
    //     ZWrite+Alpha 47 / ZWrite 21 / ZGrad&Warp+Alpha 41 / ZGrad&Warp 14 /
    //     ZGrad+Alpha 38 / ZGrad 11 / Alpha 27 / 其它 4
    //
    //   ── 槽位定义（asm 0x48F054 起的 new 序列，vftable 逐个读出）─────────
    //      0 BlitPlainXlat            1 BlitTransXlat
    //      2 BlitTransDarken          3 BlitTransZRemapXlat
    //      4 BlitTransLucent75        5 BlitTransLucent50
    //      6 BlitTransLucent25        7 BlitPlainXlatZRead
    //      8 BlitTransXlatZRead       9 BlitTransDarkenZRead
    //     10 BlitTransZRemapXlatZRead 11 BlitTransLucent75ZRead
    //     12 BlitTransLucent50ZRead   13 BlitTransLucent25ZRead
    //     14 BlitTransLucent75ZReadWarp 15 BlitTransLucent50ZReadWarp
    //     16 BlitTransLucent25ZReadWarp 17 BlitPlainXlatZReadWrite
    //     18 BlitTransXlatZReadWrite  19 BlitTransDarkenZReadWrite
    //     20 BlitTransZRemapXlatZReadWrite 21 BlitTransLucent75ZReadWrite
    //     22 BlitTransLucent50ZReadWrite   23 BlitTransLucent25ZReadWrite
    //     24 BlitPlainXlatAlpha       25 BlitTransXlatAlpha
    //     26 BlitTransZRemapXlatAlpha 27 BlitTransLucent75Alpha
    //     28 BlitTransLucent50Alpha   29 BlitTransLucent25Alpha
    //     30 BlitTransXlatWriteAlpha  31 BlitTransXlatMultWriteAlpha
    //     32 BlitTranslucentWriteAlpha 33 BlitTranslucent50NonzeroAlpha
    //     34 BlitTranslucent50ZeroAlpha 35 BlitPlainXlatAlpha
    //     36 BlitTransXlatAlphaZRead  37 BlitTransZRemapXlatAlphaZRead
    //     38 BlitTransLucent75AlphaZRead 39 BlitTransLucent50AlphaZRead
    //     40 BlitTransLucent25AlphaZRead 41 BlitTransLucent75AlphaZReadWarp
    //     42 BlitTransLucent50AlphaZReadWarp 43 BlitTransLucent25AlphaZReadWarp
    //     44 BlitPlainXlatAlpha       45 BlitTransXlatAlphaZReadWrite
    //     46 BlitTransZRemapXlatAlphaZReadWrite
    //     47 BlitTransLucent75AlphaZReadWrite
    //     48 BlitTransLucent50AlphaZReadWrite
    //     49 BlitTransLucent25AlphaZReadWrite
    //
    // 现状：SelectBlitter0 已按上表的分支树返回 asm 槽号（DecodeAsmBlitterSlot），
    //   再由 kAsmSlotToBlitter 映射到本项目的 Blitters[] 下标。已实现 30 个槽；
    //   剩余 20 个槽（Warp 族、写 alpha 族、Lucent*AlphaZRead* 族）在零售版的
    //   绘制调用里不可达（那 17 个 flags 常量没有一个带 0x10/0x8 组合），
    //   若真被请求会打印一次告警并退回 BlitTransXlat，不静默冒充。
    // ------------------------------------------------------------------------
    void* SelectBlitter0(BlitterFlags flags);
    void* SelectBlitter1(BlitterFlags flags);
    void AllocAllTheFuckingBlitters();
    static LightConvertClass* FindOrAlloc(BYTE* pBuffer, int32 a2, void* pSurface, int32 a5,
                                          int32 red, int32 green, int32 blue, int32 a9);
    void FiddleColours(int32 a1, int32 a2, int32 a3, int32 a4);

protected:
    explicit LightConvertClass(int)
        : ConvertClass(0) {}

    void BuildLightTables(int32 colorR, int32 colorG, int32 colorB, bool tinted);

public:
    RGBClass* UsedPalette1;
    RGBClass* UsedPalette2;
    BYTE* IndexesToIgnore;
    int32 RefCount;
    TintStruct Color1;
    TintStruct Color2;
    bool Tinted;
    BYTE align_1B1[3];
};

// ============================================================================
// ColorScheme - Player color scheme management
// ============================================================================

class ColorScheme
{
public:
    enum {
        Yellow = 3,
        White = 5,
        Grey = 7,
        Red = 11,
        Orange = 13,
        Pink = 15,
        Purple = 17,
        Blue = 21,
        Green = 29,
    };

    // Global array
    static DynamicVectorClass<ColorScheme*>* Array;

    // Find a ColorScheme by ID
    static ColorScheme* Find(const char* pID, int32 ShadeCount = 1)
    {
        int32 index = FindIndex(pID, ShadeCount);
        return Array ? Array->operator[](index) : nullptr;
    }

    static int32 FindIndex(const char* pID, int32 ShadeCount = 1)
    {
        if (!Array) return -1;
        for (int32 i = 0; i < Array->Size(); ++i)
        {
            ColorScheme* pItem = (*Array)[i];
            if (pItem && strcmp(pItem->ID, pID) == 0)
            {
                if (pItem->ShadeCount == ShadeCount)
                    return i;
            }
        }
        return -1;
    }

    static ColorScheme* FindByName(
        const char* pID, const ColorStruct& BaseColor,
        const BytePalette& Pal1, const BytePalette& Pal2, int32 ShadeCount);

    static int32 GetNumberOfSchemes();

    static DynamicVectorClass<ColorScheme*>* GeneratePalette(char* name);

    // ------------------------------------------------------------------------
    // asm: Colour_Modify (0x555AA0)
    //
    //   void __fastcall Colour_Modify(int32 *pA, int32 *pB, int32 *pC)
    //
    //   把三个颜色通道规范化：先各自 clamp 到 [0, 1000]，再按全局画质等级
    //   做量化（低画质丢低位，减少色阶）。
    //
    //   asm 逐段：
    //     cmp eax,3E8h / jl / mov eax,3E8h       ; clamp 上限 1000
    //     setle cl / dec ecx / and ecx,edi       ; clamp 下限 0
    //     switch (dword_A8EB78) {
    //       case 0: &= ~0x7F
    //       case 1: &= ~0x3F
    //       case 2: &= ~0x1F
    //       default: 不改
    //     }
    //
    //   画质等级来自全局 dword_A8EB78，用 Set_Colour_Quality_Level 设置。
    // ------------------------------------------------------------------------
    static void Modify_Colour(int32* pA, int32* pB, int32* pC);

    // asm: dword_A8EB78 —— 全局颜色画质等级
    static int32 ColourQualityLevel;

    static void Set_Colour_Quality_Level(int32 level) { ColourQualityLevel = level; }
    static int32 Get_Colour_Quality_Level() { return ColourQualityLevel; }

    // Constructor / Destructor
    ColorScheme(
        const char* pID, const ColorStruct& BaseColor,
        const BytePalette& Pal1, const BytePalette& Pal2,
        int32 ShadeCount, bool AddToArray);

    ~ColorScheme();

    // Properties
    int32 ArrayIndex;
    BytePalette Colors;
    char* ID;
    ColorStruct BaseColor;
    LightConvertClass* LightConvert;
    int32 ShadeCount;
    BYTE unknown_314[0x1C];
    int32 MainShadeIndex;
    BYTE unknown_334[0x8];
};