#include "AudioSampleBank.h"
#include "AudFile.h"
#include "../Core/Macros.h"
#include "../IO/CCFileClass.h"

#include <cstring>
#include <algorithm>

namespace
{
    inline uint32 ReadLE32(const uint8* p)
    {
        return static_cast<uint32>(p[0]) | (static_cast<uint32>(p[1]) << 8) |
               (static_cast<uint32>(p[2]) << 16) | (static_cast<uint32>(p[3]) << 24);
    }

    inline bool IsRiff(const uint8* p, uint32 n)
    {
        return n >= 12 && p[0] == 'R' && p[1] == 'I' && p[2] == 'F' && p[3] == 'F';
    }

    // audio.idx 里不带扩展名；调用方常带 .WAV/.AUD，两种都认。
    void StripExtension(const char* pIn, char* pOut, size_t n)
    {
        std::strncpy(pOut, pIn, n - 1);
        pOut[n - 1] = '\0';

        char* pDot = std::strrchr(pOut, '.');
        if (pDot != nullptr && pDot != pOut)
            *pDot = '\0';
    }

    bool ReadWholeFile(const char* pName, std::vector<uint8>& out)
    {
        CCFileClass file(pName);

        // 先 Open 再问大小：Exists()/Size() 走的是 stat，只能看到磁盘文件。
        // audio.idx / audio.bag 在零售安装里是 MIX 内部条目 —— 只有 Open 会
        // 去查 MIX，打开后 Size() 才返回内存缓冲的长度。
        if (!file.Open(static_cast<int32>(FileAccessMode::Read)))
            return false;

        const int64 size = file.Size();
        if (size <= 0 || size > 0x7FFFFFFF)
        {
            file.Close();
            return false;
        }

        out.resize(static_cast<size_t>(size));
        const int32 got = file.Read(out.data(), static_cast<int32>(size));
        file.Close();

        if (got != static_cast<int32>(size))
        {
            out.clear();
            return false;
        }
        return true;
    }
}

AudioSampleBank::AudioSampleBank()
    : Opened(false)
{
}

AudioSampleBank& AudioSampleBank::Instance()
{
    static AudioSampleBank s_Bank;
    return s_Bank;
}

void AudioSampleBank::Close()
{
    Entries.clear();
    Offsets.clear();
    Bag.clear();
    Scratch.clear();
    Opened = false;
}

bool AudioSampleBank::Open(const char* pIndexName)
{
    Close();

    if (pIndexName == nullptr || pIndexName[0] == '\0')
        return false;

    std::vector<uint8> index;
    if (!ReadWholeFile(pIndexName, index))
        return false;

    if (index.size() < 12)
        return false;

    const uint32 count = ReadLE32(index.data() + 8);
    if (count == 0 || count > 1000000u)
        return false;

    // 记录是定长 0x24：name[0x20] + offset。必须按整条记录推进，
    // 否则第二条起全部错位。
    const size_t need = 12 + static_cast<size_t>(count) * 0x24;
    if (index.size() < need)
        return false;

    Entries.resize(count);
    for (uint32 i = 0; i < count; ++i)
    {
        const uint8* pRec = index.data() + 12 + static_cast<size_t>(i) * 0x24;
        std::memcpy(Entries[i].Name, pRec, 0x20);
        Entries[i].Name[0x1F] = '\0';
        Entries[i].Offset = ReadLE32(pRec + 0x20);
    }

    // bag 名 = idx 名换扩展名。
    char bagName[0x104];
    std::strncpy(bagName, pIndexName, sizeof(bagName) - 1);
    bagName[sizeof(bagName) - 1] = '\0';
    char* pDot = std::strrchr(bagName, '.');
    if (pDot != nullptr)
        std::strcpy(pDot, ".bag");
    else
        std::strncat(bagName, ".bag", sizeof(bagName) - std::strlen(bagName) - 1);

    if (!ReadWholeFile(bagName, Bag))
    {
        Entries.clear();
        return false;
    }

    std::sort(Entries.begin(), Entries.end(),
              [](const Entry& a, const Entry& b) {
                  return _strcmpi(a.Name, b.Name) < 0;
              });

    Offsets.reserve(Entries.size());
    for (const Entry& e : Entries)
    {
        if (e.Offset < Bag.size())
            Offsets.push_back(e.Offset);
    }
    std::sort(Offsets.begin(), Offsets.end());
    Offsets.erase(std::unique(Offsets.begin(), Offsets.end()), Offsets.end());

    Opened = true;
    return true;
}

int32 AudioSampleBank::FindIndex(const char* pName) const
{
    if (!Opened || pName == nullptr || pName[0] == '\0')
        return -1;

    char plain[0x20];
    StripExtension(pName, plain, sizeof(plain));

    // 先按原名，再按去掉扩展名。两条都走二分。
    const char* tries[2] = { pName, plain };
    for (int32 t = 0; t < 2; ++t)
    {
        const char* key = tries[t];
        if (key[0] == '\0')
            continue;

        int32 lo = 0;
        int32 hi = Count();
        while (lo < hi)
        {
            const int32 mid = (lo + hi) / 2;
            const int cmp = _strcmpi(Entries[mid].Name, key);
            if (cmp == 0)
                return mid;
            if (cmp < 0)
                lo = mid + 1;
            else
                hi = mid;
        }
    }
    return -1;
}

const char* AudioSampleBank::GetName(int32 index) const
{
    if (index < 0 || index >= Count())
        return "";
    return Entries[index].Name;
}

bool AudioSampleBank::SliceSample(uint32 offset, uint32* pBytes)
{
    if (offset >= Bag.size())
        return false;

    const uint32 remaining = static_cast<uint32>(Bag.size()) - offset;
    const uint8* p = Bag.data() + offset;

    uint32 size = 0;

    // AUD：12 字节头，CompressedSize 是"头之后"的字节数。
    if (remaining >= 12 && Is_AUD_Data(p, static_cast<int32>(remaining)))
    {
        const uint32 comp = ReadLE32(p + 2);
        if (comp > 0 && comp + 12 <= remaining)
            size = comp + 12;
    }

    // RIFF：ChunkSize 是"RIFF 之后"的字节数。
    if (size == 0 && IsRiff(p, remaining))
    {
        const uint32 chunk = ReadLE32(p + 4);
        if (chunk >= 36 && chunk + 8 <= remaining)
            size = chunk + 8;
    }

    // 兜底：下一条样本的起点即本条终点。
    if (size == 0)
    {
        const std::vector<uint32>::const_iterator it =
            std::upper_bound(Offsets.begin(), Offsets.end(), offset);
        if (it != Offsets.end() && *it > offset)
            size = *it - offset;
        else
            size = remaining;
    }

    if (size == 0)
        return false;

    Scratch.assign(p, p + size);
    *pBytes = size;
    return true;
}

const uint8* AudioSampleBank::GetByIndex(int32 index, uint32* pBytes)
{
    if (!Opened || index < 0 || index >= Count())
        return nullptr;
    return SliceSample(Entries[index].Offset, pBytes) ? Scratch.data() : nullptr;
}

const uint8* AudioSampleBank::Get(const char* pName, uint32* pBytes)
{
    const int32 idx = FindIndex(pName);
    if (idx < 0)
        return nullptr;
    return GetByIndex(idx, pBytes);
}
