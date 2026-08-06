#pragma once

#include "Core/Definitions.h"
#include "Core/Macros.h"
#include "Rendering/Surface.h"
#include "COM/IUnknown.h"

// Forward declarations
class GadgetClass;

// ============================================================================
// IGameMap interface
// ============================================================================
class NOVTABLE IGameMap : public IUnknown
{
public:
    virtual ~IGameMap() = default;
};

// ============================================================================
// GScreenClass - Global screen management (fullscreen/window)
//
// Manages the primary display surface, window mode, double-buffering,
// and screen flipping. Acts as the top-level rendering coordinator.
// ============================================================================
class NOVTABLE GScreenClass : public IGameMap
{
public:
    static GScreenClass* Instance;

    // Static blit helper
    static void DoBlit(bool mouseCaptured, DSurface* surface, Rectangle* rect = nullptr);

    GScreenClass();
    virtual ~GScreenClass();

    // IUnknown
    virtual HRESULT QueryInterface(const IID& iid, void** ppvObject) override;
    virtual ULONG AddRef() override;
    virtual ULONG Release() override;

    // IGameMap

    // GScreenClass virtual methods
    virtual void One_Time();
    virtual void Init();
    virtual void Init_Clear();
    virtual void Init_IO();
    virtual void GetInputAndUpdate(DWORD& outKeyCode, int32& outMouseX, int32& outMouseY);
    virtual void Update(const int32& keyCode, const Point2D& mouseCoords);
    virtual bool SetButtons(GadgetClass* pGadget);
    virtual bool AddButton(GadgetClass* pGadget);
    virtual bool RemoveButton(GadgetClass* pGadget);
    virtual void MarkNeedsRedraw(int32 dwUnk);
    virtual void DrawOnTop();
    virtual void Draw(DWORD dwUnk);
    virtual void vt_entry_44();
    virtual bool SetCursor(MouseCursorType idxCursor, bool miniMap) = 0;
    virtual bool UpdateCursor(MouseCursorType idxCursor, bool miniMap) = 0;
    virtual bool RestoreCursor() = 0;
    virtual void UpdateCursorMinimapState(bool miniMap) = 0;

    // Non-virtual methods
    void Render();
    void Destroy();
    void Flip();
    void BlitToScreen();
    void SetVideoMode(int32 width, int32 height, int32 bpp);

    // Properties
    int32 ScreenWidth;
    int32 ScreenHeight;
    int32 ScreenShakeX;
    int32 ScreenShakeY;
    int32 Bitfield;
    GadgetClass* ButtonList;

    // Accessor for the back buffer (used by loading / saving screens and
    // other code that needs to draw directly before a flip).
    DSurface* Get_Back_Buffer() const { return BackBuffer; }
    DSurface* Get_Fullscreen_Surface() const { return FullscreenSurface; }

protected:
    DSurface* FullscreenSurface;
    DSurface* BackBuffer;
    ULONG RefCount;
};