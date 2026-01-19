/**
 * XP2GDL90 - X-Plane to GDL90 Bridge Plugin
 * 
 * Broadcasts X-Plane flight data in GDL90 format via UDP
 * Compatible with ForeFlight, Garmin Pilot, and other EFB applications
 * 
 * Author: XP2GDL90 Project
 * License: MIT
 */

#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include <XPLMMenus.h>

#include "gdl90_encoder.h"
#include "udp_broadcaster.h"
#include "config.h"

#include <string>
#include <memory>
#include <cstring>

// Plugin state
struct PluginState {
    std::unique_ptr<gdl90::GDL90Encoder> encoder;
    std::unique_ptr<udp::UDPBroadcaster> broadcaster;
    std::unique_ptr<config::ConfigManager> config_manager;
    
    // DataRefs
    XPLMDataRef lat_ref;
    XPLMDataRef lon_ref;
    XPLMDataRef alt_ref;
    XPLMDataRef speed_ref;
    XPLMDataRef track_ref;      // True track (true_psi)
    XPLMDataRef vs_ref;
    XPLMDataRef airborne_ref;
    XPLMDataRef sim_time_ref;
    XPLMDataRef tailnum_ref;    // Aircraft tail number
    
    // Timing
    float last_heartbeat;
    float last_position;
    
    // Status
    bool initialized;
    bool enabled;
    
    // Menu
    XPLMMenuID menu_id;
    int menu_item_enable;
};

static PluginState g_state;

// Forward declarations
static float FlightLoopCallback(float inElapsedSinceLastCall,
                                float inElapsedTimeSinceLastFlightLoop,
                                int inCounter,
                                void* inRefcon);
static void MenuHandlerCallback(void* inMenuRef, void* inItemRef);

// Logging helper
static void LogMessage(const std::string& message) {
    XPLMDebugString(("[XP2GDL90] " + message + "\n").c_str());
}

// Get ownship position data
static gdl90::PositionData GetOwnshipData(const config::Config& cfg) {
    gdl90::PositionData data;
    
    // Read position from datarefs
    data.latitude = XPLMGetDatad(g_state.lat_ref);
    data.longitude = XPLMGetDatad(g_state.lon_ref);
    
    // Altitude in meters -> convert to feet
    double alt_meters = XPLMGetDatad(g_state.alt_ref);
    data.altitude = static_cast<int32_t>(alt_meters * 3.28084);
    
    // Ground speed in m/s -> convert to knots
    float speed_ms = XPLMGetDataf(g_state.speed_ref);
    data.h_velocity = static_cast<uint16_t>(speed_ms * 1.94384);
    
    // Vertical speed (already in fpm in X-Plane)
    data.v_velocity = static_cast<int16_t>(XPLMGetDataf(g_state.vs_ref));
    
    // Track/heading (degrees)
    data.track = static_cast<uint16_t>(XPLMGetDataf(g_state.track_ref));
    data.track_type = gdl90::TrackType::TRUE_TRACK;
    
    // Airborne status
    int on_ground = XPLMGetDatai(g_state.airborne_ref);
    data.airborne = (on_ground == 0);  // 0 = airborne, 1 = on ground
    
    // Configuration data
    data.icao_address = cfg.icao_address;
    
    // Get callsign from X-Plane aircraft tail number, fallback to config
    if (g_state.tailnum_ref) {
        char tailnum[40];
        int bytes_read = XPLMGetDatab(g_state.tailnum_ref, tailnum, 0, sizeof(tailnum) - 1);
        if (bytes_read > 0) {
            tailnum[bytes_read] = '\0';  // Null terminate
            // Trim whitespace
            std::string tail_str(tailnum);
            size_t start = tail_str.find_first_not_of(" \t\r\n");
            size_t end = tail_str.find_last_not_of(" \t\r\n");
            if (start != std::string::npos && end != std::string::npos) {
                data.callsign = tail_str.substr(start, end - start + 1);
            } else {
                data.callsign = cfg.callsign;  // Use config fallback
            }
        } else {
            data.callsign = cfg.callsign;  // Use config fallback
        }
    } else {
        data.callsign = cfg.callsign;  // Use config fallback
    }
    
    data.emitter_category = static_cast<gdl90::EmitterCategory>(cfg.emitter_category);
    data.address_type = gdl90::AddressType::ADSB_ICAO;
    data.nic = cfg.nic;
    data.nacp = cfg.nacp;
    data.alert_status = 0;
    data.emergency_code = 0;
    
    return data;
}

