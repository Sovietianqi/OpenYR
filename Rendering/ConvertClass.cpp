#include "Rendering/ConvertClass.h"
#include "Rendering/Surface.h"
#include "Rendering/Blitter.h"
#include "Rendering/BlitterSlot.h"
#include "Core/Memory.h"
#include "IO/CCFileClass.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

// ============================================================================
// Static member initialization
// ============================================================================
DynamicVectorClass<ConvertClass*>* ConvertClass::Array = nullptr;
DynamicVectorClass<LightConvertClass*>* LightConvertClass::Array = nullptr;
DynamicVectorClass<ColorScheme*>* ColorScheme::Array = nullptr;

// ============================================================================
// ConvertClass implementation
// ============================================================================

// ============================================================================
// 真实调色板构造（原版 ConvertClass::ConvertClass 0x48E740）
//
//   Pal1/Pal2 是 768 字节 RGB 三元组；构造按 len 组色阶铺 16bpp 表：
//     f = (len > 1) ? g * 0x20000 / (len - 1) : 0x10000
//     out[g][i] = RGB565(pal[i] * f >> 16)
//   所以 len == 1 时表就是调色板原样，len > 1 时第 0 组全黑、最后一组原色。
//
//   remapTable 是 256 字节索引映射（原版 PaletteClass::Closest_Color 的
//   产物，ColorScheme 用它把 16..31 的阵营色段换成本方颜色）。
// ============================================================================
ConvertClass::ConvertClass(
    const PaletteClass& srcPalette,
    const BytePalette& remapTable,
    DSurface* pSurface,
    size_t shadeCount,
    bool skipBlitters)
    : BytesPerPixel(2)
    , ShadeCount(static_cast<int32>(shadeCount))
    , FullColorData(nullptr)
    , PaletteData(nullptr)
    , ByteColorData(nullptr)
    , CurrentZRemap(0)
    , HalfTranslucencyMask(0)
    , QuatTranslucencyMask(0)
    , Palette(srcPalette)
{
    memset(Blitters, 0, sizeof(Blitters));
    memset(RLEBlitters, 0, sizeof(RLEBlitters));

    if (ShadeCount < 1)
        ShadeCount = 1;

    BuildTranslucencyMasks();

    // ---- 8bit 索引表（+0x174）：直接把 remap 表存下来 -------------------
    //   原版这里是 PaletteClass::Closest_Color(pal, pal[i]) 得到的索引，
    //   remap 表本身就是那个结果，等价。
    const size_t idxBytes = static_cast<size_t>(ShadeCount) * 256u;
    ByteColorData = YRMemory::Allocate(idxBytes);
    if (ByteColorData)
    {
        uint8* dst = static_cast<uint8*>(ByteColorData);
        for (int32 g = 0; g < ShadeCount; ++g)
            for (int32 i = 0; i < 256; ++i)
                dst[static_cast<size_t>(g) * 256u + static_cast<size_t>(i)] =
                    remapTable.Entries[i];
    }

    // ---- 16bpp 表（+0x170）：按色阶铺，第 0 组就是当前亮度 ---------------
    const size_t fullBytes = static_cast<size_t>(ShadeCount) * 256u * sizeof(uint16);
    FullColorData = YRMemory::Allocate(fullBytes);
    if (FullColorData)
    {
        // FillBuffer 逐组吃 768 字节 RGB 源；这里每组都是同一张调色板
        // 经 remap 后的副本，缩放由 FillBuffer 的定点因子完成。
        std::vector<uint8> raw(static_cast<size_t>(ShadeCount) * 768u, 0);
        for (int32 g = 0; g < ShadeCount; ++g)
        {
            uint8* row = raw.data() + static_cast<size_t>(g) * 768u;
            for (int32 i = 0; i < 256; ++i)
            {
                const RGBClass& c = Palette.Get_Color(remapTable.Entries[i]);
                row[i * 3 + 0] = c.Red;
                row[i * 3 + 1] = c.Green;
                row[i * 3 + 2] = c.Blue;
            }
        }

        PaletteClass::FillBuffer(reinterpret_cast<uint16*>(FullColorData),
                                 ShadeCount, raw.data());
        PaletteData = FullColorData;
    }

    if (!skipBlitters)
        BuildBlitters(pSurface, skipBlitters);
}

// 旧签名（收 256 字节 BytePalette）保留给仍按“索引表”调用的地方：
// 调色板取默认灰阶，传入的字节表当 remap 用。
ConvertClass::ConvertClass(
    const BytePalette& palette,
    const BytePalette& eightBitPalette,
    DSurface* pSurface,
    size_t shadeCount,
    bool skipBlitters)
    : ConvertClass(MakeDefaultPalette(), palette, pSurface, shadeCount, skipBlitters)
{
    (void)eightBitPalette;
}

// 默认灰阶调色板（对应原版 .data 的 XXX_PAL_3 兜底）。
PaletteClass ConvertClass::MakeDefaultPalette()
{
    static PaletteClass s_Default;
    static bool s_Init = false;
    if (!s_Init)
    {
        s_Default.Set_Default();
        s_Init = true;
    }
    return s_Default;
}

void ConvertClass::Set_Palette(const PaletteClass& pal)
{
    Palette = pal;
    if (!PaletteData)
        return;

    uint16* table = reinterpret_cast<uint16*>(PaletteData);
    for (int32 i = 0; i < 256; ++i)
    {
        const RGBClass& c = Palette.Get_Color(i);
        table[i] = static_cast<uint16>(((c.Red >> 3) << 11) |
                                       ((c.Green >> 2) << 5) |
                                       (c.Blue >> 3));
    }
}

