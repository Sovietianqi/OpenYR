#include <UI/Slider.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Math/Rectangle.h>

// ============================================================================
// Slider.cpp - Slider implementation
//
//  Implements the SliderClass member functions.  The slider supports
//  horizontal and vertical orientations, drag-to-move and click-to-jump
//  thumb behavior, and keyboard navigation.
// ============================================================================

// ============================================================================
// Construction
// ============================================================================

SliderClass::SliderClass() noexcept
    : GadgetClass()
    , MinValue(0)
    , MaxValue(100)
    , CurrentValue(0)
    , StepSize(1)
    , SliderOrientation(SliderStyle::Horizontal)
    , ShowTicks(false)
    , TickCount(10)
    , ThumbSize(16)
    , IsDragging(false)
    , DragOffset(0)
    , ThumbColor(200, 200, 200)
    , TrackColor(60, 60, 60)
    , FillColor(100, 100, 180)
{
}

SliderClass::SliderClass(int32 x, int32 y, int32 w, int32 h,
                          int32 minVal, int32 maxVal, int32 id) noexcept
    : GadgetClass(x, y, w, h, id)
    , MinValue(minVal)
    , MaxValue(maxVal)
    , CurrentValue(minVal)
    , StepSize(1)
    , SliderOrientation(SliderStyle::Horizontal)
    , ShowTicks(false)
    , TickCount(10)
    , ThumbSize(16)
    , IsDragging(false)
    , DragOffset(0)
    , ThumbColor(200, 200, 200)
    , TrackColor(60, 60, 60)
    , FillColor(100, 100, 180)
{
}

// ============================================================================
// Destruction
// ============================================================================

SliderClass::~SliderClass()
{
}

// ============================================================================
// Range management
// ============================================================================

void SliderClass::SetRange(int32 minVal, int32 maxVal) noexcept
{
    if (minVal > maxVal)
    {
        int32 tmp = minVal;
        minVal = maxVal;
        maxVal = tmp;
    }
    MinValue = minVal;
    MaxValue = maxVal;

    // Clamp current value.
    if (CurrentValue < MinValue) CurrentValue = MinValue;
    if (CurrentValue > MaxValue) CurrentValue = MaxValue;

    SetNeedsRedraw(true);
}

// ============================================================================
// Value management
// ============================================================================

void SliderClass::SetValue(int32 value) noexcept
{
    if (value < MinValue) value = MinValue;
    if (value > MaxValue) value = MaxValue;

    if (CurrentValue == value)
        return;

    CurrentValue = value;
    SetNeedsRedraw(true);

    // Notify via callback.
    InvokeCallback(GetID());
}

float SliderClass::GetNormalized() const noexcept
{
    int32 range = MaxValue - MinValue;
    if (range <= 0)
        return 0.0f;
    return static_cast<float>(CurrentValue - MinValue) / static_cast<float>(range);
}

// ============================================================================
// Step
// ============================================================================

void SliderClass::StepUp() noexcept
{
    int32 newVal = CurrentValue + StepSize;
    if (newVal > MaxValue) newVal = MaxValue;
    SetValue(newVal);
}

void SliderClass::StepDown() noexcept
{
    int32 newVal = CurrentValue - StepSize;
    if (newVal < MinValue) newVal = MinValue;
    SetValue(newVal);
}

// ============================================================================
// Pixel <-> value conversion
// ============================================================================

int32 SliderClass::PixelToValue(int32 pixel) const noexcept
{
    int32 trackStart, trackLen;
    if (SliderOrientation == SliderStyle::Horizontal)
    {
        trackStart = ThumbSize / 2;
        trackLen = GetWidth() - ThumbSize;
    }
    else
    {
        trackStart = ThumbSize / 2;
        trackLen = GetHeight() - ThumbSize;
    }

    if (trackLen <= 0)
        return MinValue;

    int32 relPos = pixel - trackStart;
    if (relPos < 0) relPos = 0;
    if (relPos > trackLen) relPos = trackLen;

    float normalized = static_cast<float>(relPos) / static_cast<float>(trackLen);
    int32 range = MaxValue - MinValue;
    int32 value = MinValue + static_cast<int32>(normalized * range + 0.5f);

    if (value < MinValue) value = MinValue;
    if (value > MaxValue) value = MaxValue;
    return value;
}

