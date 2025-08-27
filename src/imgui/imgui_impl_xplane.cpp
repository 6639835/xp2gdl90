#include "imgui_impl_xplane.h"
#include "imgui_impl_opengl2.h"
#include "imgui_internal.h"  // For ImLerp function
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMProcessing.h"  // For XPLMGetElapsedTime
#include "XPLMUtilities.h"   // For XPLMDebugString
#include <cstring>

namespace XPlaneImGui {

static bool g_Initialized = false;
static float g_DisplayWidth = 1920.0f;
static float g_DisplayHeight = 1080.0f;
static float g_MouseX = 0.0f;
static float g_MouseY = 0.0f;
static bool g_MousePressed[3] = { false, false, false };

// Key mapping for special keys
static ImGuiKey XPlaneKeyToImGuiKey(int xplane_key) {
    switch (xplane_key) {
        case XPLM_VK_BACK: return ImGuiKey_Backspace;
        case XPLM_VK_TAB: return ImGuiKey_Tab;
        case XPLM_VK_RETURN: return ImGuiKey_Enter;
        case XPLM_VK_ESCAPE: return ImGuiKey_Escape;
        case XPLM_VK_SPACE: return ImGuiKey_Space;
        case XPLM_VK_DELETE: return ImGuiKey_Delete;
        case XPLM_VK_LEFT: return ImGuiKey_LeftArrow;
        case XPLM_VK_UP: return ImGuiKey_UpArrow;
        case XPLM_VK_RIGHT: return ImGuiKey_RightArrow;
        case XPLM_VK_DOWN: return ImGuiKey_DownArrow;
        case XPLM_VK_HOME: return ImGuiKey_Home;
        case XPLM_VK_END: return ImGuiKey_End;
        case XPLM_VK_PRIOR: return ImGuiKey_PageUp;
        case XPLM_VK_NEXT: return ImGuiKey_PageDown;
        default: return ImGuiKey_None;
    }
}

bool Init() {
    if (g_Initialized) {
        XPLMDebugString("XP2GDL90: ImGui already initialized\n");
        return true;
    }
    
    XPLMDebugString("XP2GDL90: Creating ImGui context...\n");
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    XPLMDebugString("XP2GDL90: ImGui context created successfully\n");
    
    // Enable keyboard and mouse navigation
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    XPLMDebugString("XP2GDL90: Setting up ImGui style...\n");
    // Setup Dear ImGui style
    SetStyle();
    
    XPLMDebugString("XP2GDL90: Initializing OpenGL2 backend...\n");
    // Setup Platform/Renderer backends
    if (!ImGui_ImplOpenGL2_Init()) {
        XPLMDebugString("XP2GDL90: FAILED - OpenGL2 backend initialization failed\n");
        return false;
    }
    
    XPLMDebugString("XP2GDL90: OpenGL2 backend initialized successfully\n");
    XPLMDebugString("XP2GDL90: Loading fonts...\n");
    
    // Load fonts
    LoadFonts();
    
    XPLMDebugString("XP2GDL90: Fonts loaded successfully\n");
    
    g_Initialized = true;
    XPLMDebugString("XP2GDL90: ImGui initialization completed successfully\n");
    return true;
}

void Shutdown() {
    if (!g_Initialized) return;
    
    ImGui_ImplOpenGL2_Shutdown();
    ImGui::DestroyContext();
    g_Initialized = false;
}

void NewFrame() {
    if (!g_Initialized) return;
    
    ImGuiIO& io = ImGui::GetIO();
    
    // Setup display size
    io.DisplaySize = ImVec2(g_DisplayWidth, g_DisplayHeight);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    
    // Setup time step
    static double g_Time = 0.0;
    double current_time = XPLMGetElapsedTime();
    io.DeltaTime = g_Time > 0.0 ? (float)(current_time - g_Time) : (float)(1.0f/60.0f);
    g_Time = current_time;
    
    // Update mouse position
    io.MousePos = ImVec2(g_MouseX, g_MouseY);
    
    // Update mouse buttons
    for (int i = 0; i < 3; i++) {
        io.MouseDown[i] = g_MousePressed[i];
    }
    
    ImGui_ImplOpenGL2_NewFrame();
    ImGui::NewFrame();
}

void Render() {
    if (!g_Initialized) return;
    
    ImGui::Render();
    ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

void HandleMouseClick(int button, bool down, float x, float y) {
    if (button >= 0 && button < 3) {
        g_MousePressed[button] = down;
    }
    g_MouseX = x;
    g_MouseY = y;
}

void HandleMouseMove(float x, float y) {
    g_MouseX = x;
    g_MouseY = y;
}

void HandleMouseWheel(float delta) {
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheel += delta;
}

void HandleKeyPress(char key, bool down) {
    ImGuiIO& io = ImGui::GetIO();
    if (key >= 32 && key < 127 && down) {
        io.AddInputCharacter((unsigned int)key);
    }
}

void HandleKeySpecial(int key, bool down) {
    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey imgui_key = XPlaneKeyToImGuiKey(key);
    if (imgui_key != ImGuiKey_None) {
        io.AddKeyEvent(imgui_key, down);
    }
}

void SetDisplaySize(float width, float height) {
    g_DisplayWidth = width;
    g_DisplayHeight = height;
}

bool WantCaptureMouse() {
    if (!g_Initialized) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool WantCaptureKeyboard() {
    if (!g_Initialized) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

void SetStyle() {
    // Modern aviation-style theme
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Colors
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.29f, 0.48f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.59f, 0.98f, 0.31f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab]                    = ImLerp(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
    colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_TabActive]              = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabUnfocused]           = ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImLerp(colors[ImGuiCol_TabActive],    colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    
    // Style
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
}

void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Load default font with better size for aviation displays
    io.Fonts->AddFontDefault();
    
    // You can add custom fonts here if needed
    // io.Fonts->AddFontFromFileTTF("path/to/font.ttf", 16.0f);
}

} // namespace XPlaneImGui