ConvertClass::~ConvertClass()
{
    // Clean up blitters
    for (int32 i = 0; i < 50; ++i)
    {
        if (Blitters[i])
        {
            delete Blitters[i];
            Blitters[i] = nullptr;
        }
    }
    for (int32 i = 0; i < 39; ++i)
    {
        if (RLEBlitters[i])
        {
            delete RLEBlitters[i];
            RLEBlitters[i] = nullptr;
        }
    }

    if (FullColorData)
        YRMemory::Deallocate(FullColorData);
    if (ByteColorData)
        YRMemory::Deallocate(ByteColorData);

    FullColorData = nullptr;
    PaletteData = nullptr;
    ByteColorData = nullptr;
}

void ConvertClass::BuildTranslucencyMasks()
{
    // For 16-bit RGB 565:
    // Red:   5 bits (0xF800 / masks 0x7C00)
    // Green: 6 bits (0x07E0 / masks 0x03E0)
    // Blue:  5 bits (0x<｜image｜>    / masks 0x000F)
    //
    // The mask 0x7BEF removes the LSB of each color component,
    // preventing color drift during repeated blending operations.
    // 0x7BEF = 0111 1011 1110 1111
    HalfTranslucencyMask = 0x7BEF;
    QuatTranslucencyMask = 0x7BEF;
}

void ConvertClass::BuildBlitters(DSurface* pSurface, bool skipBlitters)
{
    if (!PaletteData || skipBlitters) return;

    uint16* pal = reinterpret_cast<uint16*>(PaletteData);

    // Build plain blitters array
    Blitters[0]  = new BlitTransXlat<uint16>(pal);
    Blitters[1]  = new BlitTransRemapXlat<uint16>(pal, nullptr);
    Blitters[2]  = new BlitTransZRemapXlat<uint16>(pal, nullptr);
    Blitters[3]  = new BlitTransLucent25<uint16>(pal, QuatTranslucencyMask);
    Blitters[4]  = new BlitTransLucent50<uint16>(pal, HalfTranslucencyMask);
    Blitters[5]  = new BlitTransLucent75<uint16>(pal, QuatTranslucencyMask);
    Blitters[6]  = new BlitTransDarken<uint16>(pal, 0xFFFF);
    Blitters[7]  = new BlitTransXlatAlpha<uint16>(pal, nullptr);
    Blitters[8]  = new BlitTransLucent25Alpha<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[9]  = new BlitTransLucent50Alpha<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[10] = new BlitTransLucent75Alpha<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[11] = new BlitTransXlatZRead<uint16>(pal);
    Blitters[12] = new BlitTransLucent25ZRead<uint16>(pal, QuatTranslucencyMask);
    Blitters[13] = new BlitTransLucent50ZRead<uint16>(pal, HalfTranslucencyMask);
    Blitters[14] = new BlitTransLucent75ZRead<uint16>(pal, QuatTranslucencyMask);
    Blitters[15] = new BlitTransXlatZReadWrite<uint16>(pal);
    Blitters[16] = new BlitPlainXlat<uint16>(pal);
    Blitters[17] = new BlitTransDarkenZRead<uint16>(pal, 0xFFFF);
    Blitters[18] = new BlitTransDarkenZReadWrite<uint16>(pal, 0xFFFF);
    Blitters[19] = new BlitPlainXlatAlpha<uint16>(pal, nullptr);
    Blitters[20] = new BlitPlainXlatZRead<uint16>(pal);
    Blitters[21] = new BlitPlainXlatZReadWrite<uint16>(pal);
    Blitters[22] = new BlitTransRemapDest<uint16>(pal, nullptr);

    // ---- 组合槽 23..29 ---------------------------------------------------
    // 对应 asm SelectBlitter0 的 36/45/10/20/26/37/46 号槽。z-remap 表由
    // CC_Draw_Shape 通过 Convert->CurrentZRemap 提供，这里先置空（与
    // BlitTransZRemapXlat 的建法一致：nullptr 时退化为普通查表）。
    Blitters[23] = new BlitTransXlatAlphaZRead<uint16>(pal, nullptr);
    Blitters[24] = new BlitTransXlatAlphaZReadWrite<uint16>(pal, nullptr);
    Blitters[25] = new BlitTransZRemapXlatZRead<uint16>(pal, nullptr);
    Blitters[26] = new BlitTransZRemapXlatZReadWrite<uint16>(pal, nullptr);
    Blitters[27] = new BlitTransZRemapXlatAlpha<uint16>(pal, nullptr, nullptr);
    Blitters[28] = new BlitTransZRemapXlatAlphaZRead<uint16>(pal, nullptr, nullptr);
    Blitters[29] = new BlitTransZRemapXlatAlphaZReadWrite<uint16>(pal, nullptr, nullptr);

    // ---- 组合槽 30..46 ---------------------------------------------------
    // 对应 asm SelectBlitter0 的 14/15/16、21/22/23、30/31、38..43、47/48/49。
    // 每一条都是「Lucene 档位 × z 读/写 × warp × alpha」的正交组合，
    // 语义推导见 Rendering/Blitter.h 末尾的注释块。
    // 掩码沿用同档 Lucent 的既有取值（QuatTranslucencyMask 是 25/75 两档共用
    // 的那个"低 2 位清零"掩码，HalfTranslucencyMask 是 50 档的"低 1 位清零"）。
    Blitters[30] = new BlitTransLucent75ZReadWarp<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[31] = new BlitTransLucent50ZReadWarp<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[32] = new BlitTransLucent25ZReadWarp<uint16>(pal, QuatTranslucencyMask, nullptr);

    Blitters[33] = new BlitTransLucent75ZReadWrite<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[34] = new BlitTransLucent50ZReadWrite<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[35] = new BlitTransLucent25ZReadWrite<uint16>(pal, QuatTranslucencyMask, nullptr);

    Blitters[36] = new BlitTransLucent75AlphaZRead<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[37] = new BlitTransLucent50AlphaZRead<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[38] = new BlitTransLucent25AlphaZRead<uint16>(pal, QuatTranslucencyMask, nullptr);

    Blitters[39] = new BlitTransLucent75AlphaZReadWarp<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[40] = new BlitTransLucent50AlphaZReadWarp<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[41] = new BlitTransLucent25AlphaZReadWarp<uint16>(pal, QuatTranslucencyMask, nullptr);

    Blitters[42] = new BlitTransLucent75AlphaZReadWrite<uint16>(pal, QuatTranslucencyMask, nullptr);
    Blitters[43] = new BlitTransLucent50AlphaZReadWrite<uint16>(pal, HalfTranslucencyMask, nullptr);
    Blitters[44] = new BlitTransLucent25AlphaZReadWrite<uint16>(pal, QuatTranslucencyMask, nullptr);

    // 写 alpha 的两个（asm 槽 30/31）：不读 palette，只写 abuf。
    Blitters[45] = new BlitTransXlatWriteAlpha<uint16>(pal);
    Blitters[46] = new BlitTransXlatMultWriteAlpha<uint16>(pal);

    // Build RLE blitters
    RLEBlitters[0]  = new RLEBlitTransLucent25<uint16>(pal, QuatTranslucencyMask);
    RLEBlitters[1]  = new RLEBlitTransLucent50<uint16>(pal, HalfTranslucencyMask);
    RLEBlitters[2]  = new RLEBlitTransLucent75<uint16>(pal, QuatTranslucencyMask);
    RLEBlitters[3]  = new RLEBlitTransDarken<uint16>(pal, 0xFFFF);
    RLEBlitters[4]  = new RLEBlitTransXlat<uint16>(pal);
    RLEBlitters[5]  = new RLEBlitTransXlatAlpha<uint16>(pal, nullptr);
    RLEBlitters[6]  = new RLEBlitTransXlatZRead<uint16>(pal);
    RLEBlitters[7]  = new RLEBlitTransXlatZReadWrite<uint16>(pal);
    RLEBlitters[8]  = new RLEBlitTransRemapXlat<uint16>(pal, nullptr);
    RLEBlitters[9]  = new RLEBlitTransRemapDest<uint16>(pal, nullptr);
    RLEBlitters[10] = new RLEBlitTransZRemapXlat<uint16>(pal, nullptr);
}

