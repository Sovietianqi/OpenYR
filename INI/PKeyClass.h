#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

// ============================================================================
// PKeyClass / XMP -- the public-key blob codec used by the INI layer.
//
// asm 实体（经回 asm 核实，地址与签名均以 asm 原文为准）：
//
//   ; void __fastcall Key_Voodoo1(byte *dest, byte *key, int len)   0x5C6350
//   ; int  __fastcall Key_Voodoo3(byte *dest, int len)              0x5C6960
//   ; int  __fastcall XMP_Init(byte *buf, int startval, int len)
//   XMP_DER_Encode  proc near     ; CODE XREF: INIClass::Get_PKey+60 …  0x5C6260
//
// 调用链（asm 原文）：
//   PKey::Decode_Modulus   -> Key_Voodoo1(dest, key, 0x40) ; Key_Voodoo3(dest,0x40)
//   PKey::Decode_Exponent  -> XMP_Init(buf*2) ; Key_Voodoo1 ; Key_Voodoo3
//   sub_631F60             -> XMP_Init(buf) ; XMP_Init(buf+0x100) ; Key_Voodoo1 x2
//                          ; Key_Voodoo3 ; [obj+0x200] = ret - 1
//   INIClass::Get_PKey     -> XMP_DER_Encode(pSrc, pDest, 0x40)
//
// 归属说明（修正 AsmComplex3/7 的错误归属）：
//   * 这一组都属**密钥编解码层**，不属于 INIClass —— `INIClass::Get_PKey`
//     只是 XMP_DER_Encode 的调用者。
//   * AsmComplex3 把 XMP_DER_Encode 标 0x5C6260（正确），AsmComplex7 标
//     Plain_Text_Print 0x4A6700 / HouseType 0x4755B0（均错，见各自文件）。
//
// 铁律①（宁可留白，不写错）：Key_Voodoo3 / XMP_Init / PKey::Decode_Modulus /
// PKey::Decode_Exponent / sub_631F60 的 asm 正文尚未逐句回填，此处只固化
// 已核实到的签名与调用关系，不臆造实现。
// ============================================================================

class PKeyClass {
public:
    // asm: PKey 对象是 0x204 字节（[obj+0x200] 存 rsaBits-1）
    static constexpr int32 ObjectSize = 0x204;

    // asm 字段偏移
    uint8  Buffer0[0x100];       // asm +0x000  主 XMP 缓冲
    uint8  Buffer1[0x100];       // asm +0x100  副 XMP 缓冲
    int32  KeyBits;              // asm +0x200  (= Key_Voodoo3 返回值 - 1)
};

// ----------------------------------------------------------------------------
// Key_Voodoo1 (0x5C6350) -- Type-2 密钥记录解码器
//
// asm 原文签名（一字不改）：
//   ; void __fastcall Key_Voodoo1(byte *dest, byte *key, int len)
//
//   record = { 0x02, count(1-2 bytes), fillflag+data... }
//   b1 = record[1]:
//     b1 < 0x80            -> count = b1,                     data at +2
//     b1 >= 0x80           -> hi = b1 & 7F (must be 1..2)
//                             hi == 1 -> count = record[2],     data at +3
//                             hi == 2 -> count = record[2]<<8|record[3], data at +4
//   region = len*4 bytes; guard: count > len*4 (unsigned) -> no-op
//   fill = (data[0] & 0x80) ? 0xFF : 0x00
//   buffer[0 .. len*4-count) = fill ; buffer[len*4-count .. len*4) = data
//   （两段都是从区域末端向低地址写入，与原文一致）
// ----------------------------------------------------------------------------
void Key_Voodoo1(uint8* pDest, const uint8* pKey, int32 len);

// ----------------------------------------------------------------------------
// XMP_DER_Payload (0x5C6490) -- DER INTEGER 最小化
//
// asm: sub_5C6490  __fastcall(ecx = pDest, edx = pSrc, [esp+4] = len)
//
//   len 为 dword 数（数据区 4*len 字节，小端大整数）。
//   negative = ((int32*)pSrc)[len-1] < 0
//   fill     = negative ? 0xFF : 0x00
//   从 pSrc[4*len-1] 向前跳过与 fill 相同的字节，得到最高有效字节 i。
//   若 (pSrc[i] & 0x80) != negative，先输出一个 fill。
//   然后 pSrc[i] 降序逐字节输出。
//   返回输出字节数。
//
// 这是 XMP_DER_Encode 里 tag/长度头之后那段载荷的来源。
// ----------------------------------------------------------------------------
int32 XMP_DER_Payload(uint8* pDest, const uint8* pSrc, int32 len);

// ----------------------------------------------------------------------------
// XMP_DER_Encode (0x5C6260) -- DER 长度包裹
//
// asm 无 `;` 类型注释行，proc 原文：
//   XMP_DER_Encode  proc near  ; CODE XREF: INIClass::Get_PKey+60 ...
// 按寄存器流向判定为 __fastcall(src=ecx, dest=edx, srcLen=[esp+4])。
//
//   N = sub_5C6490(buf[0x100], src, srcLen)      -- 见 XMP_DER_Payload
//   dest[0] = 2                                  (DER tag)
//   N <= 0x7F -> dest[1] = N,               lenBytes = 1
//   else      -> lenBytes = N 的有效字节数
//                dest[1] = 0x80 | lenBytes
//                dest[2..] = N 大端（lenBytes 字节）
//   dest[1+lenBytes ..] = buf[0..N)
//   return 1 + lenBytes + N                      (写入字节数)
// ----------------------------------------------------------------------------
int32 XMP_DER_Encode(const uint8* pSrc, uint8* pDest, int32 srcLen);
