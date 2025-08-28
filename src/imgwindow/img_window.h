/*
 * ImgWindow.h
 *
 * Integration for dear imgui into X-Plane.
 *
 * Copyright (C) 2018,2020 Christopher Collins
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef IMGWINDOW_H
#define IMGWINDOW_H

#include "system_gl.h"
#include "img_font_atlas.h"

#include "XPLMDisplay.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"

#include "../imgui/imgui.h"

#include <string>
#include <memory>
#include <queue>

/** ImgWindow is a Window for creating dear imgui widgets within.
 *
 * There's a few traps to be aware of when using dear imgui with X-Plane:
 *
 * 1) The Dear ImGUI coordinate scheme is inverted in the Y axis vs the X-Plane
 *    (and OpenGL default) scheme. You must be careful if you're trying to
 *    directly manipulate positioning of widgets rather than letting imgui
 *    self-layout.  There are (private) functions in ImgWindow to do the
 *    coordinate mapping.
 *
 * 2) The Dear ImGUI rendering space is only as big as the window - this means
 *    popup elements cannot be larger than the parent window.  This was
 *    unavoidable on XP11 because of how popup windows work and the possibility
 *    for negative coordinates (which imgui doesn't like).
 *
 * 3) There is no way to detect if the window is hidden without a per-frame
 *    processing loop or similar.
 *
 * @note It should be possible to map globally on XP9 & XP10 letting you run
 *     popups as large as you need, or to use the ImGUI native titlebars instead
 *     of the XP10 ones - source for this may be provided later, but could also
 *     be trivially adapted from this one by adjusting the way the space is
 *     translated and mapped in the DrawWindowCB and constructor.
 */
class ImgWindow
{
public:
    /** sFontAtlas is the global shared font-atlas.
     *
     * If you want to share fonts between windows, this needs to be set before
     * any dialogs are actually instantiated.  It will be automatically handed
     * over to the contexts as they're created.
     */
    static std::shared_ptr<ImgFontAtlas> sFontAtlas;

    virtual ~ImgWindow();

    /** Gets the current window geometry */
    void GetWindowGeometry(int& left, int& top, int& right, int& bottom) const
    {
        XPLMGetWindowGeometry(mWindowID, &left, &top, &right, &bottom);
    }

    /** Sets the window geometry. */
    void SetWindowGeometry(int left, int top, int right, int bottom)
    {
        XPLMSetWindowGeometry(mWindowID, left, top, right, bottom);
    }

    /** Gets the current window geometry for OS coordinates (if popped out) */
    void GetWindowGeometryOS(int& left, int& top, int& right, int& bottom) const
    {
        XPLMGetWindowGeometryOS(mWindowID, &left, &top, &right, &bottom);
    }

    /** Sets the window geometry in OS coordinates (if popped out) */
    void SetWindowGeometryOS(int left, int top, int right, int bottom)
    {
        XPLMSetWindowGeometryOS(mWindowID, left, top, right, bottom);
    }

    /** Gets the current window geometry for VR mode */
    void GetWindowGeometryVR(int& width, int& height) const
    {
        XPLMGetWindowGeometryVR(mWindowID, &width, &height);
    }

    /** Sets the window geometry for VR mode */
    void SetWindowGeometryVR(int width, int height)
    {
        XPLMSetWindowGeometryVR(mWindowID, width, height);
    }

    /** Gets the current window geometry based on current mode */
    void GetCurrentWindowGeometry(int& left, int& top, int& right, int& bottom) const;

    /** Sets the window resizing limits */
    void SetWindowResizingLimits(int minW, int minH, int maxW, int maxH);

    void SetWindowTitle(const std::string& title);

    /** Set the visibility of the window.  Windows are created initially hidden.
     *
     * @param inIsVisible true if the window should be visible, false to hide it.
     */
    void SetVisible(bool inIsVisible);

    /** Get the visibility of the window.
     *
     * @return true if the window is visible.
     */
    bool GetVisible() const;

    /** Check if the window is in VR mode */
    bool IsInVR() const
    {
        return XPLMWindowIsInVR(mWindowID) != 0;
    }

    /** Check if the window is popped out */
    bool IsPoppedOut() const
    {
        return XPLMWindowIsPoppedOut(mWindowID) != 0;
    }

    /** Check if the window is inside the simulator */
    bool IsInsideSim() const
    {
        return !IsPoppedOut() && !IsInVR();
    }

    /** Safe deletion - delays actual destruction until next frame */
    void SafeDelete();

protected:
    ImgWindow(
        int left,
        int top,
        int right,
        int bottom,
        XPLMWindowDecoration decoration = xplm_WindowDecorationRoundRectangle,
        XPLMWindowLayer layer = xplm_WindowLayerFloatingWindows);

