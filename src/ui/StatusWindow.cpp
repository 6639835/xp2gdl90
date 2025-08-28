#include "StatusWindow.h"
#include "../imgui/imgui.h"
#include "XPLMUtilities.h"
#include <cstdio>
#include <cmath>

StatusWindow::StatusWindow() : 
    XPImgWindow(WND_MODE_FLOAT, WND_STYLE_HUD, WndRect(50, 700, 400, 300))
{
    SetWindowTitle("XP2GDL90 Status");
}

void StatusWindow::UpdateFlightData(const UIFlightData* data) {
    m_flightData = data;
}

void StatusWindow::UpdateTrafficTargets(const std::vector<UITrafficTarget>* targets) {
    m_trafficTargets = targets;
}

void StatusWindow::SetConnectionStatus(bool connected, const std::string& address) {
    m_isConnected = connected;
    m_connectionAddress = address;
}

void StatusWindow::buildInterface() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 180));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
    
    DrawConnectionStatus();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    DrawFlightData();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    DrawTrafficData();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    DrawControls();
    
    ImGui::PopStyleColor(2); // Text and WindowBg
}

void StatusWindow::DrawConnectionStatus() {
    ImGui::Text("GDL90 Output Status");
    
    ImGui::Text("Connection: ");
    ImGui::SameLine();
    
    if (m_isConnected) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
        ImGui::Text("ACTIVE");
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 100, 100, 255));
        ImGui::Text("INACTIVE");
    }
    ImGui::PopStyleColor();
    
    if (!m_connectionAddress.empty()) {
        ImGui::Text("Target: %s", m_connectionAddress.c_str());
    }
}

void StatusWindow::DrawFlightData() {
    ImGui::Text("Flight Data");
    
    if (!m_flightData || !m_flightData->isValid) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 100, 255));
        ImGui::Text("No flight data available");
        ImGui::PopStyleColor();
        return;
    }
    
    const auto& data = *m_flightData;
    
    // Position
    ImGui::Text("Position:");
    ImGui::Text("  Lat: %s", FormatLatLon(data.latitude, false).c_str());
    ImGui::Text("  Lon: %s", FormatLatLon(data.longitude, true).c_str());
    
    // Altitude and speeds
    ImGui::Text("Altitude: %s", FormatAltitude(data.altitude).c_str());
    ImGui::Text("Ground Speed: %s", FormatSpeed(data.groundSpeed).c_str());
    ImGui::Text("Vertical Speed: %.0f ft/min", data.verticalSpeed);
    ImGui::Text("Heading: %.0f°", data.heading);
}

void StatusWindow::DrawTrafficData() {
    ImGui::Text("Traffic Data");
    
    if (!m_trafficTargets || m_trafficTargets->empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 100, 255));
        ImGui::Text("No traffic detected");
        ImGui::PopStyleColor();
        return;
    }
    
    ImGui::Text("Targets: %zu", m_trafficTargets->size());
    
    ImGui::Checkbox("Show Details", &m_showTrafficDetails);
    
    if (m_showTrafficDetails) {
        DrawTrafficTable();
        if (m_selectedTraffic >= 0) {
            DrawTrafficDetails();
        }
    }
}

void StatusWindow::DrawTrafficTable() {
    if (!m_trafficTargets || m_trafficTargets->empty()) return;
    
    if (ImGui::BeginTable("TrafficTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Callsign", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Bearing", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Altitude", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableHeadersRow();
        
        for (size_t i = 0; i < m_trafficTargets->size() && i < 10; ++i) {
            const auto& target = (*m_trafficTargets)[i];
            
            ImGui::TableNextRow();
            
            // Callsign (selectable)
            ImGui::TableSetColumnIndex(0);
            bool isSelected = (m_selectedTraffic == static_cast<int>(i));
            if (ImGui::Selectable(target.callsign.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                m_selectedTraffic = isSelected ? -1 : static_cast<int>(i);
            }
            
            // Distance
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", FormatDistance(target.distance).c_str());
            
            // Bearing
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", FormatBearing(target.bearing).c_str());
            
            // Altitude
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", FormatAltitude(target.altitude).c_str());
        }
        
        ImGui::EndTable();
    }
}

void StatusWindow::DrawTrafficDetails() {
    if (m_selectedTraffic < 0 || !m_trafficTargets || 
        m_selectedTraffic >= static_cast<int>(m_trafficTargets->size())) return;
    
    const auto& target = (*m_trafficTargets)[m_selectedTraffic];
    
    ImGui::Spacing();
    ImGui::Text("Selected: %s", target.callsign.c_str());
    ImGui::Text("  Position: %s, %s", 
                FormatLatLon(target.latitude, false).c_str(),
                FormatLatLon(target.longitude, true).c_str());
    ImGui::Text("  Distance: %s", FormatDistance(target.distance).c_str());
    ImGui::Text("  Bearing: %s", FormatBearing(target.bearing).c_str());
    ImGui::Text("  Altitude: %s", FormatAltitude(target.altitude).c_str());
}

void StatusWindow::DrawControls() {
    float buttonWidth = 80.0f;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float startPos = (windowWidth - buttonWidth) * 0.5f;
    
    ImGui::SetCursorPosX(startPos);
    
    if (ImGui::Button("Close", ImVec2(buttonWidth, 0))) {
        SetVisible(false);
    }
}

std::string StatusWindow::FormatLatLon(float value, bool isLongitude) const {
    char buffer[32];
    char direction = 'N';
    
    if (isLongitude) {
        direction = (value >= 0) ? 'E' : 'W';
    } else {
        direction = (value >= 0) ? 'N' : 'S';
    }
    
    float absValue = std::abs(value);
    int degrees = static_cast<int>(absValue);
    float minutes = (absValue - degrees) * 60.0f;
    
    snprintf(buffer, sizeof(buffer), "%d°%.3f'%c", degrees, minutes, direction);
    return std::string(buffer);
}

std::string StatusWindow::FormatAltitude(float altitude) const {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f ft", altitude);
    return std::string(buffer);
}

std::string StatusWindow::FormatSpeed(float speed) const {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.1f kts", speed);
    return std::string(buffer);
}

std::string StatusWindow::FormatDistance(float distance) const {
    char buffer[32];
    if (distance < 1.0f) {
        snprintf(buffer, sizeof(buffer), "%.2f nm", distance);
    } else {
        snprintf(buffer, sizeof(buffer), "%.1f nm", distance);
    }
    return std::string(buffer);
}

std::string StatusWindow::FormatBearing(float bearing) const {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.0f°", bearing);
    return std::string(buffer);
}
