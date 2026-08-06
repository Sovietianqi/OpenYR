#include "Rendering/GScreenClass.h"
#include "Rendering/DisplayClass.h"
#include "Rendering/TacticalClass.h"
#include "Rendering/SidebarClass.h"
#include "Rendering/MouseClass.h"
#include "Rendering/RadarClass.h"
#include "Core/Memory.h"

#include <cstring>
#include <cmath>

// ============================================================================
// Static members
// ============================================================================
GScreenClass* GScreenClass::Instance = nullptr;

// ============================================================================
// Constants
// ============================================================================
static constexpr int32 DefaultScreenWidth = 640;
static constexpr int32 DefaultScreenHeight = 400;
static constexpr int32 MaxScreenWidth = 1920;
static constexpr int32 MaxScreenHeight = 1200;
static constexpr int32 DefaultBPP = 16;
static constexpr uint32 BackupBufferSize = 640 * 400 * 2; // 640x400x16bit

// ============================================================================
// Static methods
// ============================================================================

void GScreenClass::DoBlit(bool mouseCaptured, DSurface* surface, Rectangle* rect)
{
    if (!surface || !Instance) return;
    if (!Instance->FullscreenSurface || !Instance->FullscreenSurface->Buffer) return;

    Rectangle srcRect = rect ? *rect : Rectangle(0, 0, surface->Width, surface->Height);
    Rectangle dstRect = srcRect;

    // Clamp to screen dimensions
    if (dstRect.X + dstRect.Width > Instance->ScreenWidth)
        dstRect.Width = Instance->ScreenWidth - dstRect.X;
    if (dstRect.Y + dstRect.Height > Instance->ScreenHeight)
        dstRect.Height = Instance->ScreenHeight - dstRect.Y;
    if (dstRect.Width <= 0 || dstRect.Height <= 0) return;

    int32 bytesPerPix = surface->GetBytesPerPixel();
    int32 dstBytesPerPix = Instance->FullscreenSurface->GetBytesPerPixel();

    if (bytesPerPix == dstBytesPerPix)
    {
        for (int32 y = 0; y < dstRect.Height; ++y)
        {
            int32 srcY = srcRect.Y + y;
            int32 dstY = dstRect.Y + y;
            BYTE* srcLine = static_cast<BYTE*>(surface->Buffer) + srcY * surface->Pitch + srcRect.X * bytesPerPix;
            BYTE* dstLine = static_cast<BYTE*>(Instance->FullscreenSurface->Buffer)
                          + dstY * Instance->FullscreenSurface->Pitch + dstRect.X * bytesPerPix;
            memcpy(dstLine, srcLine, static_cast<size_t>(dstRect.Width) * bytesPerPix);
        }
    }
    else
    {
        // Bit depth conversion
        for (int32 y = 0; y < dstRect.Height; ++y)
        {
            int32 srcY = srcRect.Y + y;
            int32 dstY = dstRect.Y + y;
            BYTE* srcLine = static_cast<BYTE*>(surface->Buffer) + srcY * surface->Pitch + srcRect.X * bytesPerPix;
            BYTE* dstLine = static_cast<BYTE*>(Instance->FullscreenSurface->Buffer)
                          + dstY * Instance->FullscreenSurface->Pitch + dstRect.X * dstBytesPerPix;

            for (int32 x = 0; x < dstRect.Width; ++x)
            {
                if (bytesPerPix == 2 && dstBytesPerPix == 4)
                {
                    uint16 src = reinterpret_cast<uint16*>(srcLine)[x];
                    int32 r = ((src >> 11) & 0x1F) << 3;
                    int32 g = ((src >> 5) & 0x3F) << 2;
                    int32 b = (src & 0x1F) << 3;
                    reinterpret_cast<uint32*>(dstLine)[x] = static_cast<uint32>((r << 16) | (g << 8) | b);
                }
            }
        }
    }
}

// ============================================================================
// GScreenClass implementation
// ============================================================================

GScreenClass::GScreenClass()
    : ScreenWidth(DefaultScreenWidth)
    , ScreenHeight(DefaultScreenHeight)
    , ScreenShakeX(0)
    , ScreenShakeY(0)
    , Bitfield(2)
    , ButtonList(nullptr)
    , FullscreenSurface(nullptr)
    , BackBuffer(nullptr)
    , RefCount(1)
{
    Instance = this;
}

GScreenClass::~GScreenClass()
{
    Destroy();
    if (Instance == this)
        Instance = nullptr;
}

// ============================================================================
// IUnknown
// ============================================================================

