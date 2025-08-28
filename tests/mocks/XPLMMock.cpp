/**
 * XP2GDL90 Test Mocks - X-Plane SDK Mock Implementation
 * 
 * Mock implementations of X-Plane SDK functions for testing
 */

#include "XPLMMock.h"
#include <cstring>
#include <iostream>
#include <algorithm>

// Global mock state instance
XPLMockState& XPLMockState::getInstance() {
    static XPLMockState instance;
    return instance;
}

// Mock state management
void XPLMockState::setDataRefValue(const std::string& name, float value) {
    DataRefValue& ref = dataRefs_[name];
    ref.type = xplmType_Float;
    ref.floatVal = value;
}

void XPLMockState::setDataRefValue(const std::string& name, double value) {
    DataRefValue& ref = dataRefs_[name];
    ref.type = xplmType_Double;
    ref.doubleVal = value;
}

void XPLMockState::setDataRefValue(const std::string& name, int value) {
    DataRefValue& ref = dataRefs_[name];
    ref.type = xplmType_Int;
    ref.intVal = value;
}

void XPLMockState::setDataRefValue(const std::string& name, const std::vector<float>& values) {
    DataRefValue& ref = dataRefs_[name];
    ref.type = xplmType_FloatArray;
    ref.floatArray = values;
}

void XPLMockState::setDataRefValue(const std::string& name, const std::vector<int>& values) {
    DataRefValue& ref = dataRefs_[name];
    ref.type = xplmType_IntArray;
    ref.intArray = values;
}

float XPLMockState::getFloatDataRefValue(const std::string& name) {
    auto it = dataRefs_.find(name);
    if (it != dataRefs_.end() && it->second.type == xplmType_Float) {
        return it->second.floatVal;
    }
    return 0.0f;
}

double XPLMockState::getDoubleDataRefValue(const std::string& name) {
    auto it = dataRefs_.find(name);
    if (it != dataRefs_.end() && it->second.type == xplmType_Double) {
        return it->second.doubleVal;
    }
    return 0.0;
}

int XPLMockState::getIntDataRefValue(const std::string& name) {
    auto it = dataRefs_.find(name);
    if (it != dataRefs_.end() && it->second.type == xplmType_Int) {
        return it->second.intVal;
    }
    return 0;
}

std::vector<float> XPLMockState::getFloatArrayDataRefValue(const std::string& name) {
    auto it = dataRefs_.find(name);
    if (it != dataRefs_.end() && it->second.type == xplmType_FloatArray) {
        return it->second.floatArray;
    }
    return std::vector<float>();
}

std::vector<int> XPLMockState::getIntArrayDataRefValue(const std::string& name) {
    auto it = dataRefs_.find(name);
    if (it != dataRefs_.end() && it->second.type == xplmType_IntArray) {
        return it->second.intArray;
    }
    return std::vector<int>();
}

void XPLMockState::registerFlightLoop(XPLMFlightLoop_f callback, float interval, void* refcon) {
    FlightLoopInfo info;
    info.callback = callback;
    info.interval = interval;
    info.lastCall = 0.0f;
    info.refcon = refcon;
    flightLoops_.push_back(info);
}

void XPLMockState::unregisterFlightLoop(XPLMFlightLoop_f callback, void* refcon) {
    flightLoops_.erase(
        std::remove_if(flightLoops_.begin(), flightLoops_.end(),
            [callback, refcon](const FlightLoopInfo& info) {
                return info.callback == callback && info.refcon == refcon;
            }),
        flightLoops_.end()
    );
}

void XPLMockState::executeFlightLoops(float elapsedTime) {
    simulationTime_ += elapsedTime;
    
    for (auto& loop : flightLoops_) {
        loop.lastCall += elapsedTime;
        if (loop.lastCall >= loop.interval) {
            float nextCall = loop.callback(elapsedTime, loop.lastCall, 0, loop.refcon);
            loop.interval = nextCall > 0 ? nextCall : loop.interval;
            loop.lastCall = 0.0f;
        }
    }
}

void XPLMockState::addDebugString(const std::string& message) {
    debugStrings_.push_back(message);
}

std::vector<std::string> XPLMockState::getDebugStrings() const {
    return debugStrings_;
}

void XPLMockState::clearDebugStrings() {
    debugStrings_.clear();
}

void XPLMockState::reset() {
    dataRefs_.clear();
    flightLoops_.clear();
    debugStrings_.clear();
    simulationTime_ = 0.0;
}

void XPLMockState::setSimulationTime(double time) {
    simulationTime_ = time;
}

double XPLMockState::getSimulationTime() const {
    return simulationTime_;
}

