/**
 * XP2GDL90 Test Mocks - Processing Mock Implementation
 * 
 * Mock for X-Plane processing and timing functions
 */

#include "XPLMMock.h"
#include <chrono>

namespace {
    // Mock timing state
    static auto startTime = std::chrono::steady_clock::now();
    static int cycleCounter = 0;
}

extern "C" {

float XPLMGetElapsedTime(void) {
    // Return mock simulation time from state
    return static_cast<float>(XPLMockState::getInstance().getSimulationTime());
}

int XPLMGetCycleNumber(void) {
    return ++cycleCounter;
}

void XPLMRegisterFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, float inInterval, void* inRefcon) {
    XPLMockState::getInstance().registerFlightLoop(inFlightLoop, inInterval, inRefcon);
}

void XPLMUnregisterFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, void* inRefcon) {
    XPLMockState::getInstance().unregisterFlightLoop(inFlightLoop, inRefcon);
}

void XPLMSetFlightLoopCallbackInterval(XPLMFlightLoop_f inFlightLoop, 
                                      float inInterval, 
                                      int inRelativeToNow, 
                                      void* inRefcon) {
    // For simplicity in mock, just re-register with new interval
    (void)inRelativeToNow;
    XPLMockState::getInstance().unregisterFlightLoop(inFlightLoop, inRefcon);
    XPLMockState::getInstance().registerFlightLoop(inFlightLoop, inInterval, inRefcon);
}

// Additional processing functions that might be used
void XPLMScheduleFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, 
                                   float inInterval, 
                                   int inRelativeToNow, 
                                   void* inRefcon) {
    (void)inRelativeToNow;
    XPLMockState::getInstance().registerFlightLoop(inFlightLoop, inInterval, inRefcon);
}

} // extern "C"
