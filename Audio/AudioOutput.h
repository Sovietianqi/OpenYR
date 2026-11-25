#pragma once

// ============================================================================
// AudioOutput —— 混音器的输出后端（声卡 / 落盘 / 关闭）
//
// AudioMixer 只负责"算出采样"，把采样送到哪里是这里的职责。原版走
// DirectSound 副缓冲；本工程提供两条等价出口：
//
//   1. WAV 落盘（零依赖，默认可用于验证）
//        OPENYR_AUDIO=wav:/tmp/out.wav
//      每帧把混音结果追加到 WAV，退出时回填 RIFF/data 长度。
//      这条路径完全由游戏帧驱动，不读时钟，输出逐位可复现 —— 回归测试
//      与"声音到底有没有发出来"的取证都靠它。
//
//   2. SDL2 声卡（编译期开关 OPENYR_HAVE_SDL2）
//        OPENYR_AUDIO=sdl    （或 =auto 时优先选它）
//      SDL_OpenAudioDevice 拿 48kHz/立体声/S16，帧尾 SDL_QueueAudio。
//      没有 SDL2 开发包时整段编译掉，选中该后端会明确报错而不是静默。
//
// 未设置 OPENYR_AUDIO 时后端为 none：不做任何混音，开销为零（生产默认，
// 与之前的行为一致，回归 CRC 不受影响）。
// ============================================================================

#include "../Core/Definitions.h"

class AudioOutput
{
public:
    static AudioOutput& Instance();

    // 读环境变量并打开后端。重复调用只生效一次。
    //   返回是否真的有一个可用后端。
    bool InitFromEnv();

    bool IsEnabled() const { return Enabled; }
    const char* BackendName() const { return Name; }

    // 主循环每帧调用一次：按当前模式决定本帧要混多少采样，
    // 混完立刻送后端。
    void Pump();

    // 显式混固定帧数（定步长回归 / 单测）。后端未启用时是空操作。
    void PumpFrames(uint32 frames);

    // 收尾：刷缓冲、回填 WAV 头、关闭设备。可重复调用。
    void Shutdown();

    uint64 FramesDelivered() const { return Delivered; }

    // 定步长模式下每逻辑帧混多少帧（未启用定步长时为 0）。
    uint32 FixedFramesPerTick() const { return FixedFrames; }

private:
    AudioOutput();
    ~AudioOutput();

    bool ParseSpec(const char* pSpec);
    bool OpenWav(const char* pPath);
    bool OpenDevice();
    void Deliver(const int16* pPcm, uint32 frames);
    void CloseWav();

    enum Backend
    {
        kNone = 0,
        kWav,
        kDevice
    };

    Backend BackendKind;
    bool    Enabled;
    bool    InitDone;
    char    Name[32];
    char    WavPath[512];
    void*   WavFile;          // FILE*
    uint32  DataBytes;
    uint32  FixedFrames;      // 定步长模式每逻辑帧的采样帧数
    uint64  LastTickUs;
    uint64  Delivered;
};
