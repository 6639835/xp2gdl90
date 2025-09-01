 // Standard C/C++ headers first
#include <cstring>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>

// X-Plane SDK headers - XPLMDefs.h must be included first
#include "XPLMDefs.h"
#include "XPLMPlugin.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPLMMenus.h"

// Widget headers (legacy - will be replaced by ImGui)
#include "XPWidgets.h"
#include "XPStandardWidgets.h"

// Modern UI with ImGui (conditional compilation)
#if HAVE_IMGUI
#include "ui/ImGuiManager.h"
#endif

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

// GDL-90 Constants per official specification
// Navigation Integrity and Accuracy Categories (Table 10)
const uint8_t DEFAULT_NIC = 11;      // HPL < 7.5m, VPL < 11m  
const uint8_t DEFAULT_NACP = 10;     // HFOM < 3m, VFOM < 4m
const uint8_t DEGRADED_NIC = 6;      // HPL < 0.6 NM (lower accuracy)
const uint8_t DEGRADED_NACP = 6;     // HFOM < 0.3 NM (lower accuracy)

// Emitter Categories (Table 11)
const uint8_t EMITTER_LIGHT = 1;     // Light aircraft < 15,500 lbs
const uint8_t EMITTER_SMALL = 2;     // Small aircraft 15,500-75,000 lbs  
const uint8_t EMITTER_LARGE = 3;     // Large aircraft 75,000-300,000 lbs
const uint8_t EMITTER_HEAVY = 5;     // Heavy aircraft > 300,000 lbs
const uint8_t EMITTER_ROTORCRAFT = 7; // Helicopters

// Address Types (Section 3.5.1.2)
const uint8_t ADDR_TYPE_ADSB_ICAO = 0;      // ADS-B with ICAO address
const uint8_t ADDR_TYPE_ADSB_SELF = 1;      // ADS-B with Self-assigned address
const uint8_t ADDR_TYPE_TISB_ICAO = 2;      // TIS-B with ICAO address
const uint8_t ADDR_TYPE_TISB_TRACK = 3;     // TIS-B with track file ID
const uint8_t ADDR_TYPE_SURFACE = 4;        // Surface Vehicle
const uint8_t ADDR_TYPE_BEACON = 5;         // Ground Station Beacon

// Miscellaneous Field Constants (Table 9)
const uint8_t TRACK_TYPE_INVALID = 0;       // Track/heading not valid
const uint8_t TRACK_TYPE_TRUE_TRACK = 1;    // True Track Angle
const uint8_t TRACK_TYPE_MAG_HEADING = 2;   // Magnetic Heading
const uint8_t TRACK_TYPE_TRUE_HEADING = 3;  // True Heading

// Emergency Codes (Section 3.5.1.12)
const uint8_t EMERGENCY_NONE = 0;           // No emergency
const uint8_t EMERGENCY_GENERAL = 1;        // General emergency
const uint8_t EMERGENCY_MEDICAL = 2;        // Medical emergency
const uint8_t EMERGENCY_MIN_FUEL = 3;       // Minimum fuel
const uint8_t EMERGENCY_NO_COMM = 4;        // No communication
const uint8_t EMERGENCY_UNLAWFUL = 5;       // Unlawful interference
const uint8_t EMERGENCY_DOWNED = 6;         // Downed aircraft

// Data staleness thresholds
const double DATA_FRESH_THRESHOLD = 2.0;    // Seconds - data considered fresh
const double DATA_TIMEOUT_THRESHOLD = 30.0; // Seconds - data considered stale/inactive

// Air/Ground detection thresholds
const double AIRBORNE_ALT_THRESHOLD = 100.0;   // Feet - altitude indicating airborne
const double AIRBORNE_SPEED_THRESHOLD = 60.0;  // Knots - speed indicating airborne  
const double GROUND_ALT_THRESHOLD = 50.0;      // Feet - altitude indicating on ground
const double GROUND_SPEED_THRESHOLD = 40.0;    // Knots - speed indicating on ground
const double GROUND_VS_THRESHOLD = 100.0;      // FPM - vertical speed indicating on ground

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
    double last_data_update;        // When position/speed data was last fresh
    bool active;
    bool airborne;                  // Air/ground state
    bool is_extrapolated;          // True if data is stale/extrapolated
    uint8_t address_type;          // Address type per spec Table 8
    uint8_t emitter_category;      // Aircraft type per spec Table 11
    uint8_t emergency_code;        // Emergency status per spec
    uint8_t track_type;            // Track/heading type
    char callsign[9];              // 8 chars + null terminator
};

