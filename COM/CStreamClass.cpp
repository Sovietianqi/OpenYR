#include "CStreamClass.h"

#include "../Core/Memory.h"

#include <cstring>

// ============================================================================
// CStreamClass - block-framed LZO codec over an inner IStream
//
// asm anchors:
//   CStreamClass_CTOR           0x4A27F0   three 64 KB buffers + field priming
//   CStreamClass_RemoteRead     0x4A2B70   block decompress loop
//   CStreamClass_RemoteWrite    0x4A2CC0   block compress loop
//   CStreamClass_RemoteSeek     0x4A2E00   guarded forward to inner Seek
//   CStreamClass_SetSize        0x4A2E60   guarded forward to inner SetSize
//   CStreamClass_RemoteCopyTo   0x4A2EA0   forward to inner CopyTo
//   CStreamClass_Commit/Revert  0x4A2EE0 / 0x4A2F20
//   CStreamClass_LockRegion/UnlockRegion/Stat/Clone  0x4A2F50 .. 0x4A3000
//   sub_4A3020                  0x4A3020   compress + emit one frame
//   CStreamClass_QueryInterface 0x4A29A0
//   CStreamClass_AddRef/Release  0x4A2950 / 0x4A2970
//   CStreamClass_SDDTOR         0x4A30E0
// ============================================================================

