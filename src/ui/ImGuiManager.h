#pragma once

#include <vector>
#include <memory>
#include <string>

// Forward declarations for UI structures
class ConfigWindow;
class StatusWindow;
struct NetworkConfig;
struct UIFlightData;
struct UITrafficTarget;

// Forward declarations for main plugin structures
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
    bool IsConfigWindowVisible() const;
    
    void ShowStatusWindow();
    void HideStatusWindow();
    bool IsStatusWindowVisible() const;
    
    // Data updates
    void UpdateFlightData(const FlightData& data);
    void UpdateTrafficTargets(const std::vector<TrafficTarget>& targets);
    void UpdateConnectionStatus(bool connected, const char* ip, int port);
    
    // Configuration management
    void SetConfigCallback(void (*callback)(const char* ip, int port, bool broadcast, bool traffic));
    NetworkConfig GetCurrentConfig() const;
    void ApplyConfigFromWindow();
    
private:
    ImGuiManager() = default;
    ~ImGuiManager() = default;
    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;
    
    // Initialization state
    bool m_initialized = false;
    
    // Window instances
    std::unique_ptr<ConfigWindow> m_configWindow;
    std::unique_ptr<StatusWindow> m_statusWindow;
    
    // Status data (kept for updates)
    UIFlightData* m_flightData = nullptr;
    const std::vector<UITrafficTarget>* m_trafficTargets = nullptr;
    bool m_connected = false;
    std::string m_connectionAddress;
    
    // Configuration callback
    void (*m_configCallback)(const char* ip, int port, bool broadcast, bool traffic) = nullptr;
};
