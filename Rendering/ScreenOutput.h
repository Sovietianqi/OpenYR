#pragma once

// ============================================================================
// ScreenOutput — 屏幕呈现后端（宿主显示 / 调试导出）
//
// 原版的呈现路径是 GScreenClass::Do_Blit (asm 0x4F47xx) 里对 DirectDraw
// 主面(Primary)做 Blt/BltFast 后 Flip；本项目在 GScreenClass::BlitToScreen()
// 完成 Flip（BackBuffer -> FullscreenSurface）之后，把这张 16bpp XRGB565
// 的 FullscreenSurface 交给本模块，由宿主后端真正"显示"出来：
//
//   1. BMP 导出后端（零依赖，默认可用）
//        OPENYR_DUMP=<目录>        启用，目录不存在会自动创建
//        OPENYR_DUMP_EVERY=<N>     每 N 帧导一张，默认 30
//        OPENYR_DUMP_LIMIT=<N>     最多导 N 张，默认 60
//        文件名 frame_%06u.bmp，标准 24bpp BGR BMP（自底向上、4 字节行对齐）
//        注意：这是**新增的调试后端**，不是原版行为，不能当作 gamemd 的
//        呈现语义来引用。
//
//   2. SDL2 窗口后端（编译期开关 OPENYR_HAVE_SDL2）
//        懒初始化 SDL_Init(SDL_INIT_VIDEO) + 640x400 窗口 + RGB565 流纹理，
//        每帧 UpdateTexture/RenderCopy/RenderPresent 并处理 SDL_QUIT。
//        没有定义该宏时整段代码被编译掉（沙箱内无 SDL2 开发包）。
// ============================================================================

#include "Core/Definitions.h"

class Surface;

// SDL 后端收到 SDL_QUIT 时置位；消费方（MainLoop 等）自行决定何时退出。
// 本模块只置位，不读取、不退出进程。
extern bool g_QuitRequested;

namespace ScreenOutput
{
    // 每帧调用一次；内部按已启用的后端分发。pSource 允许为空。
    void Present(Surface* pSource);

    // 释放后端资源（SDL 窗口/纹理、关闭 BMP 导出）。可重复调用。
    void Shutdown();

    // 已经过 Present() 的帧数。
    int32 FramesPresented();

    // 已经成功落盘的 BMP 文件数（用于 [PRESENT] 调试行）。
    int32 FramesDumped();
}