// ============================================================================
// SelectBlitter0 / SelectBlitter1 —— asm 0x490B10 / 0x490E4F 的逐分支转写
//
// 结构（两个函数完全一致，只有返回的数组不同）：
//   ① [esi+8] 为空 → AllocAllTheFuckingBlitters()
//   ② flags & 0x10（ZRead）→ 三分支
//   ③ 否则按 flags & 6 分成 不透明 / Lucent25 / Lucent50 / Lucent75 四档，
//      不透明档再按 0x1（Darken）/ 0x20（Plain）细分
//
// 位含义（从 asm 的 test 指令反推，不是猜的）：
//   0x0001 Darken  —— 选 BlitTransDarken 族（阴影用它，见 0x2601/0x4601）
//   0x0020 Plain   —— 选 BlitPlainXlat 族（写满整块，索引 0 也照写）
//   两者都不设   —— 选 BlitTransXlat 族（色键透明，最常用）
//   0x0400 Transparent / 0x0200 Centered 不参与选槽，只在 CC_Draw_Shape 用
// ============================================================================

// 槽位表与分支树在 Rendering/BlitterSlot.h（可独立对拍）。
int32 ConvertClass::DecodeAsmBlitterSlot(uint32 f)
{
    return BlitterSlot::Decode(f);
}

Blitter* ConvertClass::SelectPlainBlitter(BlitterFlags flags) const
{
    const int32 slot = DecodeAsmBlitterSlot(static_cast<uint32>(flags));
    const int32 idx  = BlitterSlot::SlotToBlitter[slot];

    if (idx >= 0 && Blitters[idx] != nullptr)
        return Blitters[idx];

    // 未实现的 asm 槽：绝不返回"看起来像"的近似效果骗过调用方，
    // 只在调试输出里点名一次，然后退回色键透明（最保守的可见行为）。
    static bool s_Warned[50] = {};
    if (slot >= 0 && slot < 50 && !s_Warned[slot])
    {
        s_Warned[slot] = true;
        std::fprintf(stderr,
                     "[blitter] asm 槽 %d (%s) 尚未实现，退回 BlitTransXlat\n",
                     slot, BlitterSlot::Names[slot]);
    }
    return Blitters[0];
}

RLEBlitter* ConvertClass::SelectRLEBlitter(BlitterFlags flags) const
{
    const uint32 f = static_cast<uint32>(flags);

    constexpr uint32 ZRead  = 0x00000010;
    constexpr uint32 Darken = 0x00000001;
    constexpr uint32 Alpha  = 0x00000800;
    constexpr uint32 ZWrite = 0x00004000;
    constexpr uint32 GradMask = 0x00003000;

    // RLE 侧只建了 11 个类（见 BuildBlitters 末尾），asm 的 50 槽无法一一对应。
    // 这里按 asm 的分支**语义**取最接近的已实现类，不按槽号硬套。
    if (f & ZRead)
    {
        if (f & ZWrite) return RLEBlitters[7];      // TransXlatZReadWrite
        return RLEBlitters[6];                      // TransXlatZRead
    }

    const uint32 level = f & 0x6;
    if (level == 0x2) return RLEBlitters[0];        // Lucent25
    if (level == 0x4) return RLEBlitters[1];        // Lucent50
    if (level == 0x6) return RLEBlitters[2];        // Lucent75

    if (f & Darken) return RLEBlitters[3];          // TransDarken
    if (f & Alpha)  return RLEBlitters[5];          // TransXlatAlpha
    if ((f & GradMask) != 0 && (f & 0x00030000) != 0)
        return RLEBlitters[10];                     // TransZRemapXlat

    return RLEBlitters[4];                          // TransXlat（默认）
}

