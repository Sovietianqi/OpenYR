#include "AudioOutput.h"
#include "AudioMixer.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <vector>

#if defined(OPENYR_HAVE_SDL2)
#include <SDL2/SDL.h>
#endif

namespace
{
    // 每次 Mix 最多算这么多帧，避免主循环卡顿后一次性算出巨大的缓冲。
    const uint32 kChunkFrames = 1024;

    void WriteLE16(FILE* f, uint16 v)
    {
        uint8 b[2] = { static_cast<uint8>(v & 0xFF), static_cast<uint8>(v >> 8) };
        std::fwrite(b, 1, 2, f);
    }

    void WriteLE32(FILE* f, uint32 v)
    {
        uint8 b[4] = { static_cast<uint8>(v & 0xFF), static_cast<uint8>(v >> 8),
                       static_cast<uint8>(v >> 16), static_cast<uint8>(v >> 24) };
        std::fwrite(b, 1, 4, f);
    }

    void SeekLE32(FILE* f, long offset, uint32 v)
    {
        uint8 b[4] = { static_cast<uint8>(v & 0xFF), static_cast<uint8>(v >> 8),
                       static_cast<uint8>(v >> 16), static_cast<uint8>(v >> 24) };
        std::fseek(f, offset, SEEK_SET);
        std::fwrite(b, 1, 4, f);
        std::fseek(f, 0, SEEK_END);
    }

#if defined(OPENYR_HAVE_SDL2)
    SDL_AudioDeviceID s_Device = 0;
#endif
}

AudioOutput::AudioOutput()
    : BackendKind(kNone)
    , Enabled(false)
    , InitDone(false)
    , WavFile(nullptr)
    , DataBytes(0)
    , FixedFrames(0)
    , LastTickUs(0)
    , Delivered(0)
{
    Name[0] = '\0';
    WavPath[0] = '\0';
}

AudioOutput::~AudioOutput()
{
    Shutdown();
}

AudioOutput& AudioOutput::Instance()
{
    static AudioOutput s_Output;
    return s_Output;
}

// ============================================================================
// 初始化
// ============================================================================

bool AudioOutput::InitFromEnv()
{
    if (InitDone)
        return Enabled;
    InitDone = true;

    std::strncpy(Name, "none", sizeof(Name) - 1);
    Name[sizeof(Name) - 1] = '\0';

    // 定步长模式：主循环每轮推进固定帧数且不读时钟，音频也必须按固定
    // 帧长推进，否则离线跑出来的 WAV 会一片寂静。
    if (const char* pFixed = std::getenv("OPENYR_FIXED_TIMESTEP"))
    {
        if (pFixed[0] != '\0')
            FixedFrames = 1;   // 占位，真正长度在下面按采样率算
    }

    const char* pSpec = std::getenv("OPENYR_AUDIO");
    if (pSpec == nullptr || pSpec[0] == '\0')
    {
        // 生产默认关闭。但定步长下若显式要求过 WAV，仍走上面的分支。
        return false;
    }

    if (!ParseSpec(pSpec))
    {
        std::fprintf(stderr, "[AUDIO] 无法启用后端 \"%s\"（详见上面的原因）\n", pSpec);
        return false;
    }

    Enabled = true;

    if (FixedFrames != 0)
    {
        // 每逻辑帧的采样帧数：按 30Hz 逻辑帧折算。可用
        // OPENYR_AUDIO_FRAME_MS 覆盖（例如 15Hz 的演示录像）。
        // 缺省按 30Hz 逻辑帧折算；用除法而不是 33ms，避免每帧 1% 的累积漂移。
        FixedFrames = static_cast<uint32>(AudioMixer::Instance().GetOutputRate() / 30);

        if (const char* pMs = std::getenv("OPENYR_AUDIO_FRAME_MS"))
        {
            const long v = std::strtol(pMs, nullptr, 10);
            if (v > 0 && v <= 1000)
                FixedFrames = static_cast<uint32>(
                    AudioMixer::Instance().GetOutputRate() * v / 1000);
        }
        if (FixedFrames == 0)
            FixedFrames = 1;
    }

    LastTickUs = static_cast<uint64>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    std::fprintf(stderr, "[AUDIO] 后端=%s 采样率=%d 声道=%d%s\n",
                 Name, AudioMixer::Instance().GetOutputRate(),
                 AudioMixer::Instance().GetOutputChannels(),
                 (FixedFrames != 0) ? " (定步长)" : "");
    return true;
}