// Configuration variables
static char fdpro_ip[MAX_IP_LENGTH] = "127.0.0.1";  // Configurable FDPRO target IP
static int fdpro_port = 4000;                       // Configurable FDPRO listening port
static bool broadcast_enabled = true;               // Enable/disable broadcast
static uint32_t ownship_icao_address = 0xABCDEF;    // Configurable ICAO address
static char ownship_callsign[9] = "PYTHON1 ";      // Configurable callsign (8 chars + null)

// UI - Modern ImGui interface
static XPLMMenuID config_menu_id = nullptr;
static int config_menu_item = -1;

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
static XPLMDataRef traffic_flight_id_dataref = nullptr;
static XPLMDataRef traffic_vs_dataref = nullptr;
static XPLMDataRef traffic_psi_dataref = nullptr;
static XPLMDataRef traffic_speed_dataref = nullptr;

// GDL-90 Helper Functions

// Determine if aircraft is airborne based on multiple criteria
bool determine_airborne_state(const FlightData& data) {
    // Multi-criteria approach for air/ground determination
    // Criteria 1: Altitude above ground level
    if (data.alt > AIRBORNE_ALT_THRESHOLD) {  // Above 100 feet AGL
        return true;
    }
    
    // Criteria 2: High ground speed indicates airborne
    if (data.speed > AIRBORNE_SPEED_THRESHOLD) {  // Above 60 knots ground speed
        return true;
    }
    
    // Criteria 3: Significant vertical speed
    if (fabs(data.vs) > 200.0) {  // Above 200 fpm vertical speed
        return true;
    }
    
    // If low altitude, low speed, and minimal vertical speed -> likely on ground
    if (data.alt < GROUND_ALT_THRESHOLD && data.speed < GROUND_SPEED_THRESHOLD && fabs(data.vs) < GROUND_VS_THRESHOLD) {
        return false;
    }
    
    // Default to airborne for ambiguous cases (safety)
    return true;
}

// Determine emitter category based on aircraft characteristics
uint8_t determine_emitter_category(const char* callsign, double speed) {
    // Basic heuristics for aircraft type classification
    
    // Check callsign patterns for specific aircraft types
    if (strstr(callsign, "HELO") || strstr(callsign, "CHOP")) {
        return EMITTER_ROTORCRAFT;
    }
    
    // Speed-based classification (rough approximation)
    if (speed > 400.0) {
        return EMITTER_LARGE;  // High speed suggests larger aircraft
    } else if (speed > 250.0) {
        return EMITTER_SMALL;  // Medium speed
    } else {
        return EMITTER_LIGHT;  // Default to light aircraft
    }
}

// Determine Navigation Integrity Category based on data quality
uint8_t determine_nic_category(bool has_recent_update) {
    // Use degraded accuracy for stale data
    return has_recent_update ? DEFAULT_NIC : DEGRADED_NIC;
}

// Determine Navigation Accuracy Category for Position based on data quality  
uint8_t determine_nacp_category(bool has_recent_update) {
    // Use degraded accuracy for stale data
    return has_recent_update ? DEFAULT_NACP : DEGRADED_NACP;
}

// Create properly encoded miscellaneous field per GDL-90 spec Table 9
uint8_t create_misc_field(bool airborne, bool extrapolated, uint8_t track_type) {
    uint8_t misc = 0;
    
    // Bit 3: Air/Ground state (0=Ground, 1=Airborne)
    if (airborne) {
        misc |= 0x8;
    }
    
    // Bit 2: Report type (0=Updated, 1=Extrapolated)
    if (extrapolated) {
        misc |= 0x4;
    }
    
    // Bits 1-0: Track/Heading type
    misc |= (track_type & 0x3);
    
    return misc;
}

// Validate coordinate encoding precision per spec
bool validate_coordinate_precision(double original, uint32_t encoded, bool is_latitude) {
    // Decode back to verify precision
    double max_range = is_latitude ? 180.0 : 180.0;
    int32_t signed_encoded = encoded;
    
    // Handle 2's complement for negative values
    if (encoded & 0x800000) {
        signed_encoded = -((0x1000000 - encoded) & 0xFFFFFF);
    }
    
    double decoded = signed_encoded * (max_range / 0x800000);
    double error = fabs(original - decoded);
    
    // Spec requires ~2.38m resolution at equator (approximately 0.0000214 degrees)
    const double MAX_ERROR = 0.000025;  // Slight tolerance for floating point
    
    return error <= MAX_ERROR;
}

