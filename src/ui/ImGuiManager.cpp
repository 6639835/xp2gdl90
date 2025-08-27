#include "ImGuiManager.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_xplane.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMUtilities.h"
#include <cstring>
#include <vector>

// Forward declare the flight data structures from main plugin
struct FlightData {
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    double speed = 0.0;
    double track = 0.0;
    double vs = 0.0;
    double pitch = 0.0;
    double roll = 0.0;
};

struct TrafficTarget {
    int plane_id = 0;
    uint32_t icao_address = 0;
    FlightData data;
    double last_update = 0.0;
    bool active = false;
    char callsign[9] = "";
};

// X-Plane window callbacks
static void DrawWindowCallback(XPLMWindowID window, void* refcon);
static int HandleMouseClickCallback(XPLMWindowID window, int x, int y, int button, void* refcon);
static void HandleKeyPressCallback(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon, int losingFocus);
static int HandleCursorCallback(XPLMWindowID window, int x, int y, void* refcon);
static int HandleMouseWheelCallback(XPLMWindowID window, int x, int y, int wheel, int clicks, void* refcon);

ImGuiManager& ImGuiManager::Instance() {
    static ImGuiManager instance;
    return instance;
}

bool ImGuiManager::Initialize() {
    XPLMDebugString("XP2GDL90: Initialize() called\n");
    
    if (m_initialized) {
        XPLMDebugString("XP2GDL90: Already initialized, returning true\n");
        return true;
    }
    
    // Add a delay to ensure X-Plane is fully loaded
    static bool first_call = true;
    if (first_call) {
        first_call = false;
        XPLMDebugString("XP2GDL90: Delaying ImGui initialization...\n");
        return false; // Try again later
    }
    
    XPLMDebugString("XP2GDL90: Second call - proceeding with initialization\n");
    XPLMDebugString("XP2GDL90: Starting ImGui initialization...\n");
    
    // Initialize ImGui core
    XPLMDebugString("XP2GDL90: Step 1 - Initializing ImGui core...\n");
    if (!XPlaneImGui::Init()) {
        XPLMDebugString("XP2GDL90: FAILED - ImGui core initialization failed\n");
        return false;
    }
    
    XPLMDebugString("XP2GDL90: SUCCESS - ImGui core initialized\n");
    XPLMDebugString("XP2GDL90: Step 2 - Creating X-Plane window...\n");
    
    // Create X-Plane window for ImGui rendering
    if (!CreateXPlaneWindow()) {
        XPLMDebugString("XP2GDL90: FAILED - X-Plane window creation failed\n");
        XPlaneImGui::Shutdown();
        return false;
    }
    
    XPLMDebugString("XP2GDL90: SUCCESS - X-Plane window created\n");
    
    m_initialized = true;
    XPLMDebugString("XP2GDL90: ImGui Manager initialized successfully\n");
    return true;
}

void ImGuiManager::Shutdown() {
    if (!m_initialized) return;
    
    DestroyXPlaneWindow();
    XPlaneImGui::Shutdown();
    
    m_initialized = false;
    XPLMDebugString("XP2GDL90: ImGui Manager shutdown\n");
}

void ImGuiManager::ShowConfigWindow() {
    if (!m_initialized) {
        XPLMDebugString("XP2GDL90: Cannot show config window - ImGui not initialized\n");
        return;
    }
    XPLMDebugString("XP2GDL90: Setting m_showConfig = true\n");
    m_showConfig = true;
}

void ImGuiManager::HideConfigWindow() {
    m_showConfig = false; // Safe to hide even if not initialized
}

void ImGuiManager::ShowStatusWindow() {
    if (!m_initialized) {
        XPLMDebugString("XP2GDL90: Cannot show status window - ImGui not initialized\n");
        return;
    }
    m_showStatus = true;
}

void ImGuiManager::HideStatusWindow() {
    m_showStatus = false; // Safe to hide even if not initialized
}

