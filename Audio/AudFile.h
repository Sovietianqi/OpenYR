#pragma once

// ============================================================================
// AUD —— Westwood 音频容器（RA2 / YR / TS 的 .aud）
//
//   布局（12 字节头 + 负载）：
//     +0  u16 SampleRate
//     +2  u32 CompressedSize      —— 头之后的字节数（含块头）
//     +6  u32 OutputSize          —— 解码后的 PCM 字节数
//     +10 u8  Flags               —— bit0 立体声 / bit1 16 位
//     +11 u8  Compression         —— 0 原始 PCM / 1 WW ADPCM / 99 IMA ADPCM
//
//   负载有两种组织方式，本实现两种都认，并用 OutputSize 做交叉校验：
//     * 分块：每块 8 字节头 { u16 csize, u16 osize, u32 0xDEAF } + csize 字节。
//       WW ADPCM（type 1）与部分 IMA 文件用这种。
//     * 连续：整个负载就是一串 ADPCM 半字节，没有块头（RA2 的 type 99 常见）。
//
//   解码器与 OpenRA `OpenRA.Game/FileFormats/AudLoader.cs`（GPL-3.0）以及
//   MultimediaWiki 的 Westwood IMA ADPCM 条目一致：
//     - 每字节 = 2 个半字节，**低半字节在前**；
//     - 预测值与步长索引初值均为 0，跨块保持；
//     - 立体声按字节交错（偶数字节左、奇数字节右），两声道各自维护状态；
//     - 4 位码：bit3 符号，bit0..2 幅度。
//
//   输出统一为 16 位有符号小端 PCM（type 1 的 8 位样本左移 8 位）。
// ============================================================================

#include "../Core/Definitions.h"
#include <vector>

struct AudDecoded
{
    bool               Ok          = false;
    int32              SampleRate  = 0;
    int32              Channels    = 1;
    int32              BitsPerSample = 16;
    const char*        Error       = nullptr;   // 静态字符串，不为空即失败原因
    std::vector<uint8> PCM;                     // 16 位 LE

    // 诊断：实际走的是哪条解码路径。
    const char*        Path        = "";
};

// 12 字节头 + 至少 1 字节负载。用于 VocClass::Load 的格式嗅探。
bool Is_AUD_Data(const uint8* pData, int32 nSize);

// 完整解码。失败时 Ok=false、Error 指向静态原因（不抛异常）。
AudDecoded Decode_AUD(const uint8* pData, int32 nSize);