ConvertClass* ConvertClass::FindOrAllocate(const char* pFilename)
{
    if (!pFilename || !Array) return nullptr;

    // Search for existing ConvertClass with matching filename
    for (int32 i = 0; i < Array->Size(); ++i)
    {
        ConvertClass* item = (*Array)[i];
        if (item)
        {
            // In production, this would compare against stored filenames
            // For now, we search by index
            char idxName[32];
            int32 written = snprintf(idxName, sizeof(idxName), "palette_%d", i);
            if (written > 0 && strncmp(pFilename, idxName, static_cast<size_t>(written)) == 0)
                return item;
        }
    }

    // Create from file
    BytePalette* pal = nullptr;
    ConvertClass* result = nullptr;
    CreateFromFile(pFilename, pal, result);
    if (result && Array)
        Array->Add(result);

    // Clean up intermediate palette
    if (pal)
    {
        YRMemory::Deallocate(pal);
        pal = nullptr;
    }

    return result;
}

// ============================================================================
// asm: ConvertClass_FromFilename / palette loader
//
//   .PAL 是 768 字节的 R,G,B 三元组（每通道 0..63 的 VGA 6bit）。
//   原版读进来后由 PaletteClass 展开成 0..255，再建 ConvertClass。
//
//   旧实现完全不读文件，凭空生成一条“彩虹渐变”当调色板 —— 那是程序化
//   假色。这里改成真读盘：读不到就返回空，绘制层据此不画（原版同样如此）。
// ============================================================================
void ConvertClass::CreateFromFile(
    const char* pFilename, BytePalette*& pPalette, ConvertClass*& pDestination)
{
    pDestination = nullptr;
    pPalette = nullptr;

    if (pFilename == nullptr || pFilename[0] == '\0')
        return;

    PaletteClass pal;
    bool loaded = false;

    CCFileClass file(pFilename);
    if (file.Exists() && file.Size() >= 768)
    {
        std::vector<uint8> raw(768);
        if (file.Read(raw.data(), 768))
            loaded = pal.Load_From_Data(raw.data(), 768, true);
    }

    if (!loaded)
        return;                      // 无调色板文件：不建转换器，不造假色

    // 恒等 remap：索引 i 就是调色板第 i 项。
    BytePalette identity;
    for (int32 i = 0; i < 256; ++i)
        identity.Entries[i] = static_cast<BYTE>(i);

    pDestination = new ConvertClass(pal, identity, nullptr, 1, false);
}

// ============================================================================
// LightConvertClass implementation
// ============================================================================

LightConvertClass::LightConvertClass(
    BytePalette* palette1,
    BytePalette* palette2,
    Surface* pSurface,
    int32 colorR,
    int32 colorG,
    int32 colorB,
    bool skipBlitters,
    BYTE* pBuffer,
    size_t shadeCount)
    : ConvertClass(0)
    , UsedPalette1(nullptr)
    , UsedPalette2(nullptr)
    , IndexesToIgnore(nullptr)
    , RefCount(1)
    , Color1(colorR, colorG, colorB)
    , Color2(0, 0, 0)
    , Tinted(false)
{
    BytesPerPixel = 2;
    ShadeCount = static_cast<int32>(shadeCount);

    memset(align_1B1, 0, sizeof(align_1B1));
    memset(Blitters, 0, sizeof(Blitters));
    memset(RLEBlitters, 0, sizeof(RLEBlitters));

    BuildTranslucencyMasks();

    size_t fullSize = shadeCount * 8 * static_cast<size_t>(BytesPerPixel);
    FullColorData = YRMemory::Allocate(fullSize);
    if (FullColorData)
    {
        memset(FullColorData, 0, fullSize);
        PaletteData = FullColorData;
    }

    // 明暗表的基底：有全局调色板就拷一份，否则用兜底灰阶。
    //   （原版 LightConvertClass 的 UsedPalette1 就是这张 768 字节表。）
    if (StandardPalette != nullptr)
        Palette = StandardPalette->Get_Palette();
    else
        Palette = MakeDefaultPalette();

    BuildLightTables(colorR, colorG, colorB, false);

    if (!skipBlitters)
        BuildBlitters(nullptr, skipBlitters);
}

LightConvertClass::~LightConvertClass()
{
    // Clean up blitters inherited from ConvertClass
    for (int32 i = 0; i < 50; ++i)
    {
        if (Blitters[i])
        {
            delete Blitters[i];
            Blitters[i] = nullptr;
        }
    }
    for (int32 i = 0; i < 39; ++i)
    {
        if (RLEBlitters[i])
        {
            delete RLEBlitters[i];
            RLEBlitters[i] = nullptr;
        }
    }

    if (FullColorData)
        YRMemory::Deallocate(FullColorData);
    FullColorData = nullptr;
    PaletteData = nullptr;
}