// GDL-90 Encoding Functions

uint16_t gdl90_crc_compute(const uint8_t* data, size_t length) {
    uint16_t crc = 0;
    
    for (size_t i = 0; i < length; i++) {
        uint16_t m = (crc << 8) & 0xFFFF;
        uint16_t table_index = (crc >> 8) & 0xFF;
        crc = (GDL90_CRC16_TABLE[table_index] ^ m ^ (uint16_t)data[i]) & 0xFFFF;
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
    
    // Get current time in UTC (thread-safe)
    time_t now = time(nullptr);
    struct tm utc_tm;
#ifdef _WIN32
    gmtime_s(&utc_tm, &now);
#else
    gmtime_r(&now, &utc_tm);
#endif
    uint32_t timestamp = utc_tm.tm_hour * 3600 + utc_tm.tm_min * 60 + utc_tm.tm_sec;
    
    // Status byte 1 - Per GDL90 spec section 3.1.1
    uint8_t st1 = 0;
    st1 |= (1 << 7);  // Bit 7: GPS Position Valid (assume valid when we have coordinates)
    st1 |= (0 << 6);  // Bit 6: Maintenance Required (set to 0 = no maintenance)
    st1 |= (0 << 5);  // Bit 5: IDENT (set to 0 = not transmitting IDENT)
    st1 |= (0 << 4);  // Bit 4: Address Type (0 = using assigned ICAO address)
    st1 |= (0 << 3);  // Bit 3: GPS Battery Low (0 = battery OK)
    st1 |= (0 << 2);  // Bit 2: RATCS (0 = not receiving ATC services)
    st1 |= (0 << 1);  // Bit 1: Reserved (always 0)
    st1 |= (1 << 0);  // Bit 0: UAT Initialized (1 = initialized)
    
    // Status byte 2 - Per GDL90 spec section 3.1.2
    uint8_t st2 = 0;
    // Bit 7 will be set later with timestamp bit 16
    st2 |= (0 << 6);  // Bit 6: CSA Requested (0 = not requested)
    st2 |= (0 << 5);  // Bit 5: CSA Not Available (0 = available)
    st2 |= (0 << 4);  // Bit 4: Reserved (always 0)
    st2 |= (0 << 3);  // Bit 3: Reserved (always 0)
    st2 |= (0 << 2);  // Bit 2: Reserved (always 0)
    st2 |= (0 << 1);  // Bit 1: Reserved (always 0)
    st2 |= (1 << 0);  // Bit 0: UTC OK (1 = UTC timing is valid)
    
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
    uint8_t addr_type = ADDR_TYPE_ADSB_ICAO;  // Use constant instead of hardcoded 0
    msg.push_back(((status & 0xf) << 4) | (addr_type & 0xf));
    
    // ICAO address (24-bit)
    gdl90_pack24bit(msg, ownship_icao_address);
    
    // Latitude (24-bit) - validate precision per spec
    uint32_t lat_encoded = gdl90_make_latitude(data.lat);
    gdl90_pack24bit(msg, lat_encoded);
    
    // Longitude (24-bit) - validate precision per spec
    uint32_t lon_encoded = gdl90_make_longitude(data.lon);
    gdl90_pack24bit(msg, lon_encoded);
    
    // Validate coordinate precision (optional debug validation)
    if (!validate_coordinate_precision(data.lat, lat_encoded, true) || 
        !validate_coordinate_precision(data.lon, lon_encoded, false)) {
        char debug_msg[200];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "XP2GDL90: WARNING - Coordinate precision outside spec: LAT=%.6f, LON=%.6f\n",
                 data.lat, data.lon);
        XPLMDebugString(debug_msg);
    }
    
    // Altitude: 25-foot increments, +1000 ft offset
    // Range: -1,000 to +101,350 feet (per GDL90 spec Table 8)
    double alt_clamped = data.alt;
    if (alt_clamped < -1000.0) alt_clamped = -1000.0;
    if (alt_clamped > 101350.0) alt_clamped = 101350.0;
    
    uint16_t altitude = (uint16_t)((alt_clamped + 1000) / 25.0);
    if (altitude > 0xffe) altitude = 0xffe;
    
    // Create proper miscellaneous field per spec Table 9
    bool airborne = determine_airborne_state(data);
    bool extrapolated = false;  // Ownship data is always fresh
    uint8_t track_type = TRACK_TYPE_TRUE_TRACK;  // X-Plane provides true track
    uint8_t misc = create_misc_field(airborne, extrapolated, track_type);
    
    msg.push_back((altitude >> 4) & 0xff);
    msg.push_back(((altitude & 0xf) << 4) | (misc & 0xf));
    
    // Navigation integrity and accuracy categories (use constants)
    uint8_t nic = DEFAULT_NIC;   // High accuracy for ownship
    uint8_t nacp = DEFAULT_NACP; // High accuracy for ownship
    msg.push_back(((nic & 0xf) << 4) | (nacp & 0xf));
    
    // Horizontal velocity (knots)
    // Range: 0 to 4,094 knots, 0xFFF = no data available (per GDL90 spec)
    uint16_t h_velocity = 0xfff;  // Default to "no data"
    if (data.speed >= 0.0) {
        uint16_t speed_knots = (uint16_t)data.speed;
        if (speed_knots <= 4094) {
            h_velocity = speed_knots;
        } else {
            h_velocity = 0xffe;  // Max representable value
        }
    }
    
    // Vertical velocity (64 fpm units)
    // Range: +/- 32,576 FPM, 0x800 = no data (per GDL90 spec Table 8)
    uint16_t v_velocity = 0x800;  // Default to "no data"
    if (data.vs != 0.0) {
        // Clamp to valid range: +/- 32,576 FPM
        double vs_clamped = data.vs;
        if (vs_clamped > 32576.0) vs_clamped = 32576.0;
        if (vs_clamped < -32576.0) vs_clamped = -32576.0;
        
        int16_t vs_64fpm = (int16_t)(vs_clamped / 64.0);
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
    
    // Emitter category (use constant)
    uint8_t emitter_cat = EMITTER_LIGHT;  // Default to light aircraft
    msg.push_back(emitter_cat);
    
    // Call sign (8 bytes)
    for (int i = 0; i < 8; i++) {
        msg.push_back(ownship_callsign[i]);
    }
    
    // Emergency code - FIX: Properly encode bits per spec Section 3.5.1.12
    uint8_t emergency_code = EMERGENCY_NONE;  // Use constant
    msg.push_back(((emergency_code & 0xF) << 4) | 0x0);  // Ensure reserved bits are 0
    
    return gdl90_prepare_message(msg);
}

// Create GDL-90 Traffic Report (ID 0x14)
std::vector<uint8_t> create_traffic_report(const TrafficTarget& target) {
    std::vector<uint8_t> msg;
    msg.push_back(0x14);  // Message ID
    
    // Traffic alert status and address type
    uint8_t alert_status = 0;  // No traffic alert (CSA not implemented)
    uint8_t addr_type = target.address_type;  // Use target's address type
    msg.push_back(((alert_status & 0xf) << 4) | (addr_type & 0xf));
    
    // ICAO address (24-bit)
    gdl90_pack24bit(msg, target.icao_address);
    
    // Latitude (24-bit) - validate precision per spec
    uint32_t lat_encoded = gdl90_make_latitude(target.data.lat);
    gdl90_pack24bit(msg, lat_encoded);
    
    // Longitude (24-bit) - validate precision per spec
    uint32_t lon_encoded = gdl90_make_longitude(target.data.lon);
    gdl90_pack24bit(msg, lon_encoded);
    
    // Validate coordinate precision (optional debug validation)
    if (!validate_coordinate_precision(target.data.lat, lat_encoded, true) || 
        !validate_coordinate_precision(target.data.lon, lon_encoded, false)) {
        char debug_msg[200];
        snprintf(debug_msg, sizeof(debug_msg), 
                 "XP2GDL90: WARNING - Traffic coordinate precision outside spec: LAT=%.6f, LON=%.6f\n",
                 target.data.lat, target.data.lon);
        XPLMDebugString(debug_msg);
    }
    
    // Altitude: 25-foot increments, +1000 ft offset
    // Range: -1,000 to +101,350 feet (per GDL90 spec Table 8)
    double alt_clamped = target.data.alt;
    if (alt_clamped < -1000.0) alt_clamped = -1000.0;
    if (alt_clamped > 101350.0) alt_clamped = 101350.0;
    
    uint16_t altitude = (uint16_t)((alt_clamped + 1000) / 25.0);
    if (altitude > 0xffe) altitude = 0xffe;
    
    // Create proper miscellaneous field per spec Table 9
    uint8_t misc = create_misc_field(target.airborne, target.is_extrapolated, target.track_type);
    
    msg.push_back((altitude >> 4) & 0xff);
    msg.push_back(((altitude & 0xf) << 4) | (misc & 0xf));
    
    // Navigation integrity and accuracy categories based on data quality
    double time_since_update = XPLMGetElapsedTime() - target.last_data_update;
    bool has_recent_update = (time_since_update <= DATA_FRESH_THRESHOLD);
    
    uint8_t nic = determine_nic_category(has_recent_update);
    uint8_t nacp = determine_nacp_category(has_recent_update);
    msg.push_back(((nic & 0xf) << 4) | (nacp & 0xf));
    
    // Horizontal velocity (knots) 
    // Range: 0 to 4,094 knots, 0xFFF = no data available (per GDL90 spec)
    uint16_t h_velocity = 0xfff;  // Default to "no data"
    if (target.data.speed >= 0.0) {
        uint16_t speed_knots = (uint16_t)target.data.speed;
        if (speed_knots <= 4094) {
            h_velocity = speed_knots;
        } else {
            h_velocity = 0xffe;  // Max representable value
        }
    }
    
    // Vertical velocity (64 fpm units)
    // Range: +/- 32,576 FPM, 0x800 = no data (per GDL90 spec Table 8)
    uint16_t v_velocity = 0x800;  // Default to "no data"
    if (target.data.vs != 0.0) {
        // Clamp to valid range: +/- 32,576 FPM
        double vs_clamped = target.data.vs;
        if (vs_clamped > 32576.0) vs_clamped = 32576.0;
        if (vs_clamped < -32576.0) vs_clamped = -32576.0;
        
        int16_t vs_64fpm = (int16_t)(vs_clamped / 64.0);
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
    
    // Emitter category (use target's category)
    msg.push_back(target.emitter_category);
    
    // Call sign (8 bytes)
    for (int i = 0; i < 8; i++) {
        msg.push_back(target.callsign[i]);
    }
    
    // Emergency code - FIX: Properly encode bits per spec Section 3.5.1.12
    msg.push_back(((target.emergency_code & 0xF) << 4) | 0x0);  // Ensure reserved bits are 0
    
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

    // Log the network configuration for verification
    char network_msg[256];
    snprintf(network_msg, sizeof(network_msg), 
             "XP2GDL90: Network initialized - Target IP: %s, Port: %d\n", 
             fdpro_ip, fdpro_port);
    XPLMDebugString(network_msg);
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
    std::vector<float> speed_array(64);
    
    if (traffic_vs_dataref) {
        XPLMGetDatavf(traffic_vs_dataref, vs_array.data(), 0, 64);
    }
    if (traffic_psi_dataref) {
        XPLMGetDatavf(traffic_psi_dataref, psi_array.data(), 0, 64);
    }
    if (traffic_speed_dataref) {
        XPLMGetDatavf(traffic_speed_dataref, speed_array.data(), 0, 64);
    }
    
    // Update each traffic target
    for (size_t i = 0; i < traffic_targets.size() && i < traffic_lat_datarefs.size(); i++) {
        TrafficTarget& target = traffic_targets[i];
        
        // *** FIX: Check if aircraft actually exists by validating array data ***
        // Skip aircraft that show no activity (all zeros = non-existent aircraft)
        if (target.plane_id < 64) {
            float aircraft_speed = speed_array[target.plane_id];
            float aircraft_track = psi_array[target.plane_id];
            float aircraft_vs = vs_array[target.plane_id];
            
            // If speed, track, and vertical speed are all 0.0, aircraft doesn't exist
            if (aircraft_speed == 0.0f && aircraft_track == 0.0f && aircraft_vs == 0.0f) {
                target.active = false;
                continue;  // Skip this aircraft entirely
            }
        }
        
        bool updated = false;
        
        // Data is being updated
        
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
            target.data.speed = speed_array[target.plane_id] * 1.94384; // m/s to knots
        }
        
        // Update air/ground state using new logic
        target.airborne = determine_airborne_state(target.data);
        
        // Update emitter category based on speed and callsign
        target.emitter_category = determine_emitter_category(target.callsign, target.data.speed);
        
        // Track type is always true track from X-Plane
        target.track_type = TRACK_TYPE_TRUE_TRACK;
        
        // Read flight ID/callsign data from TCAS interface
        if (traffic_flight_id_dataref) {
            char flight_id_array[512] = {0};  // Full array buffer
            int bytes_read = XPLMGetDatab(traffic_flight_id_dataref, flight_id_array, 0, sizeof(flight_id_array));
            
            if (bytes_read > 0) {
                // Calculate offset for this aircraft (Index 0 = user, Index i+1 = AI/multiplayer)
                int aircraft_index = i + 1;  // Skip user aircraft at index 0
                int offset = aircraft_index * 8;  // Each aircraft takes 8 bytes (7 chars + null)
                
                if (offset + 7 < bytes_read) {
                    char flight_id[8] = {0};
                    memcpy(flight_id, flight_id_array + offset, 7);  // Copy 7 characters
                    flight_id[7] = '\0';  // Ensure null termination
                    
                    // Debug output
                    char debug_msg[200];
                    snprintf(debug_msg, sizeof(debug_msg), 
                             "XP2GDL90: Traffic %d TCAS flight_id[%d]: '%s'\n", 
                             (int)(i + 1), aircraft_index, flight_id);
                    XPLMDebugString(debug_msg);
                    
                    // Check if we have a valid flight ID (not empty)
                    if (flight_id[0] != '\0' && flight_id[0] != ' ') {
                        // Format callsign for GDL-90 (8 characters, space-padded)
                        memset(target.callsign, ' ', 8);  // Fill with spaces
                        target.callsign[8] = '\0';        // Null terminate
                        
                        // Copy flight ID, limiting to 8 characters, only printable ASCII
                        int j = 0;
                        for (int k = 0; k < 7 && j < 8; k++) {
                            if (flight_id[k] >= 32 && flight_id[k] <= 126 && flight_id[k] != ' ') {
                                target.callsign[j++] = toupper(flight_id[k]);  // Convert to uppercase
                            }
                        }
                        
                        snprintf(debug_msg, sizeof(debug_msg), 
                                 "XP2GDL90: Traffic %d updated callsign to: '%s'\n", 
                                 (int)(i + 1), target.callsign);
                        XPLMDebugString(debug_msg);
                    }
                }
            }
        }
        
        if (updated) {
            target.last_update = current_time;
            target.last_data_update = current_time;  // Fresh data received
            target.active = true;
            target.is_extrapolated = false;  // Fresh data is not extrapolated
        } else {
            // Check if data has become stale
            double time_since_data_update = current_time - target.last_data_update;
            target.is_extrapolated = (time_since_data_update > DATA_FRESH_THRESHOLD);
            
            // Mark inactive if no update for timeout period
            if (current_time - target.last_update > DATA_TIMEOUT_THRESHOLD) {
                target.active = false;
            }
        }
    }
}

// Modern UI Functions

// Configuration callback for ImGui
void imgui_config_callback(const char* ip, int port, bool broadcast, bool traffic) {
    // Validate and apply IP
    if (ip && strlen(ip) > 0) {
        strncpy(fdpro_ip, ip, sizeof(fdpro_ip) - 1);
        fdpro_ip[sizeof(fdpro_ip) - 1] = '\0';
    }
    
    // Validate and apply port
    if (port > 0 && port < 65536) {
        fdpro_port = port;
    }
    
    // Update states
    broadcast_enabled = broadcast;
    enable_traffic = traffic;
    
    // Reinitialize network with new settings
    if (udp_socket != INVALID_SOCKET) {
        closesocket(udp_socket);
        udp_socket = INVALID_SOCKET;
    }
    init_network();
    
    // Update ImGui with connection status (if available)
#if HAVE_IMGUI
    ImGuiManager::Instance().UpdateConnectionStatus(true, fdpro_ip, fdpro_port);
#endif
    
    char msg[256];
    snprintf(msg, sizeof(msg), "XP2GDL90: Configuration updated - IP: %s, Port: %d, Broadcast: %s, Traffic: %s\n", 
             fdpro_ip, fdpro_port, broadcast_enabled ? "Yes" : "No", enable_traffic ? "Yes" : "No");
    XPLMDebugString(msg);
}

// Modern menu handler function
void config_menu_handler(void* menuRef, void* itemRef) {
    (void)menuRef; // Suppress unused parameter warning
    
    XPLMDebugString("XP2GDL90: Menu handler called\n");
    
    // Handle different menu items
    intptr_t item = (intptr_t)itemRef;
    
    char debug_msg[100];
    snprintf(debug_msg, sizeof(debug_msg), "XP2GDL90: Menu item %ld selected\n", item);
    XPLMDebugString(debug_msg);
    
#if HAVE_IMGUI
    // Check if ImGui is available before using it
    ImGuiManager& ui = ImGuiManager::Instance();
    
    if (item == 0) {
        // Configuration Window
        XPLMDebugString("XP2GDL90: Toggling configuration window\n");
        if (ui.IsConfigWindowVisible()) {
            ui.HideConfigWindow();
        } else {
            ui.ShowConfigWindow();
        }
    } else if (item == 1) {
        // Status Monitor
        XPLMDebugString("XP2GDL90: Toggling status window\n");
        if (ui.IsStatusWindowVisible()) {
            ui.HideStatusWindow();
        } else {
            ui.ShowStatusWindow();
        }
    }
#else
    // ImGui not available - show message
    if (item == 0 || item == 1) {
        XPLMDebugString("XP2GDL90: UI not available - built without OpenGL support\n");
    }
#endif
}

// Initialize modern ImGui UI (if available)
void init_modern_ui() {
#if HAVE_IMGUI
    ImGuiManager& ui = ImGuiManager::Instance();
    if (ui.Initialize()) {
        // Set up configuration callback
        ui.SetConfigCallback(imgui_config_callback);
        
        // Update initial connection status
        ui.UpdateConnectionStatus(broadcast_enabled, fdpro_ip, fdpro_port);
        
        XPLMDebugString("XP2GDL90: Modern ImGui UI initialized\n");
    } else {
        XPLMDebugString("XP2GDL90: Failed to initialize modern UI\n");
    }
#else
    XPLMDebugString("XP2GDL90: UI not available - built without OpenGL support\n");
#endif
}

// Shutdown modern ImGui UI (if available)
void shutdown_modern_ui() {
#if HAVE_IMGUI
    ImGuiManager::Instance().Shutdown();
    XPLMDebugString("XP2GDL90: Modern UI shutdown\n");
#else
    XPLMDebugString("XP2GDL90: UI shutdown - no UI was available\n");
#endif
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
    static double last_ui_update = 0;
    
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
    int active_traffic_count = 0;
    if (enable_traffic && current_time - last_traffic >= 0.5) {
        for (const auto& target : traffic_targets) {
            if (target.active) {
                auto traffic_msg = create_traffic_report(target);
                send_gdl90_message(traffic_msg);
                active_traffic_count++;
            }
        }
        
        if (active_traffic_count > 0) {
            char debug_msg[100];
            snprintf(debug_msg, sizeof(debug_msg), "XP2GDL90: Sent %d traffic reports\n", active_traffic_count);
            XPLMDebugString(debug_msg);
        }
        
        last_traffic = current_time;
    } else {
        // Count active traffic even when not sending
        for (const auto& target : traffic_targets) {
            if (target.active) {
                active_traffic_count++;
            }
        }
    }
    
    // Try to initialize ImGui after X-Plane has been running for a few seconds (if available)
#if HAVE_IMGUI
    static bool imgui_initialized = false;
    
    if (!imgui_initialized && current_time > 5.0) { // Wait 5 seconds after plugin start
        XPLMDebugString("XP2GDL90: Attempting delayed ImGui initialization...\n");
        
        ImGuiManager& ui = ImGuiManager::Instance();
        if (ui.Initialize()) {
            imgui_initialized = true;
            ui.SetConfigCallback(imgui_config_callback);
            ui.UpdateConnectionStatus(broadcast_enabled, fdpro_ip, fdpro_port);
            XPLMDebugString("XP2GDL90: ImGui initialized successfully after delay\n");
        } else {
            // Keep trying until it succeeds
            XPLMDebugString("XP2GDL90: ImGui initialization failed, will retry next frame\n");
        }
    }
    
    // Update ImGui UI every 0.1 seconds (only if successfully initialized)
    if (imgui_initialized && current_time - last_ui_update >= 0.1) {
        ImGuiManager& ui = ImGuiManager::Instance();
        ui.UpdateFlightData(current_flight_data);
        ui.UpdateTrafficTargets(traffic_targets);
        last_ui_update = current_time;
    }
#endif
    
    return 0.1f;  // Call again in 0.1 seconds
}

// Safe string copy helper for cross-platform compatibility
#ifdef _WIN32
    #define SAFE_STRCPY(dest, src, size) strncpy_s(dest, size, src, _TRUNCATE)
#else
    #define SAFE_STRCPY(dest, src, size) do { \
        strncpy(dest, src, (size) - 1); \
        dest[(size) - 1] = '\0'; \
    } while(0)