// C interface implementation using static cast for DataRef handles
extern "C" {

XPLMDataRef XPLMFindDataRef(const char* inDataRefName) {
    if (!inDataRefName) return nullptr;
    
    // Create a simple hash of the string to use as handle
    // This is a mock, so we don't need sophisticated mapping
    size_t hash = std::hash<std::string>{}(inDataRefName);
    return reinterpret_cast<XPLMDataRef>(hash);
}

XPLMDataTypeID XPLMGetDataRefTypes(XPLMDataRef inDataRef) {
    // For mock purposes, we'll assume all datarefs support float type
    return xplmType_Float | xplmType_Double | xplmType_Int;
}

float XPLMGetDataf(XPLMDataRef inDataRef) {
    // Convert handle back to name (simplified for mock)
    size_t hash = reinterpret_cast<size_t>(inDataRef);
    
    // For testing, we'll use common X-Plane datarefs
    if (hash == std::hash<std::string>{}("sim/flightmodel/position/latitude")) {
        return XPLMockState::getInstance().getFloatDataRefValue("sim/flightmodel/position/latitude");
    } else if (hash == std::hash<std::string>{}("sim/flightmodel/position/longitude")) {
        return XPLMockState::getInstance().getFloatDataRefValue("sim/flightmodel/position/longitude");
    } else if (hash == std::hash<std::string>{}("sim/flightmodel/position/elevation")) {
        return XPLMockState::getInstance().getFloatDataRefValue("sim/flightmodel/position/elevation");
    }
    
    return 0.0f;
}

double XPLMGetDatad(XPLMDataRef inDataRef) {
    size_t hash = reinterpret_cast<size_t>(inDataRef);
    
    if (hash == std::hash<std::string>{}("sim/flightmodel/position/latitude")) {
        return static_cast<double>(XPLMockState::getInstance().getFloatDataRefValue("sim/flightmodel/position/latitude"));
    } else if (hash == std::hash<std::string>{}("sim/flightmodel/position/longitude")) {
        return static_cast<double>(XPLMockState::getInstance().getFloatDataRefValue("sim/flightmodel/position/longitude"));
    }
    
    return 0.0;
}

int XPLMGetDatai(XPLMDataRef inDataRef) {
    size_t hash = reinterpret_cast<size_t>(inDataRef);
    
    if (hash == std::hash<std::string>{}("sim/aircraft/engine/engn_running")) {
        return XPLMockState::getInstance().getIntDataRefValue("sim/aircraft/engine/engn_running");
    }
    
    return 0;
}

void XPLMSetDataf(XPLMDataRef inDataRef, float inValue) {
    // Mock implementation - in real X-Plane, not all datarefs are writable
    (void)inDataRef;
    (void)inValue;
}

void XPLMSetDatad(XPLMDataRef inDataRef, double inValue) {
    (void)inDataRef;
    (void)inValue;
}

void XPLMSetDatai(XPLMDataRef inDataRef, int inValue) {
    (void)inDataRef;
    (void)inValue;
}

int XPLMGetDatavf(XPLMDataRef inDataRef, float* outValues, int inOffset, int inMax) {
    (void)inDataRef;
    (void)inOffset;
    
    // Mock implementation for array datarefs
    if (outValues && inMax > 0) {
        for (int i = 0; i < inMax; ++i) {
            outValues[i] = 0.0f;
        }
        return inMax;
    }
    return 0;
}

int XPLMGetDatavi(XPLMDataRef inDataRef, int* outValues, int inOffset, int inMax) {
    (void)inDataRef;
    (void)inOffset;
    
    if (outValues && inMax > 0) {
        for (int i = 0; i < inMax; ++i) {
            outValues[i] = 0;
        }
        return inMax;
    }
    return 0;
}

void XPLMSetDatavf(XPLMDataRef inDataRef, float* inValues, int inOffset, int inCount) {
    (void)inDataRef;
    (void)inValues;
    (void)inOffset;
    (void)inCount;
}

void XPLMSetDatavi(XPLMDataRef inDataRef, int* inValues, int inOffset, int inCount) {
    (void)inDataRef;
    (void)inValues;
    (void)inOffset;
    (void)inCount;
}

int XPLMGetDatab(XPLMDataRef inDataRef, void* outValue, int inOffset, int inMaxBytes) {
    (void)inDataRef;
    (void)inOffset;
    
    if (outValue && inMaxBytes > 0) {
        memset(outValue, 0, inMaxBytes);
        return inMaxBytes;
    }
    return 0;
}

void XPLMSetDatab(XPLMDataRef inDataRef, void* inValue, int inOffset, int inLength) {
    (void)inDataRef;
    (void)inValue;
    (void)inOffset;
    (void)inLength;
}

void XPLMRegisterFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, float inInterval, void* inRefcon) {
    XPLMockState::getInstance().registerFlightLoop(inFlightLoop, inInterval, inRefcon);
}

void XPLMUnregisterFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, void* inRefcon) {
    XPLMockState::getInstance().unregisterFlightLoop(inFlightLoop, inRefcon);
}

void XPLMDebugString(const char* inString) {
    if (inString) {
        XPLMockState::getInstance().addDebugString(inString);
        // Also output to console for debugging
        std::cout << "XP2GDL90: " << inString;
    }
}

float XPLMGetElapsedTime(void) {
    return static_cast<float>(XPLMockState::getInstance().getSimulationTime());
}

int XPLMGetCycleNumber(void) {
    return static_cast<int>(XPLMockState::getInstance().getSimulationTime() * 100.0);
}

} // extern "C"
