#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/SidebarClass.h"
#include "Rendering/Surface.h"
#include "Abstract/ObjectClass.h"
#include "Abstract/TechnoClass.h"
#include "Math/Timer.h"

// Forward declarations
class SHPStruct;
class DSurface;

// ============================================================================
// MouseCursor - Mouse cursor animation definition
// ============================================================================
class MouseCursor
{
public:
    static constexpr int32 MaxCursors = 86;
    static MouseCursor* GetCursor(MouseCursorType cursor);

    MouseCursor()
        : Frame(0), Count(1), Interval(1)
        , MiniFrame(-1), MiniCount(0)
        , HotX(MouseHotSpotX::Center), HotY(MouseHotSpotY::Middle)
    {}

    MouseCursor(
        int32 frame, int32 count, int32 interval,
        int32 miniFrame, int32 miniCount,
        MouseHotSpotX hotX, MouseHotSpotY hotY)
        : Frame(frame), Count(count), Interval(interval)
        , MiniFrame(miniFrame), MiniCount(miniCount)
        , HotX(hotX), HotY(hotY)
    {}

    int32 Frame;
    int32 Count;
    int32 Interval;
    int32 MiniFrame;
    int32 MiniCount;
    MouseHotSpotX HotX;
    MouseHotSpotY HotY;
};

// ============================================================================
// TabDataClass - Tab data for credits display
// ============================================================================
struct TabDataClass
{
    int32 TargetValue;
    int32 LastValue;
    bool NeedsRedraw;
    bool ValueIncreased;
    bool ValueChanged;
    BYTE align_B;
    int32 ValueDelta;

    TabDataClass()
        : TargetValue(0), LastValue(0), NeedsRedraw(false)
        , ValueIncreased(false), ValueChanged(false)
        , align_B(0), ValueDelta(0)
    {}
};

// INoticeSink is defined in COM/IUnknown.h

// ============================================================================
// TabClass - Tab notification (credits display)
// ============================================================================
class TabClass : public SidebarClass, public INoticeSink
{
public:
    static TabClass* Instance;

    TabClass();
    virtual ~TabClass();

    void Activate(int32 control = 1);

    TabDataClass TabData;
    CDTimerClass unknown_timer_552C;
    CDTimerClass InsufficientFundsBlinkTimer;
    BYTE unknown_byte_5544;
    bool MissionTimerPinged;
    BYTE unknown_byte_5546;
    BYTE padding_5547;
};

// ============================================================================
// ScrollClass - Scrolling notification
// ============================================================================
class ScrollClass : public TabClass
{
public:
    static ScrollClass* Instance;

    ScrollClass();
    virtual ~ScrollClass();

    DWORD unknown_int_5548;
    BYTE unknown_byte_554C;
    BYTE align_554D[3];
    DWORD unknown_int_5550;
    DWORD unknown_int_5554;
    BYTE unknown_byte_5548_2;
    BYTE unknown_byte_5549;
    BYTE unknown_byte_554A;
    BYTE padding_554B;
};

// ============================================================================
// MouseClass - Mouse/cursor management
//
// Manages the mouse cursor, including cursor type switching,
// animation, click detection, drag operations, and object selection.
// ============================================================================
class NOVTABLE MouseClass : public ScrollClass
{
public:
    static MouseClass* Instance;

    MouseClass();
    virtual ~MouseClass();

    // GScreenClass methods
    virtual bool SetCursor(MouseCursorType idxCursor, bool miniMap);
    virtual bool UpdateCursor(MouseCursorType idxCursor, bool miniMap);
    virtual bool RestoreCursor();
    virtual void UpdateCursorMinimapState(bool miniMap);

    // DisplayClass methods
    virtual MouseCursorType GetLastMouseCursor();

    // Non-virtual methods
    void Init();
    void Draw();
    void Update();
    void Mouse_Left_Press(const Point2D& point);
    void Mouse_Left_Release(const Point2D& point);
    void Mouse_Right_Press(const Point2D& point);
    void Mouse_Right_Release(const Point2D& point);
    void Mouse_Move(const Point2D& point);
    ObjectClass* Get_Object_Under_Cursor();
    bool Place_Object(ObjectTypeClass* pType, const CoordStruct& location);

    // Properties
    Point2D CursorPosition;
    MouseCursorType CursorType;
    ObjectClass* ClickedObject;
    ObjectClass* HoveredObject;
    bool IsDragging;
    Point2D DragStart;
    bool MouseCursorIsMini;
    BYTE unknown_byte_5559[3];
    MouseCursorType MouseCursorIndex;
    MouseCursorType MouseCursorLastIndex;
    int32 MouseCursorCurrentFrame;
};