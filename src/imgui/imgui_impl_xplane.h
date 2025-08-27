#pragma once

// X-Plane ImGui Backend Integration
// This file provides X-Plane specific ImGui backend implementation

#include "imgui.h"

struct GLFWwindow;

// Forward declarations
typedef void* XPLMWindowID;

namespace XPlaneImGui {
    // Initialization and cleanup
    bool Init();
    void Shutdown();
    
    // Frame lifecycle
    void NewFrame();
    void Render();
    
    // Input handling
    void HandleMouseClick(int button, bool down, float x, float y);
    void HandleMouseMove(float x, float y);
    void HandleMouseWheel(float delta);
    void HandleKeyPress(char key, bool down);
    void HandleKeySpecial(int key, bool down);
    
    // Window management
    void SetDisplaySize(float width, float height);
    bool WantCaptureMouse();
    bool WantCaptureKeyboard();
    
    // Utility functions
    void SetStyle();
    void LoadFonts();
}