void ImGuiManager::UpdateConnectionStatus(bool connected, const char* ip, int port) {
    m_connected = connected;
    if (ip) {
        strncpy(m_configIP, ip, sizeof(m_configIP) - 1);
        m_configIP[sizeof(m_configIP) - 1] = '\0';
    }
    m_configPort = port;
}

void ImGuiManager::UpdateStatistics(int messagesSent, int trafficCount) {
    m_messagesSent = messagesSent;
    m_trafficCount = trafficCount;
}

void ImGuiManager::SetConfigCallback(void (*callback)(const char* ip, int port, bool broadcast, bool traffic)) {
    m_configCallback = callback;
}

bool ImGuiManager::CreateXPlaneWindow() {
    // Create a window that covers the entire screen but is invisible
    // We'll use this for ImGui rendering
    XPLMDebugString("XP2GDL90: Setting up window parameters...\n");
    
    XPLMCreateWindow_t create_window;
    create_window.structSize = sizeof(XPLMCreateWindow_t);
    create_window.left = -10;     // Positioned mostly off-screen
    create_window.top = 10;
    create_window.right = 1;      // Tiny 11x10 pixel window
    create_window.bottom = 0;
    create_window.visible = 1;    // Must be visible to receive draw callbacks
    create_window.drawWindowFunc = DrawWindowCallback;
    create_window.handleMouseClickFunc = HandleMouseClickCallback;
    create_window.handleKeyFunc = HandleKeyPressCallback;
    create_window.handleCursorFunc = HandleCursorCallback;
    create_window.handleMouseWheelFunc = HandleMouseWheelCallback;
    create_window.refcon = this;
    
    XPLMDebugString("XP2GDL90: Setting window layer and decoration...\n");
#if defined(XPLM300)
    create_window.layer = xplm_WindowLayerFloatingWindows;
    XPLMDebugString("XP2GDL90: Set layer to floating windows\n");
#endif
#if defined(XPLM301)
    create_window.decorateAsFloatingWindow = xplm_WindowDecorationNone;
    XPLMDebugString("XP2GDL90: Set decoration to none\n");
#endif
    
    XPLMDebugString("XP2GDL90: Calling XPLMCreateWindowEx...\n");
    m_windowID = XPLMCreateWindowEx(&create_window);
    
    if (m_windowID == nullptr) {
        XPLMDebugString("XP2GDL90: FAILED - XPLMCreateWindowEx returned null\n");
        return false;
    }
    
    XPLMDebugString("XP2GDL90: SUCCESS - Window created successfully\n");
    
    // Make window click-through when not needed
    XPLMSetWindowPositioningMode(m_windowID, xplm_WindowPositionFree, -1);
    XPLMSetWindowGravity(m_windowID, 0, 1, 0, 1); // Top-left gravity
    XPLMSetWindowResizingLimits(m_windowID, 0, 0, 1920, 1080);
    
    return true;
}

void ImGuiManager::DestroyXPlaneWindow() {
    if (m_windowID) {
        XPLMDestroyWindow(m_windowID);
        m_windowID = nullptr;
    }
}

void ImGuiManager::HandleDraw() {
    if (!m_initialized) return;
    
    // Only start ImGui frame if we have windows to draw
    if (!m_showConfig && !m_showStatus) {
        return; // No windows to draw, skip ImGui entirely
    }
    
    // Get actual screen size for ImGui (not our tiny window size)
    int screenWidth, screenHeight;
    XPLMGetScreenSize(&screenWidth, &screenHeight);
    XPlaneImGui::SetDisplaySize((float)screenWidth, (float)screenHeight);
    
    // Start ImGui frame
    XPlaneImGui::NewFrame();
    
    // Draw windows
    if (m_showConfig) {
        DrawConfigWindow();
    }
    
    if (m_showStatus) {
        DrawStatusWindow();
    }
    
    // Render ImGui
    XPlaneImGui::Render();
}

