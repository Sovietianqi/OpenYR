// ============================================================================
// BlitterSlot.h —— LightConvertClass::SelectBlitter0 的槽位解码（纯数据 + 纯函数）
//
// 单独成头文件有两个原因：
//   1) 槽位表就是 asm 0x490B9F..0x490E40 的转写，必须能单独对拍；
//   2) ConvertClass.cpp 依赖太多，测试不该为了一个分支树去链半个引擎。
//
// 位值来源与证据见 Rendering/Surface.h 里 BlitterFlags 的注释。
// ============================================================================
#ifndef OPENYR_RENDERING_BLITTERSLOT_H
#define OPENYR_RENDERING_BLITTERSLOT_H

#include <cstdint>

// 只依赖 <cstdint>：本文件要能被最小测试程序单独包含。
using int32 = std::int32_t;
using uint32 = std::uint32_t;

namespace BlitterSlot
{

// 下标 = ConvertClass::Blitters[] 的数组下标（该数组只有 50 个元素，见
// ConvertClass.h:144）。原 asm 的槽 32/33/34 落在 Blitters[47..49]：
// 47~49 是数组里仅剩的空位（0..46 已被占用），改成 50/51/52 会越界。
constexpr int32 SlotToBlitter[50] =
{
    /* 0  BlitPlainXlat              */ 16,
    /* 1  BlitTransXlat              */  0,
    /* 2  BlitTransDarken            */  6,
    /* 3  BlitTransZRemapXlat        */  2,
    /* 4  BlitTransLucent75          */  5,
    /* 5  BlitTransLucent50          */  4,
    /* 6  BlitTransLucent25          */  3,
    /* 7  BlitPlainXlatZRead         */ 20,
    /* 8  BlitTransXlatZRead         */ 11,
    /* 9  BlitTransDarkenZRead       */ 17,
    /* 10 BlitTransZRemapXlatZRead   */ 25,
    /* 11 BlitTransLucent75ZRead     */ 14,
    /* 12 BlitTransLucent50ZRead     */ 13,
    /* 13 BlitTransLucent25ZRead     */ 12,
    /* 14 BlitTransLucent75ZReadWarp */ 30,
    /* 15 BlitTransLucent50ZReadWarp */ 31,
    /* 16 BlitTransLucent25ZReadWarp */ 32,
    /* 17 BlitPlainXlatZReadWrite    */ 21,
    /* 18 BlitTransXlatZReadWrite    */ 15,
    /* 19 BlitTransDarkenZReadWrite  */ 18,
    /* 20 BlitTransZRemapXlatZReadWrite */ 26,
    /* 21 BlitTransLucent75ZReadWrite   */ 33,
    /* 22 BlitTransLucent50ZReadWrite   */ 34,
    /* 23 BlitTransLucent25ZReadWrite   */ 35,
    /* 24 BlitPlainXlatAlpha         */ 19,
    /* 25 BlitTransXlatAlpha         */  7,
    /* 26 BlitTransZRemapXlatAlpha   */ 27,
    /* 27 BlitTransLucent75Alpha     */ 10,
    /* 28 BlitTransLucent50Alpha     */  9,
    /* 29 BlitTransLucent25Alpha     */  8,
    /* 30 BlitTransXlatWriteAlpha    */ 45,
    /* 31 BlitTransXlatMultWriteAlpha*/ 46,
    /* 32 BlitTranslucentWriteAlpha  */ 47,
    /* 33 BlitTranslucent50NonzeroAlpha */ 48,
    /* 34 BlitTranslucent50ZeroAlpha */ 49,
    /* 35 BlitPlainXlatAlpha         */ 19,
    /* 36 BlitTransXlatAlphaZRead    */ 23,
    /* 37 BlitTransZRemapXlatAlphaZRead */ 28,
    /* 38 BlitTransLucent75AlphaZRead   */ 36,
    /* 39 BlitTransLucent50AlphaZRead   */ 37,
    /* 40 BlitTransLucent25AlphaZRead   */ 38,
    /* 41 BlitTransLucent75AlphaZReadWarp */ 39,
    /* 42 BlitTransLucent50AlphaZReadWarp */ 40,
    /* 43 BlitTransLucent25AlphaZReadWarp */ 41,
    /* 44 BlitPlainXlatAlpha         */ 19,
    /* 45 BlitTransXlatAlphaZReadWrite  */ 24,
    /* 46 BlitTransZRemapXlatAlphaZReadWrite */ 29,
    /* 47 BlitTransLucent75AlphaZReadWrite  */ 42,
    /* 48 BlitTransLucent50AlphaZReadWrite  */ 43,
    /* 49 BlitTransLucent25AlphaZReadWrite  */ 44,
};

// asm 槽号 → 名字，仅用于未实现槽的告警。
inline const char* const Names[50] =
{
    "BlitPlainXlat",                  "BlitTransXlat",
    "BlitTransDarken",                "BlitTransZRemapXlat",
    "BlitTransLucent75",              "BlitTransLucent50",
    "BlitTransLucent25",              "BlitPlainXlatZRead",
    "BlitTransXlatZRead",             "BlitTransDarkenZRead",
    "BlitTransZRemapXlatZRead",       "BlitTransLucent75ZRead",
    "BlitTransLucent50ZRead",         "BlitTransLucent25ZRead",
    "BlitTransLucent75ZReadWarp",     "BlitTransLucent50ZReadWarp",
    "BlitTransLucent25ZReadWarp",     "BlitPlainXlatZReadWrite",
    "BlitTransXlatZReadWrite",        "BlitTransDarkenZReadWrite",
    "BlitTransZRemapXlatZReadWrite",  "BlitTransLucent75ZReadWrite",
    "BlitTransLucent50ZReadWrite",    "BlitTransLucent25ZReadWrite",
    "BlitPlainXlatAlpha",             "BlitTransXlatAlpha",
    "BlitTransZRemapXlatAlpha",       "BlitTransLucent75Alpha",
    "BlitTransLucent50Alpha",         "BlitTransLucent25Alpha",
    "BlitTransXlatWriteAlpha",        "BlitTransXlatMultWriteAlpha",
    "BlitTranslucentWriteAlpha",      "BlitTranslucent50NonzeroAlpha",
    "BlitTranslucent50ZeroAlpha",     "BlitPlainXlatAlpha",
    "BlitTransXlatAlphaZRead",        "BlitTransZRemapXlatAlphaZRead",
    "BlitTransLucent75AlphaZRead",    "BlitTransLucent50AlphaZRead",
    "BlitTransLucent25AlphaZRead",    "BlitTransLucent75AlphaZReadWarp",
    "BlitTransLucent50AlphaZReadWarp","BlitTransLucent25AlphaZReadWarp",
    "BlitPlainXlatAlpha",             "BlitTransXlatAlphaZReadWrite",
    "BlitTransZRemapXlatAlphaZReadWrite", "BlitTransLucent75AlphaZReadWrite",
    "BlitTransLucent50AlphaZReadWrite",   "BlitTransLucent25AlphaZReadWrite",
};


inline int32 Decode(uint32 f)
{

    constexpr uint32 ZRead     = 0x00000010;
    constexpr uint32 Darken    = 0x00000001;
    constexpr uint32 Plain     = 0x00000020;
    constexpr uint32 Warp      = 0x00000008;
    constexpr uint32 Alpha     = 0x00000800;
    constexpr uint32 ZWrite    = 0x00004000;
    constexpr uint32 GradMask  = 0x00003000;   // blitter_1000_or_2000_
    constexpr uint32 Level25   = 0x00000002;
    constexpr uint32 Level50   = 0x00000004;
    constexpr uint32 Level75   = 0x00000006;

    // ── ① flags & 0x10：ZRead 三分支 ────────────────────────────────
    if (f & ZRead)
    {
        if (f & ZWrite)  return (f & Alpha) ? 46 : 20;
        if (f & GradMask) return (f & Alpha) ? 37 : 10;
        return (f & Alpha) ? 26 : 3;
    }

    // ── ② 半透明档 ─────────────────────────────────────────────────
    const uint32 level = f & Level75;   // 0 / 2 / 4 / 6
    if (level == Level25)
    {
        if (f & ZWrite)  return (f & Alpha) ? 49 : 23;
        if (f & GradMask)
        {
            if (f & Warp) return (f & Alpha) ? 43 : 16;
            return (f & Alpha) ? 40 : 13;
        }
        return (f & Alpha) ? 29 : 6;
    }
    if (level == Level50)
    {
        if (f & ZWrite)  return (f & Alpha) ? 48 : 22;
        if (f & GradMask)
        {
            if (f & Warp) return (f & Alpha) ? 42 : 15;
            return (f & Alpha) ? 39 : 12;
        }
        if (f & Alpha)   return 28;
        if (f & 0x00010000) return 34;
        if (f & 0x00020000) return 33;
        return 5;
    }
    if (level == Level75)
    {
        if (f & ZWrite)  return (f & Alpha) ? 47 : 21;
        if (f & GradMask)
        {
            if (f & Warp) return (f & Alpha) ? 41 : 14;
            return (f & Alpha) ? 38 : 11;
        }
        return (f & Alpha) ? 27 : 4;
    }

    // ── ③ 不透明档（flags & 6 == 0）────────────────────────────────
    if (f & Darken)
    {
        // asm 的 darken 分支**不看** Alpha 位。
        if (f & ZWrite)  return 19;
        if (f & GradMask) return 9;
        return 2;
    }
    if (f & Plain)
    {
        if (f & ZWrite)  return (f & Alpha) ? 45 : 18;
        if (f & GradMask) return (f & Alpha) ? 36 : 8;
        if (f & Alpha)   return 24;
        return 0;
    }

    if (f & ZWrite)  return (f & Alpha) ? 45 : 18;
    if (f & GradMask) return (f & Alpha) ? 36 : 8;
    if (f & 0x00008000) return 30;
    if (f & 0x00000100) return 31;
    if (f & 0x00000040) return 32;
    if (f & Alpha)   return 25;
    return 1;
}

} // namespace BlitterSlot

#endif // OPENYR_RENDERING_BLITTERSLOT_H
