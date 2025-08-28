#include "ImGuiManager.h"
#include "ConfigWindow.h"
#include "StatusWindow.h"
#include "../imgwindow/img_font_atlas.h"
#include "XPLMUtilities.h"
#include <cmath>
#include <stdexcept>
#include <string>

// Main plugin structures (must match definitions in xp2gdl90.cpp)
struct FlightData {
    double lat;       // Latitude (degrees)
    double lon;       // Longitude (degrees) 
    double alt;       // Altitude MSL (feet)
    double speed;     // Ground speed (knots)
    double track;     // Track/heading (degrees)
    double vs;        // Vertical speed (feet/minute)
    double pitch;     // Pitch (degrees)
    double roll;      // Roll (degrees)
};

struct TrafficTarget {
    int plane_id;
    uint32_t icao_address;
    FlightData data;
    double last_update;
    bool active;
    char callsign[9];  // 8 chars + null terminator
};

ImGuiManager& ImGuiManager::Instance() {
    static ImGuiManager instance;
    return instance;
}

bool ImGuiManager::Initialize() {
    if (m_initialized) {
        XPLMDebugString("XP2GDL90: ImGuiManager already initialized\n");
        return true;
    }

    XPLMDebugString("XP2GDL90: Initializing ImGuiManager with new architecture\n");

    try {
        // Create shared font atlas if needed
        if (!ImgWindow::sFontAtlas) {
            ImgWindow::sFontAtlas = std::make_shared<ImgFontAtlas>();
        }

        // Create window instances
        m_configWindow = std::make_unique<ConfigWindow>();
        m_statusWindow = std::make_unique<StatusWindow>();

        m_initialized = true;
        XPLMDebugString("XP2GDL90: ImGuiManager initialized successfully\n");
        return true;
    }
    catch (const std::exception& e) {
        XPLMDebugString(("XP2GDL90: Failed to initialize ImGuiManager: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
    catch (...) {
        XPLMDebugString("XP2GDL90: Failed to initialize ImGuiManager: Unknown error\n");
        return false;
    }
}

void ImGuiManager::Shutdown() {
    if (!m_initialized) {
        return;
    }

    XPLMDebugString("XP2GDL90: Shutting down ImGuiManager\n");

    // Destroy windows in reverse order
    m_statusWindow.reset();
    m_configWindow.reset();

    // Reset state
    m_initialized = false;
    m_flightData = nullptr;
    m_trafficTargets = nullptr;
    m_connected = false;
    m_connectionAddress.clear();
    m_configCallback = nullptr;

    XPLMDebugString("XP2GDL90: ImGuiManager shutdown complete\n");
}

void ImGuiManager::ShowConfigWindow() {
    if (!m_initialized || !m_configWindow) {
        XPLMDebugString("XP2GDL90: Cannot show config window - manager not initialized\n");
        return;
    }

    m_configWindow->SetVisible(true);
    XPLMDebugString("XP2GDL90: Config window shown\n");
}

void ImGuiManager::HideConfigWindow() {
    if (!m_initialized || !m_configWindow) {
        return;
    }

    m_configWindow->SetVisible(false);
    XPLMDebugString("XP2GDL90: Config window hidden\n");
}

bool ImGuiManager::IsConfigWindowVisible() const {
    return m_initialized && m_configWindow && m_configWindow->GetVisible();
}

void ImGuiManager::ShowStatusWindow() {
    if (!m_initialized || !m_statusWindow) {
        XPLMDebugString("XP2GDL90: Cannot show status window - manager not initialized\n");
        return;
    }

    m_statusWindow->SetVisible(true);
    XPLMDebugString("XP2GDL90: Status window shown\n");
}

void ImGuiManager::HideStatusWindow() {
    if (!m_initialized || !m_statusWindow) {
        return;
    }

    m_statusWindow->SetVisible(false);
    XPLMDebugString("XP2GDL90: Status window hidden\n");
}

bool ImGuiManager::IsStatusWindowVisible() const {
    return m_initialized && m_statusWindow && m_statusWindow->GetVisible();
}

void ImGuiManager::UpdateFlightData(const FlightData& data) {
    if (!m_initialized || !m_statusWindow) {
        return;
    }

    // Convert from plugin FlightData to UI FlightData
    UIFlightData uiData;
    uiData.latitude = static_cast<float>(data.lat);
    uiData.longitude = static_cast<float>(data.lon);
    uiData.altitude = static_cast<float>(data.alt);
    uiData.groundSpeed = static_cast<float>(data.speed);
    uiData.verticalSpeed = static_cast<float>(data.vs);
    uiData.heading = static_cast<float>(data.track);
    uiData.isValid = true;

    // Store for potential future use
    static UIFlightData s_flightData;
    s_flightData = uiData;
    m_flightData = &s_flightData;

    // Update status window
    m_statusWindow->UpdateFlightData(&s_flightData);
}

void ImGuiManager::UpdateTrafficTargets(const std::vector<TrafficTarget>& targets) {
    if (!m_initialized || !m_statusWindow) {
        return;
    }

    // Convert from plugin TrafficTarget to UI TrafficTarget
    static std::vector<UITrafficTarget> s_uiTargets;
    s_uiTargets.clear();

    for (const auto& target : targets) {
        if (!target.active) continue;

        UITrafficTarget uiTarget;
        uiTarget.callsign = std::string(target.callsign);
        uiTarget.latitude = static_cast<float>(target.data.lat);
        uiTarget.longitude = static_cast<float>(target.data.lon);
        uiTarget.altitude = static_cast<float>(target.data.alt);
        
        // Calculate distance and bearing (simplified - you may want to implement proper great circle calculations)
        if (m_flightData && m_flightData->isValid) {
            float dlat = uiTarget.latitude - m_flightData->latitude;
            float dlon = uiTarget.longitude - m_flightData->longitude;
            uiTarget.distance = std::sqrt(dlat * dlat + dlon * dlon) * 60.0f; // Rough nautical miles
            uiTarget.bearing = std::atan2(dlon, dlat) * 180.0f / 3.14159f;
            if (uiTarget.bearing < 0) uiTarget.bearing += 360.0f;
        }
        
        uiTarget.isValid = true;
        s_uiTargets.push_back(uiTarget);
    }

    m_trafficTargets = &s_uiTargets;
    m_statusWindow->UpdateTrafficTargets(&s_uiTargets);
}

void ImGuiManager::UpdateConnectionStatus(bool connected, const char* ip, int port) {
    if (!m_initialized || !m_statusWindow) {
        return;
    }

    m_connected = connected;
    if (ip && port > 0) {
        m_connectionAddress = std::string(ip) + ":" + std::to_string(port);
    } else {
        m_connectionAddress.clear();
    }

    m_statusWindow->SetConnectionStatus(connected, m_connectionAddress);
}

void ImGuiManager::SetConfigCallback(void (*callback)(const char* ip, int port, bool broadcast, bool traffic)) {
    m_configCallback = callback;
}

NetworkConfig ImGuiManager::GetCurrentConfig() const {
    if (!m_initialized || !m_configWindow) {
        NetworkConfig defaultConfig;
        return defaultConfig;
    }

    return m_configWindow->GetConfig();
}

void ImGuiManager::ApplyConfigFromWindow() {
    if (!m_initialized || !m_configWindow || !m_configCallback) {
        return;
    }

    NetworkConfig config = m_configWindow->GetConfig();
    
    XPLMDebugString(("XP2GDL90: Applying configuration: " + 
                    std::string(config.ip) + ":" + std::to_string(config.port) + "\n").c_str());
    
    m_configCallback(config.ip, config.port, config.enableBroadcast, config.enableTraffic);
}