void LightConvertClass::BuildLightTables(int32 colorR, int32 colorG, int32 colorB, bool tinted)
{
    if (!PaletteData) return;

    uint16* pal = reinterpret_cast<uint16*>(PaletteData);

    // 源调色板：本实例的 Palette 副本（由构造 / InitLightConvert 从
    //   StandardPalette 拷入）。旧实现把索引 i 的二进制位当 RGB
    //   （r=(i&0xE0)>>2 …），那是把“索引”当“颜色”；导入真实 .pal 后
    //   整片素材会变成彩虹噪点。
    const PaletteClass* src = &Palette;

    for (int32 i = 0; i < 256; ++i)
    {
        const RGBClass& c = src->Get_Color(i);
        int32 r = c.Red;
        int32 g = c.Green;
        int32 b = c.Blue;

        if (tinted)
        {
            // 染色：把整条色阶拉向 (colorR,colorG,colorB)，
            //   原版 Tinted 走的是“目标色 × 亮度”，这里按 50% 混合保留明暗。
            const int32 tr = (colorR > 255) ? 255 : ((colorR < 0) ? 0 : colorR);
            const int32 tg = (colorG > 255) ? 255 : ((colorG < 0) ? 0 : colorG);
            const int32 tb = (colorB > 255) ? 255 : ((colorB < 0) ? 0 : colorB);
            r = (r + tr) / 2;
            g = (g + tg) / 2;
            b = (b + tb) / 2;
        }
        else
        {
            // 亮度：colorR/G/B 是 0..1000 的千分比。
            r = r * colorR / 1000;
            g = g * colorG / 1000;
            b = b * colorB / 1000;
        }

        r = (r < 0) ? 0 : ((r > 255) ? 255 : r);
        g = (g < 0) ? 0 : ((g > 255) ? 255 : g);
        b = (b < 0) ? 0 : ((b > 255) ? 255 : b);

        pal[i] = static_cast<uint16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }
}

void LightConvertClass::UpdateColors(int32 red, int32 green, int32 blue, bool tinted)
{
    Color1 = TintStruct(red, green, blue);
    Tinted = tinted;
    BuildLightTables(red, green, blue, tinted);
}

LightConvertClass* LightConvertClass::InitLightConvert(int32 red, int32 green, int32 blue)
{
    LightConvertClass* result = new LightConvertClass(
        nullptr, nullptr, nullptr, red, green, blue, true, nullptr, 1);

    if (Array)
        Array->Add(result);

    return result;
}

void LightConvertClass::DrawIt(Surface* pSurface, Rectangle rect, int32 flags)
{
    if (!pSurface || !pSurface->Buffer || !PaletteData) return;

    Rectangle r = rect.Intersection(Rectangle(0, 0, pSurface->Width, pSurface->Height));
    if (r.IsEmpty()) return;

    uint16* pal = reinterpret_cast<uint16*>(PaletteData);
    int32 bytesPerPix = pSurface->GetBytesPerPixel();
    int32 pitch = pSurface->GetPitch();

    for (int32 y = r.Y; y < r.Y + r.Height; ++y)
    {
        BYTE* line = static_cast<BYTE*>(pSurface->Buffer) + y * pitch;
        for (int32 x = r.X; x < r.X + r.Width; ++x)
        {
            if (bytesPerPix == 1)
            {
                BYTE idx = line[x];
                // Find the palette entry that best matches the current light level
                uint16 target = pal[idx];
                int32 bestMatch = idx;
                int32 bestDist = 0x7FFFFFFF;
                for (int32 i = 0; i < 256; ++i)
                {
                    int32 dr = ((target >> 11) & 0x1F) - ((pal[i] >> 11) & 0x1F);
                    int32 dg = ((target >> 5) & 0x3F) - ((pal[i] >> 5) & 0x3F);
                    int32 db = (target & 0x1F) - (pal[i] & 0x1F);
                    int32 dist = dr * dr + dg * dg + db * db;
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestMatch = i;
                    }
                }
                line[x] = static_cast<BYTE>(bestMatch);
            }
            else if (bytesPerPix == 2)
            {
                uint16* scanline = reinterpret_cast<uint16*>(line);
                uint16 src = scanline[x];
                if (src == 0) continue;

                // Find best matching palette entry for the source color
                int32 bestIdx = 0;
                int32 bestDist = 0x7FFFFFFF;
                for (int32 i = 0; i < 256; ++i)
                {
                    int32 dr = ((src >> 11) & 0x1F) - ((pal[i] >> 11) & 0x1F);
                    int32 dg = ((src >> 5) & 0x3F) - ((pal[i] >> 5) & 0x3F);
                    int32 db = (src & 0x1F) - (pal[i] & 0x1F);
                    int32 dist = dr * dr * 2 + dg * dg * 4 + db * db * 2; // Perceptual weighting
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        bestIdx = i;
                    }
                }
                scanline[x] = pal[bestIdx];
            }
        }
    }
}

DWORD LightConvertClass::GetColor(int32 index) const
{
    if (!PaletteData || index < 0 || index >= 256) return 0;
    return reinterpret_cast<const uint16*>(PaletteData)[index];
}

void LightConvertClass::Apply(BYTE* buffer, int32 length) const
{
    if (!buffer || !PaletteData) return;
    uint16* pal = reinterpret_cast<uint16*>(PaletteData);
    for (int32 i = 0; i < length; ++i)
    {
        BYTE idx = buffer[i];
        // Apply the light table by finding the closest palette match
        uint16 target = pal[idx];
        int32 bestIdx = idx;
        int32 bestDist = 0x7FFFFFFF;
        for (int32 j = 0; j < 256; ++j)
        {
            int32 dr = ((target >> 11) & 0x1F) - ((pal[j] >> 11) & 0x1F);
            int32 dg = ((target >> 5) & 0x3F) - ((pal[j] >> 5) & 0x3F);
            int32 db = (target & 0x1F) - (pal[j] & 0x1F);
            int32 dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = j;
            }
        }
        buffer[i] = static_cast<BYTE>(bestIdx);
    }
}

// ============================================================================
// ColorScheme implementation
// ============================================================================