int32 SliderClass::ValueToPixel(int32 value) const noexcept
{
    int32 trackStart, trackLen;
    if (SliderOrientation == SliderStyle::Horizontal)
    {
        trackStart = ThumbSize / 2;
        trackLen = GetWidth() - ThumbSize;
    }
    else
    {
        trackStart = ThumbSize / 2;
        trackLen = GetHeight() - ThumbSize;
    }

    if (trackLen <= 0)
        return trackStart;

    int32 range = MaxValue - MinValue;
    if (range <= 0)
        return trackStart;

    float normalized = static_cast<float>(value - MinValue) / static_cast<float>(range);
    return trackStart + static_cast<int32>(normalized * trackLen);
}

int32 SliderClass::GetThumbPosition() const noexcept
{
    return ValueToPixel(CurrentValue);
}

// ============================================================================
// Mouse-to-value update
// ============================================================================

void SliderClass::UpdateFromMouse(int32 mouseX, int32 mouseY) noexcept
{
    int32 pixel;
    if (SliderOrientation == SliderStyle::Horizontal)
        pixel = mouseX - GetX();
    else
        pixel = mouseY - GetY();

    SetValue(PixelToValue(pixel));
}

// ============================================================================
// Drawing
// ============================================================================

void SliderClass::Draw(Surface* pSurface)
{
    (void)pSurface;

    if (!IsVisible())
        return;

    // The rendering layer would:
    // 1. Draw the track (a thin rectangle in TrackColor).
    // 2. Draw the filled portion (from min to current value) in FillColor.
    // 3. If ShowTicks, draw tick marks along the track.
    // 4. Draw the thumb (a small rectangle/circle in ThumbColor) at
    //    GetThumbPosition().
    // 5. If focused, draw a focus rectangle around the slider.

    SetNeedsRedraw(false);
}

// ============================================================================
// Event handling
//
//  The slider responds to:
//    * MouseDown  -> start dragging; jump thumb to mouse position
//    * MouseMove  -> if dragging, update value from mouse position
//    * MouseUp    -> stop dragging
//    * KeyDown    -> Left/Down = step down, Right/Up = step up,
//                    Home = min, End = max
// ============================================================================

bool SliderClass::HandleEvent(const GadgetEvent& event)
{
    if (!IsEnabled())
        return false;

    int32 absMouseX = event.MousePos.X + GetX();
    int32 absMouseY = event.MousePos.Y + GetY();

    switch (event.Type)
    {
    case GadgetEventType::MouseDown:
        // Check if the click is on the thumb.
        {
            int32 thumbPos = GetThumbPosition();
            bool onThumb = false;
            if (SliderOrientation == SliderStyle::Horizontal)
            {
                onThumb = (absMouseX - GetX() >= thumbPos - ThumbSize / 2 &&
                           absMouseX - GetX() <= thumbPos + ThumbSize / 2);
            }
            else
            {
                onThumb = (absMouseY - GetY() >= thumbPos - ThumbSize / 2 &&
                           absMouseY - GetY() <= thumbPos + ThumbSize / 2);
            }

            if (onThumb)
            {
                // Start dragging; record offset from thumb center.
                IsDragging = true;
                if (SliderOrientation == SliderStyle::Horizontal)
                    DragOffset = (absMouseX - GetX()) - thumbPos;
                else
                    DragOffset = (absMouseY - GetY()) - thumbPos;
            }
            else
            {
                // Click outside thumb: jump to the click position.
                UpdateFromMouse(absMouseX, absMouseY);
                IsDragging = true;
                DragOffset = 0;
            }
            SetState(GadgetState::Pressed);
            OnPress();
            return true;
        }

    case GadgetEventType::MouseMove:
        if (IsDragging)
        {
            // Update value, accounting for the drag offset.
            int32 adjustedX = absMouseX - DragOffset;
            int32 adjustedY = absMouseY - DragOffset;
            UpdateFromMouse(adjustedX, adjustedY);
            return true;
        }
        // Update hovered state.
        {
            bool nowHovered = Contains(absMouseX, absMouseY);
            if (nowHovered != IsHovered())
            {
                if (nowHovered) SetState(GadgetState::Hovered);
                else ClearState(GadgetState::Hovered);
                SetNeedsRedraw(true);
            }
        }
        return false;

    case GadgetEventType::MouseUp:
        if (IsDragging)
        {
            IsDragging = false;
            DragOffset = 0;
            ClearState(GadgetState::Pressed);
            OnRelease();
            return true;
        }
        return false;

    case GadgetEventType::KeyDown:
        switch (event.Key)
        {
        case 37: /* VK_LEFT */
        case 40: /* VK_DOWN */
            StepDown();
            return true;
        case 39: /* VK_RIGHT */
        case 38: /* VK_UP */
            StepUp();
            return true;
        case 36: /* VK_HOME */
            SetValue(MinValue);
            return true;
        case 35: /* VK_END */
            SetValue(MaxValue);
            return true;
        default:
            break;
        }
        return false;

    default:
        break;
    }

    return false;
}

