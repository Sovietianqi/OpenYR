#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Math/Rectangle.h"

class DSurface;

// ============================================================================
// DSurface - drawing surface
//
//  根据游戏行为，可知绘制面是位图.blit 与文字排版的统一入口：主面
//  由 CreatePrimary 建出，文本按游戏字体逐字形画出。
// ============================================================================

class DSurface {
public:
    DSurface() noexcept;
    virtual ~DSurface();

    // 主面与重建
    static DSurface* CreatePrimary(int32 width, int32 height);
    bool Reinit(int32 width, int32 height);

    // 位图搬运
    bool Blit(DSurface* pDest, const Point2D& destPos, const Rectangle& srcRect);

    // 填充
    bool FillRect(const Rectangle& rect, uint16 color);

    // 文本输出
    bool PrintText(const wchar_t* pText, const Point2D& pos, uint16 color);
    bool PrintUnicode(const wchar_t* pText, const Point2D& pos, uint16 color);

    // 游戏字体
    static void* GetGameFont();

    int32 Width;
    int32 Height;
    void* Buffer;
};
