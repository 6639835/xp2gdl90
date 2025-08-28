/**
 * XP2GDL90 Test Mocks - Data Access Mock Implementation
 * 
 * Extended mock for X-Plane data access functions
 */

#include "XPLMMock.h"
#include <map>
#include <string>

namespace {
    // Common X-Plane datarefs with mock values for testing
    struct MockDataRef {
        std::string name;
        XPLMDataTypeID type;
        float defaultFloat;
        double defaultDouble;
        int defaultInt;
    };

    static const std::map<std::string, MockDataRef> commonDataRefs = {
        // Position
        {"sim/flightmodel/position/latitude", {"sim/flightmodel/position/latitude", xplmType_Double, 0.0f, 37.524, 0}},
        {"sim/flightmodel/position/longitude", {"sim/flightmodel/position/longitude", xplmType_Double, 0.0f, -122.063, 0}},
        {"sim/flightmodel/position/elevation", {"sim/flightmodel/position/elevation", xplmType_Double, 0.0f, 100.0, 0}},
        {"sim/flightmodel/position/y_agl", {"sim/flightmodel/position/y_agl", xplmType_Float, 0.0f, 0.0, 0}},
        
        // Velocity
        {"sim/flightmodel/position/groundspeed", {"sim/flightmodel/position/groundspeed", xplmType_Float, 0.0f, 0.0, 0}},
        {"sim/flightmodel/position/vh_ind_fpm", {"sim/flightmodel/position/vh_ind_fpm", xplmType_Float, 0.0f, 0.0, 0}},
        {"sim/flightmodel/position/psi", {"sim/flightmodel/position/psi", xplmType_Float, 0.0f, 0.0, 0}},
        
        // Aircraft state
        {"sim/aircraft/gear/acf_gear_deploy", {"sim/aircraft/gear/acf_gear_deploy", xplmType_IntArray, 0.0f, 0.0, 0}},
        {"sim/flightmodel/failures/onground_any", {"sim/flightmodel/failures/onground_any", xplmType_Int, 0.0f, 0.0, 1}},
        
        // Engine
        {"sim/aircraft/engine/engn_running", {"sim/aircraft/engine/engn_running", xplmType_IntArray, 0.0f, 0.0, 0}},
        
        // Traffic/TCAS (arrays)
        {"sim/cockpit2/tcas/targets/position/lat", {"sim/cockpit2/tcas/targets/position/lat", xplmType_FloatArray, 0.0f, 0.0, 0}},
        {"sim/cockpit2/tcas/targets/position/lon", {"sim/cockpit2/tcas/targets/position/lon", xplmType_FloatArray, 0.0f, 0.0, 0}},
        {"sim/cockpit2/tcas/targets/position/ele", {"sim/cockpit2/tcas/targets/position/ele", xplmType_FloatArray, 0.0f, 0.0, 0}},
        {"sim/cockpit2/tcas/num_acf", {"sim/cockpit2/tcas/num_acf", xplmType_Int, 0.0f, 0.0, 0}},
        
        // Time
        {"sim/time/zulu_time_sec", {"sim/time/zulu_time_sec", xplmType_Float, 0.0f, 0.0, 0}},
    };

    std::string getDataRefNameFromHandle(XPLMDataRef handle) {
        size_t hash = reinterpret_cast<size_t>(handle);
        
        // Reverse lookup for common datarefs
        for (const auto& pair : commonDataRefs) {
            if (std::hash<std::string>{}(pair.first) == hash) {
                return pair.first;
            }
        }
        
        return "";
    }
}

extern "C" {

// Enhanced dataref access with better mock support
float XPLMGetDataf(XPLMDataRef inDataRef) {
    std::string name = getDataRefNameFromHandle(inDataRef);
    
    if (!name.empty()) {
        auto it = commonDataRefs.find(name);
        if (it != commonDataRefs.end()) {
            // Check if mock state has override value
            float mockValue = XPLMockState::getInstance().getFloatDataRefValue(name);
            if (mockValue != 0.0f) {
                return mockValue;
            }
            // Return default value
            return it->second.defaultFloat;
        }
    }
    
    return 0.0f;
}

double XPLMGetDatad(XPLMDataRef inDataRef) {
    std::string name = getDataRefNameFromHandle(inDataRef);
    
    if (!name.empty()) {
        auto it = commonDataRefs.find(name);
        if (it != commonDataRefs.end()) {
            // Check if mock state has override value
            double mockValue = XPLMockState::getInstance().getDoubleDataRefValue(name);
            if (mockValue != 0.0) {
                return mockValue;
            }
            // Return default value
            return it->second.defaultDouble;
        }
    }
    
    return 0.0;
}

int XPLMGetDatai(XPLMDataRef inDataRef) {
    std::string name = getDataRefNameFromHandle(inDataRef);
    
    if (!name.empty()) {
        auto it = commonDataRefs.find(name);
        if (it != commonDataRefs.end()) {
            // Check if mock state has override value
            int mockValue = XPLMockState::getInstance().getIntDataRefValue(name);
            if (mockValue != 0) {
                return mockValue;
            }
            // Return default value
            return it->second.defaultInt;
        }
    }
    
    return 0;
}

int XPLMGetDatavf(XPLMDataRef inDataRef, float* outValues, int inOffset, int inMax) {
    std::string name = getDataRefNameFromHandle(inDataRef);
    
    if (outValues && inMax > 0) {
        // Check for mock array data
        std::vector<float> mockData = XPLMockState::getInstance().getFloatArrayDataRefValue(name);
        
        if (!mockData.empty()) {
            int copyCount = std::min(inMax, static_cast<int>(mockData.size()) - inOffset);
            for (int i = 0; i < copyCount; ++i) {
                if (inOffset + i < static_cast<int>(mockData.size())) {
                    outValues[i] = mockData[inOffset + i];
                } else {
                    outValues[i] = 0.0f;
                }
            }
            return copyCount;
        } else {
            // Return default array values for testing
            for (int i = 0; i < inMax; ++i) {
                outValues[i] = 0.0f;
            }
            return inMax;
        }
    }
    
    return 0;
}

int XPLMGetDatavi(XPLMDataRef inDataRef, int* outValues, int inOffset, int inMax) {
    std::string name = getDataRefNameFromHandle(inDataRef);
    
    if (outValues && inMax > 0) {
        // Check for mock array data
        std::vector<int> mockData = XPLMockState::getInstance().getIntArrayDataRefValue(name);
        
        if (!mockData.empty()) {
            int copyCount = std::min(inMax, static_cast<int>(mockData.size()) - inOffset);
            for (int i = 0; i < copyCount; ++i) {
                if (inOffset + i < static_cast<int>(mockData.size())) {
                    outValues[i] = mockData[inOffset + i];
                } else {
                    outValues[i] = 0;
                }
            }
            return copyCount;
        } else {
            // Return default array values
            for (int i = 0; i < inMax; ++i) {
                outValues[i] = 0;
            }
            return inMax;
        }
    }
    
    return 0;
}

XPLMDataTypeID XPLMGetDataRefTypes(XPLMDataRef inDataRef) {
    std::string name = getDataRefNameFromHandle(inDataRef);
    
    if (!name.empty()) {
        auto it = commonDataRefs.find(name);
        if (it != commonDataRefs.end()) {
            return it->second.type;
        }
    }
    
    // Default to supporting all types
    return xplmType_Float | xplmType_Double | xplmType_Int | xplmType_FloatArray | xplmType_IntArray;
}

} // extern "C"