// ============================================================================
// State change callbacks
// ============================================================================

void SliderClass::OnPress()
{
    SetNeedsRedraw(true);
}

void SliderClass::OnRelease()
{
    SetNeedsRedraw(true);
}

// ============================================================================
// File-local helper functions
//
//  These provide tick-mark layout computation, value formatting, drag
//  velocity tracking for inertial scrolling, snap-to-tick logic, and
//  visual style helpers used by the slider widget.  Because the SliderClass
//  header cannot be modified, these utilities are declared as free
//  functions in the anonymous namespace and operate on the public state
//  exposed by SliderClass.
// ============================================================================

namespace
{

// --------------------------------------------------------------------------
// Slider tuning constants
// --------------------------------------------------------------------------
constexpr int32 INERTIA_DECAY          = 8;    // velocity decay per frame
constexpr int32 INERTIA_THRESHOLD      = 2;    // below this, snap to zero
constexpr int32 DEFAULT_PAGE_STEP      = 10;   // page-up/down step count
constexpr int32 TICK_LABEL_MARGIN      = 4;    // pixels between tick and label

// --------------------------------------------------------------------------
// ComputeTrackBounds - Returns the start pixel and length of the slider
// track for the given orientation and dimensions.
// --------------------------------------------------------------------------
void ComputeTrackBounds(SliderStyle orientation, int32 width, int32 height,
                        int32 thumbSize, int32& outStart, int32& outLength)
{
    if (orientation == SliderStyle::Horizontal) {
        outStart = thumbSize / 2;
        outLength = width - thumbSize;
    } else {
        outStart = thumbSize / 2;
        outLength = height - thumbSize;
    }
    if (outLength < 0) outLength = 0;
}

// --------------------------------------------------------------------------
// SnapToTick - Returns the value snapped to the nearest tick mark.
// --------------------------------------------------------------------------
int32 SnapToTick(int32 value, int32 minVal, int32 maxVal, int32 tickCount)
{
    if (tickCount <= 0) return value;
    if (maxVal <= minVal) return minVal;

    int32 range = maxVal - minVal;
    int32 step = range / tickCount;
    if (step <= 0) return value;

    int32 rel = value - minVal;
    int32 snapped = ((rel + step / 2) / step) * step;
    int32 result = minVal + snapped;
    if (result < minVal) result = minVal;
    if (result > maxVal) result = maxVal;
    return result;
}

// --------------------------------------------------------------------------
// ComputeTickPositions - Fills outPositions with the pixel positions of
// each tick mark along the track. Returns the number of ticks written.
// --------------------------------------------------------------------------
int32 ComputeTickPositions(SliderStyle orientation, int32 width, int32 height,
                           int32 thumbSize, int32 tickCount,
                           int32* outPositions, int32 maxPositions)
{
    if (!outPositions || maxPositions <= 0 || tickCount <= 0) return 0;

    int32 trackStart, trackLen;
    ComputeTrackBounds(orientation, width, height, thumbSize,
                       trackStart, trackLen);
    if (trackLen <= 0) return 0;

    int32 count = tickCount + 1; // ticks at both endpoints
    if (count > maxPositions) count = maxPositions;

    for (int32 i = 0; i < count; ++i) {
        float t = (count <= 1) ? 0.0f :
                  static_cast<float>(i) / static_cast<float>(count - 1);
        outPositions[i] = trackStart + static_cast<int32>(t * trackLen);
    }
    return count;
}

// --------------------------------------------------------------------------
// ComputeTickValues - Fills outValues with the value represented by each
// tick mark. Returns the number of values written.
// --------------------------------------------------------------------------
int32 ComputeTickValues(int32 minVal, int32 maxVal, int32 tickCount,
                        int32* outValues, int32 maxValues)
{
    if (!outValues || maxValues <= 0 || tickCount <= 0) return 0;
    if (maxVal <= minVal) {
        outValues[0] = minVal;
        return 1;
    }

    int32 count = tickCount + 1;
    if (count > maxValues) count = maxValues;
    int32 range = maxVal - minVal;

    for (int32 i = 0; i < count; ++i) {
        float t = (count <= 1) ? 0.0f :
                  static_cast<float>(i) / static_cast<float>(count - 1);
        outValues[i] = minVal + static_cast<int32>(t * range + 0.5f);
    }
    return count;
}

// --------------------------------------------------------------------------
// FormatValueToString - Formats a slider value as a decimal string.
// --------------------------------------------------------------------------
int32 FormatValueToString(int32 value, char* outBuffer, int32 bufferSize)
{
    if (!outBuffer || bufferSize <= 0) return 0;

    // Handle negative values.
    bool negative = (value < 0);
    if (negative) value = -value;

    // Build digits in reverse.
    char temp[16];
    int32 len = 0;
    if (value == 0) {
        temp[len++] = '0';
    } else {
        while (value > 0 && len < 15) {
            temp[len++] = static_cast<char>('0' + (value % 10));
            value /= 10;
        }
    }

    int32 outIdx = 0;
    if (negative && outIdx < bufferSize - 1) {
        outBuffer[outIdx++] = '-';
    }
    for (int32 i = len - 1; i >= 0 && outIdx < bufferSize - 1; --i) {
        outBuffer[outIdx++] = temp[i];
    }
    outBuffer[outIdx] = '\0';
    return outIdx;
}

// --------------------------------------------------------------------------
// FormatPercentString - Formats a normalized value as a percentage string.
// --------------------------------------------------------------------------
int32 FormatPercentString(float normalized, char* outBuffer, int32 bufferSize)
{
    if (!outBuffer || bufferSize <= 0) return 0;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;

    int32 percent = static_cast<int32>(normalized * 100.0f + 0.5f);
    if (percent > 100) percent = 100;

    // Format as "NN%".
    int32 outIdx = 0;
    if (percent >= 100 && outIdx < bufferSize - 1) {
        outBuffer[outIdx++] = '1';
        outBuffer[outIdx++] = '0';
        outBuffer[outIdx++] = '0';
    } else {
        if (percent >= 10 && outIdx < bufferSize - 1) {
            outBuffer[outIdx++] = static_cast<char>('0' + (percent / 10));
        }
        if (outIdx < bufferSize - 1) {
            outBuffer[outIdx++] = static_cast<char>('0' + (percent % 10));
        }
    }
    if (outIdx < bufferSize - 1) {
        outBuffer[outIdx++] = '%';
    }
    outBuffer[outIdx] = '\0';
    return outIdx;
}

// --------------------------------------------------------------------------
// ComputeThumbRect - Returns the bounding rectangle of the slider thumb.
// --------------------------------------------------------------------------
void ComputeThumbRect(SliderStyle orientation, int32 x, int32 y,
                      int32 width, int32 height, int32 thumbSize,
                      int32 thumbPixel, Rectangle& outRect)
{
    if (orientation == SliderStyle::Horizontal) {
        outRect.X = x + thumbPixel - thumbSize / 2;
        outRect.Y = y + (height - thumbSize) / 2;
        outRect.Width = thumbSize;
        outRect.Height = thumbSize;
    } else {
        outRect.X = x + (width - thumbSize) / 2;
        outRect.Y = y + thumbPixel - thumbSize / 2;
        outRect.Width = thumbSize;
        outRect.Height = thumbSize;
    }
}

// --------------------------------------------------------------------------
// ComputeFillRect - Returns the filled portion of the track (from min to
// the current thumb position).
// --------------------------------------------------------------------------
void ComputeFillRect(SliderStyle orientation, int32 x, int32 y,
                     int32 width, int32 height, int32 thumbSize,
                     int32 thumbPixel, Rectangle& outRect)
{
    int32 trackStart, trackLen;
    ComputeTrackBounds(orientation, width, height, thumbSize,
                       trackStart, trackLen);

    if (orientation == SliderStyle::Horizontal) {
        outRect.X = x + trackStart;
        outRect.Y = y + (height - 6) / 2;
        outRect.Width = thumbPixel - trackStart;
        outRect.Height = 6;
        if (outRect.Width < 0) outRect.Width = 0;
    } else {
        outRect.X = x + (width - 6) / 2;
        outRect.Y = y + thumbPixel;
        outRect.Width = 6;
        outRect.Height = (y + trackStart + trackLen) - (y + thumbPixel);
        if (outRect.Height < 0) outRect.Height = 0;
    }
}

// --------------------------------------------------------------------------
// ComputeTrackRect - Returns the full track rectangle.
// --------------------------------------------------------------------------
void ComputeTrackRect(SliderStyle orientation, int32 x, int32 y,
                      int32 width, int32 height, int32 thumbSize,
                      Rectangle& outRect)
{
    int32 trackStart, trackLen;
    ComputeTrackBounds(orientation, width, height, thumbSize,
                       trackStart, trackLen);

    if (orientation == SliderStyle::Horizontal) {
        outRect.X = x + trackStart;
        outRect.Y = y + (height - 6) / 2;
        outRect.Width = trackLen;
        outRect.Height = 6;
    } else {
        outRect.X = x + (width - 6) / 2;
        outRect.Y = y + trackStart;
        outRect.Width = 6;
        outRect.Height = trackLen;
    }
}

// --------------------------------------------------------------------------
// InertiaState - Tracks drag velocity for inertial scrolling after the
// user releases the thumb.
// --------------------------------------------------------------------------
struct InertiaState {
    int32 Velocity;
    int32 LastValue;
    int32 LastFrame;
    bool  IsActive;