bool AudioOutput::ParseSpec(const char* pSpec)
{
    if (pSpec == nullptr)
        return false;

    if (std::strncmp(pSpec, "none", 4) == 0)
        return false;

    if (std::strncmp(pSpec, "wav:", 4) == 0)
    {
        if (!OpenWav(pSpec + 4))
            return false;
        BackendKind = kWav;
        std::strncpy(Name, "wav", sizeof(Name) - 1);
        Name[sizeof(Name) - 1] = '\0';
        return true;
    }

    if (std::strcmp(pSpec, "sdl") == 0 || std::strcmp(pSpec, "auto") == 0)
    {
#if defined(OPENYR_HAVE_SDL2)
        if (OpenDevice())
        {
            BackendKind = kDevice;
            std::strncpy(Name, "sdl2", sizeof(Name) - 1);
            Name[sizeof(Name) - 1] = '\0';
            return true;
        }
        return false;
#else
        std::fprintf(stderr,
                     "[AUDIO] 本次构建未定义 OPENYR_HAVE_SDL2，无法打开声卡；"
                     "请改用 OPENYR_AUDIO=wav:<路径>。\n");
        return false;
#endif
    }

    std::fprintf(stderr, "[AUDIO] 未知后端 \"%s\"（支持 none / wav:<路径> / sdl）\n", pSpec);
    return false;
}

bool AudioOutput::OpenWav(const char* pPath)
{
    if (pPath == nullptr || pPath[0] == '\0')
    {
        std::fprintf(stderr, "[AUDIO] wav 后端缺少输出路径\n");
        return false;
    }

    std::FILE* f = std::fopen(pPath, "wb");
    if (f == nullptr)
    {
        std::fprintf(stderr, "[AUDIO] 打不开 \"%s\"\n", pPath);
        return false;
    }

    AudioMixer& mixer = AudioMixer::Instance();
    const uint32 rate = static_cast<uint32>(mixer.GetOutputRate());
    const uint32 channels = static_cast<uint32>(mixer.GetOutputChannels());

    std::fwrite("RIFF", 1, 4, f);
    WriteLE32(f, 36);                      // 先占位，Shutdown 回填
    std::fwrite("WAVEfmt ", 1, 8, f);
    WriteLE32(f, 16);                      // fmt 块长度
    WriteLE16(f, 1);                       // PCM
    WriteLE16(f, static_cast<uint16>(channels));
    WriteLE32(f, rate);
    WriteLE32(f, rate * channels * 2);     // ByteRate
    WriteLE16(f, static_cast<uint16>(channels * 2));  // BlockAlign
    WriteLE16(f, 16);                      // BitsPerSample
    std::fwrite("data", 1, 4, f);
    WriteLE32(f, 0);                       // 先占位

    std::strncpy(WavPath, pPath, sizeof(WavPath) - 1);
    WavPath[sizeof(WavPath) - 1] = '\0';

    WavFile = f;
    DataBytes = 0;
    return true;
}

void AudioOutput::CloseWav()
{
    std::FILE* f = static_cast<std::FILE*>(WavFile);
    if (f == nullptr)
        return;

    std::fseek(f, 0, SEEK_END);
    const long total = std::ftell(f);

    SeekLE32(f, 4, static_cast<uint32>(total - 8));        // RIFF 长度
    SeekLE32(f, 40, DataBytes);                            // data 长度
    std::fclose(f);
    WavFile = nullptr;
}

