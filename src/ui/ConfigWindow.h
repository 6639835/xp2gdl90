#pragma once

#include "../imgwindow/xp_img_window.h"
#include <string>
#include <cstring>

struct NetworkConfig {
    char ip[16] = "127.0.0.1";
    int port = 4000;
    bool enableBroadcast = true;
    bool enableTraffic = true;
};

class ConfigWindow : public XPImgWindow {
public:
    ConfigWindow();
    ~ConfigWindow() override = default;

    /// Set network configuration
    void SetConfig(const NetworkConfig& config);
    
    /// Get network configuration
    NetworkConfig GetConfig() const;
    
    /// Reset to default values
    void ResetToDefaults();

protected:
    /// Build the interface
    void buildInterface() override;

private:
    NetworkConfig m_config;
    
    // UI state
    bool m_showResetConfirm = false;
    
    // Helper methods
    void DrawNetworkSettings();
    void DrawButtons();
    void DrawResetConfirmation();
    
    // Validation
    bool IsValidIP(const char* ip) const;
    bool IsValidPort(int port) const;
};
