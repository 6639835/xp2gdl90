#ifndef XPLM_MOCK_H
#define XPLM_MOCK_H

/**
 * XP2GDL90 Test Mocks - X-Plane SDK Mock Interface
 * 
 * Provides mock implementations of X-Plane SDK functions for unit testing
 * without requiring actual X-Plane installation.
 */

#include <string>
#include <map>
#include <functional>
#include <vector>

// Mock X-Plane SDK types and constants
typedef void* XPLMDataRef;
typedef int XPLMDataTypeID;
typedef float (*XPLMFlightLoop_f)(float inElapsedSinceLastCall,
                                  float inElapsedTimeSinceLastFlightLoop, 
                                  int inCounter,
                                  void* inRefcon);

// X-Plane data types
#define xplmType_Unknown    0
#define xplmType_Int        1
#define xplmType_Float      2
#define xplmType_Double     3
#define xplmType_FloatArray 4
#define xplmType_IntArray   5
#define xplmType_Data       6

// Flight loop phases
#define xplm_FlightLoop_Phase_BeforeFlightModel  0
#define xplm_FlightLoop_Phase_AfterFlightModel   1

// Mock class for managing X-Plane state during tests
class XPLMockState {
public:
    static XPLMockState& getInstance();
    
    // DataRef management
    void setDataRefValue(const std::string& name, float value);
    void setDataRefValue(const std::string& name, double value);
    void setDataRefValue(const std::string& name, int value);
    void setDataRefValue(const std::string& name, const std::vector<float>& values);
    void setDataRefValue(const std::string& name, const std::vector<int>& values);
    
    float getFloatDataRefValue(const std::string& name);
    double getDoubleDataRefValue(const std::string& name);
    int getIntDataRefValue(const std::string& name);
    std::vector<float> getFloatArrayDataRefValue(const std::string& name);
    std::vector<int> getIntArrayDataRefValue(const std::string& name);
    
    // Flight loop management
    void registerFlightLoop(XPLMFlightLoop_f callback, float interval, void* refcon);
    void unregisterFlightLoop(XPLMFlightLoop_f callback, void* refcon);
    void executeFlightLoops(float elapsedTime);
    
    // Debug string capture
    void addDebugString(const std::string& message);
    std::vector<std::string> getDebugStrings() const;
    void clearDebugStrings();
    
    // Test utilities
    void reset(); // Reset all mock state
    void setSimulationTime(double time);
    double getSimulationTime() const;
    
private:
    XPLMockState() : simulationTime_(0.0) {}
    
    struct DataRefValue {
        XPLMDataTypeID type;
        union {
            float floatVal;
            double doubleVal;
            int intVal;
        };
        std::vector<float> floatArray;
        std::vector<int> intArray;
        std::vector<char> dataArray;
    };
    
    struct FlightLoopInfo {
        XPLMFlightLoop_f callback;
        float interval;
        float lastCall;
        void* refcon;
    };
    
    std::map<std::string, DataRefValue> dataRefs_;
    std::vector<FlightLoopInfo> flightLoops_;
    std::vector<std::string> debugStrings_;
    double simulationTime_;
};

// Mock X-Plane SDK function declarations
#ifdef __cplusplus
extern "C" {
#endif

// DataRef functions
XPLMDataRef XPLMFindDataRef(const char* inDataRefName);
XPLMDataTypeID XPLMGetDataRefTypes(XPLMDataRef inDataRef);

float XPLMGetDataf(XPLMDataRef inDataRef);
double XPLMGetDatad(XPLMDataRef inDataRef);
int XPLMGetDatai(XPLMDataRef inDataRef);

void XPLMSetDataf(XPLMDataRef inDataRef, float inValue);
void XPLMSetDatad(XPLMDataRef inDataRef, double inValue);
void XPLMSetDatai(XPLMDataRef inDataRef, int inValue);

int XPLMGetDatavf(XPLMDataRef inDataRef, float* outValues, int inOffset, int inMax);
int XPLMGetDatavi(XPLMDataRef inDataRef, int* outValues, int inOffset, int inMax);

void XPLMSetDatavf(XPLMDataRef inDataRef, float* inValues, int inOffset, int inCount);
void XPLMSetDatavi(XPLMDataRef inDataRef, int* inValues, int inOffset, int inCount);

int XPLMGetDatab(XPLMDataRef inDataRef, void* outValue, int inOffset, int inMaxBytes);
void XPLMSetDatab(XPLMDataRef inDataRef, void* inValue, int inOffset, int inLength);

// Flight loop functions
void XPLMRegisterFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, 
                                   float inInterval,
                                   void* inRefcon);
void XPLMUnregisterFlightLoopCallback(XPLMFlightLoop_f inFlightLoop, void* inRefcon);

// Debug functions
void XPLMDebugString(const char* inString);

// Time functions
float XPLMGetElapsedTime(void);
int XPLMGetCycleNumber(void);

#ifdef __cplusplus
}
#endif

#endif // XPLM_MOCK_H