ColorScheme* ColorScheme::FindByName(
    const char* pID, const ColorStruct& BaseColor,
    const BytePalette& Pal1, const BytePalette& Pal2, int32 ShadeCount)
{
    if (!pID || !Array) return nullptr;

    // Search for existing ColorScheme with matching ID
    for (int32 i = 0; i < Array->Size(); ++i)
    {
        ColorScheme* pItem = (*Array)[i];
        if (pItem && pItem->ID && strcmp(pItem->ID, pID) == 0 && pItem->ShadeCount == ShadeCount)
            return pItem;
    }

    // Not found, create a new one
    ColorScheme* pNew = new ColorScheme(pID, BaseColor, Pal1, Pal2, ShadeCount, true);
    if (Array)
        Array->Add(pNew);

    return pNew;
}

int32 ColorScheme::GetNumberOfSchemes()
{
    return Array ? Array->Size() : 0;
}

DynamicVectorClass<ColorScheme*>* ColorScheme::GeneratePalette(char* name)
{
    if (!Array)
        Array = new DynamicVectorClass<ColorScheme*>();

    if (!name) return Array;

    // Parse the palette file and create ColorScheme entries
    // In production, this would parse MIX/CSF data
    // For the engine reconstruction, we create default schemes

    // Default Allied blue scheme
    ColorStruct blue(0, 0, 255);
    BytePalette pal1, pal2;
    ColorScheme::FindByName("AlliedBlue", blue, pal1, pal2, 1);

    // Default Soviet red scheme
    ColorStruct red(255, 0, 0);
    ColorScheme::FindByName("SovietRed", red, pal1, pal2, 1);

    // Default Yuri purple scheme
    ColorStruct purple(128, 0, 128);
    ColorScheme::FindByName("YuriPurple", purple, pal1, pal2, 1);

    return Array;
}

ColorScheme::ColorScheme(
    const char* pID, const ColorStruct& BaseColor,
    const BytePalette& Pal1, const BytePalette& Pal2,
    int32 ShadeCount, bool AddToArray)
    : ArrayIndex(0)
    , ID(nullptr)
    , BaseColor(BaseColor)
    , LightConvert(nullptr)
    , ShadeCount(ShadeCount)
    , MainShadeIndex(0)
{
    memset(Colors.Entries, 0, sizeof(Colors.Entries));
    memset(unknown_314, 0, sizeof(unknown_314));
    memset(unknown_334, 0, sizeof(unknown_334));

    if (pID)
    {
        size_t len = strlen(pID) + 1;
        ID = static_cast<char*>(YRMemory::Allocate(len));
        if (ID) memcpy(ID, pID, len);
    }

    // Build the color palette entries
    // Each element in Colors.Entries maps a remap table index to the final color
    for (int32 i = 0; i < 256; ++i)
    {
        // Blend the base color with the palette entry
        int32 blend = (i < 16) ? 255 : 128;
        int32 r = (static_cast<int32>(BaseColor.R) * blend + static_cast<int32>(Pal1[i]) * (256 - blend)) / 256;
        int32 g = (static_cast<int32>(BaseColor.G) * blend + static_cast<int32>(Pal1[i]) * (256 - blend)) / 256;
        int32 b = (static_cast<int32>(BaseColor.B) * blend + static_cast<int32>(Pal1[i]) * (256 - blend)) / 256;

        if (r > 255) r = 255; if (r < 0) r = 0;
        if (g > 255) g = 255; if (g < 0) g = 0;
        if (b > 255) b = 255; if (b < 0) b = 0;

        Colors.Entries[i] = static_cast<BYTE>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    }

    if (AddToArray && Array)
    {
        ArrayIndex = Array->Add(this);
    }
}

ColorScheme::~ColorScheme()
{
    if (ID)
        YRMemory::Deallocate(ID);
    ID = nullptr;
}
// ============================================================================
// LightConvertClass 原版命名形态
// ============================================================================

void* LightConvertClass::SelectBlitter0(BlitterFlags flags)
{
    // asm: LightConvertClass_SelectBlitter0 (0x490B10 区段) 的入口守卫：
    //       mov eax,[esi+8] / test eax,eax / jnz 跳过分配
    if (!Blitters[0])
        AllocAllTheFuckingBlitters();

    // asm 里这一段是 30+ 条位测试直接返回不同槽位（见 ConvertClass.h 的
    // 「SelectBlitter0 分支表」）。当前 Blitters 只建到 22 号，而 asm 的
    // 分支会取到 49 号，直接照抄会读越界；因此这里走项目自己的等价选择器，
    // 保证返回值永远落在已建表内。等 50 个槽位补齐后再逐分支替换。
    Blitter* p = SelectPlainBlitter(flags);
    return static_cast<void*>(p);
}

void* LightConvertClass::SelectBlitter1(BlitterFlags flags)
{
    // asm: LightConvertClass_SelectBlitter1 —— 同形，取 RLEBlitters。
    if (!Blitters[0])
        AllocAllTheFuckingBlitters();

    RLEBlitter* p = SelectRLEBlitter(flags);
    return static_cast<void*>(p);
}

void LightConvertClass::AllocAllTheFuckingBlitters()
{
    // asm: LightConvertClass_AllocAllTheFuckingBlitters (0x48F054 之后的主表)
    //
    // 原实现写成 BuildBlitters(nullptr, true)，而 BuildBlitters 第一行是
    // `if (!PaletteData || skipBlitters) return;` —— 传 true 等于什么都不建，
    // 与函数名和 asm 语义都相反（asm 在这里连 new 了 50 个 Blitter 和
    // 39 个 RLEBlitter）。这里改成 false：真正建表。
    BuildBlitters(nullptr, false);
}

