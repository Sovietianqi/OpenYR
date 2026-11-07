#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class BitTextClass;

// ----------------------------------------------------------------------------
// BitFontStruct —— asm: BitText_LoadFNTFile 返回的 0x24 字节字模描述符
//
//   +0x00 / +0x04 / +0x08 / +0x0C   FNT 头直接映射的四个字段
//   +0x10  CharCount   已登记字符数（>0Ah 出口用）
//   +0x14  CharSize    每字符字节数
//   +0x18  GlyphIndex  0x10000 项 uint16 字模索引（new 20000h）
//   +0x1C  CharData    打包字模数据（new count*CharSize）
//   +0x20  GlyphCount  索引中非零项计数
// ----------------------------------------------------------------------------
struct BitFontStruct {
    int32   Field_00;
    int32   Field_04;
    int32   Field_08;
    int32   Field_0C;
    int32   CharCount;
    int32   CharSize;
    uint16* GlyphIndex;
    uint8*  CharData;
    int32   GlyphCount;
};

// ----------------------------------------------------------------------------
// asm: BitText_LoadFNTFile (0x433A80 区, ecx = 文件名)
//   读 Westwood 的 .FNT / .fonT 字体：构造 0x24 字节描述符 + 0x20000 字节
//   字模索引，按魔术字分派旧式 'fonT'（固定 1Ch 头，字段直映）或新式
//   'FoNt'（24h 头 + 每字符 12 字节的区间表，把每个 [first,last] 折进索引
//   并累计字符数），最后读入打包字模数据。失败返回 0。
//   调用者唯一：BitTextClass 构造函数。
// ----------------------------------------------------------------------------
BitFontStruct* BitText_LoadFNTFile(const char* pFname);

// ============================================================================
// BitText - GAME.FNT bitmap font loader
//
//  asm BitText_LoadGameFNT (0x434AA0) is called once from InitGame:
//    esi = ecx (this/ignored)
//    [esi] = BitText::vftable          ; stash vtable marker
//    eax = new 0x44h                   ; sizeof(BitTextClass) = 0x44
//    test eax,eax / jz fail
//    ecx = eax / push "GAME.FNT" / call BitText_CTOR
//    GameFont = eax / mov eax, esi / retn
//  fail:
//    GameFont = 0 / mov eax, esi / retn
// ============================================================================

// ----------------------------------------------------------------------------
// BitTextClass - the 0x44-byte font object.  Layout mirrors the original
// class as far as the loader touches it (the ctor parses the FNT file).
// ----------------------------------------------------------------------------
class BitTextClass {
public:
    explicit BitTextClass(const char* pFilename) noexcept;

    ~BitTextClass();

    int32 Get_Char_Width(char ch) const;
    int32 Get_Char_Height() const { return CharHeight; }

    // ---- original field offsets -------------------------------------------
    BitFontStruct* Field_04;   // asm +0x04 字模描述符（BitText_LoadFNTFile 的返回值）
    int32   Field_08;       // asm +0x08
    int32   Field_0C;       // asm +0x0C
    int32   Field_10;       // asm +0x10
    int32   Field_14;       // asm +0x14
    int32   Field_18;       // asm +0x18
    int32   Field_1C;       // asm +0x1C
    int32   Field_20;       // asm +0x20
    int32   Field_24;       // asm +0x24
    int32   Field_28;       // asm +0x28
    int32   Field_2C;       // asm +0x2C
    int32   Field_30;       // asm +0x30
    int32   Field_34;       // asm +0x34
    int32   Field_38;       // asm +0x38
    int32   CharHeight;     // asm +0x3C
    int32   Field_40;       // asm +0x40
};

// ----------------------------------------------------------------------------
// asm: BitText_LoadGameFNT (0x434AA0) -- installs the global GameFont.
// Returns the loader's `this` (an opaque marker in the original); the font
// itself lands in the GameFont global.
// ----------------------------------------------------------------------------
void* BitText_LoadGameFNT();

// ----------------------------------------------------------------------------
// asm global: GameFont (dword_7E0F4C) -- the loaded GAME.FNT font, null
// when loading failed.
// ----------------------------------------------------------------------------
BitTextClass* Get_GameFont();
