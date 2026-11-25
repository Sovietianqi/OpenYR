#include "AudioMixer.h"

#include <cmath>
#include <cstring>

// ============================================================================
// 构造 / 全局实例
// ============================================================================

AudioMixer::AudioMixer()
    : OutRate(48000)
    , OutChannels(2)
    , MasterVolume(1.0f)
    , TotalFrames(0)
    , Peak(0)
{
    std::memset(Voices, 0, sizeof(Voices));
}

AudioMixer& AudioMixer::Instance()
{
    static AudioMixer s_Mixer;
    return s_Mixer;
}

void AudioMixer::SetOutputFormat(int32 sampleRate, int32 channels)
{
    if (sampleRate < 4000 || sampleRate > 192000)
        sampleRate = 48000;
    if (channels < 1 || channels > 2)
        channels = 2;

    if (sampleRate == OutRate && channels == OutChannels)
        return;

    // 采样率/声道数变了，旧的位置全部失去意义。
    Reset();

    OutRate     = sampleRate;
    OutChannels = channels;
}

void AudioMixer::SetMasterVolume(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    MasterVolume = v;
}

void AudioMixer::Reset()
{
    for (int32 i = 0; i < kMaxVoices; ++i)
        Voices[i].Active = false;
}

// ============================================================================
// 声源取样
// ============================================================================

uint32 AudioMixer::SourceFrames(const Voice& v)
{
    const uint32 bytesPerFrame =
        static_cast<uint32>(v.Channels) * static_cast<uint32>(v.Bits / 8);
    if (bytesPerFrame == 0)
        return 0;
    return v.Bytes / bytesPerFrame;
}

float AudioMixer::SampleAt(const Voice& v, uint32 frame, int32 ch, uint32 frames)
{
    if (frame >= frames)
        return 0.0f;

    const uint32 bytesPerSample = static_cast<uint32>(v.Bits / 8);
    const uint32 offset = (frame * static_cast<uint32>(v.Channels) +
                           static_cast<uint32>(ch)) * bytesPerSample;
    if (offset + bytesPerSample > v.Bytes)
        return 0.0f;

    const uint8* p = v.Data + offset;

    if (v.Bits == 8)
    {
        // 8 位 PCM 是无符号、以 0x80 为零点。
        return (static_cast<int32>(p[0]) - 128) / 128.0f;
    }

    // 16 位有符号小端。
    const int16 s = static_cast<int16>(static_cast<uint16>(p[0]) |
                                       (static_cast<uint16>(p[1]) << 8));
    return static_cast<float>(s) / 32768.0f;
}

// ============================================================================
// 播放管理
// ============================================================================

int32 AudioMixer::FindFreeSlot() const
{
    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (!Voices[i].Active)
            return i;
    }
    return -1;
}

int32 AudioMixer::AddVoice(const void* pOwner,
                           const uint8* pData, uint32 nBytes,
                           int32 sampleRate, int32 channels, int32 bits,
                           float volume, float pan, bool loop)
{
    if (pData == nullptr || nBytes == 0)
        return -1;
    if (bits != 8 && bits != 16)
        return -1;
    if (channels < 1 || channels > 2)
        return -1;
    if (sampleRate < 4000 || sampleRate > 192000)
        return -1;

    const int32 slot = FindFreeSlot();
    if (slot < 0)
        return -1;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 4.0f) volume = 4.0f;      // 允许 4 倍叠加（音效本身偏轻时）
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    // 等功率声像：pan=-1 全左、0 居中（两路各 -3dB）、+1 全右。
    const float theta = (pan + 1.0f) * 0.785398163f;   // (pan+1)*pi/4
    const float gainL = volume * std::cos(theta);
    const float gainR = volume * std::sin(theta);

    Voice& v = Voices[slot];
    v.Owner    = pOwner;
    v.Data     = pData;
    v.Bytes    = nBytes;
    v.Rate     = sampleRate;
    v.Channels = static_cast<uint8>(channels);
    v.Bits     = static_cast<uint8>(bits);
    v.Pos      = 0.0;
    v.GainL    = gainL;
    v.GainR    = gainR;
    v.Loop     = loop;
    v.Active   = true;

    return slot;
}

bool AudioMixer::StopVoice(int32 handle)
{
    if (handle < 0 || handle >= kMaxVoices)
        return false;
    if (!Voices[handle].Active)
        return false;
    Voices[handle].Active = false;
    return true;
}

int32 AudioMixer::StopVoicesByOwner(const void* pOwner)
{
    if (pOwner == nullptr)
        return 0;

    int32 stopped = 0;
    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (Voices[i].Active && Voices[i].Owner == pOwner)
        {
            Voices[i].Active = false;
            ++stopped;
        }
    }
    return stopped;
}

bool AudioMixer::SetVoiceVolume(int32 handle, float volume)
{
    if (handle < 0 || handle >= kMaxVoices || !Voices[handle].Active)
        return false;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 4.0f) volume = 4.0f;

    // 声像比例保持不变，只缩放总增益。
    Voice& v = Voices[handle];
    const float cur = std::sqrt(v.GainL * v.GainL + v.GainR * v.GainR);
    if (cur <= 0.0f)
    {
        v.GainL = volume * 0.70710678f;
        v.GainR = volume * 0.70710678f;
    }
    else
    {
        const float scale = volume / cur;
        v.GainL *= scale;
        v.GainR *= scale;
    }
    return true;
}

bool AudioMixer::SetVoicePan(int32 handle, float pan)
{
    if (handle < 0 || handle >= kMaxVoices || !Voices[handle].Active)
        return false;

    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    Voice& v = Voices[handle];
    const float volume = std::sqrt(v.GainL * v.GainL + v.GainR * v.GainR);

    const float theta = (pan + 1.0f) * 0.785398163f;
    v.GainL = volume * std::cos(theta);
    v.GainR = volume * std::sin(theta);
    return true;
}