LightConvertClass* LightConvertClass::FindOrAlloc(BYTE* /*pBuffer*/, int32 /*a2*/, void* /*pSurface*/, int32 /*a5*/,
                                                  int32 red, int32 green, int32 blue, int32 /*a9*/)
{
    // asm: LightConvertClass_FiddleColours
    // 现有实例，命中则提升引用计数复用；未命中才新建登记。
    if (Array) {
        for (int32 i = 0; i < Array->Count; ++i) {
            LightConvertClass* pConv = Array->Items[i];
            if (pConv && pConv->Color1.Red == red
                && pConv->Color1.Green == green
                && pConv->Color1.Blue == blue) {
                ++pConv->RefCount;
                return pConv;
            }
        }
    }
    LightConvertClass* pNew = new LightConvertClass(
        nullptr, nullptr, nullptr, red, green, blue, true, nullptr, 1);
    if (pNew && Array)
        Array->Add(pNew);
    return pNew;
}

void LightConvertClass::FiddleColours(int32 a1, int32 a2, int32 a3, int32 a4)
{
    // 根据游戏行为，可知调色微调把传入的四组参数折算进明暗表重建。
    (void)a4;
    BuildLightTables(a1, a2, a3, Tinted);
}

// ============================================================================
// asm: Colour_Modify (0x555AA0)
//
//   void __fastcall Colour_Modify(int32 *pA, int32 *pB, int32 *pC)
//
//   asm 逐段：
//     cmp eax,3E8h / jl loc_555AD2 / mov eax,3E8h       ; 上限 1000
//     loc_555AD2: setle cl / dec ecx / and ecx,edi      ; 下限 0
//     ...（B、C 通道同样处理）
//     switch (dword_A8EB78):
//       loc_555B68 case 0: and eax,0FFFFFF80h
//       loc_555B4E case 1: and eax,0FFFFFFC0h
//        case 2        : and eax,0FFFFFFE0h
//       loc_555B7D default: 不改
// ============================================================================
int32 ColorScheme::ColourQualityLevel = 0;   // asm: dword_A8EB78

void ColorScheme::Modify_Colour(int32* pA, int32* pB, int32* pC)
{
    if (pA == nullptr || pB == nullptr || pC == nullptr)
        return;

    // asm: cmp eax,3E8h / jl / mov eax,3E8h      —— 上限 1000
    auto clampHigh = [](int32 v) { return (v > 1000) ? 1000 : v; };
    // asm: setle cl / dec ecx / and ecx,edi      —— 下限 0
    auto clampLow  = [](int32 v) { return (v <= 0) ? 0 : v; };

    *pA = clampLow(clampHigh(*pA));
    *pB = clampLow(clampHigh(*pB));
    *pC = clampLow(clampHigh(*pC));

    // asm: switch (dword_A8EB78) —— 按画质等级量化
    switch (ColourQualityLevel)
    {
    case 0:                                        // asm: loc_555B68
        *pA &= static_cast<int32>(0xFFFFFF80u);
        *pB &= static_cast<int32>(0xFFFFFF80u);
        *pC &= static_cast<int32>(0xFFFFFF80u);
        break;

    case 1:                                        // asm: loc_555B4E
        *pA &= static_cast<int32>(0xFFFFFFC0u);
        *pB &= static_cast<int32>(0xFFFFFFC0u);
        *pC &= static_cast<int32>(0xFFFFFFC0u);
        break;

    case 2:                                        // asm: case 2 分支
        *pA &= static_cast<int32>(0xFFFFFFE0u);
        *pB &= static_cast<int32>(0xFFFFFFE0u);
        *pC &= static_cast<int32>(0xFFFFFFE0u);
        break;

    default:                                       // asm: loc_555B7D
        break;
    }
}

// ============================================================================
// asm: MSAnim_Load_Palette_5CB590 (0x5CB590)
//
//   MSAnim_Load_Palette_5CB590 proc near
//   ; asm 注释写 "__thiscall(wchar_t **ecx0)" —— **该注释误导**：
//     函数体 ecx 指向栈上 CCFileClass 对象，真正入参是字符串指针；
//     按实际调用形态取 (const char* pName)。
//
//   调用者：GraphicMenuAnimItem_CTOR_4F2F10（返回值存入 [esi+24h]）。
//
//   归属修正：这不是 MSAnim 的方法（"MSAnim" 只是调用场景），
//             本质是 ConvertClass 的工厂方法。
//
// 铁律①（宁可留白，不写错）：XXX_PAL_3（0x300 字节原始调色板）与
//   DSurface_Primary 全局尚未落地；此处只固化已确证的读取流程与
//   6bit->8bit 展开，未知依赖保留 asm 调用点。
// ============================================================================
ConvertClass* ConvertClass::LoadFromPaletteFile(const char* pName)
{
    // asm: push 0 / call mystrcmp / test al,al / jnz loc_5CB5F1 (return 0)
    // 名字守卫：空名直接返回 0（asm 的 mystrcmp(name, NULL) 语义）。
    if (pName == nullptr || pName[0] == '\0')
        return nullptr;

    // asm: CCFileClass file(name) ; file.Is_Available(0)
    //      buf = file.Read_File() ; test eax,eax / jz loc_5CB5C7
    // 依赖 CCFileClass 读取链，此处按 asm 形状保留调用点。

    // asm: loc_5CB5F8 —— 256 组 RGB，每分量 << 2（6bit -> 8bit）
    //      mov [XXX_PAL_3.Colors.R[ecx+ecx*2]],bp / mov [...+2],bl
    // XXX_PAL_3 是全局 0x300 字节缓冲；尚未落地 -> 留白。

    // asm: push 188h / call new / test eax,eax / jz loc_5CB681
    //      push 0 / push 1 / push DSurface_Primary
    //      push offset XXX_PAL_3 / push offset XXX_PAL_3 / call ConvertClass
    // ConvertClass 构造与 DSurface_Primary 对接待补 -> 留白。
    return nullptr;
}