namespace {

// The two IStream HRESULTs the original compares against that the project's
// minimal Macros.h does not define.
const HRESULT kE_INVALIDARG = static_cast<HRESULT>(0x80070057u);
const HRESULT kE_OUTOFMEMORY = static_cast<HRESULT>(0x8007000Eu);

// Standard COM IIDs, as compared by CStreamClass_QueryInterface.
const GUID kIID_IUnknown = { 0x00000000u, 0x0000, 0x0000,
                             { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const GUID kIID_IStream  = { 0x0000000Cu, 0x0000, 0x0000,
                             { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };

// asm: sub_4A3020's LZO front-end.  The in-tree lzo1x_1_compress is only
// complete while its do-compress hook is bound; when it is not, fall back to a
// pure-literal LZO1X stream (valid, self-describing, and round-tripped by
// lzo1x_decompress).  Returns the encoded byte count, always > 0.
int32 SealLiteralBlock(const uint8* pIn, int32 nLen, uint8* pOut)
{
    int32 n = 0;

    if (nLen <= 0)
    {
        pOut[n++] = 0x11; pOut[n++] = 0x00; pOut[n++] = 0x00;
        return n;
    }

    if (nLen <= 0xEE)
    {
        // Initial literal run: the first token is (len + 0x11).
        pOut[n++] = static_cast<uint8>(nLen + 0x11);
        std::memcpy(pOut + n, pIn, static_cast<size_t>(nLen));
        n += nLen;
    }
    else
    {
        // Extended literal run: a 0x00 token, then 0xFF-per-zero length chain
        // and a terminator, then the literal bytes.  run = 0x0F + 0xFF*zeros + t.
        pOut[n++] = 0x00;
        const int32 run = nLen - 3;
        int32 rem = run - 0x0F;
        int32 zeros = rem / 0xFF;
        int32 term = rem % 0xFF;
        if (term == 0) { zeros -= 1; term = 0xFF; }
        for (int32 i = 0; i < zeros; ++i) {
            pOut[n++] = 0x00;
        }
        pOut[n++] = static_cast<uint8>(term);
        std::memcpy(pOut + n, pIn, static_cast<size_t>(nLen));
        n += nLen;
    }

    // End-of-stream marker consumed by lzo1x_decompress as the M1 EOF case.
    pOut[n++] = 0x11; pOut[n++] = 0x00; pOut[n++] = 0x00;
    return n;
}

} // namespace

// ============================================================================
// lzo1x_1_compress (asm: lzo1x_1_compress, 0x55BB90)
//
//   ecx = pIn / edx = inLen / stack = pOut, pOutLen, pDict; retn 0Ch
//
//   if (inLen == 0): *pOutLen = 0 ; goto emit_end ; return 0
//   if (inLen <= 13): out[0] = inLen + 11h ; memcpy(out+1, in, inLen)
//                     *pOutLen = inLen + 1 ; goto emit_end ; return 0
//   rc = do_compress(in, inLen, out, pOutLen, dict)
//   if (rc != 0) return rc
// emit_end: out[*pOutLen] = 11h ; +1 = 0 ; +2 = 0 ; *pOutLen += 3 ; return 0
//
// 降级点：do_compress 是 LZO1X-1 的内层匹配器，本树没有它的实体（原版是
// 静态链接的 lzo1x_1 汇编）。这里改用 SealLiteralBlock 产出等价的纯字面量
// LZO1X 流（合法、可被 lzo1x_decompress 往返），并把长度写回 pOutLen。
// ============================================================================
int32 lzo1x_1_compress(const uint8* pIn, int32 inLen, uint8* pOut,
                       int32* pOutLen, void* pDict)
{
    (void)pDict;

    if (pOutLen == nullptr || pOut == nullptr)
        return 0;

    // asm: test edx,edx / ja  -- 长度按无符号比较
    const uint32 ulen = static_cast<uint32>(inLen);

    // emit_end 尾巴：0x11 00 00
    auto emit_end = [pOut, pOutLen]() {
        const int32 n = *pOutLen;
        pOut[n] = 0x11;
        pOut[n + 1] = 0;
        pOut[n + 2] = 0;
        *pOutLen = n + 3;
    };

    if (ulen == 0) {
        *pOutLen = 0;                        // asm: mov [esi], eax
        emit_end();
        return 0;
    }

    if (ulen <= 13) {                        // asm: cmp edx,0Dh / ja
        pOut[0] = static_cast<uint8>(ulen + 0x11);
        std::memcpy(pOut + 1, pIn, static_cast<size_t>(ulen));
        *pOutLen = static_cast<int32>(ulen) + 1;
        emit_end();
        return 0;
    }

    // asm: call do_compress -> 降级为纯字面量块（自带 0x11 00 00 结尾）。
    *pOutLen = SealLiteralBlock(pIn, inLen, pOut);
    return 0;
}

// ============================================================================
// lzo1x_decompress (asm: 0x55C0C0 区, 原版为静态链接的 LZO1X 解码汇编)
//
//   ecx = pIn / edx = inLen / stack = pDest, pOutLen ; retn 0Ch
//
//   完整 LZO1X 解码器：字面量游程（15+0xFF 扩展链）、M1 三字节→游程形态、
//   M2/M3/M4 匹配、token&3 尾部字面量技巧、801h/4000h 距离偏置，以及
//   流尾判定（距离字段归零即停止）。输入恰好耗尽返回 0，否则返回 -1。
//
//   寄存器模型：eax = ip，esi = op，ebp = ipEnd，ebx = pOutLen，
//   edx = 游程/令牌，ecx/edi = 匹配源游标。
//   标签按 dump 保留：loc_55C109（顶部）、loc_55C13B（3 字节字面量头）、
//   loc_55C14E/F（字面量游程）、loc_55C183/29B（匹配拷贝）、
//   loc_55C18E/19A（token&3 尾部字面量）、loc_55C1C1（M3/M4）、
//   loc_55C1E6（M2）、loc_55C22A（M1）、loc_55C288（EOF 探测）。
// ============================================================================
int32 lzo1x_decompress(const uint8* pIn, int32 inLen,
                       uint8* pDest, int32* pOutLen)
{
    const uint8* ip = pIn;                            // asm: mov eax, ecx
    const uint8* const ipEnd = pIn + inLen;           // asm: lea ebp, [eax+edx]
    uint8* op = pDest;                                // asm: esi
    *pOutLen = 0;                                     // asm: mov [ebx], 0

    uint32 run = 0;                                   // edx role
    const uint8* mp = op;                             // edi/ecx role
    uint32 tok;

    // ---- entry: first token drives the initial literal run ---------------
    tok = ip[0];
    if (tok > 0x11) {
        run = tok - 0x11;
        ++ip;                                         // asm: loc_55C14E inc eax
        while (run) { *op++ = *ip++; --run; }
        goto first_token;
    }

    for (;;) {                                        // asm: loc_55C109
        tok = *ip++;
        if (tok >= 0x10)
            goto match_decode;                        // asm: loc_55C1C1
        if (tok != 0) {
            run = tok;
            goto copy3_head;                          // asm: loc_55C13B
        }
        // asm loc_55C126 chain: 0Fh + 0FFh per zero byte + terminator
        run = 0xF;
        while (*ip == 0) { run += 0xFF; ++ip; }
        run += *ip++;                                 // asm: loc_55C134

copy3_head:                                           // asm: loc_55C13B
        *op++ = ip[0];
        *op++ = ip[1];
        *op++ = ip[2];
        ip += 3;

        while (run) { *op++ = *ip++; --run; }         // asm: loc_55C14F

first_token:
        tok = *ip++;
        if (tok >= 0x10)
            goto match_decode;

        // M1 fast path (asm 55C183 chain): 3-byte match with the 801h
        // distance bias: m_pos = op - (tok>>2) - (ip[0]<<2) - 0x801.
        {
            mp = op - (tok >> 2);
            const uint8 v = mp[-0x801];               // asm: mov dl,[ecx-801h]
            mp -= 0x801;                              // asm: sub ecx,801h
            ++ip;                                     // asm: inc eax
            *op++ = v;                                // single byte
            *op++ = *mp;                              // asm: loc_55C183
            *op++ = mp[1];
            ++mp;
        }
        goto trailing;                                // asm: loc_55C18E

match_decode:                                         // asm: loc_55C1C1
        if (tok >= 0x40) {
            // M3/M4: m_pos = op - ((tok>>2)&7) - (ip[0]<<3) - 1
            //        len    = (tok>>5) - 1  (+2 copied by do_match)
            mp = op - ((tok >> 2) & 7);
            mp -= static_cast<uint32>(ip[0]) << 3;
            --mp;                                     // asm: dec edi
            ++ip;                                     // asm: inc eax
            run = (tok >> 5) - 1;                     // asm: shr edx,5 / dec edx
            goto do_match;
        }
        if (tok >= 0x20) {
            // M2 (asm loc_55C1E6): len = tok&1Fh (extended 1Fh chain)
            run = tok & 0x1F;
            if (run == 0) {
                run = 0x1F;
                while (*ip == 0) { run += 0xFF; ++ip; }   // asm: loc_55C1FB
                run += *ip++;                              // asm: loc_55C209
            }
            // asm loc_55C210: m_pos = op - (ip[0]>>2) - 1 - (ip[1]<<6)
            mp = op - (ip[0] >> 2) - 1;
            mp -= static_cast<uint32>(ip[1]) << 6;
            ip += 2;
            goto do_match;
        }
        // M1 (asm loc_55C22A): len = tok&7 (extended 7 chain),
        // distance = ((tok&8)<<11) + (ip[0]>>2) + (ip[1]<<6) + 4000h.
        mp = op - ((tok & 8) << 11);                  // asm: shl ecx,0Bh
        run = tok & 7;
        if (run == 0) {
            run = 7;
            while (*ip == 0) { run += 0xFF; ++ip; }   // asm: loc_55C246
            run += *ip++;                             // asm: loc_55C254
        }
        mp -= ip[0] >> 2;                             // asm: shr ecx,2 / sub
        ++ip;
        mp -= static_cast<uint32>(ip[0]) << 6;        // asm: shl ecx,6 / sub
        ++ip;
        if (mp == op) {                               // asm: cmp edi,esi / jz EOF
            *pOutLen = static_cast<int32>(op - pDest);
            return (ipEnd != ip) ? -1 : 0;            // asm: neg eax / sbb eax,eax
        }
        mp -= 0x4000;                                 // asm: loc_55C288

do_match:                                             // asm: loc_55C28E
        *op++ = *mp;
        *op++ = mp[1];
        mp += 2;
        while (run) { *op++ = *mp++; --run; }         // asm: loc_55C29B

trailing:                                             // asm: loc_55C18E
        // t = ip[-2] & 3 trailing literal bytes (0 -> straight back to top).
        {
            uint32 extra = ip[-2] & 3;
            if (extra == 0)
                continue;                             // asm: jz loc_55C109
            while (extra) { *op++ = *ip++; --extra; } // asm: loc_55C19A
            tok = *ip++;
            if (tok >= 0x10)
                goto match_decode;
            // M1 2-byte form (asm loc_55C19A fallthrough):
            // m_pos = op - (tok>>2) - (ip[0]<<2) - 1.
            mp = op - (tok >> 2);
            mp -= static_cast<uint32>(ip[0]) << 2;
            --mp;                                     // asm: dec ecx
            ++ip;                                     // asm: inc eax
            *op++ = *mp;                              // asm: loc_55C183
            *op++ = mp[1];
            ++mp;
            goto trailing;
        }
    }
}

// ============================================================================
// lzo1x_decompress_safe —— 带边界的同算法解码器
//
//   原版 lzo1x_decompress 只被 LZOStraw 调用，而上层 Straw 会返回 0 截断，
//   所以裸解码器本身不做 ip/op 边界检查。本树把它接到"读外部地图文件"的
//   路径（[IsoMapPack5]）后，损坏文件可以让裸指针副本越界读写。
//
//   本函数逐条复用同一套 token 语义（字面量游程 / M1~M4 / 尾部字面量 /
//   801h、4000h 距离偏置），额外做三件事：
//     * 每次读写前检查 ip / op 余量；
//     * 用"相对 opBase 的偏移"计算匹配源，偏移 < 0 直接判失败（不做指针减法）；
//     * 输出超过 outCap 立即判失败。
//   返回值与 lzo1x_decompress 一致：0 = 输入恰好耗尽，-1 = 数据不合法/越界。
// ============================================================================
int32 lzo1x_decompress_safe(const uint8* pIn, int32 inLen,
                            uint8* pDest, int32 outCap, int32* pOutLen)
{
    if (pIn == nullptr || pDest == nullptr || pOutLen == nullptr ||
        inLen <= 0 || outCap <= 0)
        return -1;

    const uint8* ip          = pIn;
    const uint8* const ipEnd = pIn + inLen;
    uint8* const opBase      = pDest;
    uint8* const opEnd       = pDest + outCap;
    uint8* op                = pDest;

    // 相对宽度快捷量：都按 int32 处理（地图数据远小于 2GB）。
#define YR_IN_LEFT()  static_cast<int32>(ipEnd - ip)
#define YR_OUT_LEFT() static_cast<int32>(opEnd - op)

    *pOutLen = 0;

    uint32 tok = 0;
    uint32 run = 0;
    const uint8* mp = op;
    int32  off = 0;

    // ---- 入口：首 token 决定初始字面量游程 --------------------------------
    tok = *ip++;
    if (tok > 0x11)
    {
        run = tok - 0x11;
        if (static_cast<int32>(run) > YR_IN_LEFT() ||
            static_cast<int32>(run) > YR_OUT_LEFT())
            return -1;
        std::memcpy(op, ip, run);
        op += run;
        ip += run;
    }

    for (;;)
    {
        if (YR_IN_LEFT() < 1)
            return -1;

        tok = *ip++;
        if (tok >= 0x10)
            goto match_decode;
        if (tok != 0)
        {
            run = tok;
            goto copy3_head;
        }

        // asm loc_55C126：0Fh + 0FFh/零字节 + 终止字节
        run = 0xF;
        while (YR_IN_LEFT() > 0 && *ip == 0)
        {
            run += 0xFF;
            if (run > 0x1000000u)      // 防止 0 填充把长度推到无意义的大数
                return -1;
            ++ip;
        }
        if (YR_IN_LEFT() < 1)
            return -1;
        run += *ip++;

copy3_head:                            // asm loc_55C13B
        if (YR_IN_LEFT() < 3 || YR_OUT_LEFT() < 3)
            return -1;
        if (static_cast<int32>(run) > YR_IN_LEFT() - 3 ||
            static_cast<int32>(run) > YR_OUT_LEFT() - 3)
            return -1;

        std::memcpy(op, ip, 3);
        op += 3;
        ip += 3;
        std::memcpy(op, ip, run);
        op += run;
        ip += run;

first_token:                           // asm loc_55C183 的 M1 三字节形态
        if (YR_IN_LEFT() < 1 || YR_OUT_LEFT() < 3)
            return -1;

        tok = *ip++;
        if (tok >= 0x10)
            goto match_decode;

        off = static_cast<int32>(op - opBase)
              - static_cast<int32>(tok >> 2) - 0x801;
        if (off < 0)
            return -1;
        mp = opBase + off;
        ++ip;
        // 原式：v = mp0[-0x801]（mp0 = mp + 0x801）⇒ v == mp[0]；
        // 随后 mp0 -= 0x801 再写 *mp0 / mp0[1]，即 mp[0] / mp[1]。
        *op++ = mp[0];
        *op++ = mp[0];
        *op++ = mp[1];
        ++mp;
        goto trailing;

match_decode:                          // asm loc_55C1C1
        if (tok >= 0x40)
        {
            // M3/M4
            if (YR_IN_LEFT() < 1)
                return -1;
            off = static_cast<int32>(op - opBase)
                  - static_cast<int32>((tok >> 2) & 7)
                  - (static_cast<int32>(ip[0]) << 3) - 1;
            if (off < 0)
                return -1;
            mp = opBase + off;
            ++ip;
            run = (tok >> 5) - 1;
            goto do_match;
        }

        if (tok >= 0x20)
        {
            // M2
            run = tok & 0x1F;
            if (run == 0)
            {
                run = 0x1F;
                while (YR_IN_LEFT() > 0 && *ip == 0)
                {
                    run += 0xFF;
                    if (run > 0x1000000u)
                        return -1;
                    ++ip;
                }
                if (YR_IN_LEFT() < 1)
                    return -1;
                run += *ip++;
            }
            if (YR_IN_LEFT() < 2)
                return -1;
            off = static_cast<int32>(op - opBase)
                  - static_cast<int32>(ip[0] >> 2) - 1
                  - (static_cast<int32>(ip[1]) << 6);
            if (off < 0)
                return -1;
            mp = opBase + off;
            ip += 2;
            goto do_match;
        }

        // M1 长形态
        off = static_cast<int32>(op - opBase)
              - static_cast<int32>((tok & 8) << 11);
        run = tok & 7;
        if (run == 0)
        {
            run = 7;
            while (YR_IN_LEFT() > 0 && *ip == 0)
            {
                run += 0xFF;
                if (run > 0x1000000u)
                    return -1;
                ++ip;
            }
            if (YR_IN_LEFT() < 1)
                return -1;
            run += *ip++;
        }
        if (YR_IN_LEFT() < 2)
            return -1;
        off -= static_cast<int32>(ip[0] >> 2);
        ++ip;
        off -= static_cast<int32>(ip[0]) << 6;
        ++ip;
        if (off < 0)
            return -1;
        if (off == static_cast<int32>(op - opBase))   // asm: cmp edi, esi / EOF
        {
            *pOutLen = static_cast<int32>(op - opBase);
            return (ipEnd != ip) ? -1 : 0;
        }
        off -= 0x4000;
        if (off < 0)
            return -1;
        mp = opBase + off;

do_match:                              // asm loc_55C28E（允许重叠，逐字节）
        if (YR_OUT_LEFT() < 2)
            return -1;
        *op++ = *mp++;
        *op++ = *mp++;
        while (run)
        {
            if (YR_OUT_LEFT() < 1)
                return -1;
            *op++ = *mp++;
            --run;
        }

trailing:                              // asm loc_55C18E
        {
            if (ip - pIn < 2)
                return -1;

            uint32 extra = ip[-2] & 3;
            if (extra == 0)
                continue;

            if (static_cast<int32>(extra) > YR_IN_LEFT() ||
                static_cast<int32>(extra) > YR_OUT_LEFT())
                return -1;
            std::memcpy(op, ip, extra);
            op += extra;
            ip += extra;

            if (YR_IN_LEFT() < 1)
                return -1;
            tok = *ip++;
            if (tok >= 0x10)
                goto match_decode;

            if (YR_IN_LEFT() < 1 || YR_OUT_LEFT() < 2)
                return -1;
            off = static_cast<int32>(op - opBase)
                  - static_cast<int32>(tok >> 2)
                  - (static_cast<int32>(ip[0]) << 2) - 1;
            if (off < 0)
                return -1;
            mp = opBase + off;
            ++ip;
            *op++ = *mp++;
            *op++ = *mp++;
            goto trailing;
        }
    }

#undef YR_IN_LEFT
#undef YR_OUT_LEFT
}

// ============================================================================
// Construction / Destruction
// ============================================================================

// asm: CStreamClass_CTOR (0x4A27F0)
//
//   Allocates the three 0x10000-byte scratch buffers at [this+18h]/[+1Ch]/[+20h],
//   primes [this+14h] = 0, [this+24h] = 0xFFFF, and clears the read/write locks.
//   The retail form has no explicit inner stream (it is installed later); the
//   port takes it as a constructor argument.
CStreamClass::CStreamClass() noexcept
    : InnerStream(nullptr)
    , BufferA(nullptr)
    , BufferB(nullptr)
    , BufferC(nullptr)
    , BufferFill(0)
    , BlockBytes(0)
    , Reading(false)
    , Writing(false)
{
    RefCount = 1;
    std::memset(BlockHeader, 0, sizeof(BlockHeader));
    BlockHeader[0] = 0xFF;
    BlockHeader[1] = 0xFF;   // asm: [this+24h] = 0xFFFF at construction

    BufferA = static_cast<uint8*>(YRMemory::Allocate(BlockSize));
    BufferB = static_cast<uint8*>(YRMemory::Allocate(BlockSize));
    BufferC = static_cast<uint8*>(YRMemory::Allocate(BlockSize));
}

CStreamClass::CStreamClass(IStream* pInner) noexcept
    : CStreamClass()
{
    InnerStream = pInner;
}

// asm: CStreamClass_SDDTOR (0x4A30E0) / _DTOR (0x4A2880)
//
//   Releases the three buffers, then the inner stream (through its Release
//   slot) when one is held.
CStreamClass::~CStreamClass()
{
    if (BufferA) { YRMemory::Deallocate(BufferA); BufferA = nullptr; }
    if (BufferB) { YRMemory::Deallocate(BufferB); BufferB = nullptr; }
    if (BufferC) { YRMemory::Deallocate(BufferC); BufferC = nullptr; }

    if (InnerStream)
    {
        InnerStream->Release();
        InnerStream = nullptr;
    }
}

// ============================================================================
// IUnknown
// ============================================================================

// asm: CStreamClass_QueryInterface (0x4A29A0)
//
//   A null out-pointer yields E_POINTER.  The IID is compared against
//   IID_IUnknown and IID_IStream; a match stores the object (or its IStream
//   sub-object in the original) and AddRefs, otherwise E_NOINTERFACE.
HRESULT CStreamClass::QueryInterface(REFIID iid, LPVOID* ppvObject)
{
    if (ppvObject == nullptr) {
        return E_POINTER;
    }
    *ppvObject = nullptr;

    if (iid == kIID_IUnknown || iid == kIID_IStream)
    {
        *ppvObject = static_cast<IStream*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

// asm: CStreamClass_AddRef (0x4A2950)
ULONG CStreamClass::AddRef()
{
    return ++RefCount;
}

// asm: CStreamClass_Release (0x4A2970)
ULONG CStreamClass::Release()
{
    const ULONG remaining = --RefCount;
    if (remaining == 0) {
        delete this;
    }
    return remaining;
}

// ============================================================================
// Block codec
// ============================================================================

// asm: sub_4A3020 (0x4A3020) - lzo1x_1_compress into BufferB.
int32 CStreamClass::CompressBlock(const uint8* pIn, int32 nLen, uint8* pOut)
{
    int32 outLen = -1;
    const int32 rc = lzo1x_1_compress(pIn, nLen, pOut, &outLen, nullptr);

    if (rc == 0 && outLen > 0 && outLen <= nLen + (nLen / 0xFF) + 8) {
        return outLen;
    }

    // The generic compressor is unavailable/incomplete: emit a valid literal
    // stream so the frame always round-trips.
    return SealLiteralBlock(pIn, nLen, pOut);
}

// asm: lzo1x_decompress (asm line 47091)
int32 CStreamClass::DecompressBlock(const uint8* pIn, int32 nLen,
                                    uint8* pOut, int32* pOutLen)
{
    return lzo1x_decompress(pIn, nLen, pOut, pOutLen);
}

// asm: sub_4A3020 (0x4A3020) - compress and emit one [size][data] frame.
HRESULT CStreamClass::FlushBlock(const uint8* pData, int32 nLen)
{
    if (InnerStream == nullptr) {
        return E_FAIL;
    }

    const int32 compLen = CompressBlock(pData, nLen, BufferB);

    // asm: [this+24h] = compressed size, [this+28h] = uncompressed size.
    *reinterpret_cast<int32*>(&BlockHeader[0]) = compLen;
    *reinterpret_cast<int32*>(&BlockHeader[4]) = nLen;
    BlockBytes = nLen;

    ULONG wrote = 0;
    HRESULT hr = InnerStream->Write(BlockHeader, 8, &wrote);
    if (hr < 0) {
        return hr;
    }
    if (wrote != 8) {
        return E_FAIL;
    }

    hr = InnerStream->Write(BufferB, static_cast<ULONG>(compLen), &wrote);
    if (hr < 0) {
        return hr;
    }
    if (static_cast<int32>(wrote) != compLen) {
        return E_FAIL;
    }

    return S_OK;
}

// ============================================================================
// IStream - read / write (asm: CStreamClass_RemoteRead / _RemoteWrite)
// ============================================================================

// asm: CStreamClass_RemoteRead (0x4A2B70)
//
//   Guard: null buffer -> E_POINTER; negative count -> E_INVALIDARG; missing
//   inner stream -> E_FAIL; a write-locked handle -> E_FAIL.  The read lock is
//   taken, *pcbRead is zeroed, then the loop drains BufferA and refills it from
//   successive [8-byte header][compressed block] frames.  On exit *pcbRead
//   reports the requested count.
HRESULT CStreamClass::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
    if (pv == nullptr) {
        return E_POINTER;
    }
    if (static_cast<int32>(cb) < 0) {
        return kE_INVALIDARG;
    }
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    if (Writing) {
        return E_FAIL;
    }

    Reading = true;
    if (pcbRead) {
        *pcbRead = 0;
    }

    uint8* dst = static_cast<uint8*>(pv);
    int32 remaining = static_cast<int32>(cb);

    while (remaining > 0)
    {
        // Drain whatever is left of the current decompressed block.
        if (BufferFill > 0)
        {
            const int32 n = (remaining < BufferFill) ? remaining : BufferFill;
            std::memcpy(dst, BufferA + (BlockBytes - BufferFill), static_cast<size_t>(n));
            dst += n;
            remaining -= n;
            BufferFill -= n;
        }

        if (remaining == 0) {
            break;
        }

        // Pull the next frame: 8-byte header, then the compressed payload.
        ULONG got = 0;
        HRESULT hr = InnerStream->Read(BlockHeader, 8, &got);
        if (hr < 0) {
            return hr;
        }
        if (got != 8) {
            return E_FAIL;
        }

        const int32 compLen = *reinterpret_cast<int32*>(&BlockHeader[0]);
        const int32 uncompLen = *reinterpret_cast<int32*>(&BlockHeader[4]);

        if (compLen <= 0 || compLen > BlockSize) {
            return E_FAIL;
        }

        hr = InnerStream->Read(BufferB, static_cast<ULONG>(compLen), &got);
        if (hr < 0) {
            return hr;
        }
        if (static_cast<int32>(got) != compLen) {
            return E_FAIL;
        }

        int32 outLen = BlockSize;
        DecompressBlock(BufferB, compLen, BufferA, &outLen);

        // asm: fill = [this+28h] (the header's uncompressed length).
        BlockBytes = (uncompLen > 0 && uncompLen <= BlockSize) ? uncompLen : outLen;
        BufferFill = BlockBytes;
    }

    if (pcbRead) {
        *pcbRead = cb;
    }
    return S_OK;
}

// asm: CStreamClass_RemoteWrite (0x4A2CC0)
//
//   Guard mirrors RemoteRead but tests the read lock.  Bytes accumulate into
//   BufferA; a full 64 KB buffer (or each aligned 64 KB run in the input) is
//   framed and emitted, and the remainder is retained for the next call.  On
//   exit *pcbWritten reports the accepted count.
HRESULT CStreamClass::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
    if (pv == nullptr) {
        return E_POINTER;
    }
    if (static_cast<int32>(cb) < 0) {
        return kE_INVALIDARG;
    }
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    if (Reading) {
        return E_FAIL;
    }

    Writing = true;
    if (pcbWritten) {
        *pcbWritten = 0;
    }

    if (cb == 0) {
        return S_OK;
    }

    const uint8* src = static_cast<const uint8*>(pv);
    int32 size = static_cast<int32>(cb);

    // Top up the pending partial block first.
    if (BufferFill > 0)
    {
        const int32 space = BlockSize - BufferFill;
        const int32 n = (size < space) ? size : space;
        std::memcpy(BufferA + BufferFill, src, static_cast<size_t>(n));
        BufferFill += n;
        src += n;
        size -= n;

        if (BufferFill == BlockSize)
        {
            HRESULT hr = FlushBlock(BufferA, BlockSize);
            if (hr < 0) {
                return hr;
            }
            BufferFill = 0;
        }
    }

    // Emit every whole 64 KB run directly.
    while (size >= BlockSize)
    {
        HRESULT hr = FlushBlock(src, BlockSize);
        if (hr < 0) {
            return hr;
        }
        size -= BlockSize;
        src += BlockSize;
    }

    // Retain the tail for the next call.
    if (size > 0)
    {
        std::memcpy(BufferA, src, static_cast<size_t>(size));
        BufferFill = size;
    }

    if (pcbWritten) {
        *pcbWritten = cb;
    }
    return S_OK;
}

// ============================================================================
// IStream - seek / size / transaction (guarded forwards)
// ============================================================================

// asm: CStreamClass_RemoteSeek (0x4A2E00)
HRESULT CStreamClass::Seek(int64 dlibMove, DWORD dwOrigin, uint64* plibNewPosition)
{
    if (Reading || Writing) {
        return E_FAIL;
    }
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    return InnerStream->Seek(dlibMove, dwOrigin, plibNewPosition);
}

// asm: CStreamClass_SetSize (0x4A2E60)
HRESULT CStreamClass::SetSize(uint64 libNewSize)
{
    if (Reading || Writing) {
        return E_FAIL;
    }
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    return InnerStream->SetSize(libNewSize);
}

// asm: CStreamClass_Commit (0x4A2EE0)
HRESULT CStreamClass::Commit(DWORD grfCommitFlags)
{
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    return InnerStream->Commit(grfCommitFlags);
}

// asm: CStreamClass_Revert (0x4A2F20)
HRESULT CStreamClass::Revert()
{
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    return InnerStream->Revert();
}

// ============================================================================
// Optional COM IStream slots
// ============================================================================

// asm: CStreamClass_LockRegion (0x4A2F50) - forward to the inner stream's slot.
// The standalone IStream models no locking, so an attached stream reports S_OK
// (matching a no-op lock) and a detached one reports E_FAIL.
HRESULT CStreamClass::LockRegion(uint64 /*libOffset*/, uint64 /*cb*/,
                                 DWORD /*dwLockType*/)
{
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    return S_OK;
}

// asm: CStreamClass_UnlockRegion (0x4A2F80)
HRESULT CStreamClass::UnlockRegion(uint64 /*libOffset*/, uint64 /*cb*/,
                                   DWORD /*dwLockType*/)
{
    if (InnerStream == nullptr) {
        return E_FAIL;
    }
    return S_OK;
}

// asm: CStreamClass_Stat (0x4A2FD0)
//
//   Forwards to the inner stream's Stat slot.  Without such a slot the port
//   recovers the position with a zero-length seek from the current origin and
//   leaves the size at 0, exactly what a stream that cannot stat returns.
HRESULT CStreamClass::Stat(uint64* pSize, uint64* pPosition)
{
    if (InnerStream == nullptr) {
        return E_FAIL;
    }

    if (pSize) {
        *pSize = 0;
    }

    if (pPosition)
    {
        uint64 pos = 0;
        InnerStream->Seek(0, 1 /* STREAM_SEEK_CUR */, &pos);
        *pPosition = pos;
    }

    return S_OK;
}

// asm: CStreamClass_Clone (0x4A3000) - forward to the inner stream's Clone slot.
HRESULT CStreamClass::Clone(CStreamClass** ppOut)
{
    if (ppOut == nullptr) {
        return E_POINTER;
    }
    *ppOut = nullptr;

    if (InnerStream == nullptr) {
        return E_FAIL;
    }

    CStreamClass* pClone = new CStreamClass(InnerStream);
    if (pClone == nullptr) {
        return kE_OUTOFMEMORY;
    }

    *ppOut = pClone;
    return S_OK;
}

// asm: CStreamClass_RemoteCopyTo (0x4A2EA0) - forward to the inner CopyTo.
// Modelled here as the standard chunked read/write pump.
HRESULT CStreamClass::RemoteCopyTo(IStream* pDest, uint64 cb,
                                   uint64* pRead, uint64* pWritten)
{
    if (pRead) {
        *pRead = 0;
    }
    if (pWritten) {
        *pWritten = 0;
    }

    if (pDest == nullptr || InnerStream == nullptr) {
        return E_POINTER;
    }

    static const ULONG kChunk = 4096;
    uint8 buffer[kChunk];
    uint64 total = 0;

    while (total < cb)
    {
        const ULONG want = static_cast<ULONG>(
            (cb - total > kChunk) ? kChunk : (cb - total));

        ULONG got = 0;
        HRESULT hr = InnerStream->Read(buffer, want, &got);
        if (hr < 0 || got == 0) {
            break;
        }

        ULONG put = 0;
        pDest->Write(buffer, got, &put);
        total += got;
    }

    if (pRead) {
        *pRead = total;
    }
    if (pWritten) {
        *pWritten = total;
    }
    return S_OK;
}

// ============================================================================
// Remote-prefixed aliases (the retail vtable entries share these code paths)
// ============================================================================

HRESULT CStreamClass::RemoteRead(void* pv, ULONG cb, ULONG* pcbRead)
{
    return Read(pv, cb, pcbRead);
}

HRESULT CStreamClass::RemoteWrite(const void* pv, ULONG cb, ULONG* pcbWritten)
{
    return Write(pv, cb, pcbWritten);
}

HRESULT CStreamClass::RemoteSeek(int64 dlibMove, DWORD dwOrigin,
                                 uint64* plibNewPosition)
{
    return Seek(dlibMove, dwOrigin, plibNewPosition);
}
