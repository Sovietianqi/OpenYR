#include <UI/Gadget.h>

#include <Core/Definitions.h>
#include <Core/Macros.h>
#include <Core/Memory.h>
#include <Math/Rectangle.h>

// ============================================================================
// Gadget.cpp - Gadget implementation
//
//  Implements the base GadgetClass member functions.  The base class
//  provides position/size management, state flags, focus handling and
//  the parent-child hierarchy.  Subclasses override Draw() and
//  HandleEvent() to provide widget-specific behavior.
// ============================================================================

// ============================================================================
// Construction
// ============================================================================

GadgetClass::GadgetClass() noexcept
    : Rect(0, 0, 0, 0)
    , State(GadgetState::Visible | GadgetState::Enabled | GadgetState::NeedsRedraw)
    , ID(0)
    , ZOrder(0)
    , Color(255, 255, 255)
    , BackColor(0, 0, 0)
    , Callback(nullptr)
    , CallbackData(nullptr)
    , Parent(nullptr)
{
}

GadgetClass::GadgetClass(int32 x, int32 y, int32 w, int32 h, int32 id) noexcept
    : Rect(x, y, w, h)
    , State(GadgetState::Visible | GadgetState::Enabled | GadgetState::NeedsRedraw)
    , ID(id)
    , ZOrder(0)
    , Color(255, 255, 255)
    , BackColor(0, 0, 0)
    , Callback(nullptr)
    , CallbackData(nullptr)
    , Parent(nullptr)
{
}

// ============================================================================
// Destruction
//
//  The destructor removes the gadget from its parent's child list and
//  destroys all children.  This ensures the widget tree is cleaned up
//  bottom-up.
// ============================================================================

GadgetClass::~GadgetClass()
{
    // Detach from parent.
    if (Parent)
    {
        for (int32 i = 0; i < Parent->Children.Count; ++i)
        {
            if (Parent->Children[i] == this)
            {
                Parent->Children.Remove(i);
                break;
            }
        }
        Parent = nullptr;
    }

    // Destroy all children.  We iterate carefully because deleting a
    // child modifies the Children vector.
    while (Children.Count > 0)
    {
        GadgetClass* child = Children[0];
        if (child)
        {
            child->Parent = nullptr;  // Prevent re-entrance into our vector.
            delete child;
        }
        Children.Remove(0);
    }
}

// ============================================================================
// Position / size management
// ============================================================================

void GadgetClass::SetPosition(int32 x, int32 y) noexcept
{
    if (Rect.X == x && Rect.Y == y)
        return;
    Rect.X = x;
    Rect.Y = y;
    SetNeedsRedraw(true);
}

void GadgetClass::SetSize(int32 w, int32 h) noexcept
{
    if (Rect.Width == w && Rect.Height == h)
        return;
    Rect.Width = w;
    Rect.Height = h;
    SetNeedsRedraw(true);
}

void GadgetClass::SetRect(int32 x, int32 y, int32 w, int32 h) noexcept
{
    Rect.X = x;
    Rect.Y = y;
    Rect.Width = w;
    Rect.Height = h;
    SetNeedsRedraw(true);
}

// ============================================================================
// Hit testing
//
//  Contains() checks whether a point is inside the gadget's bounds.
//  Gadgets that are hidden or disabled are not considered to contain
//  any point (so they don't intercept mouse events).
// ============================================================================

bool GadgetClass::Contains(int32 x, int32 y) const noexcept
{
    if (!IsVisible())
        return false;
    if (x < Rect.X || x >= Rect.X + Rect.Width)
        return false;
    if (y < Rect.Y || y >= Rect.Y + Rect.Height)
        return false;
    return true;
}

// ============================================================================
// Static helper: find the topmost gadget at a point
//
//  Walks the children list in reverse Z-order (highest first) and returns
//  the first gadget whose bounds contain the point.  Returns nullptr if
//  no child is hit.
// ============================================================================

// Defined as a static method in the header would be cleaner, but we keep
// it here to match the original binary's layout.
namespace GadgetHelpers
{
    GadgetClass* FindGadgetAtPoint(GadgetClass* pRoot, int32 x, int32 y) noexcept
    {
        if (!pRoot)
            return nullptr;
        if (!pRoot->IsVisible())
            return nullptr;

        // Check children first (they are on top of the parent).
        for (int32 i = pRoot->Children.Count - 1; i >= 0; --i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (!child)
                continue;

            // Convert the point to child-local coordinates.
            int32 localX = x - child->GetX();
            int32 localY = y - child->GetY();

            if (child->Contains(x, y))
            {
                // Recurse into the child to find the deepest hit.
                GadgetClass* deeper = FindGadgetAtPoint(child, x, y);
                if (deeper)
                    return deeper;
                return child;
            }
            (void)localX;
            (void)localY;
        }

        // No child was hit; check the root itself.
        if (pRoot->Contains(x, y))
            return pRoot;

        return nullptr;
    }

