#pragma once

#include "Core/Definitions.h"
#include "Rendering/GScreenClass.h"

// ============================================================================
// GameScreenClass - GScreenClass 的具体实例
//
// 原版里 GScreenClass 没有独立的实例：屏幕对象就是 UI 链上的那个对象
//   （MapClass 一侧把 SetCursor / UpdateCursor / RestoreCursor /
//   UpdateCursorMinimapState 四个虚函数填上，vtable 因此不是抽象的）。
//   本树的 UI 链是 `DisplayClass : MapClass : AbstractClass`，与 GScreenClass
//   是两条并列的线，GScreenClass 因此仍是抽象类（那四个槽是纯虚）。
//
//   这里给出最小具体子类承载"游戏屏幕"这一角色：分辨率、后备缓冲、
//   全屏面、翻转都在基类里，本类只补四个光标槽。它们不改系统光标——
//   真实光标由 MouseClass / WWMouseClass 负责，本类只记住请求，
//   供需要查询当前光标状态的 UI 代码使用。
//
//   等 UI 链合并进 GScreenClass（原版形态）后，本类应当被删除。
// ============================================================================
class GameScreenClass : public GScreenClass
{
public:
    GameScreenClass();
    virtual ~GameScreenClass();

    // GScreenClass 的四个光标槽（原版由 MapClass 一侧实现）
    virtual bool SetCursor(MouseCursorType idxCursor, bool miniMap) override;
    virtual bool UpdateCursor(MouseCursorType idxCursor, bool miniMap) override;
    virtual bool RestoreCursor() override;
    virtual void UpdateCursorMinimapState(bool miniMap) override;

    MouseCursorType CurrentCursor = MouseCursorType::None;
    bool            CursorIsMiniMap = false;
};