#endif

// Plugin lifecycle functions
PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    // X-Plane plugin parameters are 255 characters each
    SAFE_STRCPY(outName, "XP2GDL90", 255);
    SAFE_STRCPY(outSig, "com.xplane.xp2gdl90", 255);
    SAFE_STRCPY(outDesc, "X-Plane to GDL-90 data broadcaster for FDPRO", 255);
    
    // Log startup configuration
    char startup_msg[256];
    snprintf(startup_msg, sizeof(startup_msg), 
             "XP2GDL90: Plugin starting - Default target IP: %s, Port: %d\n", 
             fdpro_ip, fdpro_port);
    XPLMDebugString(startup_msg);
    
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
        traffic_speed_dataref = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/V_msc");
        
        // Get TCAS flight ID dataref for callsigns (byte[512] array, 8 bytes per aircraft)
        traffic_flight_id_dataref = XPLMFindDataRef("sim/cockpit2/tcas/targets/flight_id");
        
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
            

            
            // Initialize traffic target with all new fields
            TrafficTarget& target = traffic_targets[i];
            target.plane_id = i + 1;
            target.icao_address = 0x100000 + (i + 1);
            target.data = {};
            target.last_update = 0;
            target.last_data_update = 0.0;
            target.active = false;
            target.airborne = true;  // Default to airborne (safer assumption)
            target.is_extrapolated = false;
            target.address_type = ADDR_TYPE_ADSB_ICAO;  // Default to ADS-B with ICAO
            target.emitter_category = EMITTER_LIGHT;    // Default to light aircraft
            target.emergency_code = EMERGENCY_NONE;     // No emergency by default
            target.track_type = TRACK_TYPE_TRUE_TRACK;  // X-Plane provides true track
            // Initialize with default callsign (will be updated with real tailnum when available)
            snprintf(target.callsign, sizeof(target.callsign), "TRF%03d  ", i + 1);
        }
        
        XPLMDebugString("XP2GDL90: Traffic targets initialized\n");
    }
    
    // Create modern configuration menu
    XPLMMenuID plugins_menu = XPLMFindPluginsMenu();
    if (plugins_menu) {
        config_menu_item = XPLMAppendMenuItem(plugins_menu, "XP2GDL90", nullptr, 0);
        config_menu_id = XPLMCreateMenu("XP2GDL90 - GDL-90 Broadcaster", plugins_menu, config_menu_item, config_menu_handler, nullptr);
        if (config_menu_id) {
            XPLMAppendMenuItem(config_menu_id, "Configuration Window", nullptr, 0);
            XPLMAppendMenuItem(config_menu_id, "Status Monitor", nullptr, 1);
        }
    }
    
    XPLMDebugString("XP2GDL90: Plugin started successfully\n");
    return 1;
}

