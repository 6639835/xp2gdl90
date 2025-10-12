#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <cstdint>

/**
 * Configuration management for XP2GDL90 plugin
 * Reads configuration from INI file
 */

namespace config {

struct Config {
    // Network settings
    std::string target_ip;      // Target IP address (e.g., "192.168.1.100")
    uint16_t target_port;       // Target port number (default: 4000)
    
    // Ownship settings
    uint32_t icao_address;      // 24-bit ICAO address (default: 0xABCDEF)
    std::string callsign;       // Aircraft callsign (up to 8 characters)
    uint8_t emitter_category;   // Emitter category (default: 1 = Light)
    
    // Update rates
    float heartbeat_rate;       // Heartbeat messages per second (default: 1.0)
    float position_rate;        // Position report rate per second (default: 2.0)
    
    // Accuracy settings
    uint8_t nic;                // Navigation Integrity Category (default: 11)
    uint8_t nacp;               // Navigation Accuracy Category (default: 11)
    
    // Debug settings
    bool debug_logging;         // Enable debug logging to Log.txt
    bool log_messages;          // Log all sent messages
    
    // Default constructor with default values
    Config();
};

class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager() = default;

    /**
     * Load configuration from file
     * @param filename Path to config file (relative to X-Plane root or absolute)
     * @return true if successful, false otherwise
     */
    bool load(const std::string& filename);
    
    /**
     * Save current configuration to file
     * @param filename Path to config file
     * @return true if successful, false otherwise
     */
    bool save(const std::string& filename);
    
    /**
     * Get configuration
     */
    const Config& getConfig() const { return config_; }
    Config& getConfig() { return config_; }
    
    /**
     * Get last error message
     */
    std::string getLastError() const { return last_error_; }

private:
    Config config_;
    std::string last_error_;
    
    /**
     * Parse a single line from INI file
     */
    void parseLine(const std::string& line);
    
    /**
     * Trim whitespace from string
     */
    static std::string trim(const std::string& str);
    
    /**
     * Parse boolean value
     */
    static bool parseBool(const std::string& value);
};

} // namespace config

#endif // CONFIG_H

