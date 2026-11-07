#include "FileFormats/LCW.h"

#include <cstring>

namespace {

// Unaligned little-endian read; LCW streams are byte packed.
inline uint16 ReadLE16(const uint8* p)
{
    return static_cast<uint16>(static_cast<uint16>(p[0]) |
                               (static_cast<uint16>(p[1]) << 8));
}

} // namespace

// ============================================================================
// LCW (Westwood Format80) decompression
//
// asm: sub_7C812F (the standalone copy used by the codec reader sub_7BEA80)
//      and sub_551C60 (LCWStraw::Get). Both dispatch in exactly this order:
//
//   0x7C812F: mov esi,ebx / lodsb / or al,al / js -> 0x7C8166
//             (cmd < 0x80 -> short back copy)
//   0x7C8166: test al,40h / jnz 0x7C8188
//             cmp al,80h / jz 0x7C8181        ; 0x80 == end of stream
//             and ecx,3Fh ; rep movsb          ; 0x81..0xBF literal run
//   0x7C8188: and ecx,3Fh / add ecx,3
//   0x7C81AB: cmp al,0FFh / jnz 0x7C81B5      ; 0xFF = long copy (word,word)
//   0x7C81B5: lodsw (absolute dst offset) / rep movsb
//   0x7C819A: 0xFE -> count=word, value=byte, rep stosb
//
// The 0x80 terminator and the "0x81..0xBF = literal" mapping are the two
// points where the binary disagrees with the Format80 summary in the task.
// ============================================================================
int32 LCW_Uncompress(const uint8* pSrc, int32 srcLen, uint8* pDst, int32 dstLen)
{
    if (pSrc == nullptr || pDst == nullptr || srcLen < 0 || dstLen <= 0)
        return -1;

    int32 s = 0;                                // source cursor
    uint8* const pDstStart = pDst;
    uint8* const pDstEnd   = pDst + dstLen;

    while (pDst < pDstEnd)
    {
        if (s >= srcLen)
            return -1;                          // source exhausted mid-stream

        const uint8 cmd = pSrc[s++];

        if ((cmd & 0x80) == 0)
        {
            // 0x00..0x7F -- short back copy.
            // asm 0x7C813E: count = (cmd >> 4) + 3, offset = (cmd & 0xF) << 8 | next
            if (s >= srcLen)
                return -1;
            const int32 count  = ((cmd >> 4) & 0x07) + 3;
            const int32 offset = ((cmd & 0x0F) << 8) | pSrc[s++];
            const int32 written = static_cast<int32>(pDst - pDstStart);

            if (offset == 0 || offset > written)
                return -1;                      // back reference outside output

            int32 n = count;
            if (n > static_cast<int32>(pDstEnd - pDst))
                n = static_cast<int32>(pDstEnd - pDst);   // asm clamps to remaining

            // Byte copy on purpose: source and destination windows overlap.
            for (int32 i = 0; i < n; ++i)
                pDst[i] = pDst[i - offset];
            pDst += n;
        }
        else if (cmd == 0x80)
        {
            // asm 0x7C8166 / 0x7C8181 -- explicit end marker.
            break;
        }
        else if ((cmd & 0xC0) == 0x80)
        {
            // 0x81..0xBF -- literal run, count = cmd & 0x3F (no +3!).
            // asm 0x7C8166: and ecx,3Fh ; rep movsb
            int32 n = cmd & 0x3F;
            const int32 srcLeft = srcLen - s;
            if (n > srcLeft)
                n = srcLeft;
            if (n > static_cast<int32>(pDstEnd - pDst))
                n = static_cast<int32>(pDstEnd - pDst);
            if (n <= 0)
                return -1;

            std::memcpy(pDst, pSrc + s, static_cast<size_t>(n));
            s += n;
            pDst += n;
        }
        else if (cmd == 0xFE)
        {
            // asm 0x7C8192 -- long fill: count = word, value = byte.
            if (srcLen - s < 3)
                return -1;
            const int32 count = ReadLE16(pSrc + s);
            const uint8 value = pSrc[s + 2];
            s += 3;

            int32 n = count;
            if (n > static_cast<int32>(pDstEnd - pDst))
                n = static_cast<int32>(pDstEnd - pDst);
            std::memset(pDst, value, static_cast<size_t>(n));
            pDst += n;
        }
        else if (cmd == 0xFF)
        {
            // asm 0x7C81AB -- long copy: count = word, offset = word (absolute).
            if (srcLen - s < 4)
                return -1;
            const int32 count  = ReadLE16(pSrc + s);
            const int32 offset = ReadLE16(pSrc + s + 2);
            s += 4;

            const int32 written = static_cast<int32>(pDst - pDstStart);
            if (offset < 0 || offset >= written)
                return -1;

            int32 n = count;
            if (n > static_cast<int32>(pDstEnd - pDst))
                n = static_cast<int32>(pDstEnd - pDst);
            const uint8* pCopy = pDstStart + offset;
            for (int32 i = 0; i < n; ++i)
                pDst[i] = pCopy[i];
            pDst += n;
        }
        else
        {
            // 0xC0..0xFD -- segment copy with a 16-bit absolute offset.
            // asm 0x7C8188: count = (cmd & 0x3F) + 3 ; 0x7C81B5: lo
            if (srcLen - s < 2)
                return -1;
            const int32 count  = (cmd & 0x3F) + 3;
            const int32 offset = ReadLE16(pSrc + s);
            s += 2;

            const int32 written = static_cast<int32>(pDst - pDstStart);
            if (offset < 0 || offset >= written)
                return -1;

            int32 n = count;
            if (n > static_cast<int32>(pDstEnd - pDst))
                n = static_cast<int32>(pDstEnd - pDst);
            const uint8* pCopy = pDstStart + offset;
            for (int32 i = 0; i < n; ++i)
                pDst[i] = pCopy[i];
            pDst += n;
        }
    }

    return static_cast<int32>(pDst - pDstStart);
}

int32 LCW_Uncompress(const uint8* pSrc, uint8* pDst, int32 dstLen)
{
    // The original decoder (asm 0x7C812F) takes no source length either; the
    // stream is self-delimiting through the 0x80 terminator / output window.
    return LCW_Uncompress(pSrc, 0x7FFFFFFF, pDst, dstLen);
}