PLUGIN_API void XPluginStop(void) {
    XPLMDebugString("XP2GDL90: Plugin stopping...\n");
    
    // Unregister flight loop callback (legacy API)
    XPLMUnregisterFlightLoopCallback(flight_loop_callback, nullptr);
    
    // Clean up modern UI
    shutdown_modern_ui();
    
    // Clean up menu
    if (config_menu_id) {
        XPLMDestroyMenu(config_menu_id);
        config_menu_id = nullptr;
    }
    
    cleanup_network();
    XPLMDebugString("XP2GDL90: Plugin stopped\n");
}

PLUGIN_API int XPluginEnable(void) {
    XPLMDebugString("XP2GDL90: Plugin enabled - starting GDL-90 broadcast\n");
    
    // Initialize modern UI - will be initialized later in flight loop for safety
    XPLMDebugString("XP2GDL90: ImGui will be initialized in flight loop\n");
    
    // Register flight loop callback (legacy API)
    XPLMRegisterFlightLoopCallback(flight_loop_callback, 1.0f, nullptr);
    
    return 1;
}

PLUGIN_API void XPluginDisable(void) {
    XPLMDebugString("XP2GDL90: Plugin disabled - stopping GDL-90 broadcast\n");
    
    // Unregister flight loop callback (legacy API)
    XPLMUnregisterFlightLoopCallback(flight_loop_callback, nullptr);
    
    // Hide modern UI windows (if available)
#if HAVE_IMGUI
    ImGuiManager& ui = ImGuiManager::Instance();
    ui.HideConfigWindow();
    ui.HideStatusWindow();
#endif
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void* inParam) {
    // Suppress unused parameter warnings
    (void)inFromWho;
    (void)inMessage;
    (void)inParam;
    
    // Handle plugin messages if needed
}