// Initialize plugin
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    // Plugin info
    strcpy(outName, "XP2GDL90");
    strcpy(outSig, "com.xp2gdl90.plugin");
    strcpy(outDesc, "GDL90 ADS-B data broadcaster for EFB applications");
    
    LogMessage("Plugin starting...");
    
    // Initialize state
    g_state.initialized = false;
    g_state.enabled = false;
    g_state.last_heartbeat = 0.0f;
    g_state.last_position = 0.0f;
    
    // Create objects
    g_state.encoder = std::make_unique<gdl90::GDL90Encoder>();
    g_state.config_manager = std::make_unique<config::ConfigManager>();
    
    // Load configuration
    // Get X-Plane system path and ensure it's in POSIX format
    char path[512];
    XPLMGetSystemPath(path);
    std::string system_path(path);
    
    // Convert HFS path to POSIX if needed (macOS)
    #if APL
    // On macOS, XPLMGetSystemPath may return HFS format (with colons)
    // Convert to POSIX format
    if (!system_path.empty() && system_path.find(':') != std::string::npos) {
        // Simple HFS to POSIX conversion
        // "Macintosh HD:Users:..." -> "/Users/..."
        size_t first_colon = system_path.find(':');
        if (first_colon != std::string::npos) {
            system_path = "/" + system_path.substr(first_colon + 1);
            // Replace remaining colons with slashes
            for (char& c : system_path) {
                if (c == ':') c = '/';
            }
        }
    }
    #endif
    
    // Ensure path ends with slash
    if (!system_path.empty() && system_path.back() != '/') {
        system_path += '/';
    }
    
    std::string config_path = system_path + "Resources/plugins/xp2gdl90/xp2gdl90.ini";
    
    if (!g_state.config_manager->load(config_path)) {
        LogMessage("Warning: Could not load config file, using defaults");
        // Save default configuration
        g_state.config_manager->save(config_path);
        LogMessage("Default configuration saved");
    }
    
    const config::Config& cfg = g_state.config_manager->getConfig();
    
    // Create UDP broadcaster
    g_state.broadcaster = std::make_unique<udp::UDPBroadcaster>(cfg.target_ip, cfg.target_port);
    if (!g_state.broadcaster->initialize()) {
        LogMessage("ERROR: Failed to initialize UDP broadcaster: " + 
                   g_state.broadcaster->getLastError());
        return 0;
    }
    LogMessage("UDP broadcaster initialized: " + cfg.target_ip + ":" + 
               std::to_string(cfg.target_port));
    
    // Find datarefs
    g_state.lat_ref = XPLMFindDataRef("sim/flightmodel/position/latitude");
    g_state.lon_ref = XPLMFindDataRef("sim/flightmodel/position/longitude");
    g_state.alt_ref = XPLMFindDataRef("sim/flightmodel/position/elevation");
    g_state.speed_ref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
    g_state.track_ref = XPLMFindDataRef("sim/flightmodel/position/true_psi");  // True track angle
    g_state.vs_ref = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm");
    g_state.airborne_ref = XPLMFindDataRef("sim/flightmodel/failures/onground_any");
    g_state.sim_time_ref = XPLMFindDataRef("sim/time/total_flight_time_sec");
    g_state.tailnum_ref = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");  // Aircraft tail number
    
    // Verify required datarefs
    bool datarefs_ok = true;
    if (!g_state.lat_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/position/latitude");
        datarefs_ok = false;
    }
    if (!g_state.lon_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/position/longitude");
        datarefs_ok = false;
    }
    if (!g_state.alt_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/position/elevation");
        datarefs_ok = false;
    }
    if (!g_state.speed_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/position/groundspeed");
        datarefs_ok = false;
    }
    if (!g_state.track_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/position/true_psi");
        datarefs_ok = false;
    }
    if (!g_state.vs_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/position/vh_ind_fpm");
        datarefs_ok = false;
    }
    if (!g_state.airborne_ref) {
        LogMessage("ERROR: Missing dataref sim/flightmodel/failures/onground_any");
        datarefs_ok = false;
    }
    if (!g_state.sim_time_ref) {
        LogMessage("ERROR: Missing dataref sim/time/total_flight_time_sec");
        datarefs_ok = false;
    }

    if (!datarefs_ok) {
        LogMessage("ERROR: Failed to find required datarefs");
        return 0;
    }
    
    // Create menu
    int menu_container = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "XP2GDL90", nullptr, 0);
    g_state.menu_id = XPLMCreateMenu("XP2GDL90", XPLMFindPluginsMenu(), menu_container,
                                      MenuHandlerCallback, nullptr);
    g_state.menu_item_enable = XPLMAppendMenuItem(g_state.menu_id, "Enable Broadcasting",
                                                   (void*)1, 0);
    
    g_state.initialized = true;
    LogMessage("Plugin initialized successfully");
    
    return 1;
}

