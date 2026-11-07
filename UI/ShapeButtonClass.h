#pragma once

#include "../Core/Definitions.h"
#include "../Core/Macros.h"
#include "../Core/Memory.h"

class GScreenClass;

// ----------------------------------------------------------------------------
// asm 布局（ShapeButtonClass，基类链 ToggleClass -> ControlClass -> GadgetClass）
//   +0x0C Left           +0x10 Top
//   +0x14 Width          +0x18 Height
//   +0x1C 需重绘          +0x1E Enabled 取反（1 = 禁用）
//   +0x24 提示串槽        +0x2C 悬停     +0x2D 按下/高亮
//   +0x34 闪烁相位        +0x38 闪烁总时长  +0x3C 闪烁剩余
//   +0x40 选中            +0x44/+0x48 绘制偏移
//   +0x4C 用侧栏面        +0x50 调色板
//   +0x55 阴影位（-> 0x800 标志）
//   +0x58 SHP 指针        +0x5C 拥有权
//
// 绘制后端（CC_Draw_Shape）未建模，故通过一个可注入的 blit 回调落点，
// 帧号选择与状态机完全按 asm 复刻。
// ----------------------------------------------------------------------------
class ShapeButtonClass {
public:
    ShapeButtonClass(int32 id, GScreenClass* pScreen, int32 shapeFrame);
    ~ShapeButtonClass();

    // asm: ShapeButtonClass::Draw_Me (asm:427)
    bool Draw_Me(bool forced);
    // asm: ShapeButtonClass_Enabled (asm:55) —— 返回 Enabled 取反
    bool Enabled() const;
    // asm: ShapeButtonClass_SetDimensions (asm:82) —— 只写宽/高
    void SetDimensions(int32 width, int32 height);
    // asm: Gadget_SetTopLeft (asm:71)
    void SetTopLeft(int32 left, int32 top);
    void SetDrawOffset(int32 dx, int32 dy);          // +0x44 / +0x48
    // asm: ShapeButtonClass_SetTooltip (asm:772)
    void SetTooltip(const char* pText);
    // asm: ShapeButtonClass_Erase (asm:384)
    void Erase();
    // asm: sub_69DEA0 —— 写 +0x5C
    void SetOwnsShape(bool owns);
    // asm: sub_69DEA0 的 SHP 装载侧
    void SetShape(void* pShape, bool owns);

    // asm: ShapeButtonClass_F2 (asm:571) —— 启动一次闪烁
    bool Flash(int32 duration, int32 phase, int32 flag);
    // asm: ShapeButtonClass_F1 (asm:615) —— 取消闪烁
    bool StopFlash();
    // asm: ShapeButtonClass_69E010 (asm:651) —— 每帧推进闪烁
    bool AI();
    // asm: ShapeButtonClass_69E050 (asm:710)
    bool IsFlashing() const;
    // asm: ShapeButtonClass_69E060 (asm:741)
    bool GetFlashFlag() const;

    // asm: Gadget_40 / Gadget_3C —— 写 +0x1E 并请求重绘
    void SetDisabled(bool disabled);
    void SetSelected(bool on);       // +0x40
    void SetHovered(bool on);        // +0x2C
    void SetPressed(bool on);        // +0x2D

    // 帧号选择（asm: Draw_Me 的 ebx 分支）
    int32 SelectFrame() const;

    // ------------------------------------------------------------------
    // 绘制后端落点。CC_Draw_Shape(Surface, SHP, frame, pos, &rect,
    // Convert, flags, ...) 在本工程未建模，这里以回调固化同样的入参。
    // ------------------------------------------------------------------
    typedef bool (*ShapeBlitProc)(void* pShape, int32 frame, int32 x, int32 y,
                                  int32 width, int32 height, uint32 flags);
    static void Set_Shape_Blit(ShapeBlitProc pProc);

    // ------------------------------------------------------------------
    // 提示条后端落点（asm sub_724730 设标题 / sub_724580 设区域）。
    // ------------------------------------------------------------------
    class TooltipSink {
    public:
        virtual ~TooltipSink() {}
        virtual void Set_Tooltip(const int32* pRect, const char* pText) = 0;
    };
    static void Set_Tooltip_Sink(TooltipSink* pSink);

    int32 ID;
    int32 X;                     // +0x0C Left
    int32 Y;                     // +0x10 Top
    int32 Width;                 // +0x14
    int32 Height;                // +0x18
    int32 ShapeFrame;
    const char* TooltipText;     // +0x24 的语义替身
    bool IsHovered;              // +0x2C

    bool  Disabled;              // +0x1E
    bool  Pressed;               // +0x2D
    bool  Selected;              // +0x40
    bool  UseSidebarSurface;     // +0x4C
    bool  Shadow;                // +0x55
    int32 OffsetX;               // +0x44
    int32 OffsetY;               // +0x48
    int32 FlashPhase;            // +0x34
    int32 FlashDuration;         // +0x38
    int32 FlashRemaining;        // +0x3C
    int32 LastFrame;             // 最近一次选中的帧号
    bool  ShapeDrawn;            // asm: ShapeDrawn
    bool  NeedsRedraw;           // +0x1C
    void* Shape;                 // +0x58
    bool  OwnsShape;             // +0x5C
};
