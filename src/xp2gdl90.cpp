// Standard C/C++ headers first
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>

// X-Plane SDK headers
#include "XPLMPlugin.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPLMMenus.h"

// Widget headers
#include "XPWidgets.h"
#include "XPStandardWidgets.h"

// Platform-specific networking headers
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

// Configuration constants
#define MAX_IP_LENGTH 16
#define MAX_PORT_LENGTH 6
const int MAX_TRAFFIC_TARGETS = 63;  // Maximum traffic targets

// Default configuration values
const char* DEFAULT_FDPRO_IP = "127.0.0.1";  // Default FDPRO target IP

// GDL-90 CRC-16-CCITT lookup table
static const uint16_t GDL90_CRC16_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

// Flight data structure
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

// Traffic target structure
struct TrafficTarget {
    int plane_id;
    uint32_t icao_address;
    FlightData data;
    double last_update;
    bool active;
    char callsign[9];  // 8 chars + null terminator
};

// Configuration variables
static char fdpro_ip[MAX_IP_LENGTH] = "127.0.0.1";  // Configurable FDPRO target IP
static int fdpro_port = 4000;                       // Configurable FDPRO listening port
static bool broadcast_enabled = true;               // Enable/disable broadcast

// UI widget handles
static XPLMMenuID config_menu_id = nullptr;
static int config_menu_item = -1;
static XPWidgetID config_window = nullptr;
static XPWidgetID enable_button = nullptr;
static XPWidgetID ip_field = nullptr;
static XPWidgetID port_field = nullptr;
static XPWidgetID apply_button = nullptr;
static XPWidgetID close_button = nullptr;

// Global variables
static SOCKET udp_socket = INVALID_SOCKET;
static struct sockaddr_in fdpro_addr;
static FlightData current_flight_data;
static std::vector<TrafficTarget> traffic_targets;
static bool enable_traffic = true;  // Enable traffic by default

// Dataref handles
static XPLMDataRef lat_dataref = nullptr;
static XPLMDataRef lon_dataref = nullptr;
static XPLMDataRef alt_dataref = nullptr;
static XPLMDataRef speed_dataref = nullptr;
static XPLMDataRef track_dataref = nullptr;
static XPLMDataRef vs_dataref = nullptr;
static XPLMDataRef pitch_dataref = nullptr;
static XPLMDataRef roll_dataref = nullptr;

// Traffic datarefs
static std::vector<XPLMDataRef> traffic_lat_datarefs;
static std::vector<XPLMDataRef> traffic_lon_datarefs;
static std::vector<XPLMDataRef> traffic_alt_datarefs;
static XPLMDataRef traffic_vs_dataref = nullptr;
static XPLMDataRef traffic_psi_dataref = nullptr;

// GDL-90 Encoding Functions

uint16_t gdl90_crc_compute(const uint8_t* data, size_t length) {
    uint16_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        uint8_t tmp = (crc >> 8) ^ data[i];
        crc = (crc << 8) ^ GDL90_CRC16_TABLE[tmp];
    }
    return crc;
}

void gdl90_add_crc(std::vector<uint8_t>& msg) {
    uint16_t crc = gdl90_crc_compute(msg.data(), msg.size());
    msg.push_back(crc & 0xff);        // Low byte first
    msg.push_back((crc >> 8) & 0xff); // High byte second
}

void gdl90_escape(std::vector<uint8_t>& msg) {
    std::vector<uint8_t> escaped;
    for (uint8_t byte : msg) {
        if (byte == 0x7d || byte == 0x7e) {
            escaped.push_back(0x7d);  // Escape character
            escaped.push_back(byte ^ 0x20);  // Modified byte
        } else {
            escaped.push_back(byte);
        }
    }
    msg = escaped;
}

std::vector<uint8_t> gdl90_prepare_message(std::vector<uint8_t> msg) {
    gdl90_add_crc(msg);
    gdl90_escape(msg);
    msg.insert(msg.begin(), 0x7e);  // Start flag
    msg.push_back(0x7e);            // End flag
    return msg;
}

void gdl90_pack24bit(std::vector<uint8_t>& msg, uint32_t value) {
    msg.push_back((value >> 16) & 0xff);
    msg.push_back((value >> 8) & 0xff);
    msg.push_back(value & 0xff);
}