int32 AudioMixer::SetOwnerVolume(const void* pOwner, float volume)
{
    if (pOwner == nullptr)
        return 0;

    int32 n = 0;
    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (Voices[i].Active && Voices[i].Owner == pOwner)
        {
            if (SetVoiceVolume(i, volume))
                ++n;
        }
    }
    return n;
}

int32 AudioMixer::SetOwnerPan(const void* pOwner, float pan)
{
    if (pOwner == nullptr)
        return 0;

    int32 n = 0;
    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (Voices[i].Active && Voices[i].Owner == pOwner)
        {
            if (SetVoicePan(i, pan))
                ++n;
        }
    }
    return n;
}

bool AudioMixer::IsVoiceActive(int32 handle) const{
    if (handle < 0 || handle >= kMaxVoices)
        return false;
    return Voices[handle].Active;
}

bool AudioMixer::IsOwnerPlaying(const void* pOwner) const
{
    if (pOwner == nullptr)
        return false;

    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (Voices[i].Active && Voices[i].Owner == pOwner)
            return true;
    }
    return false;
}

int32 AudioMixer::CountOwnerVoices(const void* pOwner) const
{
    if (pOwner == nullptr)
        return 0;

    int32 n = 0;
    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (Voices[i].Active && Voices[i].Owner == pOwner)
            ++n;
    }
    return n;
}

int32 AudioMixer::ActiveVoiceCount() const
{
    int32 n = 0;
    for (int32 i = 0; i < kMaxVoices; ++i)
    {
        if (Voices[i].Active)
            ++n;
    }
    return n;
}

// ============================================================================
// 混音
//
//   逐输出帧：把每一路当前取样值按声像增益累加，再乘总线增益、钳位成
//   int16。累加顺序固定为槽位 0..63，浮点运算顺序固定，因此同一输入序列
//   的输出逐位可复现。
// ============================================================================

void AudioMixer::Mix(int16* pOut, uint32 frames)
{
    if (pOut == nullptr || frames == 0)
        return;

    const uint32 samples = frames * static_cast<uint32>(OutChannels);
    std::memset(pOut, 0, sizeof(int16) * samples);

    const bool stereoOut = (OutChannels == 2);

    for (uint32 f = 0; f < frames; ++f)
    {
        float accL = 0.0f;
        float accR = 0.0f;

        for (int32 i = 0; i < kMaxVoices; ++i)
        {
            Voice& v = Voices[i];
            if (!v.Active)
                continue;

            const uint32 srcFrames = SourceFrames(v);
            if (srcFrames == 0)
            {
                v.Active = false;
                continue;
            }

            const uint32 idx  = static_cast<uint32>(v.Pos);
            const float  frac = static_cast<float>(v.Pos - static_cast<double>(idx));

            float sL, sR;

            if (v.Channels == 1)
            {
                const float s0 = SampleAt(v, idx, 0, srcFrames);
                const float s1 = (idx + 1 < srcFrames)
                                     ? SampleAt(v, idx + 1, 0, srcFrames)
                                     : (v.Loop ? SampleAt(v, 0, 0, srcFrames) : s0);
                sL = sR = s0 + (s1 - s0) * frac;
            }
            else
            {
                const float l0 = SampleAt(v, idx, 0, srcFrames);
                const float l1 = (idx + 1 < srcFrames)
                                     ? SampleAt(v, idx + 1, 0, srcFrames)
                                     : (v.Loop ? SampleAt(v, 0, 0, srcFrames) : l0);
                const float r0 = SampleAt(v, idx, 1, srcFrames);
                const float r1 = (idx + 1 < srcFrames)
                                     ? SampleAt(v, idx + 1, 1, srcFrames)
                                     : (v.Loop ? SampleAt(v, 0, 1, srcFrames) : r0);
                sL = l0 + (l1 - l0) * frac;
                sR = r0 + (r1 - r0) * frac;
            }

            accL += sL * v.GainL;
            accR += sR * v.GainR;

            const double step = static_cast<double>(v.Rate) /
                                static_cast<double>(OutRate);
            v.Pos += step;

            if (v.Pos >= static_cast<double>(srcFrames))
            {
                if (v.Loop)
                {
                    v.Pos -= static_cast<double>(srcFrames);
                    if (v.Pos < 0.0)
                        v.Pos = 0.0;
                }
                else
                {
                    v.Active = false;   // 本路播完；后续帧不再参与
                }
            }
        }

        float outL = accL * MasterVolume;
        float outR = accR * MasterVolume;

        // 钳位（硬限幅）。混音超过满刻度时保形优先，不做软压缩。
        if (outL >  1.0f) outL =  1.0f;
        if (outL < -1.0f) outL = -1.0f;
        if (outR >  1.0f) outR =  1.0f;
        if (outR < -1.0f) outR = -1.0f;

        int32 qL = static_cast<int32>(outL * 32767.0f);
        int32 qR = static_cast<int32>(outR * 32767.0f);

        if (stereoOut)
        {
            pOut[f * 2 + 0] = static_cast<int16>(qL);
            pOut[f * 2 + 1] = static_cast<int16>(qR);
        }
        else
        {
            pOut[f] = static_cast<int16>((qL + qR) / 2);
        }
    }

    for (uint32 s = 0; s < samples; ++s)
    {
        const int32 a = (pOut[s] < 0) ? -static_cast<int32>(pOut[s])
                                      :  static_cast<int32>(pOut[s]);
        if (a > Peak)
            Peak = a;
    }

    TotalFrames += frames;
}