PLUGIN_API void XPluginStop(void) {
    LogMessage("Plugin stopping...");
    
    if (g_state.enabled) {
        XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);
    }
    
    // Clean up
    g_state.broadcaster.reset();
    g_state.encoder.reset();
    g_state.config_manager.reset();
    
    LogMessage("Plugin stopped");
}

PLUGIN_API int XPluginEnable(void) {
    if (!g_state.initialized) {
        return 0;
    }
    
    LogMessage("Enabling plugin...");
    
    // Register flight loop callback
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, nullptr);
    
    g_state.enabled = true;
    XPLMCheckMenuItem(g_state.menu_id, g_state.menu_item_enable, xplm_Menu_Checked);
    
    LogMessage("Plugin enabled - Broadcasting GDL90 data");
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    LogMessage("Disabling plugin...");
    
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);
    
    g_state.enabled = false;
    XPLMCheckMenuItem(g_state.menu_id, g_state.menu_item_enable, xplm_Menu_Unchecked);
    
    LogMessage("Plugin disabled");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) {
    // Handle messages if needed
    (void)inFrom;
    (void)inMsg;
    (void)inParam;
}

// Flight loop callback - called every frame
static float FlightLoopCallback(float inElapsedSinceLastCall,
                                float inElapsedTimeSinceLastFlightLoop,
                                int inCounter,
                                void* inRefcon) {
    (void)inElapsedSinceLastCall;
    (void)inElapsedTimeSinceLastFlightLoop;
    (void)inCounter;
    (void)inRefcon;
    
    if (!g_state.enabled || !g_state.initialized) {
        return -1.0f;  // Disable callback
    }
    
    float sim_time = XPLMGetDataf(g_state.sim_time_ref);
    const config::Config& cfg = g_state.config_manager->getConfig();
    
    // Send heartbeat
    if (cfg.heartbeat_rate > 0.0f &&
        sim_time - g_state.last_heartbeat >= (1.0f / cfg.heartbeat_rate)) {
        auto heartbeat = g_state.encoder->createHeartbeat(true, true);
        g_state.broadcaster->send(heartbeat);
        g_state.last_heartbeat = sim_time;
    }
    
    // Send ownship position
    if (cfg.position_rate > 0.0f &&
        sim_time - g_state.last_position >= (1.0f / cfg.position_rate)) {
        gdl90::PositionData ownship = GetOwnshipData(cfg);
        auto position_msg = g_state.encoder->createOwnshipReport(ownship);
        g_state.broadcaster->send(position_msg);
        g_state.last_position = sim_time;
    }
    
    // Call again next frame
    return -1.0f;
}

// Menu handler
static void MenuHandlerCallback(void* inMenuRef, void* inItemRef) {
    (void)inMenuRef;
    
    intptr_t item = reinterpret_cast<intptr_t>(inItemRef);
    
    if (item == 1) {
        // Toggle enable/disable
        if (g_state.enabled) {
            XPluginDisable();
        } else {
            XPluginEnable();
        }
    }
}
