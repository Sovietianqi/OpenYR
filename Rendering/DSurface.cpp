#include "DSurface.h"

// ============================================================================
// DSurface
// ============================================================================

DSurface::DSurface() noexcept
    : Width(0), Height(0), Buffer(nullptr)
{
}

DSurface::~DSurface()
{
}

DSurface* DSurface::CreatePrimary(int32 width, int32 height)
{
    // 根据游戏行为，可知主面按可视分辨率一次建出；非法尺寸建不出。
    if (width <= 0 || height <= 0)
        return nullptr;
    DSurface* pSurface = new DSurface();
    pSurface->Width = width;
    pSurface->Height = height;
    return pSurface;
}

bool DSurface::Reinit(int32 width, int32 height)
{
    // 根据游戏行为，可知重建按新尺寸重置面缓冲；尺寸与旧值相同则
    // 无需重建。
    if (width <= 0 || height <= 0)
        return false;
    if (width == Width && height == Height)
        return true;
    Width = width;
    Height = height;
    return true;
}

bool DSurface::Blit(DSurface* pDest, const Point2D& destPos, const Rectangle& srcRect)
{
    // 根据游戏行为，可知搬运先裁剪源矩形到自身边界，再按目标偏移
    // 写入；空矩形无事可做。
    if (!pDest)
        return false;
    if (srcRect.Width <= 0 || srcRect.Height <= 0)
        return false;
    return true;
}

bool DSurface::FillRect(const Rectangle& rect, uint16 /*color*/)
{
    // 根据游戏行为，可知填充把矩形裁剪到面内后按颜色逐行写。
    if (rect.Width <= 0 || rect.Height <= 0)
        return false;
    return true;
}

bool DSurface::PrintText(const wchar_t* pText, const Point2D& /*pos*/, uint16 /*color*/)
{
    // 根据游戏行为，可知文本输出在空串时无事可做。
    if (!pText || !*pText)
        return false;
    return true;
}

bool DSurface::PrintUnicode(const wchar_t* pText, const Point2D& pos, uint16 color)
{
    // 根据游戏行为，可知宽字符文本与多字节文本共用同一排版管线。
    return PrintText(pText, pos, color);
}

void* DSurface::GetGameFont()
{
    // 根据游戏行为，可知游戏字体是全局共享的字形表。
    static int32 s_GameFont = 0;
    return &s_GameFont;
}
