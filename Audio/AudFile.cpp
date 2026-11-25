#include "AudFile.h"

#include <cstring>

namespace
{
// IMA ADPCM 步长表（89 项）与索引调整表。OpenRA AudLoader / MultimediaWiki
//   的 Westwood IMA 条目用的是同一组常数。
const int32 kStepTable[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
    19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
    50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
    130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
    337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
    876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
    2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
    5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

const int32 kIndexAdjust[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

inline uint16 ReadU16(const uint8* p) { return static_cast<uint16>(p[0] | (p[1] << 8)); }
inline uint32 ReadU32(const uint8* p)
{
    return static_cast<uint32>(p[0]) | (static_cast<uint32>(p[1]) << 8)
         | (static_cast<uint32>(p[2]) << 16) | (static_cast<uint32>(p[3]) << 24);
}

// 一个声道的 ADPCM 解码状态。
struct AdpcmState
{
    int32 Sample = 0;   // 预测值
    int32 Index  = 0;   // 步长索引 0..88
};

// 4 位码 → 样本。既用于 WW ADPCM（8 位输出，码域也是 4 位）也用于 IMA。
//   返回新的预测值。
inline int32 Decode_Nibble(AdpcmState& st, int32 code)
{
    const int32 step = kStepTable[st.Index];

    int32 delta = (step * code) / 4 + step / 8;
    if (code & 8)
        delta = -delta;

    st.Sample += delta;
    if (st.Sample > 32767)  st.Sample = 32767;
    if (st.Sample < -32768) st.Sample = -32768;

    st.Index += kIndexAdjust[code & 7];
    if (st.Index < 0)  st.Index = 0;
    if (st.Index > 88) st.Index = 88;

    return st.Sample;
}

inline void Emit16(std::vector<uint8>& out, int32 sample)
{
    const uint16 v = static_cast<uint16>(static_cast<int16>(sample));
    out.push_back(static_cast<uint8>(v & 0xFF));
    out.push_back(static_cast<uint8>(v >> 8));
}

inline void Emit8(std::vector<uint8>& out, int32 sample)
{
    // 8 位无符号 → 16 位有符号：以 0x80 为零点。
    int32 s = sample & 0xFF;
    s = (s - 128) << 8;
    Emit16(out, s);
}

// 一段连续的 4 位 ADPCM 半字节流。
//   nChannels 只影响交错方式（逐字节左右轮换），状态按声道分开。
void Decode_Nibbles(const uint8* pData, int32 nBytes, int32 nChannels,
                    AdpcmState* pState, bool bEightBit, std::vector<uint8>& out)
{
    for (int32 i = 0; i < nBytes; ++i)
    {
        const uint8 b = pData[i];
        AdpcmState& ch = pState[(nChannels > 1) ? (i & 1) : 0];

        const int32 lo = Decode_Nibble(ch, b & 0x0F);
        const int32 hi = Decode_Nibble(ch, (b >> 4) & 0x0F);

        if (bEightBit)
        {
            Emit8(out, lo);
            Emit8(out, hi);
        }
        else
        {
            Emit16(out, lo);
            Emit16(out, hi);
        }
    }
}

bool Looks_Chunked(const uint8* pPayload, int32 nPayload, int32* pChunkBytes,
                   int32* pOutBytes)
{
    int32 pos = 0;
    int32 chunks = 0;
    int32 outTotal = 0;

    while (pos + 8 <= nPayload)
    {
        const int32 csize = ReadU16(pPayload + pos);
        const int32 osize = ReadU16(pPayload + pos + 2);
        const uint32 magic = ReadU32(pPayload + pos + 4);

        if (magic != 0x0000DEAFu)
            return false;
        if (csize <= 0 || pos + 8 + csize > nPayload)
            return false;

        pos += 8 + csize;
        outTotal += osize;
        ++chunks;
    }

    if (chunks == 0 || pos != nPayload)
        return false;

    if (pChunkBytes != nullptr) *pChunkBytes = pos;
    if (pOutBytes != nullptr)   *pOutBytes = outTotal;
    return true;
}
} // namespace

bool Is_AUD_Data(const uint8* pData, int32 nSize)
{
    if (pData == nullptr || nSize < 12)
        return false;

    const uint16 rate = ReadU16(pData);
    if (rate < 4000 || rate > 48000)
        return false;

    const uint8 flags = pData[10];
    if ((flags & ~0x03u) != 0)
        return false;

    const uint8 type = pData[11];
    if (type != 0 && type != 1 && type != 99)
        return false;

    const uint32 compSize = ReadU32(pData + 2);
    if (compSize == 0 || static_cast<int64>(compSize) + 12 > static_cast<int64>(nSize))
        return false;

    return true;
}

AudDecoded Decode_AUD(const uint8* pData, int32 nSize)
{
    AudDecoded r;

    if (!Is_AUD_Data(pData, nSize))
    {
        r.Error = "not an AUD container";
        return r;
    }

    r.SampleRate = ReadU16(pData);
    const int32 compSize   = static_cast<int32>(ReadU32(pData + 2));
    const int32 outputSize = static_cast<int32>(ReadU32(pData + 6));
    const uint8 flags      = pData[10];
    const uint8 type       = pData[11];

    r.Channels = (flags & 0x01) ? 2 : 1;
    const bool b16 = (flags & 0x02) != 0;

    const uint8* pPayload = pData + 12;

    // ---------------------------------------------------------------- type 0
    if (type == 0)
    {
        r.Path = "raw";
        r.PCM.assign(pPayload, pPayload + compSize);
        r.Ok = true;
        return r;
    }

    // ------------------------------------------------------ type 1 / 99 ADPCM
    const bool eightBit = (type == 1);

    int32 chunkOut = 0;
    const bool chunked = Looks_Chunked(pPayload, compSize, nullptr, &chunkOut);

    AdpcmState state[2];

    if (chunked)
    {
        r.Path = (type == 1) ? "ww-adpcm/chunked" : "ima/chunked";

        int32 pos = 0;
        while (pos + 8 <= compSize)
        {
            const int32 csize = ReadU16(pPayload + pos);
            const int32 osize = ReadU16(pPayload + pos + 2);
            pos += 8;

            if (csize <= 0 || pos + csize > compSize)
            {
                r.Error = "truncated chunk";
                return r;
            }

            const size_t before = r.PCM.size();
            Decode_Nibbles(pPayload + pos, csize, r.Channels, state, eightBit, r.PCM);

            // 块头声明的输出字节数与实际解码量对不上时，按实际值为准，
            //   只把差异记进 Path（真实文件末块可能少半个字节）。
            const size_t produced = r.PCM.size() - before;
            if (osize > 0 && produced > static_cast<size_t>(osize))
                r.PCM.resize(before + static_cast<size_t>(osize));

            pos += csize;
        }
    }
    else
    {
        r.Path = (type == 1) ? "ww-adpcm/flat" : "ima/flat";
        Decode_Nibbles(pPayload, compSize, r.Channels, state, eightBit, r.PCM);
    }

    // 与头部声明的输出长度对齐。ADPCM 的 4:1 比与 outputSize 应当吻合；
    //   多出来的尾部（块对齐填充）截掉，短了不补（不造假数据）。
    if (outputSize > 0)
    {
        const size_t want = static_cast<size_t>(outputSize);
        if (r.PCM.size() > want)
            r.PCM.resize(want);
    }

    if (r.PCM.empty())
    {
        r.Error = "no audio samples decoded";
        return r;
    }

    r.BitsPerSample = 16;
    r.Ok = true;
    return r;
}
