// ============================================================================
// ScreenOutput.cpp — 屏幕呈现后端实现
//
// 背景（asm 权威）：
//   gamemd.exe 里真正把画面送上显示器的是 GScreenClass::Do_Blit (0x4F47xx)，
//   它用 DirectDraw 接口（vtable [edx+7Ch]/[eax+80h] 取宽高、Blt/BltFast 送
//   主面）把后台面拷到 Primary，再等 Flip。本项目把这一步拆成
//   GScreenClass::Flip()（BackBuffer -> FullscreenSurface 的 CPU 拷贝，语义
//   等价）+ 本模块 Present()（宿主后端）。
//
//   Present() 的两个后端都是**宿主侧新增**的：
//     * BMP 导出：纯调试用，方便无显示器沙箱里检查画面是否真的画出来了；
//     * SDL2 窗口：编译期可选，用于桌面机上直接看窗口。
//   两者都不改变原版 DirectDraw 路径的任何语义。
// ============================================================================

#include "Rendering/ScreenOutput.h"
#include "Rendering/Surface.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#if defined(OPENYR_HAVE_SDL2)
#include <SDL2/SDL.h>
#endif

// 定义在全局作用域（头文件里已 extern 声明），供 MainLoop 消费。
bool g_QuitRequested = false;

namespace ScreenOutput
{
namespace
{

// ----------------------------------------------------------------------------
// 全局状态
// ----------------------------------------------------------------------------
bool        s_Initialized = false;   // 环境变量 / 后端是否已解析初始化
bool        s_Shutdown    = false;   // 已 Shutdown，Present 直接返回
bool        s_DumpEnabled = false;   // BMP 导出是否启用
bool        s_DumpFailed  = false;   // 导出失败后永久关闭（避免每帧刷屏）
std::string s_DumpDir;
int32       s_DumpEvery   = 30;      // 每 N 帧导一张
int32       s_DumpLimit   = 60;      // 最多导 N 张
int32       s_DumpedFiles = 0;       // 已落盘张数
int32       s_Frames      = 0;       // 已 Present 帧数

// ----------------------------------------------------------------------------
// 小工具
// ----------------------------------------------------------------------------

// 源面的每像素字节数。
// 注意：Surface::GetBytesPerPixel() 在本工程里没有被 XSurface/DSurface 覆写，
// 恒返回基类默认值 1（见 Rendering/TileArt.h 的说明），因此不能直接用它，
// 只能由 Pitch / Width 反推：DSurface::Allocate 保证 Pitch = Width * bpp。
int32 SurfaceBytesPerPixel(const Surface* pSurface)
{
    if (pSurface == nullptr || pSurface->Width <= 0) return 2;
    if (pSurface->Pitch < pSurface->Width) return 2;

    int32 bpp = pSurface->Pitch / pSurface->Width;
    if (bpp != 2 && bpp != 4) bpp = 2;   // 主面无条件 16bpp(565)；异常时按 565 处理
    return bpp;
}

int32 ReadEnvInt(const char* pName, int32 fallback)
{
    const char* raw = std::getenv(pName);
    if (raw == nullptr || *raw == '\0') return fallback;

    char* end = nullptr;
    long value = std::strtol(raw, &end, 10);
    if (end == raw) return fallback;
    if (value < 0) return fallback;
    if (value > 1000000L) return 1000000;
    return static_cast<int32>(value);
}

// 单层建目录；已存在视为成功（其余错误：权限 / 同名普通文件等，交给 fopen 报错）。
bool CreateOneDir(const std::string& path)
{
    if (path.empty()) return true;

    errno = 0;
#ifdef _WIN32
    if (_mkdir(path.c_str()) == 0) return true;
#else
    if (mkdir(path.c_str(), 0755) == 0) return true;
#endif
    return errno == EEXIST;
}

// 逐级创建目录（OPENYR_DUMP 允许传多级路径）。
bool EnsureDirectory(const std::string& path)
{
    if (path.empty()) return false;

    std::string current;
    size_t i = 0;
#ifdef _WIN32
    if (path.size() >= 2 && path[1] == ':')
    {
        current = path.substr(0, 2);
        i = 2;
    }
#endif
    for (; i < path.size(); ++i)
    {
        const char c = path[i];
        if (c == '/' || c == '\\')
        {
            if (!current.empty() && current.back() != ':' &&
                current.back() != '/' && current.back() != '\\')
            {
                if (!CreateOneDir(current)) return false;
            }
            current.push_back(c);
        }
        else
        {
            current.push_back(c);
        }
    }

    if (!current.empty() && current.back() != '/' && current.back() != '\\')
    {
        if (!CreateOneDir(current)) return false;
    }
    return true;
}

std::string BuildFramePath(int32 frame)
{
    char name[64];
    std::snprintf(name, sizeof(name), "frame_%06u.bmp", static_cast<unsigned>(frame));

    std::string path = s_DumpDir;
    if (!path.empty() && path.back() != '/' && path.back() != '\\')
        path.push_back('/');
    path += name;
    return path;
}

// ----------------------------------------------------------------------------
// CRC32（帧 1 的像素校验，证明 Present 拿到的确实是非空缓冲）
// ----------------------------------------------------------------------------
inline uint32 Crc32Update(uint32 crc, BYTE value)
{
    crc ^= static_cast<uint32>(value);
    for (int32 bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    return crc;
}

void TraceFirstFrame(const Surface* pSource, int32 bpp, int32 pitch)
{
    const BYTE* base = static_cast<const BYTE*>(pSource->Buffer);
    if (base == nullptr) return;

    uint32 crc = 0xFFFFFFFFu;
    int32 nonBlack = 0;

    for (int32 y = 0; y < pSource->Height; ++y)
    {
        const BYTE* row = base + static_cast<size_t>(y) * static_cast<size_t>(pitch);
        const int32 rowBytes = pSource->Width * bpp;
        for (int32 i = 0; i < rowBytes; ++i)
            crc = Crc32Update(crc, row[i]);

        if (bpp == 2)
        {
            const uint16* pixels = reinterpret_cast<const uint16*>(row);
            for (int32 x = 0; x < pSource->Width; ++x)
                if (pixels[x] != 0) ++nonBlack;
        }
        else if (bpp == 4)
        {
            const uint32* pixels = reinterpret_cast<const uint32*>(row);
            for (int32 x = 0; x < pSource->Width; ++x)
                if ((pixels[x] & 0x00FFFFFFu) != 0) ++nonBlack;
        }
        else
        {
            for (int32 x = 0; x < pSource->Width; ++x)
                if (row[x] != 0) ++nonBlack;
        }
    }

    crc ^= 0xFFFFFFFFu;
    std::fprintf(stderr,
                 "[PRESENT] crc=%08lX nonblack=%d size=%dx%d bpp=%d\n",
                 static_cast<unsigned long>(crc), nonBlack,
                 pSource->Width, pSource->Height, bpp);
}

// ----------------------------------------------------------------------------
// BMP 写出
// ----------------------------------------------------------------------------

inline void PutLE16(BYTE* p, uint32 value)
{
    p[0] = static_cast<BYTE>(value & 0xFF);
    p[1] = static_cast<BYTE>((value >> 8) & 0xFF);
}

inline void PutLE32(BYTE* p, uint32 value)
{
    p[0] = static_cast<BYTE>(value & 0xFF);
    p[1] = static_cast<BYTE>((value >> 8) & 0xFF);
    p[2] = static_cast<BYTE>((value >> 16) & 0xFF);
    p[3] = static_cast<BYTE>((value >> 24) & 0xFF);
}

// 写标准 24bpp BMP：
//   BITMAPFILEHEADER 14 字节（'BM'、文件大小、保留、像素数据偏移 54）
//   BITMAPINFOHEADER 40 字节（宽高、1 平面、24 位、BI_RGB、正向高 = 自底向上）
//   像素行：BMP 是 BGR 顺序且行倒序（最后一行先写），每行 4 字节对齐补 0。
bool WriteBMP(const std::string& path, const Surface* pSource, int32 bpp, int32 pitch)
{
    const int32 width = pSource->Width;
    const int32 height = pSource->Height;
    if (width <= 0 || height <= 0) return false;

    const int32 rowSize = (width * 3 + 3) & ~3;              // 4 字节对齐
    const uint32 dataSize = static_cast<uint32>(rowSize) * static_cast<uint32>(height);
    const uint32 fileSize = 14u + 40u + dataSize;

    BYTE header[54];
    std::memset(header, 0, sizeof(header));

    // BITMAPFILEHEADER
    header[0] = 'B';
    header[1] = 'M';
    PutLE32(&header[2], fileSize);       // bfSize
    // bfReserved1/bfReserved2 = 0
    PutLE32(&header[10], 14u + 40u);     // bfOffBits = 54

    // BITMAPINFOHEADER
    PutLE32(&header[14], 40);                                  // biSize
    PutLE32(&header[18], static_cast<uint32>(width));          // biWidth
    PutLE32(&header[22], static_cast<uint32>(height));         // biHeight（正 = 自底向上）
    PutLE16(&header[26], 1);                                   // biPlanes
    PutLE16(&header[28], 24);                                  // biBitCount
    PutLE32(&header[30], 0);                                   // biCompression = BI_RGB
    PutLE32(&header[34], dataSize);                            // biSizeImage
    PutLE32(&header[38], 2835);                                // biXPelsPerMeter (72dpi)
    PutLE32(&header[42], 2835);                                // biYPelsPerMeter
    PutLE32(&header[46], 0);                                   // biClrUsed
    PutLE32(&header[50], 0);                                   // biClrImportant

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (fp == nullptr) return false;

    bool ok = (std::fwrite(header, 1, sizeof(header), fp) == sizeof(header));

    // 行缓冲：补位字节始终为 0（只覆盖前 width*3 字节）。
    std::vector<BYTE> line(static_cast<size_t>(rowSize), 0);

    for (int32 y = height - 1; ok && y >= 0; --y)
    {
        const BYTE* srcRow = static_cast<const BYTE*>(pSource->Buffer)
                           + static_cast<size_t>(y) * static_cast<size_t>(pitch);

        for (int32 x = 0; x < width; ++x)
        {
            BYTE r = 0;
            BYTE g = 0;
            BYTE b = 0;

            if (bpp == 2)
            {
                // XRGB565：rrrrrggg gggbbbbb -> 8/8/8（低位复制高位，满量程展开）
                const uint16 value = reinterpret_cast<const uint16*>(srcRow)[x];
                const uint32 r5 = (value >> 11) & 0x1F;
                const uint32 g6 = (value >> 5) & 0x3F;
                const uint32 b5 = value & 0x1F;
                r = static_cast<BYTE>((r5 << 3) | (r5 >> 2));
                g = static_cast<BYTE>((g6 << 2) | (g6 >> 4));
                b = static_cast<BYTE>((b5 << 3) | (b5 >> 2));
            }
            else if (bpp == 4)
            {
                // 32bpp：本工程按 XRGB8888 存放（DoBlit 的转换分支同此约定）
                const uint32 value = reinterpret_cast<const uint32*>(srcRow)[x];
                r = static_cast<BYTE>((value >> 16) & 0xFF);
                g = static_cast<BYTE>((value >> 8) & 0xFF);
                b = static_cast<BYTE>(value & 0xFF);
            }
            else
            {
                // 8bpp（调色板索引）无法还原颜色，退化为灰度，仅用于确认几何形状
                const BYTE value = srcRow[x];
                r = g = b = value;
            }

            line[x * 3 + 0] = b;   // BMP 像素顺序是 BGR
            line[x * 3 + 1] = g;
            line[x * 3 + 2] = r;
        }

        if (std::fwrite(line.data(), 1, static_cast<size_t>(rowSize), fp)
            != static_cast<size_t>(rowSize))
        {
            ok = false;
        }
    }

    if (std::fclose(fp) != 0) ok = false;
    return ok;
}

// ----------------------------------------------------------------------------
// SDL2 窗口后端（OPENYR_HAVE_SDL2 未定义时全部编译掉）
// ----------------------------------------------------------------------------
#if defined(OPENYR_HAVE_SDL2)

SDL_Window*   s_SdlWindow   = nullptr;
SDL_Renderer* s_SdlRenderer = nullptr;
SDL_Texture*  s_SdlTexture  = nullptr;
int32         s_SdlTexW     = 0;
int32         s_SdlTexH     = 0;
bool          s_SdlFailed   = false;

void InitSDL()
{
    if (s_SdlWindow != nullptr || s_SdlFailed) return;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::fprintf(stderr, "[PRESENT] SDL_Init failed: %s\n", SDL_GetError());
        s_SdlFailed = true;
        return;
    }

    s_SdlWindow = SDL_CreateWindow("OpenYR",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   640, 400, 0);
    if (s_SdlWindow == nullptr)
    {
        std::fprintf(stderr, "[PRESENT] SDL_CreateWindow failed: %s\n", SDL_GetError());
        s_SdlFailed = true;
        return;
    }

    s_SdlRenderer = SDL_CreateRenderer(s_SdlWindow, -1,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (s_SdlRenderer == nullptr)
        s_SdlRenderer = SDL_CreateRenderer(s_SdlWindow, -1, 0);

    if (s_SdlRenderer == nullptr)
    {
        std::fprintf(stderr, "[PRESENT] SDL_CreateRenderer failed: %s\n", SDL_GetError());
        s_SdlFailed = true;
    }
}

void PresentSDL(const Surface* pSource)
{
    InitSDL();
    if (s_SdlFailed || s_SdlWindow == nullptr || s_SdlRenderer == nullptr) return;

    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
            g_QuitRequested = true;
    }
    if (g_QuitRequested) return;

    const int32 width = pSource->Width;
    const int32 height = pSource->Height;
    if (width <= 0 || height <= 0) return;

    // 纹理只接受 565；bpp 异常时本帧跳过（不影响 BMP 后端）。
    if (SurfaceBytesPerPixel(pSource) != 2) return;

    if (s_SdlTexture == nullptr || s_SdlTexW != width || s_SdlTexH != height)
    {
        if (s_SdlTexture != nullptr) SDL_DestroyTexture(s_SdlTexture);
        s_SdlTexture = SDL_CreateTexture(s_SdlRenderer, SDL_PIXELFORMAT_RGB565,
                                         SDL_TEXTUREACCESS_STREAMING, width, height);
        s_SdlTexW = width;
        s_SdlTexH = height;
        if (s_SdlTexture == nullptr)
        {
            std::fprintf(stderr, "[PRESENT] SDL_CreateTexture failed: %s\n", SDL_GetError());
            s_SdlFailed = true;
            return;
        }
    }

    SDL_UpdateTexture(s_SdlTexture, nullptr, pSource->Buffer, pSource->Pitch);
    SDL_RenderClear(s_SdlRenderer);
    SDL_RenderCopy(s_SdlRenderer, s_SdlTexture, nullptr, nullptr);
    SDL_RenderPresent(s_SdlRenderer);
}

void ShutdownSDL()
{
    if (s_SdlTexture != nullptr)
    {
        SDL_DestroyTexture(s_SdlTexture);
        s_SdlTexture = nullptr;
    }
    if (s_SdlRenderer != nullptr)
    {
        SDL_DestroyRenderer(s_SdlRenderer);
        s_SdlRenderer = nullptr;
    }
    if (s_SdlWindow != nullptr)
    {
        SDL_DestroyWindow(s_SdlWindow);
        s_SdlWindow = nullptr;
    }
    s_SdlTexW = 0;
    s_SdlTexH = 0;
    s_SdlFailed = false;
    if (SDL_WasInit(SDL_INIT_VIDEO) != 0)
        SDL_Quit();
}

#else  // !OPENYR_HAVE_SDL2

// 无 SDL2 开发包时保持同名空实现，宿主后端只剩 BMP 导出。
void InitSDL() {}
void PresentSDL(const Surface*) {}
void ShutdownSDL() {}

#endif // OPENYR_HAVE_SDL2

// ----------------------------------------------------------------------------
// 初始化：解析环境变量 + 建立导出目录 + 懒初始化 SDL
// ----------------------------------------------------------------------------
void InitBackends()
{
    if (s_Initialized) return;
    s_Initialized = true;
    s_Shutdown = false;

    const char* dumpDir = std::getenv("OPENYR_DUMP");
    if (dumpDir != nullptr && *dumpDir != '\0')
    {
        s_DumpDir = dumpDir;

        s_DumpEvery = ReadEnvInt("OPENYR_DUMP_EVERY", 30);
        if (s_DumpEvery <= 0) s_DumpEvery = 30;

        s_DumpLimit = ReadEnvInt("OPENYR_DUMP_LIMIT", 60);
        if (s_DumpLimit < 0) s_DumpLimit = 60;

        if (!EnsureDirectory(s_DumpDir))
        {
            std::fprintf(stderr,
                         "[PRESENT] cannot create dump directory '%s' (errno=%d); BMP export disabled\n",
                         s_DumpDir.c_str(), errno);
            s_DumpFailed = true;
        }
        else if (s_DumpLimit == 0)
        {
            s_DumpFailed = true;   // limit=0：明确不要导出
        }
        else
        {
            s_DumpEnabled = true;
            std::fprintf(stderr,
                         "[PRESENT] BMP dump enabled: dir='%s' every=%d limit=%d\n",
                         s_DumpDir.c_str(), s_DumpEvery, s_DumpLimit);
        }
    }

    InitSDL();
}

} // anonymous namespace

// ============================================================================
// 公共接口
// ============================================================================

void Present(Surface* pSource)
{
    if (s_Shutdown) return;
    if (pSource == nullptr || pSource->Buffer == nullptr) return;
    if (pSource->Width <= 0 || pSource->Height <= 0) return;

    InitBackends();

    ++s_Frames;

    const int32 bpp = SurfaceBytesPerPixel(pSource);
    const int32 pitch = (pSource->Pitch > 0) ? pSource->Pitch : pSource->Width * bpp;

    // 帧 1 打一次像素指纹，证明读到的不是空缓冲。
    if (s_Frames == 1)
        TraceFirstFrame(pSource, bpp, pitch);

    // 宿主窗口（宏未开时为空实现）。
    PresentSDL(pSource);

    // BMP 调试导出。
    if (s_DumpEnabled && !s_DumpFailed && s_DumpLimit > 0 &&
        (s_Frames % s_DumpEvery) == 0 && s_DumpedFiles < s_DumpLimit)
    {
        const std::string path = BuildFramePath(s_Frames);
        if (WriteBMP(path, pSource, bpp, pitch))
        {
            ++s_DumpedFiles;
        }
        else
        {
            // 只报一次，随后彻底关闭导出，避免每帧刷屏。
            std::fprintf(stderr,
                         "[PRESENT] BMP dump failed at '%s' (errno=%d); export disabled\n",
                         path.c_str(), errno);
            s_DumpFailed = true;
            s_DumpEnabled = false;
        }
    }
}

void Shutdown()
{
    if (s_Shutdown) return;

    ShutdownSDL();

    s_DumpEnabled = false;
    s_Shutdown = true;
    s_Initialized = false;   // 允许新的 GScreenClass 重新解析环境变量
}

int32 FramesPresented()
{
    return s_Frames;
}

int32 FramesDumped()
{
    return s_DumpedFiles;
}

} // namespace ScreenOutput
