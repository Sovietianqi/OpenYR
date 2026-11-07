#pragma once

#include "Core/Definitions.h"

// ============================================================================
// LCW (Westwood Format80) 解压
//
// 从 FileFormats/SHP.cpp 抽出到此，原因：它是所有 SHP 帧解码的公共前置，
// 且不依赖 SHPStruct / CCFileClass 任何一方，独立成编译单元后可以直接
// 用 tools/test_lcw.cpp 做回归（SHP.cpp 会牵进整个 IO 层，链接代价太大）。
//
// asm 依据：解码器在二进制里内联了两份，指令派发完全一致：
//   sub_7C812F  -- codec reader sub_7BEA80 用的独立副本
//   sub_551C60  -- LCWStraw::Get
//
//   0x00..0x7F  short back-copy   count=((cmd>>4)&7)+3, off=((cmd&0xF)<<8)|b
//   0x80        end of stream
//   0x81..0xBF  literal run       count=cmd&0x3F  （没有 +3，注意）
//   0xC0..0xFD  segment copy      count=(cmd&0x3F)+3, off=word（相对输出起点）
//   0xFE        long fill         count=word, value=byte
//   0xFF        long copy         count=word, off=word（相对输出起点）
//
// 返回写入字节数；输入非法 / 越出输出窗口时返回 -1。
// 三参重载没有源长度（原版也没有），无法检测源耗尽；已知块长度时优先用四参。
// ============================================================================
int32 LCW_Uncompress(const uint8* pSrc, int32 srcLen, uint8* pDst, int32 dstLen);
int32 LCW_Uncompress(const uint8* pSrc, uint8* pDst, int32 dstLen);
