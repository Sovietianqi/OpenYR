#pragma once

// ============================================================================
// AudioSampleBank —— RA2/YR 的 audio.idx + audio.bag 采样库
//
// 原版的声音素材不是散落的 .wav，而是打进 audio.bag 的一整块数据，
// 由 audio.idx 建名字索引：
//
//   audio.idx : 0x0C 字节头 { u32 保留, u32 版本, u32 条数 }，
//               随后每条 0x24 字节 { char name[0x20]; u32 offset }。
//               记录按 name 升序，故可二分查找。
//   audio.bag : 裸字节流，offset 指向某条样本的起点。
//
// 样本本身的长度靠容器头判定（优先 AUD 的 CompressedSize，其次 RIFF 的
// ChunkSize）；两者都不成立时退回"下一条偏移 - 本条偏移"，最后一条取到
// 文件尾。这个兜底是必要的：bag 里也混有少量非 AUD 的历史素材。
//
// 注意：本模块只做"名字 → 原始字节"的定位，解码仍由 VocClass::ParseAUD /
// ParseWAV 完成，这样 ADPCM 的正确性只在一个地方验证。
// ============================================================================

#include "../Core/Definitions.h"
#include <vector>

class AudioSampleBank
{
public:
    static AudioSampleBank& Instance();

    // pIndexName 形如 "audio.idx"；bag 名由它换扩展名得到。
    // 相对路径按游戏目录解析，也会穿过已挂载的 MIX。
    bool Open(const char* pIndexName);
    bool IsOpen() const { return Opened; }
    int32 Count() const { return static_cast<int32>(Entries.size()); }

    // 按名字取原始字节（大小写不敏感，允许调用方带 .wav/.aud 扩展名）。
    // 返回内部缓冲指针，下次调用前有效；未命中返回 nullptr。
    const uint8* Get(const char* pName, uint32* pBytes);

    // 按索引表下标取（[SoundList] 里记录的就是这个下标）。
    const uint8* GetByIndex(int32 index, uint32* pBytes);

    // 名字是否在表里（不做读取）。
    int32 FindIndex(const char* pName) const;

    const char* GetName(int32 index) const;
    void Close();

private:
    AudioSampleBank();

    struct Entry
    {
        char   Name[0x20];
        uint32 Offset;
    };

    bool SliceSample(uint32 offset, uint32* pBytes);

    std::vector<Entry>  Entries;
    std::vector<uint32> Offsets;     // 升序，供"下一条偏移"兜底
    std::vector<uint8>  Bag;
    std::vector<uint8>  Scratch;
    bool                Opened;
};
