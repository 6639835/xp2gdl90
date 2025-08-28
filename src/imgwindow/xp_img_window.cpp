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

#include "xp_img_window.h"
#include "XPLMUtilities.h"

XPImgWindow::XPImgWindow(WndMode _mode, WndStyle _style, WndRect _initPos) :
    ImgWindow(_initPos.left(), _initPos.top(), _initPos.right(), _initPos.bottom(),
              toDeco(_style), toLayer(_style)),
    wndStyle(_style),
    mIsVREnabled("sim/graphics/VR/enabled")
{
    // Set initial position
    rectFloat = _initPos;
    
    // Set up flight loop for mode changes
    XPLMCreateFlightLoop_t flParams{
        sizeof(flParams),
        xplm_FlightLoop_Phase_BeforeFlightModel,
        &XPImgWindow::cbChangeWndMode,
        this,
    };
    flChangeWndMode = XPLMCreateFlightLoop(&flParams);
    
    // Set initial mode
    SetMode(_mode);
}

XPImgWindow::~XPImgWindow()
{
    if (flChangeWndMode) {
        XPLMDestroyFlightLoop(flChangeWndMode);
        flChangeWndMode = nullptr;
    }
}

void XPImgWindow::SetMode(WndMode _mode)
{
    nextWinMode = _mode;
    ScheduleWndModeChange();
}

WndMode XPImgWindow::GetMode() const
{
    if (IsPoppedOut()) return WND_MODE_POPOUT;
    if (IsInVR()) return WND_MODE_VR;
    return WND_MODE_FLOAT;
}

WndRect XPImgWindow::GetCurrentWindowGeometry() const
{
    int left, top, right, bottom;
    ImgWindow::GetCurrentWindowGeometry(left, top, right, bottom);
    return WndRect(left, top, right, bottom);
}

bool XPImgWindow::ReturnKeyboardFocus()
{
    if (XPLMHasKeyboardFocus(nullptr)) {  // Check if we have focus
        XPLMTakeKeyboardFocus(nullptr);   // Return focus to X-Plane
        return true;
    }
    return false;
}

float XPImgWindow::cbChangeWndMode(
    float                /*inElapsedSinceLastCall*/,
    float                /*inElapsedTimeSinceLastFlightLoop*/,
    int                  /*inCounter*/,
    void*                inRefcon)
{
    XPImgWindow* me = static_cast<XPImgWindow*>(inRefcon);
    
    if (me->nextWinMode == WND_MODE_NONE)
        return 0.0f;
    
    WndMode mode = me->nextWinMode;
    me->nextWinMode = WND_MODE_NONE;
    
    switch (mode) {
    case WND_MODE_CLOSE:
        me->SetVisible(false);
        break;
        
    case WND_MODE_FLOAT:
        if (me->GetMode() != WND_MODE_FLOAT) {
            XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowPositionFree, -1);
            // Restore saved position if we have one
            if (!me->rectFloat.empty()) {
                me->SetWindowGeometry(me->rectFloat.left(), me->rectFloat.top(),
                                    me->rectFloat.right(), me->rectFloat.bottom());
            }
        }
        break;
        
    case WND_MODE_POPOUT:
        // Save current position if we're floating
        if (me->GetMode() == WND_MODE_FLOAT) {
            me->rectFloat = me->GetCurrentWindowGeometry();
        }
        XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowPopOut, -1);
        break;
        
    case WND_MODE_VR:
        // Save current position if we're floating
        if (me->GetMode() == WND_MODE_FLOAT) {
            me->rectFloat = me->GetCurrentWindowGeometry();
        }
        XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowVR, -1);
        break;
        
    case WND_MODE_FLOAT_CENTERED:
        XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowCenterOnMonitor, -1);
        break;
        
    case WND_MODE_FLOAT_OR_VR:
        if (me->IsVREnabled()) {
            XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowVR, -1);
        } else {
            XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowPositionFree, -1);
        }
        break;
        
    case WND_MODE_FLOAT_CNT_VR:
        if (me->IsVREnabled()) {
            XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowVR, -1);
        } else {
            XPLMSetWindowPositioningMode(me->mWindowID, xplm_WindowCenterOnMonitor, -1);
        }
        break;
        
    default:
        // Unknown mode, do nothing
        break;
    }
    
    return 0.0f;  // Don't reschedule
}