uint32_t gdl90_make_latitude(double lat_deg) {
    if (lat_deg > 90.0) lat_deg = 90.0;
    if (lat_deg < -90.0) lat_deg = -90.0;
    
    int32_t lat = (int32_t)(lat_deg * (0x800000 / 180.0));
    if (lat < 0) {
        lat = (0x1000000 + lat) & 0xffffff;  // 2's complement
    }
    return (uint32_t)lat;
}

uint32_t gdl90_make_longitude(double lon_deg) {
    if (lon_deg > 180.0) lon_deg = 180.0;
    if (lon_deg < -180.0) lon_deg = -180.0;
    
    int32_t lon = (int32_t)(lon_deg * (0x800000 / 180.0));
    if (lon < 0) {
        lon = (0x1000000 + lon) & 0xffffff;  // 2's complement
    }
    return (uint32_t)lon;
}

// Create GDL-90 Heartbeat message (ID 0x00)
std::vector<uint8_t> create_heartbeat() {
    std::vector<uint8_t> msg;
    msg.push_back(0x00);  // Message ID
    
    // Get current time in UTC
    time_t now = time(nullptr);
    struct tm* utc_tm = gmtime(&now);
    uint32_t timestamp = utc_tm->tm_hour * 3600 + utc_tm->tm_min * 60 + utc_tm->tm_sec;
    
    uint8_t st1 = 0x81;  // Status byte 1
    uint8_t st2 = 0x01;  // Status byte 2
    
    // Move bit 16 of timestamp to bit 7 of status byte 2
    uint8_t ts_bit16 = (timestamp & 0x10000) >> 16;
    st2 = (st2 & 0x7f) | (ts_bit16 << 7);
    
    msg.push_back(st1);
    msg.push_back(st2);
    
    // Timestamp (little endian)
    uint16_t ts_low = timestamp & 0xffff;
    msg.push_back(ts_low & 0xff);
    msg.push_back((ts_low >> 8) & 0xff);
    
    // Message count (big endian)
    msg.push_back(0x00);
    msg.push_back(0x00);
    
    return gdl90_prepare_message(msg);
}

// Create GDL-90 Ownship Position Report (ID 0x0a)
std::vector<uint8_t> create_position_report(const FlightData& data) {
    std::vector<uint8_t> msg;
    msg.push_back(0x0a);  // Message ID
    
    // Status and address type
    uint8_t status = 0;
    uint8_t addr_type = 0;
    msg.push_back(((status & 0xf) << 4) | (addr_type & 0xf));
    
    // ICAO address (24-bit)
    uint32_t icao_address = 0xabcdef;  // Default ICAO address
    gdl90_pack24bit(msg, icao_address);
    
    // Latitude (24-bit)
    gdl90_pack24bit(msg, gdl90_make_latitude(data.lat));
    
    // Longitude (24-bit)
    gdl90_pack24bit(msg, gdl90_make_longitude(data.lon));
    
    // Altitude: 25-foot increments, +1000 ft offset
    uint16_t altitude = (uint16_t)((data.alt + 1000) / 25.0);
    if (altitude > 0xffe) altitude = 0xffe;
    
    uint8_t misc = 9;  // Miscellaneous indicator
    msg.push_back((altitude >> 4) & 0xff);
    msg.push_back(((altitude & 0xf) << 4) | (misc & 0xf));
    
    // Navigation integrity and accuracy categories
    uint8_t nic = 11;   // Navigation Integrity Category
    uint8_t nacp = 10;  // Navigation Accuracy Category
    msg.push_back(((nic & 0xf) << 4) | (nacp & 0xf));
    
    // Horizontal velocity
    uint16_t h_velocity = (uint16_t)data.speed;
    if (h_velocity > 0xffe) h_velocity = 0xffe;
    
    // Vertical velocity (64 fpm units)
    uint16_t v_velocity = 0x800;  // No data flag
    if (data.vs != 0) {
        int16_t vs_64fpm = (int16_t)(data.vs / 64);
        if (vs_64fpm < 0) {
            v_velocity = (0x1000 + vs_64fpm) & 0xfff;  // 12-bit 2's complement
        } else {
            v_velocity = vs_64fpm & 0xfff;
        }
    }
    
    // Pack velocities (3 bytes total)
    msg.push_back((h_velocity >> 4) & 0xff);
    msg.push_back(((h_velocity & 0xf) << 4) | ((v_velocity >> 8) & 0xf));
    msg.push_back(v_velocity & 0xff);
    
    // Track/heading
    uint8_t track_heading = (uint8_t)(data.track / (360.0 / 256));
    msg.push_back(track_heading);
    
    // Emitter category
    uint8_t emitter_cat = 1;  // Light aircraft
    msg.push_back(emitter_cat);
    
    // Call sign (8 bytes)
    const char* callsign = "PYTHON1 ";
    for (int i = 0; i < 8; i++) {
        msg.push_back(callsign[i]);
    }
    
    // Emergency code
    uint8_t emergency_code = 0;
    msg.push_back(emergency_code << 4);
    
    return gdl90_prepare_message(msg);
}

