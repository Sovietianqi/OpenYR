#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"

class Surface;

// ============================================================================
// MovieHandleClass - 影片播放句柄（原版 vec_MovieHandles 的元素类型）
//
// asm 恢复自 BINK32_Handle_Movie_432E40 (0x432E40)。原版句柄是 0x18 字节的
// 多态体：+0 处装 BIK/VQ 两张 vftable 之一，+4 处挂解码器，+8/+0C 是
// BLinkCopyToBuffer 的 flags 与目标面，+10h/+14h 是目标坐标。
//
// 本项目没有 Bink SDK，也没有 VQA 解码器，因此：
//   * 句柄布局、焦点暂停/恢复状态机、逐帧流水线的**控制流**按 asm 保留；
//   * 解码器侧的 BinkDoFrame / BinkWait / BinkNextFrame / BinkCopyToBuffer /
//     BinkPause / BinkSetVolume 没有实体。所有落到解码器的动作都收敛到
//     下面几个虚口，默认实现只记录状态、不碰像素（降级点，已在各函数注释
//     中逐条标注）。
//   * Decoder 为 0 时（本项目永远如此）AI() 在焦点分支之后就返回，不会
//     进入原版那条 "while (BinkWait()==0)" 的循环，因此不会空转。
// ============================================================================
class MovieHandleClass
{
public:
    MovieHandleClass() noexcept;
    virtual ~MovieHandleClass();

    // ------------------------------------------------------------------------
    // asm: BINK32_Handle_Movie_432E40 (0x432E40, __thiscall, retn 0Ch)
    //
    //   if (g_MovieVolume != g_SfxVolume):        ; 音量跟随全局
    //       g_MovieVolume = g_SfxVolume
    //       BinkSetVolume([this+4], (int)(g_SfxVolume * flt_7E3A70))
    //   if (GameInFocus):
    //       if ([this+2Ch] == 0):                 ; 未在播 -> 恢复
    //           WWDebugString("Resume bink movie\n")
    //           [this+2Ch] = 1 ; BinkPause([this+4], 0)
    //           若 [this+0Ch] == DSurface_Primary:
    //               GetClientRect/ClientToScreen 把 X/Y 加上客户区原点
    //           sub_4331F0(this, 1, y, x, [this+0Ch])     ; 立刻补一帧
    //   else:
    //       if ([this+2Ch] == 1):                 ; 在播 -> 暂停
    //           WWDebugString("Pause bink movie\n")
    //           [this+2Ch] = 0 ; BinkPause([this+4], 1)
    //   if ([this+2Dh] == 0 && BinkWait([this+4]) != 0) return 1
    //   do {
    //       BinkDoFrame([this+4])
    //       [this+2Dh] = 0 ; bl = 0
    //       if ([this+20h] && sub_6C9C60([decoder+0Ch] * [this+24h])):
    //           sub_433330(this) ; bl = 1         ; 换解码缓冲
    //       若面可锁: 锁面 -> BinkCopyToBuffer -> 解锁
    //       sub_433270(this)
    //       [this+30h] = [decoder+0Ch]
    //       BinkNextFrame([this+4])
    //   } while (BinkWait([this+4]) == 0)
    //   return 1
    // ------------------------------------------------------------------------
    virtual int32 AI();

    // ------------------------------------------------------------------------
    // asm: sub_4331F0 (0x4331F0, __thiscall, retn 10h)
    //   把当前帧搬到目标面：可选先 sub_433330，然后
    //   锁面 -> [vt+74h] -> [vt+80h] -> BinkCopyToBuffer -> 解锁 -> sub_433270。
    // ------------------------------------------------------------------------
    virtual void Blit_Frame(Surface* pSurface, int32 flags, int32 y, int32 x);

    // 解码器侧的三个虚口（原版是 Bink SDK 的静态导入）。
    // 本项目无解码器：默认实现留空，返回"已播完"。
    virtual void    Decoder_Pause(bool paused) { (void)paused; }
    virtual int32   Decoder_Wait() const { return 1; }   // 非 0 = 该帧已就绪/已结束
    virtual void    Decoder_Next_Frame() {}
    virtual void    Decoder_Do_Frame() {}
    virtual void    Decoder_Set_Volume(int32 volume) { (void)volume; }

    // asm 字段（偏移即原版句柄的偏移）
    void*   Decoder;             // +0x04  Bink/VQA 解码器
    int32   Flags;               // +0x08  BinkCopyToBuffer flags
    Surface* TargetSurface;      // +0x0C  目标面
    int32   X;                   // +0x10
    int32   Y;                   // +0x14
    int32   Field20;             // +0x20  非 0 时每帧检查一次解码缓冲
    int32   Field24;             // +0x24  缓冲检查的乘数
    uint8   IsPlaying;           // +0x2C  1 = 正在播（未暂停）
    uint8   SkipWait;            // +0x2D  1 = 本帧不等 BinkWait
    int32   Field30;             // +0x30  上一帧的 [decoder+0Ch]

    // asm: dword_89C490 / unk_A8EB9C / flt_7E3A70
    static float MovieVolume;    // dword_89C490 —— 上次下发的音量
    static float SfxVolume;      // unk_A8EB9C   —— 全局音效音量
    static float VolumeScale;    // flt_7E3A70   —— 音量缩放
};