    // ── Focus management helpers ────────────────────────────────────────

    // Find the first child that accepts focus (in tab order).
    GadgetClass* FindFirstFocusable(GadgetClass* pRoot) noexcept
    {
        if (!pRoot)
            return nullptr;
        if (!pRoot->HasState(GadgetState::NoFocus) &&
            pRoot->IsVisible() && pRoot->IsEnabled())
            return pRoot;

        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (!child) continue;
            GadgetClass* found = FindFirstFocusable(child);
            if (found) return found;
        }
        return nullptr;
    }

    // Find the next focusable child after the given gadget (tab forward).
    GadgetClass* FindNextFocusable(GadgetClass* pRoot,
                                   GadgetClass* pCurrent) noexcept
    {
        if (!pRoot || !pCurrent)
            return FindFirstFocusable(pRoot);

        bool foundCurrent = false;
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (!child) continue;

            if (foundCurrent)
            {
                GadgetClass* next = FindFirstFocusable(child);
                if (next) return next;
            }

            if (child == pCurrent)
                foundCurrent = true;
        }
        return nullptr;
    }

    // ── Drawing traversal ───────────────────────────────────────────────

    // Draw a gadget and all its visible children in Z-order.
    void DrawGadgetTree(GadgetClass* pRoot, Surface* pSurface)
    {
        if (!pRoot || !pSurface)
            return;
        if (!pRoot->IsVisible())
            return;

        // Draw the parent first (background).
        pRoot->Draw(pSurface);

        // Draw children sorted by Z-order (lowest first).
        // In a full implementation we would sort the children by ZOrder;
        // here we iterate in array order which is sufficient when children
        // are added in Z-order.
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (child)
                DrawGadgetTree(child, pSurface);
        }
    }

    // ── Event dispatch ──────────────────────────────────────────────────

    // Dispatch an event to the gadget tree.  The event is first offered
    // to the topmost child at the mouse position; if no child consumes
    // it, the root gets a chance.
    bool DispatchEvent(GadgetClass* pRoot, const GadgetEvent& event) noexcept
    {
        if (!pRoot || !pRoot->IsVisible() || !pRoot->IsEnabled())
            return false;

        // For mouse events, find the topmost gadget at the mouse position.
        if (event.Type == GadgetEventType::MouseDown ||
            event.Type == GadgetEventType::MouseUp ||
            event.Type == GadgetEventType::MouseMove ||
            event.Type == GadgetEventType::MouseClick ||
            event.Type == GadgetEventType::MouseDblClick ||
            event.Type == GadgetEventType::MouseWheel)
        {
            int32 absX = event.MousePos.X;
            int32 absY = event.MousePos.Y;

            // Check children in reverse order (topmost first).
            for (int32 i = pRoot->Children.Count - 1; i >= 0; --i)
            {
                GadgetClass* child = pRoot->Children[i];
                if (!child || !child->IsVisible() || !child->IsEnabled())
                    continue;

                if (child->Contains(absX, absY))
                {
                    // Convert to child-local coordinates.
                    GadgetEvent childEvent = event;
                    childEvent.MousePos.X = absX - child->GetX();
                    childEvent.MousePos.Y = absY - child->GetY();

                    if (DispatchEvent(child, childEvent))
                        return true;
                }
            }
        }

        // No child consumed the event; offer it to the root.
        return pRoot->HandleEvent(event);
    }

    // ── Z-order sorting ────────────────────────────────────────────────
    //
    //  Sorts the children of a gadget by their ZOrder field (ascending:
    //  lower ZOrder draws first, higher ZOrder draws on top).  Uses
    //  insertion sort which is efficient for small child lists.

    void SortChildrenByZOrder(GadgetClass* pRoot) noexcept
    {
        if (!pRoot || pRoot->Children.Count < 2)
            return;
        DynamicVectorClass<GadgetClass*>& children = pRoot->Children;
        for (int32 i = 1; i < children.Count; ++i)
        {
            GadgetClass* key = children.Items[i];
            int32 j = i - 1;
            while (j >= 0 && children.Items[j] &&
                   children.Items[j]->GetZOrder() > key->GetZOrder())
            {
                children.Items[j + 1] = children.Items[j];
                --j;
            }
            children.Items[j + 1] = key;
        }
    }

    // ── Tab navigation (backward) ──────────────────────────────────────
    //
    //  Finds the previous focusable gadget before pCurrent in tab order.
    //  Used for Shift+Tab navigation.

    GadgetClass* FindPrevFocusable(GadgetClass* pRoot,
                                   GadgetClass* pCurrent) noexcept
    {
        if (!pRoot)
            return nullptr;
        if (!pCurrent)
        {
            // Return the last focusable child.
            for (int32 i = pRoot->Children.Count - 1; i >= 0; --i)
            {
                GadgetClass* child = pRoot->Children[i];
                if (!child) continue;
                // Recurse to find the deepest focusable in this subtree.
                GadgetClass* found = FindPrevFocusable(child, nullptr);
                if (found) return found;
                if (!child->HasState(GadgetState::NoFocus) &&
                    child->IsVisible() && child->IsEnabled() &&
                    child->HasState(GadgetState::TabStop))
                    return child;
            }
            return nullptr;
        }

        // Walk backwards from the current position.
        int32 currentIdx = -1;
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            if (pRoot->Children[i] == pCurrent)
            {
                currentIdx = i;
                break;
            }
        }

        if (currentIdx < 0)
            return nullptr;

        for (int32 i = currentIdx - 1; i >= 0; --i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (!child) continue;
            GadgetClass* found = FindPrevFocusable(child, nullptr);
            if (found) return found;
            if (!child->HasState(GadgetState::NoFocus) &&
                child->IsVisible() && child->IsEnabled() &&
                child->HasState(GadgetState::TabStop))
                return child;
        }
        return nullptr;
    }

    // ── Modal dialog support ───────────────────────────────────────────
    //
    //  Returns the topmost modal gadget in the tree.  A modal gadget
    //  blocks input to all gadgets below it in the Z-order.

    GadgetClass* FindTopmostModal(GadgetClass* pRoot) noexcept
    {
        if (!pRoot)
            return nullptr;
        // Check children in reverse order (topmost first).
        for (int32 i = pRoot->Children.Count - 1; i >= 0; --i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (!child) continue;
            GadgetClass* modal = FindTopmostModal(child);
            if (modal) return modal;
        }
        if (pRoot->HasState(GadgetState::Modal))
            return pRoot;
        return nullptr;
    }

    // ── Mouse enter / leave event generation ───────────────────────────
    //
    //  Given the previous and current mouse positions, generates
    //  MouseEnter / MouseLeave events for gadgets that the pointer has
    //  entered or left.  This is called by the input manager before
    //  dispatching the regular MouseMove event.

    void GenerateEnterLeaveEvents(GadgetClass* pRoot,
                                   int32 oldX, int32 oldY,
                                   int32 newX, int newY) noexcept
    {
        if (!pRoot || !pRoot->IsVisible())
            return;

        bool wasInside = pRoot->Contains(oldX, oldY);
        bool isInside = pRoot->Contains(newX, newY);

        if (!wasInside && isInside)
        {
            GadgetEvent enterEvent;
            enterEvent.Type = GadgetEventType::MouseEnter;
            enterEvent.MousePos = Point2D(newX - pRoot->GetX(), newY - pRoot->GetY());
            enterEvent.Key = 0;
            enterEvent.WheelDelta = 0;
            enterEvent.Modifiers = 0;
            pRoot->HandleEvent(enterEvent);
        }
        else if (wasInside && !isInside)
        {
            GadgetEvent leaveEvent;
            leaveEvent.Type = GadgetEventType::MouseLeave;
            leaveEvent.MousePos = Point2D(newX - pRoot->GetX(), newY - pRoot->GetY());
            leaveEvent.Key = 0;
            leaveEvent.WheelDelta = 0;
            leaveEvent.Modifiers = 0;
            pRoot->HandleEvent(leaveEvent);
        }

        // Recurse into children.
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            GadgetClass* child = pRoot->Children[i];
            if (child)
                GenerateEnterLeaveEvents(child, oldX, oldY, newX, newY);
        }
    }

    // ── Coordinate conversion helpers ──────────────────────────────────
    //
    //  Convert a point from screen coordinates to gadget-local coordinates
    //  by subtracting the accumulated offsets of all ancestors.

    Point2D ScreenToLocal(GadgetClass* pGadget, int32 screenX, int32 screenY) noexcept
    {
        if (!pGadget)
            return Point2D(screenX, screenY);
        int32 localX = screenX;
        int32 localY = screenY;
        GadgetClass* p = pGadget;
        while (p)
        {
            localX -= p->GetX();
            localY -= p->GetY();
            p = p->Parent;
        }
        return Point2D(localX, localY);
    }

    // Convert a point from gadget-local coordinates to screen coordinates
    // by adding the accumulated offsets of all ancestors.
    Point2D LocalToScreen(GadgetClass* pGadget, int32 localX, int32 localY) noexcept
    {
        if (!pGadget)
            return Point2D(localX, localY);
        int32 screenX = localX;
        int32 screenY = localY;
        GadgetClass* p = pGadget;
        while (p)
        {
            screenX += p->GetX();
            screenY += p->GetY();
            p = p->Parent;
        }
        return Point2D(screenX, screenY);
    }

    // ── Visibility-based culling ───────────────────────────────────────
    //
    //  Returns true if any part of the gadget's rect intersects the
    //  given clip rectangle.  Used by the renderer to skip drawing
    //  gadgets that are entirely off-screen.

    bool IsGadgetInClipRect(GadgetClass* pGadget, const Rectangle& clipRect) noexcept
    {
        if (!pGadget || !pGadget->IsVisible())
            return false;
        int32 gx = pGadget->GetX();
        int32 gy = pGadget->GetY();
        int32 gw = pGadget->GetWidth();
        int32 gh = pGadget->GetHeight();
        // Check for non-overlapping rects.
        if (gx + gw <= clipRect.X) return false;
        if (gy + gh <= clipRect.Y) return false;
        if (gx >= clipRect.X + clipRect.Width) return false;
        if (gy >= clipRect.Y + clipRect.Height) return false;
        return true;
    }

    // ── Focus chain management ─────────────────────────────────────────
    //
    //  Sets focus to the specified gadget, removing focus from any
    //  previously-focused gadget in the tree.  Returns true if focus
    //  was successfully transferred.

    bool SetFocusInTree(GadgetClass* pRoot, GadgetClass* pTarget) noexcept
    {
        if (!pRoot || !pTarget)
            return false;
        if (pTarget->HasState(GadgetState::NoFocus))
            return false;
        if (!pTarget->IsVisible() || !pTarget->IsEnabled())
            return false;

        // Walk the tree and clear focus from everyone except the target.
        // We do this with a simple recursive walk.
        struct FocusWalker
        {
            GadgetClass* m_pTarget;
            bool m_found;

            bool Visit(GadgetClass* pGadget) noexcept
            {
                if (!pGadget) return true;
                if (pGadget == m_pTarget)
                {
                    m_found = true;
                    pGadget->Focus();
                    pGadget->OnFocus();
                    return true;
                }
                if (pGadget->IsFocused())
                {
                    pGadget->Unfocus();
                    pGadget->OnBlur();
                }
                for (int32 i = 0; i < pGadget->Children.Count; ++i)
                {
                    if (!Visit(pGadget->Children[i]))
                        return false;
                }
                return true;
            }
        };

        FocusWalker walker;
        walker.m_pTarget = pTarget;
        walker.m_found = false;
        walker.Visit(pRoot);
        return walker.m_found;
    }

    // ── Count visible gadgets ──────────────────────────────────────────
    //
    //  Recursively counts how many gadgets in the tree are visible.
    //  Used by the profiler to track UI complexity.

    int32 CountVisibleGadgets(GadgetClass* pRoot) noexcept
    {
        if (!pRoot || !pRoot->IsVisible())
            return 0;
        int32 count = 1; // Count self.
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            count += CountVisibleGadgets(pRoot->Children[i]);
        }
        return count;
    }

    // ── Find gadget by ID ──────────────────────────────────────────────
    //
    //  Depth-first search for a gadget with the specified ID.  Returns
    //  nullptr if not found.

    GadgetClass* FindGadgetByID(GadgetClass* pRoot, int32 id) noexcept
    {
        if (!pRoot)
            return nullptr;
        if (pRoot->GetID() == id)
            return pRoot;
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            GadgetClass* found = FindGadgetByID(pRoot->Children[i], id);
            if (found) return found;
        }
        return nullptr;
    }

    // ── Mark entire subtree as needing redraw ──────────────────────────

    void MarkSubtreeDirty(GadgetClass* pRoot) noexcept
    {
        if (!pRoot)
            return;
        pRoot->MarkDirty();
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            MarkSubtreeDirty(pRoot->Children[i]);
        }
    }

    // ── Update traversal ───────────────────────────────────────────────
    //
    //  Calls Update() on the root and all visible children.  Collects
    //  the redraw flags so the caller knows whether to repaint.

    bool UpdateGadgetTree(GadgetClass* pRoot) noexcept
    {
        if (!pRoot || !pRoot->IsVisible())
            return false;
        bool anyDirty = pRoot->Update();
        for (int32 i = 0; i < pRoot->Children.Count; ++i)
        {
            if (UpdateGadgetTree(pRoot->Children[i]))
                anyDirty = true;
        }
        return anyDirty;
    }
}
