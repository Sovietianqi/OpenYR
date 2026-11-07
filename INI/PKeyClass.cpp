#include "PKeyClass.h"

#include <cstring>

// ============================================================================
// Key_Voodoo1 -- asm 0x5C6350
//
//   ; void __fastcall Key_Voodoo1(byte *dest, byte *key, int len)
//
// asm 正文（逐句对照）：
//   mov al,[edx] / inc edx / cmp al,2 / jnz bail
//   mov al,[edx] / test al,80h / jnz long_form
//   and eax,0FFh / mov edi,eax / jmp loc_5C63AA(inc edx)
// long_form:
//   and eax,7Fh / inc edx / cmp eax,2 / jg bail
//   xor ecx,ecx / mov cl,[edx] / inc edx / cmp eax,1 / mov edi,ecx
//   jle loc_5C63AB
//   xor eax,eax / mov al,[edx] / shl edi,8 / or edi,eax
// loc_5C63AA: inc edx
// loc_5C63AB:
//   mov ecx,[esp+len] / lea esi,[ecx*4] / cmp edi,esi / ja bail
//   mov al,[edx] / mov ebx,edx / and al,80h / neg al / sbb al,al
//   and eax,0FFh                        ; fill = data[0] & 80h ? FF : 00
//   lea ecx,[esi+ebp]                   ; end = buf + len*4
//   mov edx,esi / sub edx,edi / test edx,edx / jle copy
// fill:
//   dec ecx / dec edx / mov [ecx],al / jnz fill
// copy (loc_5C63DA):
//   test edi,edi / jle bail
// copy_loop (loc_5C63DE):
//   mov dl,[ebx] / dec ecx / inc ebx / dec edi / mov [ecx],dl / jnz
// bail: retn 4
// ============================================================================
void Key_Voodoo1(uint8* pBuffer, const uint8* pRecord, int32 len)
{
    if (pRecord == nullptr || pBuffer == nullptr)
        return;                                          // asm: bail

    const uint8* p = pRecord;                            // asm: edx

    if (*p++ != 0x02)                                    // asm: cmp al,2
        return;

    // --- count field -----------------------------------------------------
    int32 count = 0;                                     // asm: edi
    const uint8 b1 = *p++;                               // asm: mov al,[edx]

    if ((b1 & 0x80u) == 0) {
        // asm: and eax,0FFh / mov edi,eax / inc edx
        count = b1;
    } else {
        // asm: and eax,7Fh / cmp eax,2 / jg bail
        const int32 hi = b1 & 0x7F;
        if (hi > 2)
            return;

        // asm: xor ecx,ecx / mov cl,[edx] / inc edx
        count = *p++;

        // asm: cmp eax,1 / jle loc_5C63AB
        if (hi > 1) {
            // asm: mov al,[edx] / shl edi,8 / or edi,eax
            //      然后落到 loc_5C63AA 的 inc edx —— 两个字节都要吃掉，
            //      否则下面的 data 指针会停在长度字段的第二个字节上。
            count = (count << 8) | *p++;
        }
    }

    // asm: lea esi,[ecx*4] / cmp edi,esi / ja bail
    const int32 region = len * 4;                        // asm: esi
    if (count < 0 || static_cast<uint32>(count) > static_cast<uint32>(region))
        return;

    // asm: mov al,[edx] / and al,80h / neg al / sbb al,al / and eax,0FFh
    // The first data byte's high bit selects the fill value.
    const uint8 fill = ((*p & 0x80u) != 0) ? 0xFFu : 0x00u;

    // asm: lea ecx,[esi+ebp] -- walk the region end downward
    uint8* pEnd = pBuffer + region;                      // asm: ecx

    // asm: mov edx,esi / sub edx,edi / test edx,edx / jle copy
    int32 fillCount = region - count;
    while (fillCount > 0) {                              // asm: loc_5C63D4
        *--pEnd = fill;                                  // asm: dec/mov
        --fillCount;
    }

    // asm: test edi,edi / jle bail
    const uint8* pSrc = p;                               // asm: ebx
    int32 remaining = count;                             // asm: edi
    while (remaining > 0) {                              // asm: loc_5C63DE
        *--pEnd = *pSrc++;                               // asm: mov dl,[ebx]...
        --remaining;
    }
}