void ImGuiManager::DrawConfigWindow() {
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("XP2GDL90 Configuration", &m_showConfig, ImGuiWindowFlags_NoCollapse)) {
        
        // Connection Settings
        ImGui::SeparatorText("Connection Settings");
        ImGui::InputText("FDPRO IP Address", m_configIP, sizeof(m_configIP));
        ImGui::InputInt("Port", &m_configPort);
        
        if (m_configPort < 1 || m_configPort > 65535) {
            m_configPort = 4000;
        }
        
        ImGui::Spacing();
        
        // Feature Settings
        ImGui::SeparatorText("Features");
        ImGui::Checkbox("Enable Broadcast", &m_configBroadcast);
        ImGui::Checkbox("Enable Traffic Reports", &m_configTraffic);
        
        ImGui::Spacing();
        
        // Connection Status
        ImGui::SeparatorText("Status");
        ImVec4 statusColor = m_connected ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        ImGui::TextColored(statusColor, "Status: %s", m_connected ? "Connected" : "Disconnected");
        
        if (m_connected) {
            ImGui::Text("Broadcasting to: %s:%d", m_configIP, m_configPort);
        }
        
        ImGui::Spacing();
        
        // Control Buttons
        ImGui::Separator();
        if (ImGui::Button("Apply Settings", ImVec2(120, 0))) {
            ApplyConfiguration();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset to Defaults", ImVec2(120, 0))) {
            ResetConfiguration();
        }
        ImGui::SameLine();
        if (ImGui::Button("Status Window", ImVec2(120, 0))) {
            ShowStatusWindow();
        }
    }
    ImGui::End();
}

void ImGuiManager::DrawStatusWindow() {
    ImGui::SetNextWindowPos(ImVec2(520, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("XP2GDL90 Status Monitor", &m_showStatus, ImGuiWindowFlags_NoCollapse)) {
        
        // Connection Status
        DrawConnectionStatus();
        
        ImGui::Spacing();
        
        // Statistics
        DrawStatistics();
        
        ImGui::Spacing();
        
        // Traffic Table
        DrawTrafficTable();
    }
    ImGui::End();
}

void ImGuiManager::DrawConnectionStatus() {
    ImGui::SeparatorText("Connection Status");
    
    // Status indicator
    ImVec4 statusColor = m_connected ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "● %s", m_connected ? "CONNECTED" : "DISCONNECTED");
    
    ImGui::Text("Target: %s:%d", m_configIP, m_configPort);
    ImGui::Text("Broadcast: %s", m_configBroadcast ? "Enabled" : "Disabled");
    ImGui::Text("Traffic: %s", m_configTraffic ? "Enabled" : "Disabled");
}

void ImGuiManager::DrawStatistics() {
    ImGui::SeparatorText("Statistics");
    
    ImGui::Text("Messages Sent: %d", m_messagesSent);
    ImGui::Text("Active Traffic: %d", m_trafficCount);
    
    // You could add more statistics here like:
    // - Messages per second
    // - Data rate
    // - Uptime
    // - Error count
}

