#include "BitText.h"
#include "../Core/Memory.h"
#include "IO/CCFileClass.h"

#include <cstdlib>
#include <cstring>

// ============================================================================
// BitText implementations
// ============================================================================

// asm: GameFont (dword_7E0F4C)
static BitTextClass* g_GameFont = nullptr;

BitTextClass* Get_GameFont()
{
    return g_GameFont;
}

// ============================================================================
// asm: BitText_LoadFNTFile (0x433A80 区, ecx = 文件名)
//
//   ebx = CCFileClass(new 6Ch) ; esi = 描述符(new 24h) ; 索引 = new 20000h
//   eax = 魔术字分派：'fonT' 固定 1Ch 头、'FoNt' 24h 头 + 12 字节区间表
//
// 局部变量按 dump 的栈槽命名：
//   var_40..var_28  1Ch 旧式头
//   [esp+30h]       24h 新式头（0x24 字节）
//   var_44          区间表（malloc 出来的）
// ============================================================================
BitFontStruct* BitText_LoadFNTFile(const char* pFname)
{
    BitFontStruct* font = nullptr;                      // esi role

    CCFileClass file(pFname);                           // asm: push 6Ch / new / CCFileClass_CTOR

    if (!file.Is_Available(0))                          // asm: vt[+14h](0) / test al
        goto done;                                      // asm: lct_Done
    if (!file.OpenFile(1))                              // asm: vt[+1Ch](1) / test eax
        goto done;

    font = static_cast<BitFontStruct*>(YRMemory::Allocate(0x24));   // asm: push 24h / new
    if (!font)                                          // asm: cmp esi,ebp / jz lct_Done
        goto done;
    std::memset(font, 0, 0x24);                         // asm: mov ecx,9 / rep stosd

    {
        uint16* glyph = static_cast<uint16*>(YRMemory::Allocate(0x20000));  // asm: push 20000h
        if (!glyph)                                     // asm: jz lct_ErrOut
            goto errout;
        std::memset(glyph, 0, 0x20000);                 // asm: mov ecx,8000h / rep stosd
        font->GlyphIndex = glyph;                       // asm: [esi+18h]

        uint8 hdr[0x1C];                                // asm: var_40..var_28
        if (file.ReadNextBytes(hdr, 0x1C) != 0x1C)      // asm: cmp eax,1Ch / jnz errout
            goto errout;

        const int32 magic = *reinterpret_cast<const int32*>(hdr);
        uint8 nh[0x24];                                 // asm: [esp+30h] 新式头
        int32 nChars = 0;
        uint8* entries = nullptr;                       // asm: var_44

        if (magic == 0x546E6F66) {                      // 'fonT'
            // 固定头：4..18h 直接映射到描述符对应槽。
            font->Field_08 = *reinterpret_cast<const int32*>(hdr + 0x0C);
            font->Field_0C = *reinterpret_cast<const int32*>(hdr + 0x10);
            font->Field_04 = *reinterpret_cast<const int32*>(hdr + 0x08);
            font->Field_00 = *reinterpret_cast<const int32*>(hdr + 0x04);
            font->CharSize = *reinterpret_cast<const int32*>(hdr + 0x18);
            font->CharCount = *reinterpret_cast<const int32*>(hdr + 0x14);
            if (file.ReadNextBytes(glyph, 0x20000) != 0x20000)
                goto errout;
            goto readchars;
        }
        if (magic != 0x744E6F46)                        // 'FoNt'
            goto errout;

        file.MoveFilePointer(0, 0);                     // asm: push ebp / push ebp / vt+28h
        if (file.ReadNextBytes(nh, 0x24) != 0x24)
            goto errout;

        font->Field_0C = *reinterpret_cast<const int32*>(nh + 0x10);
        font->Field_04 = *reinterpret_cast<const int32*>(nh + 0x08);
        font->CharSize = *reinterpret_cast<const int32*>(nh + 0x18);
        font->Field_08 = *reinterpret_cast<const int32*>(nh + 0x0C);
        font->Field_00 = *reinterpret_cast<const int32*>(nh + 0x04);
        nChars = *reinterpret_cast<const int32*>(nh + 0x14);

        // asm: lea ecx,[eax+eax*2] / shl ecx,2 —— nChars*12 的区间表
        entries = static_cast<uint8*>(std::malloc(
            static_cast<size_t>(static_cast<uint32>(nChars)) * 12u));
        if (!entries)                                   // asm: cmp edi,ebp / jz errout
            goto errout;
        file.MoveFilePointer(*reinterpret_cast<const int32*>(nh + 0x1C), 0);
        if (file.ReadNextBytes(entries, nChars * 12) != nChars * 12)
            goto errout;
        font->CharCount = 0;                            // asm: [esi+10h] = 0

        // 每个 12 字节区间 {word base, dword first, dword last} 折进索引
        // （asm loc_433B39 / loc_433B47）。
        {
            int32 used = 0;                             // [esi+10h]
            for (int32 i = 0; i < nChars; ++i) {
                const uint8* e = entries + i * 12;
                const int32 first = *reinterpret_cast<const int32*>(e + 4);
                const int32 last = *reinterpret_cast<const int32*>(e + 8);
                if (first > last)                       // asm: cmp eax,[ecx] / jg
                    continue;
                int32 ch = first;
                int32 val = static_cast<int16>(
                    *reinterpret_cast<const int16*>(e)) + 1;   // asm: lea edx,[edi+1]
                while (ch <= last) {                    // asm: loc_433B47
                    font->GlyphIndex[ch] = static_cast<uint16>(val);
                    ++used;                             // asm: inc [esi+10h]
                    ++ch;
                    ++val;                              // asm: inc edx
                }
            }
            font->CharCount = used;
        }
        std::free(entries);                             // asm: push edi / call _free
        entries = nullptr;
        file.MoveFilePointer(*reinterpret_cast<const int32*>(nh + 0x20), 0);

readchars:                                             // asm: lct_ReadCharacters
        {
            const int32 count = font->CharCount;        // [esi+10h]
            const int32 itemSize = font->CharSize;      // [esi+14h]
            const int32 dataLen = count * itemSize;     // asm: imul edi,[esi+14h]
            int32 nonzero = 0;                          // asm: [esi+20h]
            for (int32 i = 0; i < 0x10000; ++i) {
                if (font->GlyphIndex[i] != 0)
                    ++nonzero;
            }
            font->GlyphCount = nonzero;
            uint8* data = static_cast<uint8*>(
                YRMemory::Allocate(static_cast<uint32>(dataLen)));
            if (!data)                                  // asm: cmp eax,ebp / jz errout
                goto errout;
            font->CharData = data;                      // asm: [esi+1Ch]
            if (file.ReadNextBytes(data, dataLen) == dataLen)
                goto done;                              // asm: cmp eax,edi / jz lct_Done
            goto errout;
        }
    }

errout:                                                // asm: lct_ErrOut
    if (font->GlyphIndex)                              // 0x18 字模索引
        YRMemory::Deallocate(font->GlyphIndex);
    if (font->CharData)                                // 0x1C 打包数据
        YRMemory::Deallocate(font->CharData);
    YRMemory::Deallocate(font);
    font = nullptr;                                    // asm: xor esi,esi

done:                                                  // asm: lct_Done
    file.Close();                                      // asm: push 1 / call [eax]（delete file）
    return font;                                       // asm: loc_433BF6 mov eax,esi
}

