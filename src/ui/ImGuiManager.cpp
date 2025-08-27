#include "ImGuiManager.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_xplane.h"
#include "XPLMDefs.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMUtilities.h"
#include <cstring>
#include <vector>

// OpenGL headers for rendering
#if APL
#include <OpenGL/gl.h>
#elif IBM
#include <GL/gl.h>
#elif LIN
#include <GL/gl.h>
#endif

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
    
    // Make the window visible when showing config
    if (m_windowID) {
        // Keep host window invisible/off-screen; just take keyboard focus
        XPLMTakeKeyboardFocus(m_windowID);
        // Resize host window to compact dialog size (400x300)
        int l, t, r, b;
        XPLMGetScreenBoundsGlobal(&l, &t, &r, &b); // top-left origin
        // Keep it near previous pos or default 100,500
        int left = 100;
        int top = 500;
        int right = left + 400;
        int bottom = top - 300;
        XPLMSetWindowGeometry(m_windowID, left, top, right, bottom);
    }
    
    char debug_msg[100];
    snprintf(debug_msg, sizeof(debug_msg), "XP2GDL90: m_showConfig is now %s\n", m_showConfig ? "TRUE" : "FALSE");
    XPLMDebugString(debug_msg);
}

void ImGuiManager::HideConfigWindow() {
    m_showConfig = false; // Safe to hide even if not initialized
    
    // Hide the window when no dialogs are active
    if (m_windowID && !m_showConfig && !m_showStatus) {
        XPLMSetWindowIsVisible(m_windowID, 0);
    }
}

void ImGuiManager::ShowStatusWindow() {
    if (!m_initialized) {
        XPLMDebugString("XP2GDL90: Cannot show status window - ImGui not initialized\n");
        return;
    }
    m_showStatus = true;
    
    // Make the window visible when showing status
    if (m_windowID) {
        XPLMSetWindowIsVisible(m_windowID, 1);
        XPLMBringWindowToFront(m_windowID);
    }
}