void ImGuiManager::DrawTrafficTable() {
    ImGui::SeparatorText("Traffic Targets");
    
    if (m_trafficTargets && !m_trafficTargets->empty()) {
        if (ImGui::BeginTable("TrafficTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Callsign");
            ImGui::TableSetupColumn("Altitude (ft)");
            ImGui::TableSetupColumn("Speed (kt)");
            ImGui::TableSetupColumn("Track (°)");
            ImGui::TableSetupColumn("Status");
            ImGui::TableHeadersRow();
            
            for (const auto& target : *m_trafficTargets) {
                if (!target.active) continue;
                
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", target.callsign);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", target.data.alt);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", target.data.speed);
                
                ImGui::TableNextColumn();
                ImGui::Text("%.0f", target.data.track);
                
                ImGui::TableNextColumn();
                ImVec4 color = target.active ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                ImGui::TextColored(color, "Active");
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::Text("No active traffic targets");
    }
}

void ImGuiManager::ApplyConfiguration() {
    if (m_configCallback) {
        m_configCallback(m_configIP, m_configPort, m_configBroadcast, m_configTraffic);
    }
}

void ImGuiManager::ResetConfiguration() {
    strcpy(m_configIP, "127.0.0.1");
    m_configPort = 4000;
    m_configBroadcast = true;
    m_configTraffic = true;
}

int ImGuiManager::HandleMouseClick(XPLMWindowID window, int x, int y, int button, void* refcon) {
    (void)window; (void)refcon;
    
    if (!m_initialized) return 0;
    
    // Convert coordinates and handle click
    bool down = true; // X-Plane calls this on mouse down
    XPlaneImGui::HandleMouseClick(button, down, (float)x, (float)y);
    
    // Return 1 if ImGui wants to capture the mouse, 0 otherwise
    return XPlaneImGui::WantCaptureMouse() ? 1 : 0;
}

void ImGuiManager::HandleKeyPress(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon, int losingFocus) {
    (void)window; (void)flags; (void)refcon; (void)losingFocus;
    
    if (!m_initialized) return;
    
    XPlaneImGui::HandleKeyPress(key, true);
    if (virtualKey) {
        XPlaneImGui::HandleKeySpecial(virtualKey, true);
    }
}

int ImGuiManager::HandleCursor(XPLMWindowID window, int x, int y, void* refcon) {
    (void)window; (void)refcon;
    
    if (!m_initialized) return xplm_CursorDefault;
    
    XPlaneImGui::HandleMouseMove((float)x, (float)y);
    return xplm_CursorDefault;
}

int ImGuiManager::HandleMouseWheel(XPLMWindowID window, int x, int y, int wheel, int clicks, void* refcon) {
    (void)window; (void)x; (void)y; (void)wheel; (void)refcon;
    
    if (!m_initialized) return 0;
    
    XPlaneImGui::HandleMouseWheel((float)clicks);
    return XPlaneImGui::WantCaptureMouse() ? 1 : 0;
}

void ImGuiManager::UpdateFlightData(const FlightData& data) {
    // Store flight data for display if needed
    // Could be used for showing ownship data in status window
    (void)data; // Suppress unused parameter warning for now
}

void ImGuiManager::UpdateTrafficTargets(const std::vector<TrafficTarget>& targets) {
    // Store reference to traffic data for display
    m_trafficTargets = const_cast<std::vector<TrafficTarget>*>(&targets);
}

// X-Plane callback implementations
static void DrawWindowCallback(XPLMWindowID window, void* refcon) {
    ImGuiManager* manager = static_cast<ImGuiManager*>(refcon);
    if (manager) {
        manager->HandleDraw();
    }
}

static int HandleMouseClickCallback(XPLMWindowID window, int x, int y, int button, void* refcon) {
    ImGuiManager* manager = static_cast<ImGuiManager*>(refcon);
    if (manager) {
        return manager->HandleMouseClick(window, x, y, button, refcon);
    }
    return 0;
}

static void HandleKeyPressCallback(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon, int losingFocus) {
    ImGuiManager* manager = static_cast<ImGuiManager*>(refcon);
    if (manager) {
        manager->HandleKeyPress(window, key, flags, virtualKey, refcon, losingFocus);
    }
}

static int HandleCursorCallback(XPLMWindowID window, int x, int y, void* refcon) {
    ImGuiManager* manager = static_cast<ImGuiManager*>(refcon);
    if (manager) {
        return manager->HandleCursor(window, x, y, refcon);
    }
    return xplm_CursorDefault;
}

static int HandleMouseWheelCallback(XPLMWindowID window, int x, int y, int wheel, int clicks, void* refcon) {
    ImGuiManager* manager = static_cast<ImGuiManager*>(refcon);
    if (manager) {
        return manager->HandleMouseWheel(window, x, y, wheel, clicks, refcon);
    }
    return 0;
}