bool AudioOutput::OpenDevice()
{
#if defined(OPENYR_HAVE_SDL2)
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        std::fprintf(stderr, "[AUDIO] SDL_InitSubSystem(AUDIO) 失败: %s\n", SDL_GetError());
        return false;
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = AudioMixer::Instance().GetOutputRate();
    want.format   = AUDIO_S16SYS;
    want.channels = static_cast<Uint8>(AudioMixer::Instance().GetOutputChannels());
    want.samples  = 1024;
    want.callback = nullptr;                 // 队列模式

    SDL_AudioSpec have;
    s_Device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (s_Device == 0)
    {
        std::fprintf(stderr, "[AUDIO] SDL_OpenAudioDevice 失败: %s\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(s_Device, 0);
    return true;
#else
    return false;
#endif
}

void AudioOutput::Shutdown()
{
    if (!Enabled && WavFile == nullptr)
        return;

#if defined(OPENYR_HAVE_SDL2)
    if (BackendKind == kDevice && s_Device != 0)
    {
        SDL_CloseAudioDevice(s_Device);
        s_Device = 0;
    }
#endif

    if (WavFile != nullptr)
        CloseWav();

    if (Enabled)
    {
        // 退出时给一行可对账的收尾统计：录了多长、峰值多少、有没有声源。
        // 只跑静音路径时峰值为 0，正好用来证明"不是没输出，而是确实没声音"。
        std::fprintf(stderr,
                     "[AUDIO] 输出结束：%llu 帧（%.2f 秒）峰值=%d 活动声源=%d\n",
                     static_cast<unsigned long long>(Delivered),
                     static_cast<double>(Delivered) /
                         static_cast<double>(AudioMixer::Instance().GetOutputRate()),
                     AudioMixer::Instance().PeakAmplitude(),
                     AudioMixer::Instance().ActiveVoiceCount());
    }

    Enabled = false;
    BackendKind = kNone;
}

// ============================================================================
// 输出
// ============================================================================

void AudioOutput::Deliver(const int16* pPcm, uint32 frames)
{
    if (frames == 0)
        return;

    const uint32 channels = static_cast<uint32>(AudioMixer::Instance().GetOutputChannels());
    const uint32 bytes = frames * channels * sizeof(int16);

    if (BackendKind == kWav && WavFile != nullptr)
    {
        std::FILE* f = static_cast<std::FILE*>(WavFile);
        std::fwrite(pPcm, 1, bytes, f);
        DataBytes += bytes;
    }
#if defined(OPENYR_HAVE_SDL2)
    else if (BackendKind == kDevice && s_Device != 0)
    {
        // 队列满了会阻塞（SDL 默认），这里控制在两帧以内，避免主循环卡死。
        while (SDL_GetQueuedAudioSize(s_Device) > bytes * 4)
            SDL_Delay(1);
        SDL_QueueAudio(s_Device, pPcm, bytes);
    }
#endif

    Delivered += frames;
}

void AudioOutput::PumpFrames(uint32 frames)
{
    if (!Enabled || frames == 0)
        return;

    static std::vector<int16> s_Buffer;
    const uint32 channels = static_cast<uint32>(AudioMixer::Instance().GetOutputChannels());
    const uint32 capacity = kChunkFrames * channels;
    if (s_Buffer.size() < capacity)
        s_Buffer.resize(capacity);

    uint32 remaining = frames;
    while (remaining > 0)
    {
        const uint32 n = (remaining < kChunkFrames) ? remaining : kChunkFrames;
        AudioMixer::Instance().Mix(s_Buffer.data(), n);
        Deliver(s_Buffer.data(), n);
        remaining -= n;
    }
}

void AudioOutput::Pump()
{
    if (!Enabled)
        return;

    uint32 frames = 0;

    if (FixedFrames != 0)
    {
        frames = FixedFrames;
    }
    else
    {
        const uint64 now = static_cast<uint64>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());

        uint64 delta = (now > LastTickUs) ? (now - LastTickUs) : 0;
        LastTickUs = now;

        // 卡顿保护：单帧最多补 200ms，否则一次卡顿会把声音拖长。
        if (delta > 200000ULL)
            delta = 200000ULL;

        frames = static_cast<uint32>(
            (delta * static_cast<uint64>(AudioMixer::Instance().GetOutputRate())) / 1000000ULL);
    }

    if (frames > 0)
        PumpFrames(frames);
}
