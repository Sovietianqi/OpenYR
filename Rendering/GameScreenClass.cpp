#include "Rendering/GameScreenClass.h"

#include "Rendering/DisplayClass.h"

#include <cstdio>

// ============================================================================
// GameScreenClass
// ============================================================================

GameScreenClass::GameScreenClass()
{
    // 基类构造已把 Instance 指向自己（GScreenClass::GScreenClass）。
}

GameScreenClass::~GameScreenClass()
{
}

// ----------------------------------------------------------------------------
// 光标四槽：原版在 MapClass 一侧实现，这里是等价的最小实现——
//   记住请求的指针形状与"是否小地图区"，不回写系统光标。鼠标子系统
//   （MouseClass / WWMouseClass）负责真正的光标位图与热点。
// ----------------------------------------------------------------------------
bool GameScreenClass::SetCursor(MouseCursorType idxCursor, bool miniMap)
{
    if (idxCursor < MouseCursorType::None || idxCursor >= MouseCursorType::Count)
        return false;

    CurrentCursor    = idxCursor;
    CursorIsMiniMap  = miniMap;
    return true;
}

bool GameScreenClass::UpdateCursor(MouseCursorType idxCursor, bool miniMap)
{
    // asm: 只有"与当前不同"才真正切换；相同视为成功（幂等）。
    if (idxCursor == CurrentCursor && miniMap == CursorIsMiniMap)
        return true;

    return SetCursor(idxCursor, miniMap);
}

bool GameScreenClass::RestoreCursor()
{
    // asm: 恢复到默认箭头。
    CurrentCursor   = MouseCursorType::Normal;
    CursorIsMiniMap = false;
    return true;
}

void GameScreenClass::UpdateCursorMinimapState(bool miniMap)
{
    CursorIsMiniMap = miniMap;
}
