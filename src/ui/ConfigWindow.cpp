#include "ConfigWindow.h"
#include "../imgui/imgui.h"
#include "XPLMUtilities.h"
#include "ImGuiManager.h"
#include <cstdio>
#include <cstring>

ConfigWindow::ConfigWindow() : 
    XPImgWindow(WND_MODE_FLOAT_CENTERED, WND_STYLE_SOLID, WndRect(100, 600, 450, 300))
{
    SetWindowTitle("XP2GDL90 Configuration");
    
    // Initialize with defaults
    ResetToDefaults();
}

void ConfigWindow::SetConfig(const NetworkConfig& config) {
    m_config = config;
}

NetworkConfig ConfigWindow::GetConfig() const {
    return m_config;
}

void ConfigWindow::ResetToDefaults() {
    strncpy(m_config.ip, "127.0.0.1", sizeof(m_config.ip) - 1);
    m_config.ip[sizeof(m_config.ip) - 1] = '\0';
    m_config.port = 4000;
    m_config.enableBroadcast = true;
    m_config.enableTraffic = true;
}

void ConfigWindow::buildInterface() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, DEF_WND_BG_COL);
    
    ImGui::Spacing();
    ImGui::Text("Network Configuration");
    ImGui::Separator();
    ImGui::Spacing();
    
    DrawNetworkSettings();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    DrawButtons();
    
    // Show reset confirmation dialog if needed
    if (m_showResetConfirm) {
        DrawResetConfirmation();
    }
    
    ImGui::PopStyleColor(); // WindowBg
}

void ConfigWindow::DrawNetworkSettings() {
    ImGui::Text("GDL90 Output Settings:");
    ImGui::Spacing();
    
    // IP Address input
    ImGui::Text("IP Address:");
    ImGui::SameLine();
    ImGui::PushItemWidth(120);
    bool ipChanged = ImGui::InputText("##ip", m_config.ip, sizeof(m_config.ip));
    ImGui::PopItemWidth();
    
    if (ipChanged && !IsValidIP(m_config.ip)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid IP");
    }
    
    // Port input
    ImGui::Text("Port:");
    ImGui::SameLine();
    ImGui::PushItemWidth(80);
    bool portChanged = ImGui::InputInt("##port", &m_config.port);
    ImGui::PopItemWidth();
    
    if (portChanged && !IsValidPort(m_config.port)) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid Port");
    }
    
    ImGui::Spacing();
    
    // Checkboxes
    ImGui::Checkbox("Enable Broadcast", &m_config.enableBroadcast);
    ImGui::Checkbox("Enable Traffic Data", &m_config.enableTraffic);
    
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("Send traffic information for nearby aircraft");
        ImGui::EndTooltip();
    }
}

void ConfigWindow::DrawButtons() {
    float buttonWidth = 80.0f;
    float spacing = 10.0f;
    float totalWidth = buttonWidth * 3 + spacing * 2;
    float windowWidth = ImGui::GetContentRegionAvail().x;
    float startPos = (windowWidth - totalWidth) * 0.5f;
    
    ImGui::SetCursorPosX(startPos);
    
    // Apply button
    if (ImGui::Button("Apply", ImVec2(buttonWidth, 0))) {
        XPLMDebugString("XP2GDL90: Apply button clicked - applying configuration\n");
        ImGuiManager::Instance().ApplyConfigFromWindow();
    }
    
    ImGui::SameLine(0, spacing);
    
    // Reset button
    if (ImGui::Button("Reset", ImVec2(buttonWidth, 0))) {
        m_showResetConfirm = true;
    }
    
    ImGui::SameLine(0, spacing);
    
    // Close button
    if (ImGui::Button("Close", ImVec2(buttonWidth, 0))) {
        SetVisible(false);
    }
}

void ConfigWindow::DrawResetConfirmation() {
    ImGui::OpenPopup("Reset Configuration");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Reset Configuration", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Reset all settings to default values?");
        ImGui::Spacing();
        
        float buttonWidth = 60.0f;
        float spacing = 10.0f;
        float totalWidth = buttonWidth * 2 + spacing;
        float windowWidth = ImGui::GetContentRegionAvail().x;
        float startPos = (windowWidth - totalWidth) * 0.5f;
        
        ImGui::SetCursorPosX(startPos);
        
        if (ImGui::Button("Yes", ImVec2(buttonWidth, 0))) {
            ResetToDefaults();
            m_showResetConfirm = false;
            ImGui::CloseCurrentPopup();
            XPLMDebugString("XP2GDL90: Configuration reset to defaults\n");
        }
        
        ImGui::SameLine(0, spacing);
        
        if (ImGui::Button("No", ImVec2(buttonWidth, 0))) {
            m_showResetConfirm = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

bool ConfigWindow::IsValidIP(const char* ip) const {
    if (!ip || strlen(ip) == 0) return false;
    
    int parts[4];
    int parsed = sscanf(ip, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]);
    
    if (parsed != 4) return false;
    
    for (int i = 0; i < 4; i++) {
        if (parts[i] < 0 || parts[i] > 255) return false;
    }
    
    return true;
}

bool ConfigWindow::IsValidPort(int port) const {
    return port > 0 && port <= 65535;
}