// ----------------------------------------------------------------------------
// BitTextClass ctor —— asm: BitText_CTOR (0x4338A0)
//
//   [esi]     = BitFont::vftable
//   [esi+4]   = 0 ; word[+24h] = 7FFFh ; word[+26h] = 3555h ; dword[+28h] = 40h
//   byte[+40h] = byte[+41h] = 1 ; [+18h] = [+1Ch] = 1 ; [+20h] = [+0Ch] = [+8] = 0
//   SetRect(&this+30h, 0,0,0,0) ; [+2Ch] = 1
//   eax = BitText_LoadFNTFile(fname) ; [esi+4] = eax
//   if (eax) { [esi+18h] = [eax+4] ; [esi+1Ch] = [eax+0Ch] ; sub_434700(this) }
//   else     { 复位同上一组初值，重新 SetRect }
// ----------------------------------------------------------------------------
BitTextClass::BitTextClass(const char* pFilename) noexcept
    : Field_04(nullptr)
    , Field_08(0)
    , Field_0C(0)
    , Field_10(0)
    , Field_14(0)
    , Field_18(0)
    , Field_1C(0)
    , Field_20(0)
    , Field_24(0x35557FFF)   // asm: word[+24h] = 7FFFh, word[+26h] = 3555h
    , Field_28(0x40)
    , Field_2C(1)
    , Field_30(0)
    , Field_34(0)
    , Field_38(0)
    , CharHeight(0)
    , Field_40(1)
{
    if (pFilename == nullptr)
        return;

    Field_04 = BitText_LoadFNTFile(pFilename);          // asm: call BitText_LoadFNTFile
    if (Field_04 != nullptr) {
        Field_18 = Field_04->Field_04;                  // asm: mov ecx,[eax+4] / [esi+18h]
        Field_1C = Field_04->Field_0C;                  // asm: mov edx,[eax+0Ch] / [esi+1Ch]
        // 降级点：asm 随后 call sub_434700(this)（按字模重建位图属性），
        // 本树尚无该子程序，字模已完整落在描述符里，留待渲染层接入。
    }
    // asm: loc_4338FB —— 失败分支只把上面那组初值再写一遍，此处构造已置同值。
}

// asm: BitText_DTOR (0x433940)
//   free([esi+8]) ; esi = [esi+4] ; if (esi) { free([esi+18h]) ; free([esi+1Ch]) ; free(esi) }
BitTextClass::~BitTextClass()
{
    // asm 先无条件释放 [esi+8]（本树的 +08 槽从不分配，故无动作）。
    if (Field_04 != nullptr) {
        if (Field_04->GlyphIndex)
            YRMemory::Deallocate(Field_04->GlyphIndex);
        if (Field_04->CharData)
            YRMemory::Deallocate(Field_04->CharData);
        YRMemory::Deallocate(Field_04);
        Field_04 = nullptr;
    }
}

int32 BitTextClass::Get_Char_Width(char ch) const
{
    (void)ch;
    // asm: width = widthTable[(uint8)ch]
    return 0;
}

// ----------------------------------------------------------------------------
// asm: BitText_LoadGameFNT (0x434AA0)
// ----------------------------------------------------------------------------
void* BitText_LoadGameFNT()
{
    // asm: push 44h / call new            ; sizeof(BitTextClass) == 0x44
    void* pMem = YRMemory::Allocate(0x44);

    // asm: test eax,eax / jz fail
    if (pMem != nullptr) {
        // asm: push offset aGame_fnt ("GAME.FNT") / mov ecx, eax
        //      call BitText_CTOR
        BitTextClass* pFont = new (pMem) BitTextClass("GAME.FNT");

        // asm: mov GameFont, eax
        g_GameFont = pFont;
    } else {
        // asm: mov GameFont, 0
        g_GameFont = nullptr;
    }

    // asm: mov eax, esi / retn   (returns the loader's own this-marker)
    static int32 s_loaderMarker = 0;
    return &s_loaderMarker;
}
