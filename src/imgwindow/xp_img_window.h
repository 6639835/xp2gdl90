/*
 * Adapted from LiveTraffic's LTImgWindow and xPilot's XPImgWindow
 * (c) 2018-2020 Birger Hoppe
 * (c) 2019-2024 Justin Shannon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

#ifndef XPImgWindow_h
#define XPImgWindow_h

#include "img_window.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"

constexpr int WND_FONT_SIZE = 15;
constexpr ImU32 DEF_WND_BG_COL = IM_COL32(15, 15, 15, 240);

/// 2D window position
struct WndPos
{
    int x = 0;
    int y = 0;

    /// Shift both values
    void shift(int _dx, int _dy)
    {
        x += _dx; y += _dy;
    }

    /// Return a shifted copy
    WndPos shiftedBy(int _dx, int _dy) const
    {
        WndPos ret = *this; ret.shift(_dx, _dy); return ret;
    }
};

/// 2D rectangle
struct WndRect
{
    WndPos tl;          ///< top left
    WndPos br;          ///< bottom right

    /// Default Constructor -> all zero
    WndRect() {}
    /// Constructor takes four ints as a convenience
    WndRect(int _l, int _t, int _r, int _b) :
        tl{ _l, _t }, br{ _r, _b } {}
    /// Constructor taking two positions
    WndRect(const WndPos& _tl, const WndPos& _br) :
        tl(_tl), br(_br) {}

    // Accessor to individual coordinates
    int     left() const { return tl.x; }    ///< reading left
    int&    left() { return tl.x; }    ///< writing left
    int     top() const { return tl.y; }    ///< reading top
    int&    top() { return tl.y; }    ///< writing top
    int     right() const { return br.x; }    ///< reading right
    int&    right() { return br.x; }    ///< writing right
    int     bottom() const { return br.y; }    ///< reading bottom
    int&    bottom() { return br.y; }    ///< writing bottom

    int     width() const { return right() - left(); }    ///< width
    int     height() const { return top() - bottom(); }    ///< height

    /// Does window contain the position?
    bool    contains(const WndPos& _p) const
    {
        return
            left() <= _p.x && _p.x <= right() &&
            bottom() <= _p.y && _p.y <= top();
    }

    // Clear all to zero
    void    clear() { tl.x = tl.y = br.x = br.y = 0; }
    bool    empty() const { return !tl.x && !tl.y && !br.x && !br.y; }

    /// Shift position right/down
    WndRect& shift(int _dx, int _dy)
    {
        tl.shift(_dx, _dy); br.shift(_dx, _dy); return *this;
    }
};

/// Mode the window is to open in / does currently operate in
enum WndMode
{
    WND_MODE_NONE = 0,      ///< unknown, not yet set mode
    WND_MODE_FLOAT,         ///< XP11 modern floating window
    WND_MODE_POPOUT,        ///< XP11 popped out window in "first class OS window"
    WND_MODE_VR,            ///< XP11 moved to VR window
    // temporary modes for init/set only:
    WND_MODE_FLOAT_OR_VR,   ///< VR if in VR-mode, otherwise float (initialization use only)
    WND_MODE_FLOAT_CENTERED,///< will be shown centered on main screen
    WND_MODE_FLOAT_CNT_VR,  ///< VR if in VR-mode, centered otherwise
    // temporary mode for closing the window
    WND_MODE_CLOSE,         ///< close the window
};

inline XPLMWindowPositioningMode toPosMode(WndMode _m, bool vrEnabled)
{
    switch (_m) {
    case WND_MODE_FLOAT:    return xplm_WindowPositionFree;
    case WND_MODE_POPOUT:   return xplm_WindowPopOut;
    case WND_MODE_VR:       return xplm_WindowVR;
    case WND_MODE_FLOAT_OR_VR:
        return vrEnabled ? xplm_WindowVR : xplm_WindowPositionFree;
    case WND_MODE_FLOAT_CENTERED:
        return xplm_WindowCenterOnMonitor;
    case WND_MODE_FLOAT_CNT_VR:
        return vrEnabled ? xplm_WindowVR : xplm_WindowCenterOnMonitor;
    default:
        return xplm_WindowPositionFree;
    }
}

enum WndStyle
{
    WND_STYLE_NONE = 0, ///< unknown, not yet set style
    WND_STYLE_SOLID,    ///< solid window like settings
    WND_STYLE_HUD,      ///< HUD-like window, transparent, lower layer in wnd-hierarchie
};

inline XPLMWindowDecoration toDeco(WndStyle _s)
{
    return _s == WND_STYLE_HUD ? xplm_WindowDecorationSelfDecorated : xplm_WindowDecorationRoundRectangle;
}

inline XPLMWindowLayer toLayer(WndStyle _s)
{
    return _s == WND_STYLE_HUD ? xplm_WindowLayerFlightOverlay : xplm_WindowLayerFloatingWindows;
}

class XPImgWindow : public ImgWindow
{
public:
    /// The style this window operates in
    const WndStyle wndStyle;
    /// Return whether VR is enabled or not
    bool IsVREnabled() const { return mIsVREnabled.get() != 0; }

protected:
    // Helpers for window mode changes, which should not happen during drawing,
    // so we delay them to a flight loop callback

    /// Note to myself that a change of window mode is requested
    WndMode nextWinMode = WND_MODE_NONE;
    // Our flight loop callback in case we need one for mode changes
    XPLMFlightLoopID flChangeWndMode = nullptr;
    // Last known in-sim position before moving out
    WndRect rectFloat;

public:
    /// Constructor sets up the window basically (no title, not visible yet)
    XPImgWindow(WndMode _mode, WndStyle _style, WndRect _initPos);
    /// Destructor cleans up
    ~XPImgWindow() override;

    /// Set the window mode, move the window if needed
    void SetMode(WndMode _mode);
    /// Get current window mode
    WndMode GetMode() const;

    /// Get current window geometry as an WndRect structure
    WndRect GetCurrentWindowGeometry() const;

    /// @brief Loose keyboard foucs, ie. return focus to X-Plane proper, if I have it now
    /// @return Actually returned focus to X-Plane?
    bool ReturnKeyboardFocus();

protected:
    /// Schedule the callback for window mode changes
    void ScheduleWndModeChange() { XPLMScheduleFlightLoop(flChangeWndMode, -1.0, 1); }

    static float cbChangeWndMode(
        float                inElapsedSinceLastCall,
        float                inElapsedTimeSinceLastFlightLoop,
        int                  inCounter,
        void* inRefcon);

private:
    class DataRefAccess {
    public:
        DataRefAccess(const char* dataref_name) : dataref(XPLMFindDataRef(dataref_name)) {}
        int get() const { return dataref ? XPLMGetDatai(dataref) : 0; }
    private:
        XPLMDataRef dataref;
    };

    DataRefAccess mIsVREnabled;
};

#endif // !XPImgWindow_h