// Create GDL-90 Traffic Report (ID 0x14)
std::vector<uint8_t> create_traffic_report(const TrafficTarget& target) {
    std::vector<uint8_t> msg;
    msg.push_back(0x14);  // Message ID
    
    // Traffic alert status and address type
    uint8_t alert_status = 0;  // No traffic alert
    uint8_t addr_type = 0;     // ADS-B with ICAO address
    msg.push_back(((alert_status & 0x1) << 7) | ((addr_type & 0x7) << 4));
    
    // ICAO address (24-bit)
    gdl90_pack24bit(msg, target.icao_address);
    
    // Latitude (24-bit)
    gdl90_pack24bit(msg, gdl90_make_latitude(target.data.lat));
    
    // Longitude (24-bit)
    gdl90_pack24bit(msg, gdl90_make_longitude(target.data.lon));
    
    // Altitude: 25-foot increments, +1000 ft offset
    uint16_t altitude = (uint16_t)((target.data.alt + 1000) / 25.0);
    if (altitude > 0xffe) altitude = 0xffe;
    
    uint8_t misc = 9;  // Miscellaneous indicator
    msg.push_back((altitude >> 4) & 0xff);
    msg.push_back(((altitude & 0xf) << 4) | (misc & 0xf));
    
    // Navigation integrity and accuracy categories
    uint8_t nic = 11;   // Navigation Integrity Category
    uint8_t nacp = 10;  // Navigation Accuracy Category
    msg.push_back(((nic & 0xf) << 4) | (nacp & 0xf));
    
    // Horizontal velocity
    uint16_t h_velocity = (uint16_t)target.data.speed;
    if (h_velocity > 0xffe) h_velocity = 0xffe;
    
    // Vertical velocity (64 fpm units)
    uint16_t v_velocity = 0x800;  // No data flag
    if (target.data.vs != 0) {
        int16_t vs_64fpm = (int16_t)(target.data.vs / 64);
        if (vs_64fpm < 0) {
            v_velocity = (0x1000 + vs_64fpm) & 0xfff;  // 12-bit 2's complement
        } else {
            v_velocity = vs_64fpm & 0xfff;
        }
    }
    
    // Pack velocities (3 bytes total)
    msg.push_back((h_velocity >> 4) & 0xff);
    msg.push_back(((h_velocity & 0xf) << 4) | ((v_velocity >> 8) & 0xf));
    msg.push_back(v_velocity & 0xff);
    
    // Track/heading
    uint8_t track_heading = (uint8_t)(target.data.track / (360.0 / 256));
    msg.push_back(track_heading);
    
    // Emitter category
    uint8_t emitter_cat = 1;  // Light aircraft
    msg.push_back(emitter_cat);
    
    // Call sign (8 bytes)
    for (int i = 0; i < 8; i++) {
        msg.push_back(target.callsign[i]);
    }
    
    // Emergency code
    uint8_t emergency_code = 0;
    msg.push_back(emergency_code << 4);
    
    return gdl90_prepare_message(msg);
}

// Network functions
bool init_network() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        XPLMDebugString("XP2GDL90: Failed to initialize Winsock\n");
        return false;
    }
#endif

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == INVALID_SOCKET) {
        XPLMDebugString("XP2GDL90: Failed to create UDP socket\n");
        return false;
    }

    // Enable broadcast
    int broadcast_enable = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, 
                   (const char*)&broadcast_enable, sizeof(broadcast_enable)) < 0) {
        XPLMDebugString("XP2GDL90: Failed to enable broadcast\n");
        return false;
    }

    // Setup FDPRO address
    memset(&fdpro_addr, 0, sizeof(fdpro_addr));
    fdpro_addr.sin_family = AF_INET;
    fdpro_addr.sin_port = htons(fdpro_port);
    inet_pton(AF_INET, fdpro_ip, &fdpro_addr.sin_addr);

    XPLMDebugString("XP2GDL90: Network initialized successfully\n");
    return true;
}

