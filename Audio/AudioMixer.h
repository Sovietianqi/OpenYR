#pragma once

// ============================================================================
// AudioMixer —— 软件混音器（把若干路 PCM 音源混成一条 16 位立体声输出）
//
// 为什么需要它：原版的声音走 DirectSound 的副缓冲，VocClass 只是把采样
// 塞进副缓冲后由声卡推进播放位置。本工程没有 DSound，若只"记一个时间戳"
// 就永远听不见（此前 VocClass::Play 正是如此）。混音器把"播放"变成真正
// 可验证的采样运算：按输出采样率逐帧取样、重采样、按音量/声像加权、相加
// 后钳位，输出交错的 int16 缓冲。设备/落盘由 AudioOutput 负责。
//
// 设计约束：
//   * 不打线程、不加锁：Mix() 由主循环按帧驱动，播放位置只由 Mix 推进，
//     因此同样的调用序列必然得到同样的采样（可做逐位回归）。
//   * 音量/声像沿用原版的两套表示：0..255 的 Volume、0..0x4000 的 Pan
//     （0x2000 为正中）。AudioMixer 内部统一折算成 0..1 增益。
//   * 采样缓冲由调用方持有（VocClass::VocData），混音器只保存裸指针；
//     调用方 Stop/Unload 前必须 StopVoice —— VocClass 已经这样做。
// ============================================================================

#include "../Core/Definitions.h"

class AudioMixer
{
public:
    // 64 路足够覆盖 RA2 的场景密度（原版静态音效池上限也是 32 左右，
    // 加上 EVA/音乐/UI 各一路仍有余量）。
    static const int32 kMaxVoices = 64;

    static AudioMixer& Instance();

    // 输出格式。改动会重置所有播放位置（采样率变了，位置无意义）。
    void  SetOutputFormat(int32 sampleRate, int32 channels);
    int32 GetOutputRate() const     { return OutRate; }
    int32 GetOutputChannels() const { return OutChannels; }
    int32 GetOutputFramesPerSecond() const { return OutRate; }

    // 开一路播放。
    //   pOwner   —— 归属对象（通常是 VocClass*），用于按宿主批量停止/查询；
    //               可为空，此时只能用返回的 handle 操作。
    //   pData    —— PCM 采样起始地址（8 位无符号或 16 位有符号小端）。
    //   nBytes   —— 采样字节数。
    //   sampleRate / channels / bits —— 源格式；bits 只支持 8 / 16。
    //   volume   —— 0..1 线性增益。
    //   pan      —— -1（全左）..+1（全右）。
    //   loop     —— 播完是否从头再来。
    // 返回 handle（>=0），失败返回 -1（池满或参数非法）。
    int32 AddVoice(const void* pOwner,
                   const uint8* pData, uint32 nBytes,
                   int32 sampleRate, int32 channels, int32 bits,
                   float volume, float pan, bool loop);

    bool StopVoice(int32 handle);
    int32 StopVoicesByOwner(const void* pOwner);

    bool SetVoiceVolume(int32 handle, float volume);
    bool SetVoicePan(int32 handle, float pan);

    // 按宿主批量调整（播报类对象在播放期间改音量/声像时用）。
    int32 SetOwnerVolume(const void* pOwner, float volume);
    int32 SetOwnerPan(const void* pOwner, float pan);

    bool IsVoiceActive(int32 handle) const;
    bool IsOwnerPlaying(const void* pOwner) const;
    int32 CountOwnerVoices(const void* pOwner) const;
    int32 ActiveVoiceCount() const;

    // 把 frames 帧（每帧 OutChannels 个 int16）混进 pOut。pOut 会被覆盖：
    // 先清零再累加，静音时输出全 0。
    void Mix(int16* pOut, uint32 frames);

    // 总线增益（0..1），缺省 1。
    void  SetMasterVolume(float v);
    float GetMasterVolume() const { return MasterVolume; }

    uint64 FramesMixed() const { return TotalFrames; }
    int32  PeakAmplitude() const { return Peak; }   // 0..32767，绝对值峰值
    void   ResetPeak() { Peak = 0; }

    // 停掉所有声音（换关/退出）。不清总线音量。
    void Reset();

private:
    AudioMixer();

    struct Voice
    {
        const void*  Owner;
        const uint8* Data;
        uint32       Bytes;
        int32        Rate;         // 源采样率
        uint8        Channels;
        uint8        Bits;
        double       Pos;          // 源采样帧位置（带小数，用于线性插值）
        float        GainL;
        float        GainR;
        bool         Loop;
        bool         Active;
    };

    // 取源文件第 frame 帧的第 ch 声道，归一化到 -1..1（越界取 0）。
    static float SampleAt(const Voice& v, uint32 frame, int32 ch, uint32 frames);
    static uint32 SourceFrames(const Voice& v);

    int32 FindFreeSlot() const;

    Voice    Voices[kMaxVoices];
    int32    OutRate;
    int32    OutChannels;
    float    MasterVolume;
    uint64   TotalFrames;
    int32    Peak;
};