void ImGuiManager::HideStatusWindow() {
    m_showStatus = false; // Safe to hide even if not initialized
    
    // Hide the window when no dialogs are active
    if (m_windowID && !m_showConfig && !m_showStatus) {
        XPLMSetWindowIsVisible(m_windowID, 0);
    }
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
    // Create a properly sized window instead of full-screen overlay
    XPLMDebugString("XP2GDL90: Setting up window parameters...\n");
    
    // Create a normal sized window that won't interfere with X-Plane
    XPLMCreateWindow_t create_window;
    create_window.structSize = sizeof(XPLMCreateWindow_t);
    // Create a 1×1 px window far off-screen so it never draws but still receives callbacks
    create_window.left = 0;
    create_window.top  = 1;
    create_window.right  = 1;
    create_window.bottom = 0;   // 1x1 px window in corner
    create_window.visible = 1;        // Visible but tiny
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
    create_window.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
    XPLMDebugString("XP2GDL90: Set decoration to round rectangle\n");
#endif
    
    XPLMDebugString("XP2GDL90: Calling XPLMCreateWindowEx...\n");
    m_windowID = XPLMCreateWindowEx(&create_window);
    
    if (m_windowID == nullptr) {
        XPLMDebugString("XP2GDL90: FAILED - XPLMCreateWindowEx returned null\n");
        return false;
    }
    
    XPLMDebugString("XP2GDL90: SUCCESS - Window created successfully\n");
    
    // Set proper window behavior
    XPLMSetWindowPositioningMode(m_windowID, xplm_WindowPositionFree, -1);
    XPLMSetWindowGravity(m_windowID, 0, 1, 0, 1);
    XPLMSetWindowResizingLimits(m_windowID, 300, 200, 800, 600);
    XPLMSetWindowTitle(m_windowID, "XP2GDL90 Configuration");
    
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
    
    // Only process ImGui if we have windows to draw and window is visible
    if (!m_showConfig && !m_showStatus) {
        return;
    }
    
    // Get window geometry for ImGui
    int left, top, right, bottom;
    XPLMGetWindowGeometry(m_windowID, &left, &top, &right, &bottom);
    float width = (float)(right - left);
    float height = (float)(top - bottom);
    
    XPlaneImGui::SetDisplaySize(width, height);
    
    // (Removed glClear to avoid painting background over the entire screen)
    
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
    XPLMDebugString("XP2GDL90: DrawConfigWindow called\n");
    
    // Fill the entire X-Plane window with the ImGui window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    XPLMDebugString("XP2GDL90: About to call ImGui::Begin\n");
    
    if (ImGui::Begin("XP2GDL90 Configuration", &m_showConfig, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        XPLMDebugString("XP2GDL90: Inside ImGui::Begin - window created successfully\n");
        
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
    
    // Check if window was closed via ImGui
    if (!m_showConfig) {
        HideConfigWindow();
    }
    
    XPLMDebugString("XP2GDL90: DrawConfigWindow completed\n");
}

void ImGuiManager::DrawStatusWindow() {
    // Fill the entire X-Plane window with the ImGui window
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    if (ImGui::Begin("XP2GDL90 Status Monitor", &m_showStatus, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
        
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
    
    // Check if window was closed via ImGui
    if (!m_showStatus) {
        HideStatusWindow();
    }
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

int ImGuiManager::HandleMouseClick(XPLMWindowID window, int x, int y, XPLMMouseStatus mouseStatus, void* refcon) {
    (void)window; (void)refcon;
    
    if (!m_initialized) return 0;
    
    // X-Plane passes x/y already relative to this window (origin top-left).
    // Convert to ImGui coordinates by just flipping Y within the window.
    int left, top, right, bottom;
    XPLMGetWindowGeometry(m_windowID, &left, &top, &right, &bottom);
    float windowHeight = (float)(top - bottom);
    float imgui_x = (float)x;
    float imgui_y = windowHeight - (float)y; // flip Y
    
    // Update mouse position for all events
    XPlaneImGui::HandleMouseMove(imgui_x, imgui_y);
    
    // Convert X-Plane mouse status to ImGui events
    int button = 0; // Left mouse button
    
    switch (mouseStatus) {
        case xplm_MouseDown:
            XPlaneImGui::HandleMouseClick(button, true, imgui_x, imgui_y);
            
            // Always take keyboard focus when window is clicked
            XPLMTakeKeyboardFocus(m_windowID);
            XPLMBringWindowToFront(m_windowID);
            break;
            
        case xplm_MouseUp:
            XPlaneImGui::HandleMouseClick(button, false, imgui_x, imgui_y);
            break;
            
        case xplm_MouseDrag:
            // Mouse position already updated above
            break;
    }
    
    // Capture mouse only if ImGui actively requests it
    return XPlaneImGui::WantCaptureMouse() ? 1 : 0;
}

void ImGuiManager::HandleKeyPress(XPLMWindowID window, char key, XPLMKeyFlags flags, char virtualKey, void* refcon, int losingFocus) {
    (void)window; (void)flags; (void)refcon;
    
    if (!m_initialized) return;
    
    if (losingFocus) {
        XPLMDebugString("XP2GDL90: Lost keyboard focus\n");
        return;
    }
    
    char debug_msg[150];
    snprintf(debug_msg, sizeof(debug_msg), "XP2GDL90: Key press - char: %c (%d), virtual: %d\n", 
             key, (int)key, virtualKey);
    XPLMDebugString(debug_msg);
    
    // Always send keyboard input to ImGui - let ImGui decide if it wants it
    // Send character input to ImGui
    if (key >= 32 && key < 127) {  // Printable ASCII characters
        XPlaneImGui::HandleKeyPress(key, true);
    }
    
    // Send special keys to ImGui
    if (virtualKey) {
        XPlaneImGui::HandleKeySpecial(virtualKey, true);
    }
}

int ImGuiManager::HandleCursor(XPLMWindowID window, int x, int y, void* refcon) {
    (void)window; (void)refcon;
    
    if (!m_initialized) return xplm_CursorDefault;
    
    // Convert window coordinates to ImGui coordinates
    int left, top, right, bottom;
    XPLMGetWindowGeometry(m_windowID, &left, &top, &right, &bottom);
    
    float imgui_x = (float)(x - left);
    float imgui_y = (float)(top - y); // Flip Y coordinate for ImGui
    
    XPlaneImGui::HandleMouseMove(imgui_x, imgui_y);
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

static int HandleMouseClickCallback(XPLMWindowID window, int x, int y, XPLMMouseStatus mouseStatus, void* refcon) {
    ImGuiManager* manager = static_cast<ImGuiManager*>(refcon);
    if (manager) {
        return manager->HandleMouseClick(window, x, y, mouseStatus, refcon);
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