HRESULT GScreenClass::QueryInterface(const IID& iid, void** ppvObject)
{
    if (!ppvObject) return E_POINTER;
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

ULONG GScreenClass::AddRef()
{
    return ++RefCount;
}

ULONG GScreenClass::Release()
{
    ULONG count = --RefCount;
    if (count == 0) delete this;
    return count;
}

// ============================================================================
// Virtual methods
// ============================================================================

void GScreenClass::One_Time()
{
    Init();
}

void GScreenClass::Init()
{
    ScreenWidth = DefaultScreenWidth;
    ScreenHeight = DefaultScreenHeight;
    ScreenShakeX = 0;
    ScreenShakeY = 0;

    // Create back buffer surface (16-bit color)
    BackBuffer = GameCreate<DSurface>();
    if (BackBuffer)
    {
        BackBuffer->Allocate(ScreenWidth, ScreenHeight);
    }

    // Create fullscreen surface
    FullscreenSurface = GameCreate<DSurface>();
    if (FullscreenSurface)
    {
        FullscreenSurface->Allocate(ScreenWidth, ScreenHeight);
    }
}

void GScreenClass::Init_Clear()
{
    if (BackBuffer && BackBuffer->Buffer)
    {
        size_t bufferSize = static_cast<size_t>(BackBuffer->Width) * BackBuffer->Height * BackBuffer->BytesPerPixel;
        memset(BackBuffer->Buffer, 0, bufferSize);
    }

    if (FullscreenSurface && FullscreenSurface->Buffer)
    {
        size_t bufferSize = static_cast<size_t>(FullscreenSurface->Width) * FullscreenSurface->Height
                          * FullscreenSurface->BytesPerPixel;
        memset(FullscreenSurface->Buffer, 0, bufferSize);
    }

    ScreenShakeX = 0;
    ScreenShakeY = 0;
}

void GScreenClass::Init_IO()
{
    // Initialize the input/output subsystems: reset the mouse handler,
    // clear the active gadget/button chain, and zero the screen-shake
    // offsets so no residual input or shake state carries over from a
    // previous session.
    if (MouseClass::Instance)
    {
        MouseClass::Instance->Init();
    }

    ButtonList = nullptr;

    ScreenShakeX = 0;
    ScreenShakeY = 0;
}

void GScreenClass::GetInputAndUpdate(
    DWORD& outKeyCode, int32& outMouseX, int32& outMouseY)
{
    outKeyCode = 0;
    outMouseX = 0;
    outMouseY = 0;

    // In a production build, this would poll the OS input system
    // For the engine reconstruction, we provide the interface
}

void GScreenClass::Update(const int32& keyCode, const Point2D& mouseCoords)
{
    // Process key input
    if (keyCode != 0)
    {
        // Handle keyboard shortcuts, game controls
        switch (keyCode)
        {
            case 0x1B: // ESC - cancel/clear
                break;
            case 0x20: // Space - go to last event
                break;
            case 0x09: // Tab - cycle select
                break;
            default:
                break;
        }
    }

    // Update mouse position
    if (MouseClass::Instance)
    {
        MouseClass::Instance->Mouse_Move(mouseCoords);
    }
}

bool GScreenClass::SetButtons(GadgetClass* pGadget)
{
    ButtonList = pGadget;
    return true;
}

bool GScreenClass::AddButton(GadgetClass* pGadget)
{
    if (!pGadget) return false;
    // For simplicity, prepend to the list since GadgetClass manages its own chain
    ButtonList = pGadget;
    return true;
}

bool GScreenClass::RemoveButton(GadgetClass* pGadget)
{
    if (!pGadget) return false;
    if (ButtonList == pGadget) {
        ButtonList = nullptr;
        return true;
    }
    return false;
}

void GScreenClass::MarkNeedsRedraw(int32 dwUnk)
{
    // Flag that the screen needs a redraw on the next render pass. The dirty
    // notification is propagated to the tactical display (which owns the
    // isometric tile renderer); a non-zero dwUnk additionally marks the
    // sidebar for a repaint and requests an immediate (un-deferred) dirty
    // region on the tactical renderer.
    if (DisplayClass::Instance)
    {
        DisplayClass::Instance->MarkToRedraw();
    }

    if (TacticalClass::Instance)
    {
        TacticalClass::Instance->RegisterDirtyArea(
            Rectangle(0, 0, ScreenWidth, ScreenHeight), dwUnk != 0);
    }

    if (dwUnk != 0 && SidebarClass::Instance)
    {
        SidebarClass::Instance->SidebarNeedsRepaint();
    }
}

void GScreenClass::DrawOnTop()
{
    // Draw overlay elements that appear on top of everything:
    // - Mouse cursor
    // - Tooltips
    // - Message text
    // - Screen shake offset

    if (!BackBuffer || !BackBuffer->Buffer) return;

    // Apply screen shake
    int32 shakeX = ScreenShakeX;
    int32 shakeY = ScreenShakeY;

    // Draw mouse cursor
    if (MouseClass::Instance)
    {
        MouseClass::Instance->Draw();
    }

    // Draw tooltip text
    if (SidebarClass::Instance)
    {
        const wchar_t* tooltip = reinterpret_cast<const wchar_t*>(SidebarClass::TooltipBuffer);
        if (tooltip && tooltip[0] != L'\0')
        {
            Point2D tipPos(16, ScreenHeight - 24);
            BackBuffer->DrawText(tooltip, &tipPos, 0xFFFFFFFF);
        }
    }
}

void GScreenClass::Draw(DWORD dwUnk)
{
    if (!BackBuffer || !BackBuffer->Buffer) return;

    // Clear the back buffer to black
    Init_Clear();

    // Draw the tactical map
    if (TacticalClass::Instance)
    {
        TacticalClass::Instance->Render(BackBuffer, true, TacticalRenderMode::All);
    }

    // Draw the sidebar
    if (SidebarClass::Instance)
    {
        SidebarClass::Instance->Draw(0);
    }

    // Draw the radar
    if (RadarClass::Instance)
    {
        RadarClass::Instance->Draw();
    }

    // Draw any overlay elements
    DrawOnTop();
}

void GScreenClass::vt_entry_44()
{
    // Legacy vtable stub 0x44: performs a light screen-state refresh.
    // Resets any residual screen-shake offset (so a stalled shake does not
    // leave the framebuffer offset indefinitely) and requests a redraw so
    // the next render pass presents a clean, stable frame.
    ScreenShakeX = 0;
    ScreenShakeY = 0;

    MarkNeedsRedraw(0);
}

// ============================================================================
// Non-virtual methods
// ============================================================================

void GScreenClass::Render()
{
    Draw(0);

    // Apply screen shake
    if (ScreenShakeX != 0 || ScreenShakeY != 0)
    {
        if (BackBuffer && FullscreenSurface)
        {
            // Offset the blit by shake amount
            Rectangle srcRect(
                (ScreenShakeX > 0) ? ScreenShakeX : 0,
                (ScreenShakeY > 0) ? ScreenShakeY : 0,
                ScreenWidth - std::abs(ScreenShakeX),
                ScreenHeight - std::abs(ScreenShakeY)
            );

            if (srcRect.IsValid())
            {
                DoBlit(false, BackBuffer, &srcRect);
            }
        }

        // Decay screen shake
        if (ScreenShakeX != 0)
        {
            ScreenShakeX = (ScreenShakeX > 0) ? -(ScreenShakeX / 2) : std::abs(ScreenShakeX) / 2;
        }
        if (ScreenShakeY != 0)
        {
            ScreenShakeY = (ScreenShakeY > 0) ? -(ScreenShakeY / 2) : std::abs(ScreenShakeY) / 2;
        }
    }
    else
    {
        Flip();
    }
}

void GScreenClass::Destroy()
{
    if (BackBuffer)
    {
        GameDelete(BackBuffer);
        BackBuffer = nullptr;
    }

    if (FullscreenSurface)
    {
        GameDelete(FullscreenSurface);
        FullscreenSurface = nullptr;
    }
}

void GScreenClass::Flip()
{
    if (!BackBuffer || !FullscreenSurface) return;
    if (!BackBuffer->Buffer || !FullscreenSurface->Buffer) return;

    // Copy back buffer to front buffer (simulated vsync page flip)
    int32 bytesPerPix = BackBuffer->GetBytesPerPixel();
    int32 copySize = BackBuffer->Width * BackBuffer->Height * bytesPerPix;
    int32 fullSize = FullscreenSurface->Width * FullscreenSurface->Height * bytesPerPix;

    if (copySize > fullSize) copySize = fullSize;

    memcpy(FullscreenSurface->Buffer, BackBuffer->Buffer, static_cast<size_t>(copySize));
}

void GScreenClass::BlitToScreen()
{
    // Direct blit from back buffer to screen
    // This is the same as Flip but may use hardware acceleration
    Flip();
}

void GScreenClass::SetVideoMode(int32 width, int32 height, int32 bpp)
{
    if (width <= 0 || height <= 0) return;
    if (width > MaxScreenWidth || height > MaxScreenHeight) return;

    ScreenWidth = width;
    ScreenHeight = height;

    // Destroy old surfaces
    Destroy();

    // Create new surfaces at the requested resolution
    BackBuffer = GameCreate<DSurface>();
    if (BackBuffer)
    {
        BackBuffer->BytesPerPixel = (bpp + 7) / 8;
        BackBuffer->Allocate(width, height);
    }

    FullscreenSurface = GameCreate<DSurface>();
    if (FullscreenSurface)
    {
        FullscreenSurface->BytesPerPixel = (bpp + 7) / 8;
        FullscreenSurface->Allocate(width, height);
    }

    // Notify the display class of the new resolution
    if (DisplayClass::Instance)
    {
        DisplayClass::Instance->Set_View_Dimensions(Rectangle(0, 0, width, height));
    }
}