    InertiaState() : Velocity(0), LastValue(0), LastFrame(0), IsActive(false) {}
};

InertiaState g_InertiaState;

// --------------------------------------------------------------------------
// UpdateInertia - Applies inertia to a slider value and returns the new
// value. The velocity decays each frame.
// --------------------------------------------------------------------------
int32 UpdateInertia(InertiaState& state, int32 currentValue,
                    int32 minVal, int32 maxVal, int32 frame)
{
    if (!state.IsActive) return currentValue;

    // Decay velocity.
    if (state.Velocity > 0) {
        state.Velocity -= INERTIA_DECAY;
        if (state.Velocity < INERTIA_THRESHOLD) state.Velocity = 0;
    } else if (state.Velocity < 0) {
        state.Velocity += INERTIA_DECAY;
        if (state.Velocity > -INERTIA_THRESHOLD) state.Velocity = 0;
    }

    if (state.Velocity == 0) {
        state.IsActive = false;
        return currentValue;
    }

    int32 newVal = currentValue + state.Velocity;
    if (newVal < minVal) {
        newVal = minVal;
        state.Velocity = 0;
        state.IsActive = false;
    } else if (newVal > maxVal) {
        newVal = maxVal;
        state.Velocity = 0;
        state.IsActive = false;
    }

    (void)frame;
    return newVal;
}

// --------------------------------------------------------------------------
// RecordDragSample - Records a drag sample to compute the release velocity.
// --------------------------------------------------------------------------
void RecordDragSample(InertiaState& state, int32 value, int32 frame)
{
    if (state.LastFrame > 0 && frame > state.LastFrame) {
        int32 dt = frame - state.LastFrame;
        if (dt > 0) {
            state.Velocity = (value - state.LastValue) / dt;
        }
    }
    state.LastValue = value;
    state.LastFrame = frame;
}

// --------------------------------------------------------------------------
// StartInertia - Begins inertial scrolling with the recorded velocity.
// --------------------------------------------------------------------------
void StartInertia(InertiaState& state)
{
    if (state.Velocity != 0) {
        state.IsActive = true;
    }
}

// --------------------------------------------------------------------------
// StopInertia - Immediately stops inertial scrolling.
// --------------------------------------------------------------------------
void StopInertia(InertiaState& state)
{
    state.IsActive = false;
    state.Velocity = 0;
}

// --------------------------------------------------------------------------
// LerpColor - Linearly interpolates between two colors.
// --------------------------------------------------------------------------
ColorStruct LerpColor(const ColorStruct& a, const ColorStruct& b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return ColorStruct(
        static_cast<uint8>(a.R + (b.R - a.R) * t),
        static_cast<uint8>(a.G + (b.G - a.G) * t),
        static_cast<uint8>(a.B + (b.B - a.B) * t),
        static_cast<uint8>(a.A + (b.A - a.A) * t));
}

// --------------------------------------------------------------------------
// GetStateColor - Returns the appropriate thumb color based on the gadget
// state (pressed, hovered, disabled, normal).
// --------------------------------------------------------------------------
ColorStruct GetStateColor(const ColorStruct& baseColor, bool isPressed,
                          bool isHovered, bool isEnabled)
{
    if (!isEnabled) {
        return LerpColor(baseColor, ColorStruct(80, 80, 80), 0.5f);
    }
    if (isPressed) {
        return LerpColor(baseColor, ColorStruct(255, 255, 255), 0.3f);
    }
    if (isHovered) {
        return LerpColor(baseColor, ColorStruct(255, 255, 255), 0.15f);
    }
    return baseColor;
}

// --------------------------------------------------------------------------
// ComputePageStep - Returns the step size for page-up/page-down operations,
// which is typically larger than the normal step.
// --------------------------------------------------------------------------
int32 ComputePageStep(int32 minVal, int32 maxVal, int32 stepSize)
{
    int32 range = maxVal - minVal;
    if (range <= 0) return 1;
    int32 pageStep = range / DEFAULT_PAGE_STEP;
    if (pageStep < stepSize) pageStep = stepSize;
    return pageStep;
}

// --------------------------------------------------------------------------
// IsPointOnThumb - Returns true if the given point (relative to the slider
// origin) lies within the thumb's bounding rectangle.
// --------------------------------------------------------------------------
bool IsPointOnThumb(SliderStyle orientation, int32 width, int32 height,
                    int32 thumbSize, int32 thumbPixel, int32 px, int32 py)
{
    Rectangle thumbRect;
    ComputeThumbRect(orientation, 0, 0, width, height, thumbSize,
                     thumbPixel, thumbRect);
    return thumbRect.ContainsPoint(px, py);
}

// --------------------------------------------------------------------------
// IsPointOnTrack - Returns true if the given point lies within the track
// rectangle (allowing click-to-jump behavior).
// --------------------------------------------------------------------------
bool IsPointOnTrack(SliderStyle orientation, int32 width, int32 height,
                    int32 thumbSize, int32 px, int32 py)
{
    Rectangle trackRect;
    ComputeTrackRect(orientation, 0, 0, width, height, thumbSize, trackRect);
    // Expand the hit area vertically/horizontally for easier clicking.
    return trackRect.ContainsPoint(px, py, 4);
}

// --------------------------------------------------------------------------
// DescribeSliderState - Returns a short string describing the slider state
// for debugging overlays.
// --------------------------------------------------------------------------
const char* DescribeSliderState(const SliderClass& slider)
{
    if (!slider.IsEnabled()) return "Disabled";
    if (slider.GetThumbSize() == 0) return "NoThumb";
    return "Active";
}

// --------------------------------------------------------------------------
// ComputeWheelDelta - Converts a mouse wheel delta into a slider value
// change, scaled by the step size.
// --------------------------------------------------------------------------
int32 ComputeWheelDelta(int32 wheelDelta, int32 stepSize)
{
    if (wheelDelta == 0 || stepSize <= 0) return 0;
    int32 magnitude = (wheelDelta < 0) ? -wheelDelta : wheelDelta;
    int32 sign = (wheelDelta < 0) ? -1 : 1;
    return sign * stepSize * ((magnitude + 119) / 120);
}

// --------------------------------------------------------------------------
// ClampValue - Clamps a value to the [min, max] range.
// --------------------------------------------------------------------------
int32 ClampValue(int32 value, int32 minVal, int32 maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

// --------------------------------------------------------------------------
// ValueToPercent - Returns the value as an integer percentage (0..100).
// --------------------------------------------------------------------------
int32 ValueToPercent(int32 value, int32 minVal, int32 maxVal)
{
    if (maxVal <= minVal) return 0;
    int32 range = maxVal - minVal;
    int32 rel = value - minVal;
    int32 percent = (rel * 100 + range / 2) / range;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}

// --------------------------------------------------------------------------
// PercentToValue - Converts a percentage back to a value.
// --------------------------------------------------------------------------
int32 PercentToValue(int32 percent, int32 minVal, int32 maxVal)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (maxVal <= minVal) return minVal;
    int32 range = maxVal - minVal;
    return minVal + (percent * range + 50) / 100;
}

// --------------------------------------------------------------------------
// ShouldShowValueLabel - Returns true if the slider should display a text
// label showing the current value. This depends on whether the slider is
// wide enough to fit the label.
// --------------------------------------------------------------------------
bool ShouldShowValueLabel(SliderStyle orientation, int32 width, int32 height,
                          int32 thumbSize)
{
    if (orientation == SliderStyle::Horizontal) {
        return width >= thumbSize * 3;
    } else {
        return height >= thumbSize * 3;
    }
}

// --------------------------------------------------------------------------
// ComputeValueLabelPosition - Returns the screen-space position where the
// value label should be drawn.
// --------------------------------------------------------------------------
Point2D ComputeValueLabelPosition(SliderStyle orientation, int32 x, int32 y,
                                  int32 width, int32 height, int32 thumbPixel)
{
    if (orientation == SliderStyle::Horizontal) {
        return Point2D(x + thumbPixel, y + height + TICK_LABEL_MARGIN);
    } else {
        return Point2D(x + width + TICK_LABEL_MARGIN, y + thumbPixel);
    }
}

} // namespace