// ============================================================================
// XMP_DER_Payload -- asm 0x5C6490
//
//   __fastcall(ecx = pDest, edx = pSrc, [esp+4] = len)
//
// 语义：把 pSrc 处 len 个 dword 组成的大整数（小端）截成 DER INTEGER 的
// 最小字节串——去掉高位与符号字节相同的填充，再在需要时补一个符号字节，
// 使「最高位 = 符号」成立。返回字节数。
//
// asm 逐句：
//   mov ebx,[edx+eax*4-4]      ; 读最后一个 dword（最高 32 位）
//   test ebx,ebx / setl cl     ; cl = 该 dword 的符号位
//   neg bl / sbb bl,bl / and ebx,0FFh   ; fill = cl ? 0xFF : 0x00
//   lea esi,[edx+eax*4-1]      ; 从最后一个字节往前
// loc_5C64B7: cmp [esi],bl / jnz …   ; 跳过尾部（高位）与 fill 相同的字节
// loc_5C64C0: al = [esi] & 80h       ; 最高有效字节的符号位
//   5C64C0/5C64CC/5C64D4 三个分支化简后即：msb != negative 时先写一个 fill
// loc_5C64DC/5C64E6: 由高到低逐字节抄到 pDest
//
// 依据说明：asm 文本把倒数第二行显示为 `lea esi,[edx+eax-1]`，但同一函数
// 上一行是 `[edx+eax*4-4]`，且 Key_Voodoo1 用 `lea esi,[ecx*4]` 把 len 换算
// 成字节数、INIClass::Get_PKey 传 len=0x40 而缓冲是 0x100 字节——三处互证
// len 为 dword 数、数据区 4*len 字节，故取 4*len-1。
// ============================================================================
int32 XMP_DER_Payload(uint8* pDest, const uint8* pSrc, int32 len)
{
    if (pDest == nullptr || pSrc == nullptr)
        return 0;

    const int32 byteLen = len * 4;
    if (byteLen <= 0)
        return 0;

    int32 tail = 0;
    std::memcpy(&tail, pSrc + byteLen - 4, 4);           // asm: mov ebx,[edx+eax*4-4]
    const bool negative = (tail < 0);                    // asm: test ebx,ebx / setl cl
    const uint8 fill = negative ? 0xFFu : 0x00u;         // asm: neg/sbb/and 0FFh

    int32 i = byteLen - 1;                               // asm: lea esi,[edx+eax*4-1]
    while (i > 0 && pSrc[i] == fill)                     // asm: loc_5C64B7
        --i;

    int32 out = 0;                                       // asm: xor edi,edi
    const bool msbSet = (pSrc[i] & 0x80u) != 0;          // asm: mov al,[esi]/and al,80h

    if (msbSet != negative)                              // asm: loc_5C64D4
        pDest[out++] = fill;

    for (;;) {                                           // asm: loc_5C64DC / 5C64E6
        pDest[out++] = pSrc[i];
        if (i == 0)
            break;
        --i;
    }

    return out;                                          // asm: mov eax,edi
}

// ============================================================================
// XMP_DER_Encode -- asm 0x5C6260
//
// asm 无参数类型注释行；调用者 INIClass::Get_PKey+60（asm 证实）。
// 寄存器流向：ecx = src, edx = dest, [esp+4] = srcLen。
//
// 契约：srcLen 为 dword 数，pSrc 至少 4*srcLen 字节；实测调用点固定 0x40，
// 对应 asm 里 `sub esp,104h` 的 0x100 字节暂存区。
// ============================================================================
int32 XMP_DER_Encode(const uint8* pSrc, uint8* pDest, int32 srcLen)
{
    if (pDest == nullptr || pSrc == nullptr || srcLen <= 0)
        return 0;

    // asm: sub esp,104h -- 0x100-byte scratch payload buffer
    uint8 buf[0x100];

    // asm: push eax(srcLen) / lea ecx,[buf] / call sub_5C6490
    const int32 dataLen = XMP_DER_Payload(buf, pSrc, srcLen);

    pDest[0] = 2;                                        // asm: mov [ebp],2

    int32 lenBytes;                                      // asm: edx

    // asm: cmp eax,7Fh / ja loc_5C62DF
    if (static_cast<uint32>(dataLen) <= 0x7Fu) {
        pDest[1] = static_cast<uint8>(dataLen);          // asm: mov [ebp+1],al
        lenBytes = 1;                                    // asm: mov edx,1
    } else {
        // Pass 1: locate the highest set byte of dataLen.
        // asm: mov edx,4 / mov ecx,18h
        //   loop: esi = eax >> cl ; if esi != 0 break
        //         ecx -= 8 ; edx-- ; while ecx != -8
        lenBytes = 4;
        for (int32 shift = 0x18; ; shift -= 8) {
            if ((dataLen >> shift) != 0)
                break;
            --lenBytes;
            if (shift == -8)
                break;
        }

        // asm: or dl,80h / mov [ebp+1],dl / mov edx,1
        pDest[1] = static_cast<uint8>(lenBytes | 0x80);

        // Pass 2: same scan again into edi (the original recomputes rather
        // than reusing pass 1's counter -- reproduced faithfully).
        // asm: mov edi,4 / mov ecx,18h ... loop
        int32 byteCount = 4;                             // asm: edi
        for (int32 shift = 0x18; ; shift -= 8) {
            if ((dataLen >> shift) != 0)
                break;
            --byteCount;
            if (shift == -8)
                break;
        }

        // asm: loc_5C6320: test edi,edi / jz done
        //      lea esi,[0FFFFFFF8h+edi*8]  (top-byte shift)
        int32 shift = byteCount * 8 - 8;
        int32 idx = 1;                                   // asm: edx
        while (byteCount != 0) {                         // asm: loc_5C632B
            // asm: mov ebx,eax / shr ebx,cl / inc edx / mov [edx+ebp],bl
            ++idx;
            pDest[idx] = static_cast<uint8>(
                static_cast<uint32>(dataLen) >> shift);
            shift -= 8;
            --byteCount;
        }

        // asm: lea eax,[eax+edx+1]
        if (dataLen > 0)
            std::memcpy(pDest + idx + 1, buf, static_cast<uint32>(dataLen));
        return dataLen + idx + 1;
    }

    // Short-form path tail: copy the payload after the 2-byte header.
    // asm: lea edi,[edx+ebp+1] / rep movsd / rep movsb
    if (dataLen > 0)
        std::memcpy(pDest + 2, buf, static_cast<uint32>(dataLen));

    return dataLen + lenBytes + 1;                       // asm: lea eax,[eax+edx+1]
}
