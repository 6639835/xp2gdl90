#pragma once

#include "../imgwindow/xp_img_window.h"
#include <vector>
#include <string>

struct UIFlightData {
    float latitude = 0.0f;
    float longitude = 0.0f;
    float altitude = 0.0f;
    float groundSpeed = 0.0f;
    float verticalSpeed = 0.0f;
    float heading = 0.0f;
    bool isValid = false;
};

struct UITrafficTarget {
    std::string callsign;
    float latitude = 0.0f;
    float longitude = 0.0f;
    float altitude = 0.0f;
    float distance = 0.0f;
    float bearing = 0.0f;
    bool isValid = false;
};

class StatusWindow : public XPImgWindow {
public:
    StatusWindow();
    ~StatusWindow() override = default;

    /// Update flight data
    void UpdateFlightData(const UIFlightData* data);
    
    /// Update traffic targets
    void UpdateTrafficTargets(const std::vector<UITrafficTarget>* targets);
    
    /// Set connection status
    void SetConnectionStatus(bool connected, const std::string& address);

protected:
    /// Build the interface
    void buildInterface() override;

private:
    const UIFlightData* m_flightData = nullptr;
    const std::vector<UITrafficTarget>* m_trafficTargets = nullptr;
    
    // Connection status
    bool m_isConnected = false;
    std::string m_connectionAddress;
    
    // UI state
    bool m_showTrafficDetails = true;
    int m_selectedTraffic = -1;
    
    // Helper methods
    void DrawConnectionStatus();
    void DrawFlightData();
    void DrawTrafficData();
    void DrawTrafficTable();
    void DrawTrafficDetails();
    void DrawControls();
    
    // Utility functions
    std::string FormatLatLon(float value, bool isLongitude) const;
    std::string FormatAltitude(float altitude) const;
    std::string FormatSpeed(float speed) const;
    std::string FormatDistance(float distance) const;
    std::string FormatBearing(float bearing) const;
};