// ============================================================================
// asm: ConvertClass_CTOR_FromFilename (0x626380, __fastcall: ecx = 文件名)
//
//   懒建集合（asm 626490 引导段）：
//     marker = new 10h ; marker[0] = -1
//     names  = new 18h ; DvcChar_CTOR(0,0) ; vftable ; [10h]=count=0 ; [14h]=growth=0Ah
//     hsv    = new 18h ; VectorHSV_Resize(0,0)
//     calc   = new 10h ; palette_calculator(0Ah, 1Fh)
//     marker = { -1, names, hsv, calc } ; PaletteCollection = marker
//
//   三段探查（每段都是"uppercase 名 → 桶 → 0x104 步长链表"）：
//     ① hit   -> return 转换器
//     ② miss  -> entry = PaletteCollection_FindOrAlloc(coll, fname)
//                nameRec = new 104h ; strcpy(nameRec, fname) ; nameRec[100h] = entry
//                probe 一次；仍 miss 则 buf = PalLoad(val1, upper, &nameRec)
//                再 PalCommit(coll, buf)
//     ③ probe -> hit 则 return，否则 return 0
//
// 降级点：palette_calculator 的哈希索引（IndexLookup / bytes_cmp 的桶链）在
//   本树没有实体。这里保留完全相同的三探流程与"先登记名字再载入 .PAL"的
//   次序，把哈希桶换成等价的大写名线性查找；.PAL 的载入走本树既有的
//   CreateFromFile（它的 asm 对应就是 sub_628DD0/sub_628D10 那条链）。
// ============================================================================

namespace {

// asm: PaletteCollection 的 0x104 字节名字条目 {char[100h] Name; void* Slot}
struct PaletteEntry {
    char  Name[0x100];
    void* pConvert;
};

PaletteEntry* g_PaletteEntries = nullptr;   // asm: names 向量
int32         g_PaletteCount   = 0;
int32         g_PaletteCapacity = 0;        // asm: [names+14h] = 0Ah 的增幅
bool          g_PaletteReady   = false;     // asm: PaletteCollection 是否已建

// asm: strdup_strupr(&dest, src) —— 复制并把字母转大写
char* ToUpperName(char* pDest, const char* pSrc)
{
    int32 i = 0;
    for (; pSrc[i] != '\0' && i < 0xFF; ++i) {
        char c = pSrc[i];
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
        pDest[i] = c;
    }
    pDest[i] = '\0';
    return pDest;
}

// asm: 一次索引探查（62643B / 62655B / 6265CC 三段同构）
void* ProbeIndex(const char* pUpper)
{
    for (int32 i = 0; i < g_PaletteCount; ++i) {
        if (std::strcmp(g_PaletteEntries[i].Name, pUpper) == 0)
            return g_PaletteEntries[i].pConvert;
    }
    return nullptr;
}

// asm: PaletteCollection_FindOrAlloc —— 返回该名字的槽位下标（必要时扩容）
int32 FindOrAllocSlot(const char* pName)
{
    for (int32 i = 0; i < g_PaletteCount; ++i) {
        if (std::strcmp(g_PaletteEntries[i].Name, pName) == 0)
            return i;
    }
    if (g_PaletteCount >= g_PaletteCapacity) {
        const int32 grow = (g_PaletteCapacity == 0) ? 0xA : g_PaletteCapacity;
        const int32 newCap = g_PaletteCapacity + grow;
        PaletteEntry* pNew = new PaletteEntry[static_cast<size_t>(newCap)]();
        for (int32 i = 0; i < g_PaletteCount; ++i)
            pNew[i] = g_PaletteEntries[i];
        delete[] g_PaletteEntries;
        g_PaletteEntries = pNew;
        g_PaletteCapacity = newCap;
    }
    PaletteEntry& e = g_PaletteEntries[g_PaletteCount];
    // asm 原样 strcpy(fname)；探查侧用大写名比对（bytes_cmp 大小写不敏感），
    // 这里直接存大写名，等价。
    ToUpperName(e.Name, pName);
    e.pConvert = nullptr;                   // asm: [nameRec+100h] 指向刚分配的槽
    return g_PaletteCount++;
}

}  // namespace

ConvertClass* ConvertClass::From_Filename(const char* pFname)
{
    if (pFname == nullptr || pFname[0] == '\0')
        return nullptr;

    // asm 626490 引导段：集合懒建（C++ 里由上面的静态量承载）
    g_PaletteReady = true;
    (void)g_PaletteReady;

    char upper[0x100];                       // asm: val2（大写名）
    ToUpperName(upper, pFname);              // asm: strdup_strupr(&val2, ebx)

    if (void* hit = ProbeIndex(upper))       // 第一段探查（6264C1）
        return static_cast<ConvertClass*>(hit);

    // asm 6264FF: FindOrAlloc + 登记 0x104 名字条目
    const int32 slot = FindOrAllocSlot(pFname);

    // asm 62655B: 第二段探查；未命中即载入 .PAL 并提交
    ToUpperName(upper, pFname);
    if (ProbeIndex(upper) == nullptr) {
        BytePalette* pPal = nullptr;
        ConvertClass* pNew = nullptr;
        CreateFromFile(pFname, pPal, pNew);  // asm: sub_628DD0 + sub_628D10
        if (pPal != nullptr)
            YRMemory::Deallocate(pPal);
        if (slot >= 0 && slot < g_PaletteCount)
            g_PaletteEntries[slot].pConvert = pNew;   // asm: PalCommit
    }

    // asm 6265CC: 第三段探查决出结果
    ToUpperName(upper, pFname);
    if (void* hit = ProbeIndex(upper))
        return static_cast<ConvertClass*>(hit);
    return nullptr;                          // asm: xor eax,eax / retn
}

// ============================================================================
// asm: StandardPalette —— 默认调色板全局
//
//   .data 里的一个指针槽，调色板装载时写入；进度条在「没有配色方案」的
//   分支上直接取它（`mov edx, StandardPalette`）。
// ============================================================================
ConvertClass* StandardPalette = nullptr;