// ============================================================================
// File-local entry points that bridge the SliderClass to the helper
// functions above. These are kept as file-local free functions so the
// header does not need to change, yet other translation units can invoke
// them when needed.
// ============================================================================

extern "C" {

// ----------------------------------------------------------------------------
// Slider_ComputeTrackBounds - Track start and length.
// ----------------------------------------------------------------------------
void Slider_ComputeTrackBounds(SliderStyle orientation, int32 width, int32 height,
                               int32 thumbSize, int32* outStart, int32* outLength)
{
    if (!outStart || !outLength) return;
    ComputeTrackBounds(orientation, width, height, thumbSize, *outStart, *outLength);
}

// ----------------------------------------------------------------------------
// Slider_SnapToTick - Snap a value to the nearest tick.
// ----------------------------------------------------------------------------
int32 Slider_SnapToTick(int32 value, int32 minVal, int32 maxVal, int32 tickCount)
{
    return SnapToTick(value, minVal, maxVal, tickCount);
}

// ----------------------------------------------------------------------------
// Slider_ComputeTickPositions - Pixel positions of tick marks.
// ----------------------------------------------------------------------------
int32 Slider_ComputeTickPositions(SliderStyle orientation, int32 width, int32 height,
                                  int32 thumbSize, int32 tickCount,
                                  int32* outPositions, int32 maxPositions)
{
    return ComputeTickPositions(orientation, width, height, thumbSize,
                                tickCount, outPositions, maxPositions);
}

// ----------------------------------------------------------------------------
// Slider_ComputeTickValues - Values at each tick mark.
// ----------------------------------------------------------------------------
int32 Slider_ComputeTickValues(int32 minVal, int32 maxVal, int32 tickCount,
                               int32* outValues, int32 maxValues)
{
    return ComputeTickValues(minVal, maxVal, tickCount, outValues, maxValues);
}

// ----------------------------------------------------------------------------
// Slider_FormatValue - Format a value as a decimal string.
// ----------------------------------------------------------------------------
int32 Slider_FormatValue(int32 value, char* outBuffer, int32 bufferSize)
{
    return FormatValueToString(value, outBuffer, bufferSize);
}

// ----------------------------------------------------------------------------
// Slider_FormatPercent - Format a normalized value as a percentage.
// ----------------------------------------------------------------------------
int32 Slider_FormatPercent(float normalized, char* outBuffer, int32 bufferSize)
{
    return FormatPercentString(normalized, outBuffer, bufferSize);
}

// ----------------------------------------------------------------------------
// Slider_ComputeThumbRect - Bounding rectangle of the thumb.
// ----------------------------------------------------------------------------
void Slider_ComputeThumbRect(SliderStyle orientation, int32 x, int32 y,
                             int32 width, int32 height, int32 thumbSize,
                             int32 thumbPixel, Rectangle* pOutRect)
{
    if (!pOutRect) return;
    ComputeThumbRect(orientation, x, y, width, height, thumbSize,
                     thumbPixel, *pOutRect);
}

// ----------------------------------------------------------------------------
// Slider_ComputeFillRect - Filled portion of the track.
// ----------------------------------------------------------------------------
void Slider_ComputeFillRect(SliderStyle orientation, int32 x, int32 y,
                            int32 width, int32 height, int32 thumbSize,
                            int32 thumbPixel, Rectangle* pOutRect)
{
    if (!pOutRect) return;
    ComputeFillRect(orientation, x, y, width, height, thumbSize,
                    thumbPixel, *pOutRect);
}

// ----------------------------------------------------------------------------
// Slider_ComputeTrackRect - Full track rectangle.
// ----------------------------------------------------------------------------
void Slider_ComputeTrackRect(SliderStyle orientation, int32 x, int32 y,
                             int32 width, int32 height, int32 thumbSize,
                             Rectangle* pOutRect)
{
    if (!pOutRect) return;
    ComputeTrackRect(orientation, x, y, width, height, thumbSize, *pOutRect);
}

// ----------------------------------------------------------------------------
// Slider_UpdateInertia - Advance inertial scrolling.
// ----------------------------------------------------------------------------
int32 Slider_UpdateInertia(int32 currentValue, int32 minVal, int32 maxVal, int32 frame)
{
    return UpdateInertia(g_InertiaState, currentValue, minVal, maxVal, frame);
}

// ----------------------------------------------------------------------------
// Slider_RecordDragSample - Record a drag velocity sample.
// ----------------------------------------------------------------------------
void Slider_RecordDragSample(int32 value, int32 frame)
{
    RecordDragSample(g_InertiaState, value, frame);
}

// ----------------------------------------------------------------------------
// Slider_StartInertia - Begin inertial scrolling.
// ----------------------------------------------------------------------------
void Slider_StartInertia()
{
    StartInertia(g_InertiaState);
}

// ----------------------------------------------------------------------------
// Slider_StopInertia - Stop inertial scrolling.
// ----------------------------------------------------------------------------
void Slider_StopInertia()
{
    StopInertia(g_InertiaState);
}

// ----------------------------------------------------------------------------
// Slider_LerpColor - Interpolate between two colors.
// ----------------------------------------------------------------------------
ColorStruct Slider_LerpColor(const ColorStruct* pA, const ColorStruct* pB, float t)
{
    if (!pA || !pB) return ColorStruct(0, 0, 0);
    return LerpColor(*pA, *pB, t);
}

// ----------------------------------------------------------------------------
// Slider_GetStateColor - State-dependent thumb color.
// ----------------------------------------------------------------------------
ColorStruct Slider_GetStateColor(const ColorStruct* pBaseColor, bool isPressed,
                                 bool isHovered, bool isEnabled)
{
    if (!pBaseColor) return ColorStruct(200, 200, 200);
    return GetStateColor(*pBaseColor, isPressed, isHovered, isEnabled);
}

// ----------------------------------------------------------------------------
// Slider_ComputePageStep - Page-up/down step size.
// ----------------------------------------------------------------------------
int32 Slider_ComputePageStep(int32 minVal, int32 maxVal, int32 stepSize)
{
    return ComputePageStep(minVal, maxVal, stepSize);
}

// ----------------------------------------------------------------------------
// Slider_IsPointOnThumb - Hit test for the thumb.
// ----------------------------------------------------------------------------
bool Slider_IsPointOnThumb(SliderStyle orientation, int32 width, int32 height,
                           int32 thumbSize, int32 thumbPixel, int32 px, int32 py)
{
    return IsPointOnThumb(orientation, width, height, thumbSize,
                          thumbPixel, px, py);
}

// ----------------------------------------------------------------------------
// Slider_IsPointOnTrack - Hit test for the track.
// ----------------------------------------------------------------------------
bool Slider_IsPointOnTrack(SliderStyle orientation, int32 width, int32 height,
                           int32 thumbSize, int32 px, int32 py)
{
    return IsPointOnTrack(orientation, width, height, thumbSize, px, py);
}

// ----------------------------------------------------------------------------
// Slider_DescribeState - Debug state string.
// ----------------------------------------------------------------------------
const char* Slider_DescribeState(const SliderClass* pSlider)
{
    if (!pSlider) return "None";
    return DescribeSliderState(*pSlider);
}

// ----------------------------------------------------------------------------
// Slider_ComputeWheelDelta - Wheel delta to value change.
// ----------------------------------------------------------------------------
int32 Slider_ComputeWheelDelta(int32 wheelDelta, int32 stepSize)
{
    return ComputeWheelDelta(wheelDelta, stepSize);
}

// ----------------------------------------------------------------------------
// Slider_ClampValue - Clamp to [min, max].
// ----------------------------------------------------------------------------
int32 Slider_ClampValue(int32 value, int32 minVal, int32 maxVal)
{
    return ClampValue(value, minVal, maxVal);
}

// ----------------------------------------------------------------------------
// Slider_ValueToPercent - Value to percentage (0..100).
// ----------------------------------------------------------------------------
int32 Slider_ValueToPercent(int32 value, int32 minVal, int32 maxVal)
{
    return ValueToPercent(value, minVal, maxVal);
}

// ----------------------------------------------------------------------------
// Slider_PercentToValue - Percentage to value.
// ----------------------------------------------------------------------------
int32 Slider_PercentToValue(int32 percent, int32 minVal, int32 maxVal)
{
    return PercentToValue(percent, minVal, maxVal);
}

// ----------------------------------------------------------------------------
// Slider_ShouldShowValueLabel - Whether to show the value label.
// ----------------------------------------------------------------------------
bool Slider_ShouldShowValueLabel(SliderStyle orientation, int32 width,
                                 int32 height, int32 thumbSize)
{
    return ShouldShowValueLabel(orientation, width, height, thumbSize);
}

// ----------------------------------------------------------------------------
// Slider_ComputeValueLabelPosition - Position of the value label.
// ----------------------------------------------------------------------------
Point2D Slider_ComputeValueLabelPosition(SliderStyle orientation, int32 x, int32 y,
                                         int32 width, int32 height, int32 thumbPixel)
{
    return ComputeValueLabelPosition(orientation, x, y, width, height, thumbPixel);
}

} // extern "C"