void cleanup_network() {
    if (udp_socket != INVALID_SOCKET) {
        closesocket(udp_socket);
        udp_socket = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

void send_gdl90_message(const std::vector<uint8_t>& message) {
    if (udp_socket == INVALID_SOCKET || !broadcast_enabled) return;
    
    int result = sendto(udp_socket, (const char*)message.data(), message.size(), 0,
                        (struct sockaddr*)&fdpro_addr, sizeof(fdpro_addr));
    
    if (result == SOCKET_ERROR) {
        XPLMDebugString("XP2GDL90: Failed to send UDP packet\n");
    }
}

// Dataref reading functions
void read_flight_data() {
    if (lat_dataref) current_flight_data.lat = XPLMGetDatad(lat_dataref);
    if (lon_dataref) current_flight_data.lon = XPLMGetDatad(lon_dataref);
    if (alt_dataref) current_flight_data.alt = XPLMGetDatad(alt_dataref) * 3.28084; // m to ft
    if (speed_dataref) current_flight_data.speed = XPLMGetDataf(speed_dataref) * 1.94384; // m/s to kt
    if (track_dataref) current_flight_data.track = XPLMGetDataf(track_dataref);
    if (vs_dataref) current_flight_data.vs = XPLMGetDataf(vs_dataref);
    if (pitch_dataref) current_flight_data.pitch = XPLMGetDataf(pitch_dataref);
    if (roll_dataref) current_flight_data.roll = XPLMGetDataf(roll_dataref);
}

void update_traffic_targets() {
    if (!enable_traffic) return;
    
    double current_time = XPLMGetElapsedTime();
    
    // Read traffic data arrays
    std::vector<float> vs_array(64);
    std::vector<float> psi_array(64);
    
    if (traffic_vs_dataref) {
        XPLMGetDatavf(traffic_vs_dataref, vs_array.data(), 0, 64);
    }
    if (traffic_psi_dataref) {
        XPLMGetDatavf(traffic_psi_dataref, psi_array.data(), 0, 64);
    }
    
    // Update each traffic target
    for (size_t i = 0; i < traffic_targets.size() && i < traffic_lat_datarefs.size(); i++) {
        TrafficTarget& target = traffic_targets[i];
        
        bool updated = false;
        
        // Read position data
        if (traffic_lat_datarefs[i]) {
            double lat = XPLMGetDatad(traffic_lat_datarefs[i]);
            if (fabs(lat) > 0.00001) {
                target.data.lat = lat;
                updated = true;
            }
        }
        
        if (traffic_lon_datarefs[i]) {
            double lon = XPLMGetDatad(traffic_lon_datarefs[i]);
            if (fabs(lon) > 0.00001) {
                target.data.lon = lon;
                updated = true;
            }
        }
        
        if (traffic_alt_datarefs[i]) {
            double alt = XPLMGetDatad(traffic_alt_datarefs[i]);
            if (fabs(alt) > 1.0) {
                target.data.alt = alt * 3.28084; // m to ft
                updated = true;
            }
        }
        
        // Read array data (plane_id is 1-based, but arrays are 0-based with plane 0 being ownship)
        if (target.plane_id < 64) {
            target.data.vs = vs_array[target.plane_id];
            target.data.track = psi_array[target.plane_id];
        }
        
        if (updated) {
            target.last_update = current_time;
            target.active = true;
        } else {
            // Mark inactive if no update for 30 seconds
            if (current_time - target.last_update > 30.0) {
                target.active = false;
            }
        }
    }
}

// UI Functions

// Apply configuration changes
void apply_config_changes() {
    // Get IP address from text field
    char ip_buffer[MAX_IP_LENGTH];
    XPGetWidgetDescriptor(ip_field, ip_buffer, sizeof(ip_buffer));
    
    // Get port from text field
    char port_buffer[MAX_PORT_LENGTH];
    XPGetWidgetDescriptor(port_field, port_buffer, sizeof(port_buffer));
    
    // Validate and apply IP
    if (strlen(ip_buffer) > 0) {
        strncpy(fdpro_ip, ip_buffer, sizeof(fdpro_ip) - 1);
        fdpro_ip[sizeof(fdpro_ip) - 1] = '\0';
    }
    
    // Validate and apply port
    if (strlen(port_buffer) > 0) {
        int new_port = atoi(port_buffer);
        if (new_port > 0 && new_port < 65536) {
            fdpro_port = new_port;
        }
    }
    
    // Update broadcast state
    broadcast_enabled = XPGetWidgetProperty(enable_button, xpProperty_ButtonState, nullptr);
    
    // Reinitialize network with new settings
    if (udp_socket != INVALID_SOCKET) {
        closesocket(udp_socket);
        udp_socket = INVALID_SOCKET;
    }
    init_network();
    
    char msg[256];
    snprintf(msg, sizeof(msg), "XP2GDL90: Configuration updated - IP: %s, Port: %d, Enabled: %s\n", 
             fdpro_ip, fdpro_port, broadcast_enabled ? "Yes" : "No");
    XPLMDebugString(msg);
}

// Widget callback function
int config_widget_callback(XPWidgetMessage inMessage, XPWidgetID inWidget, intptr_t inParam1, intptr_t inParam2) {
    // Suppress unused parameter warnings
    (void)inWidget;
    (void)inParam2;
    
    if (inMessage == xpMessage_CloseButtonPushed) {
        if (config_window != nullptr) {
            XPHideWidget(config_window);
        }
        return 1;
    }
    
    if (inMessage == xpMsg_PushButtonPressed) {
        if (inParam1 == (intptr_t)apply_button) {
            apply_config_changes();
            return 1;
        }
        if (inParam1 == (intptr_t)close_button) {
            if (config_window != nullptr) {
                XPHideWidget(config_window);
            }
            return 1;
        }
        if (inParam1 == (intptr_t)enable_button) {
            // Toggle button state is handled automatically
            return 1;
        }
    }
    
    return 0;
}

// Menu handler function
void config_menu_handler(void* menuRef, void* itemRef) {
    (void)menuRef; // Suppress unused parameter warning
    (void)itemRef; // Suppress unused parameter warning
    
    if (config_window != nullptr) {
        if (XPIsWidgetVisible(config_window)) {
            XPHideWidget(config_window);
        } else {
            XPShowWidget(config_window);
            XPBringRootWidgetToFront(config_window);
        }
    }
}

// Create configuration window
void create_config_window() {
    if (config_window != nullptr) return; // Already created
    
    int left = 200, top = 600, right = 600, bottom = 350;
    
    // Create main window
    config_window = XPCreateWidget(left, top, right, bottom, 1, "XP2GDL90 Configuration", 1, nullptr, xpWidgetClass_MainWindow);
    XPSetWidgetProperty(config_window, xpProperty_MainWindowType, xpMainWindowStyle_MainWindow);
    XPSetWidgetProperty(config_window, xpProperty_MainWindowHasCloseBoxes, 1);
    XPAddWidgetCallback(config_window, config_widget_callback);
    
    // Create enable/disable button
    enable_button = XPCreateWidget(left + 20, top - 40, left + 150, top - 65, 1, "Enable Broadcast", 0, config_window, xpWidgetClass_Button);
    XPSetWidgetProperty(enable_button, xpProperty_ButtonType, xpRadioButton);
    XPSetWidgetProperty(enable_button, xpProperty_ButtonBehavior, xpButtonBehaviorCheckBox);
    XPSetWidgetProperty(enable_button, xpProperty_ButtonState, broadcast_enabled ? 1 : 0);
    
    // Create IP address label and field
    XPCreateWidget(left + 20, top - 80, left + 100, top - 105, 1, "IP Address:", 0, config_window, xpWidgetClass_Caption);
    ip_field = XPCreateWidget(left + 110, top - 80, left + 250, top - 105, 1, fdpro_ip, 0, config_window, xpWidgetClass_TextField);
    XPSetWidgetProperty(ip_field, xpProperty_TextFieldType, xpTextEntryField);
    XPSetWidgetProperty(ip_field, xpProperty_MaxCharacters, MAX_IP_LENGTH - 1);
    
    // Create port label and field
    XPCreateWidget(left + 20, top - 120, left + 100, top - 145, 1, "Port:", 0, config_window, xpWidgetClass_Caption);
    port_field = XPCreateWidget(left + 110, top - 120, left + 200, top - 145, 1, "", 0, config_window, xpWidgetClass_TextField);
    XPSetWidgetProperty(port_field, xpProperty_TextFieldType, xpTextEntryField);
    XPSetWidgetProperty(port_field, xpProperty_MaxCharacters, MAX_PORT_LENGTH - 1);
    
    // Set initial port value
    char port_str[MAX_PORT_LENGTH];
    snprintf(port_str, sizeof(port_str), "%d", fdpro_port);
    XPSetWidgetDescriptor(port_field, port_str);
    
    // Create Apply button
    apply_button = XPCreateWidget(left + 20, top - 170, left + 100, top - 195, 1, "Apply", 0, config_window, xpWidgetClass_Button);
    XPSetWidgetProperty(apply_button, xpProperty_ButtonType, xpPushButton);
    XPSetWidgetProperty(apply_button, xpProperty_ButtonBehavior, xpButtonBehaviorPushButton);
    
    // Create Close button
    close_button = XPCreateWidget(left + 120, top - 170, left + 200, top - 195, 1, "Close", 0, config_window, xpWidgetClass_Button);
    XPSetWidgetProperty(close_button, xpProperty_ButtonType, xpPushButton);
    XPSetWidgetProperty(close_button, xpProperty_ButtonBehavior, xpButtonBehaviorPushButton);
    
    // Initially hide the window
    XPHideWidget(config_window);
}

// Destroy configuration window
void destroy_config_window() {
    if (config_window != nullptr) {
        XPDestroyWidget(config_window, 1);
        config_window = nullptr;
        enable_button = nullptr;
        ip_field = nullptr;
        port_field = nullptr;
        apply_button = nullptr;
        close_button = nullptr;
    }
}

// Flight loop callback
float flight_loop_callback(float elapsedMe, float elapsedSim, int counter, void* refcon) {
    // Suppress unused parameter warnings
    (void)elapsedMe;
    (void)elapsedSim;
    (void)counter;
    (void)refcon;
    
    static double last_heartbeat = 0;
    static double last_position = 0;
    static double last_traffic = 0;
    
    double current_time = XPLMGetElapsedTime();
    
    // Read current flight data
    read_flight_data();
    
    // Update traffic targets if enabled
    if (enable_traffic) {
        update_traffic_targets();
    }
    
    // Send heartbeat every 1 second
    if (current_time - last_heartbeat >= 1.0) {
        auto heartbeat_msg = create_heartbeat();
        send_gdl90_message(heartbeat_msg);
        last_heartbeat = current_time;
        
        char debug_msg[100];
        snprintf(debug_msg, sizeof(debug_msg), "XP2GDL90: Sent heartbeat (%zu bytes)\n", heartbeat_msg.size());
        XPLMDebugString(debug_msg);
    }
    
    // Send position report every 0.5 seconds
    if (current_time - last_position >= 0.5) {
        auto position_msg = create_position_report(current_flight_data);
        send_gdl90_message(position_msg);
        last_position = current_time;
        
        char debug_msg[200];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "XP2GDL90: Sent position report (%zu bytes): LAT=%.6f, LON=%.6f, ALT=%.0f ft\n",
                 position_msg.size(), current_flight_data.lat, current_flight_data.lon, current_flight_data.alt);
        XPLMDebugString(debug_msg);
    }
    
    // Send traffic reports every 0.5 seconds if enabled
    if (enable_traffic && current_time - last_traffic >= 0.5) {
        int active_count = 0;
        for (const auto& target : traffic_targets) {
            if (target.active) {
                auto traffic_msg = create_traffic_report(target);
                send_gdl90_message(traffic_msg);
                active_count++;
            }
        }
        
        if (active_count > 0) {
            char debug_msg[100];
            snprintf(debug_msg, sizeof(debug_msg), "XP2GDL90: Sent %d traffic reports\n", active_count);
            XPLMDebugString(debug_msg);
        }
        
        last_traffic = current_time;
    }
    
    return 0.1f;  // Call again in 0.1 seconds
}

// Plugin lifecycle functions
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    strcpy(outName, "XP2GDL90");
    strcpy(outSig, "com.xplane.xp2gdl90");
    strcpy(outDesc, "X-Plane to GDL-90 data broadcaster for FDPRO");
    
    XPLMDebugString("XP2GDL90: Plugin starting...\n");
    
    // Initialize network
    if (!init_network()) {
        XPLMDebugString("XP2GDL90: Failed to initialize network\n");
        return 0;
    }
    
    // Get ownship datarefs
    lat_dataref = XPLMFindDataRef("sim/flightmodel/position/latitude");
    lon_dataref = XPLMFindDataRef("sim/flightmodel/position/longitude");
    alt_dataref = XPLMFindDataRef("sim/flightmodel/position/elevation");
    speed_dataref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
    track_dataref = XPLMFindDataRef("sim/flightmodel/position/psi");
    vs_dataref = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm");
    pitch_dataref = XPLMFindDataRef("sim/flightmodel/position/theta");
    roll_dataref = XPLMFindDataRef("sim/flightmodel/position/phi");
    
    // Initialize traffic targets if enabled
    if (enable_traffic) {
        traffic_targets.resize(MAX_TRAFFIC_TARGETS);
        traffic_lat_datarefs.resize(MAX_TRAFFIC_TARGETS);
        traffic_lon_datarefs.resize(MAX_TRAFFIC_TARGETS);
        traffic_alt_datarefs.resize(MAX_TRAFFIC_TARGETS);
        
        // Get traffic array datarefs
        traffic_vs_dataref = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/vertical_speed");
        traffic_psi_dataref = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/psi");
        
        // Get individual traffic datarefs and initialize targets
        for (int i = 0; i < MAX_TRAFFIC_TARGETS; i++) {
            char dataref_name[256];
            
            // TCAS position datarefs (1-based plane IDs)
            snprintf(dataref_name, sizeof(dataref_name), 
                     "sim/cockpit2/tcas/targets/position/double/plane%d_lat", i + 1);
            traffic_lat_datarefs[i] = XPLMFindDataRef(dataref_name);
            
            snprintf(dataref_name, sizeof(dataref_name), 
                     "sim/cockpit2/tcas/targets/position/double/plane%d_lon", i + 1);
            traffic_lon_datarefs[i] = XPLMFindDataRef(dataref_name);
            
            snprintf(dataref_name, sizeof(dataref_name), 
                     "sim/cockpit2/tcas/targets/position/double/plane%d_ele", i + 1);
            traffic_alt_datarefs[i] = XPLMFindDataRef(dataref_name);
            
            // Initialize traffic target
            TrafficTarget& target = traffic_targets[i];
            target.plane_id = i + 1;
            target.icao_address = 0x100000 + (i + 1);
            target.data = {};
            target.last_update = 0;
            target.active = false;
            snprintf(target.callsign, sizeof(target.callsign), "TRF%03d  ", i + 1);
        }
        
        XPLMDebugString("XP2GDL90: Traffic targets initialized\n");
    }
    
    // Create configuration menu
    XPLMMenuID plugins_menu = XPLMFindPluginsMenu();
    if (plugins_menu) {
        config_menu_item = XPLMAppendMenuItem(plugins_menu, "XP2GDL90 Config", nullptr, 0);
        config_menu_id = XPLMCreateMenu("XP2GDL90 Configuration", plugins_menu, config_menu_item, config_menu_handler, nullptr);
        if (config_menu_id) {
            XPLMAppendMenuItem(config_menu_id, "Open Configuration Window", nullptr, 0);
        }
    }
    
    XPLMDebugString("XP2GDL90: Plugin started successfully\n");
    return 1;
}