    /** Allow the ImgWindow to render ImGUI controls.
     *
     * This should be overridden by the subclass to render the actual content.
     */
    virtual void buildInterface() = 0;

    /** Allow the ImgWindow to do something after rendering is finished
     *
     * This is called after all rendering is done, so it's a good place to
     * do things that might affect the window state.
     */
    virtual void afterRendering() {}

    /** Overrideable hook - allows early abortion of SetVisible() attempts. */
    virtual bool onShow() { return true; }

    /** Allows returning additional flags for the root window in dear imgui. */
    virtual ImGuiWindowFlags_ beforeBegin() { return ImGuiWindowFlags_None; }

    /** Update ImGUI and build the interface */
    void updateImgui();

    /** Define an area which allows the window to be dragged around by mouse */
    void SetWindowDragArea(int left, int top, int right, int bottom);

    /** Clear any previously defined window drag area */
    void ClearWindowDragArea();

    /** Check if a window drag area is defined */
    bool HasWindowDragArea(int* pL = nullptr, int* pT = nullptr,
        int* pR = nullptr, int* pB = nullptr) const;

    /** Check if given position is inside the window drag area */
    bool IsInsideWindowDragArea(int x, int y) const;

protected:
    XPLMWindowID        mWindowID;

private:
    bool                mFirstRender;
    std::string         mWindowTitle;
    ImGuiContext*       mImGuiContext;
    std::shared_ptr<ImgFontAtlas> mFontAtlas;
    GLuint              mFontTexture;
    XPLMWindowLayer     mPreferredLayer;

    // Window dimensions (updated each frame)
    int mLeft, mTop, mRight, mBottom;

    // Font texture management
    // Note: mTextureBound removed as it's not used in current implementation

    // Window sizing constraints
    int minWidth = 50, minHeight = 50;
    int maxWidth = 2000, maxHeight = 2000;

    // Drag area definition (-1 = not defined)
    int dragLeft = -1, dragTop = -1, dragRight = -1, dragBottom = -1;

    // Window resizing support
    bool bHandleWndResize;

    // Mouse drag tracking
    struct DragWhat {
        bool wnd = false;       // drag entire window
        bool left = false;      // resize left edge
        bool top = false;       // resize top edge
        bool right = false;     // resize right edge
        bool bottom = false;    // resize bottom edge

        void clear() { wnd = left = top = right = bottom = false; }
        operator bool() const { return wnd || left || top || right || bottom; }
    } dragWhat;

    int lastMouseDragX = -1, lastMouseDragY = -1;

    // Key input handling
    struct KeyPress {
        char inKey;
        XPLMKeyFlags inFlags;
        char inVirtualKey;
    };
    std::queue<KeyPress> mKeyQueue;

    // Matrix calculations for coordinate transformation
    GLfloat mModelView[16], mProjection[16];
    GLint mViewport[4];

    void updateMatrices();
    void boxelsToNative(int x, int y, int& outX, int& outY);

    // Coordinate transformation functions
    void translateToImguiSpace(int inX, int inY, float& outX, float& outY);
    void translateImguiToBoxel(float inX, float inY, int& outX, int& outY);

    // ImGui rendering
    void RenderImGui(ImDrawData* draw_data);

    // VR support
    void moveForVR();

    // X-Plane callbacks
    static void DrawWindowCB(XPLMWindowID inWindowID, void* inRefcon);
    static int HandleMouseClickCB(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse, void* inRefcon);
    static void HandleKeyFuncCB(XPLMWindowID inWindowID, char inKey, XPLMKeyFlags inFlags, char inVirtualKey, void* inRefcon, int losingFocus);
    static XPLMCursorStatus HandleCursorFuncCB(XPLMWindowID inWindowID, int x, int y, void* inRefcon);
    static int HandleMouseWheelFuncCB(XPLMWindowID inWindowID, int x, int y, int wheel, int clicks, void* inRefcon);
    static int HandleRightClickFuncCB(XPLMWindowID inWindowID, int x, int y, XPLMMouseStatus inMouse, void* inRefcon);

    // Generic mouse handling
    int HandleMouseClickGeneric(int x, int y, XPLMMouseStatus inMouse, int button);

    // Safe deletion support
    static std::queue<ImgWindow*> sPendingDestruction;
    static XPLMFlightLoopID sSelfDestructHandler;
    static float SelfDestructCallback(float inElapsedSinceLastCall,
        float inElapsedTimeSinceLastFlightLoop,
        int inCounter,
        void* inRefcon);

    // Thread safety
    bool IsXPThread() const { return true; }  // Simplified - assume always on XP thread
    void ThisThreadIsXP() const {}             // Simplified
};

#endif //IMGWINDOW_H
