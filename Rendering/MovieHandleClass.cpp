#include "MovieHandleClass.h"

#include "Surface.h"
#include "../Game/Game.h"

#include <cstdio>

// ============================================================================
// MovieHandleClass - 影片句柄（asm: BINK32_Handle_Movie_432E40 / sub_4331F0）
//
// 控制流逐段对齐 0x432E40；解码器侧动作按头文件说明收敛到虚口。
// ============================================================================

float MovieHandleClass::MovieVolume = 0.0f;   // asm: dword_89C490
float MovieHandleClass::SfxVolume   = 0.0f;   // asm: unk_A8EB9C
float MovieHandleClass::VolumeScale = 1.0f;   // asm: flt_7E3A70

MovieHandleClass::MovieHandleClass() noexcept
    : Decoder(nullptr)
    , Flags(0)
    , TargetSurface(nullptr)
    , X(0)
    , Y(0)
    , Field20(0)
    , Field24(0)
    , IsPlaying(0)
    , SkipWait(0)
    , Field30(0)
{
}

MovieHandleClass::~MovieHandleClass() = default;

// ============================================================================
// asm: BINK32_Handle_Movie_432E40 (0x432E40)
// ============================================================================
int32 MovieHandleClass::AI()
{
    // ---- 音量跟随 ----------------------------------------------------------
    // asm: fld dword_89C490 / fcomp unk_A8EB9C / test ah,40h / jnz skip
    if (MovieVolume != SfxVolume) {
        MovieVolume = SfxVolume;                       // asm: mov dword_89C490,eax
        const int32 vol = static_cast<int32>(SfxVolume * VolumeScale);
        Decoder_Set_Volume(vol);                       // asm: BinkSetVolume([this+4], vol)
    }

    // ---- 焦点切换 ----------------------------------------------------------
    if (Game::GameInFocus) {
        // asm: loc_432EB6 —— 有焦点且未在播 -> 恢复
        if (IsPlaying == 0) {
            std::printf("Resume bink movie\n");        // asm: WWDebugString
            IsPlaying = 1;                             // asm: mov byte [esi+2Ch],1
            Decoder_Pause(false);                      // asm: BinkPause([this+4], 0)

            int32 x = X;
            int32 y = Y;
            // asm: cmp [esi+0Ch], DSurface_Primary / 加客户区原点
            //
            // 降级点：本项目的 DSurface 没有"主面"全局（DSurface 只有
            // CreatePrimary 的返回值），无法做这个相等比较，因此恒走
            // loc_432F24 的"不加原点"分支。
            Blit_Frame(TargetSurface, 1, y, x);         // asm: sub_4331F0(this,1,y,x,surf)
        }
    } else if (IsPlaying == 1) {
        // asm: cmp al,1 / jnz skip -> 失焦且在播 -> 暂停
        std::printf("Pause bink movie\n");             // asm: WWDebugString
        IsPlaying = 0;                                 // asm: mov byte [esi+2Ch],0
        Decoder_Pause(true);                           // asm: BinkPause([this+4], 1)
    }

    // ---- 首帧等待 ----------------------------------------------------------
    // asm: mov al,[esi+2Dh] / test / jnz loc_432F58 ; BinkWait != 0 -> retn 1
    if (SkipWait == 0 && Decoder_Wait() != 0)
        return 1;

    // ---- 逐帧循环 ----------------------------------------------------------
    // 原版这里是 `do { ... } while (BinkWait()==0)`。Decoder 为空时
    // （本项目永远如此）Decoder_Wait() 恒非 0，循环只走一圈就退出。
    uint8 swapped = 0;                                 // asm: xor bl,bl
    for (;;) {
        Decoder_Do_Frame();                            // asm: BinkDoFrame([this+4])
        SkipWait = 0;                                  // asm: mov byte [esi+2Dh],0
        swapped = 0;

        // asm: if ([esi+20h] && sub_6C9C60([decoder+0Ch]*[esi+24h])) ...
        if (Field20 != 0 && Decoder != nullptr) {
            const int32 width = *reinterpret_cast<const int32*>(
                static_cast<uint8*>(Decoder) + 0x0C);
            const int32 need = width * Field24;
            (void)need;                                // asm: sub_6C9C60(need)
            // 降级点：sub_6C9C60 是"解码缓冲是否够大"的判定，本项目无
            // 解码器缓冲，故恒判失败（bl 保持 0）。
        }

        // asm: 锁面 -> 拷贝 -> 解锁（整段在 Blit_Frame 里）
        Blit_Frame(TargetSurface, swapped != 0 ? (Flags | static_cast<int32>(0x80000000u)) : Flags, Y, X);

        // asm: sub_433270(this) —— 动画/计时推进
        // 降级点：sub_433270 操作解码器内部帧表，本项目无实体。

        if (Decoder != nullptr) {
            Field30 = *reinterpret_cast<const int32*>(
                static_cast<uint8*>(Decoder) + 0x0C);  // asm: mov [esi+30h],[eax+0Ch]
        }
        Decoder_Next_Frame();                          // asm: BinkNextFrame([this+4])

        if (Decoder_Wait() != 0)                       // asm: while (BinkWait()==0)
            break;
    }

    return 1;
}

// ============================================================================
// asm: sub_4331F0 (0x4331F0)
//
//   if (a_flags) sub_433330(this)                 ; 换解码缓冲
//   lock = surface->[vt+5Ch](0,0)
//   if (lock):
//       [vt+74h] -> ebp ; [vt+80h] -> eax
//       flags = [edi+8] | (a_flags ? 80000000h : 0)
//       BinkCopyToBuffer([edi+4], lock, ..., eax, ebp, edx, flags)
//       surface->[vt+60h]()
//   sub_433270(this)
// ============================================================================
void MovieHandleClass::Blit_Frame(Surface* pSurface, int32 flags, int32 y, int32 x)
{
    (void)x;
    (void)y;

    if (pSurface == nullptr)
        return;

    // asm: mov edx,[esi] / xor ecx,ecx / xor eax,eax / call [edx+5Ch]
    void* lock = pSurface->Lock(0, 0);
    if (lock != nullptr) {
        int32 copyFlags = Flags;                       // asm: mov ecx,[edi+8]
        if ((flags & 1) != 0)                          // asm: test dl,dl / or 80000000h
            copyFlags |= static_cast<int32>(0x80000000u);

        // 降级点：BinkCopyToBuffer 是 Bink SDK 的静态导入，本项目没有
        // 解码器，也就没有源缓冲可拷。锁/解锁与 flags 组织按 asm 保留。
        (void)lock;
        (void)copyFlags;

        pSurface->Unlock();                            // asm: surface->[vt+60h]()
    }

    // asm: sub_433270(this)
}