PLUGIN_API void XPluginStop(void) {
    XPLMDebugString("XP2GDL90: Plugin stopping...\n");
    
    // Unregister flight loop callback (legacy API)
    XPLMUnregisterFlightLoopCallback(flight_loop_callback, nullptr);
    
    // Clean up UI
    destroy_config_window();
    if (config_menu_id) {
        XPLMDestroyMenu(config_menu_id);
        config_menu_id = nullptr;
    }
    
    cleanup_network();
    XPLMDebugString("XP2GDL90: Plugin stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
    XPLMDebugString("XP2GDL90: Plugin enabled - starting GDL-90 broadcast\n");
    
    // Create configuration window
    create_config_window();
    
    // Register flight loop callback (legacy API)
    XPLMRegisterFlightLoopCallback(flight_loop_callback, 1.0f, nullptr);
    
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    XPLMDebugString("XP2GDL90: Plugin disabled - stopping GDL-90 broadcast\n");
    
    // Unregister flight loop callback (legacy API)
    XPLMUnregisterFlightLoopCallback(flight_loop_callback, nullptr);
    
    // Hide configuration window
    if (config_window) {
        XPHideWidget(config_window);
    }
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void* inParam) {
    // Suppress unused parameter warnings
    (void)inFromWho;
    (void)inMessage;
    (void)inParam;
    
    // Handle plugin messages if needed
}
