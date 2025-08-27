#pragma once

#include "XPLMDisplay.h"
#include "XPLMDataAccess.h"
#include <vector>
#include <cstdint>

// Forward declarations
struct FlightData;
struct TrafficTarget;

class ImGuiManager {
public:
    static ImGuiManager& Instance();
    
    // Lifecycle
    bool Initialize();
    void Shutdown();
    
    // Window management
    void ShowConfigWindow();
    void HideConfigWindow();
    bool IsConfigWindowVisible() const { return m_initialized && m_showConfig; }
    
    void ShowStatusWindow();
    void HideStatusWindow();
    bool IsStatusWindowVisible() const { return m_initialized && m_showStatus; }
    
    // Data updates
    void UpdateFlightData(const FlightData& data);
    void UpdateTrafficTargets(const std::vector<TrafficTarget>& targets);
    void UpdateConnectionStatus(bool connected, const char* ip, int port);
    void UpdateStatistics(int messagesSent, int trafficCount);
    
    // Callbacks for configuration changes
    void SetConfigCallback(void (*callback)(const char* ip, int port, bool broadcast, bool traffic));
    
    // X-Plane integration
    void HandleDraw();
    int HandleMouseClick(XPLMWindowID window, int x, int y, XPLMMouseStatus mouseStatus, void* refcon);
    void HandleKeyPress(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon, int losingFocus);
    int HandleCursor(XPLMWindowID window, int x, int y, void* refcon);
    int HandleMouseWheel(XPLMWindowID window, int x, int y, int wheel, int clicks, void* refcon);
    
private:
    ImGuiManager() = default;
    ~ImGuiManager() = default;
    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;
    
    // Window state
    bool m_initialized = false;
    bool m_showConfig = false;
    bool m_showStatus = false;
    
    // Window handles
    XPLMWindowID m_windowID = nullptr;
    
    // Configuration
    char m_configIP[16] = "127.0.0.1";
    int m_configPort = 4000;
    bool m_configBroadcast = true;
    bool m_configTraffic = true;
    
    // Status data
    FlightData* m_flightData = nullptr;
    std::vector<TrafficTarget>* m_trafficTargets = nullptr;
    bool m_connected = false;
    int m_messagesSent = 0;
    int m_trafficCount = 0;
    
    // Callback
    void (*m_configCallback)(const char* ip, int port, bool broadcast, bool traffic) = nullptr;
    
    // UI Methods
    void DrawConfigWindow();
    void DrawStatusWindow();
    void DrawTrafficTable();
    void DrawConnectionStatus();
    void DrawStatistics();
    
    // Helper methods
    void ApplyConfiguration();
    void ResetConfiguration();
    
    // X-Plane window creation
    bool CreateXPlaneWindow();
    void DestroyXPlaneWindow();
};
