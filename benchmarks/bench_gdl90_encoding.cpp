/**
 * XP2GDL90 Benchmarks - GDL-90 Encoding Performance Tests
 * 
 * Performance benchmarks for GDL-90 message encoding operations
 */

#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include <random>
#include <cmath>

// Mock GDL-90 encoding functions for benchmarking
namespace gdl90_bench {
    
    // Position structure for benchmarking
    struct Position {
        double latitude;
        double longitude;
        int32_t altitude;
        float groundSpeed;
        float track;
        int16_t verticalVelocity;
    };
    
    // CRC calculation benchmark
    uint16_t calculateCRC(const uint8_t* data, size_t length) {
        uint16_t crc = 0;
        for (size_t i = 0; i < length; i++) {
            crc ^= (data[i] << 8);
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc = crc << 1;
                }
            }
        }
        return crc;
    }
    
    // Coordinate encoding benchmark
    int32_t encodeCoordinate(double degrees) {
        const double resolution = 180.0 / (1 << 23);
        int32_t encoded = static_cast<int32_t>(degrees / resolution);
        
        const int32_t max_24bit = (1 << 23) - 1;
        const int32_t min_24bit = -(1 << 23);
        
        if (encoded > max_24bit) encoded = max_24bit;
        if (encoded < min_24bit) encoded = min_24bit;
        
        return encoded & 0x00FFFFFF;
    }
    
    // Altitude encoding benchmark
    uint16_t encodeAltitude(int32_t altitudeFeet) {
        int32_t encoded = (altitudeFeet + 1000) / 25;
        
        if (encoded < 0) encoded = 0;
        if (encoded > 0xFFE) encoded = 0xFFE;
        
        return static_cast<uint16_t>(encoded);
    }
    
    // Heartbeat message creation
    std::vector<uint8_t> createHeartbeat(uint32_t timestamp) {
        std::vector<uint8_t> message;
        message.reserve(7);
        
        message.push_back(0x00);  // Message ID
        message.push_back(0x01);  // Status
        message.push_back((timestamp >> 16) & 0xFF);
        message.push_back((timestamp >> 8) & 0xFF);
        message.push_back(timestamp & 0xFF);
        message.push_back(0x00);  // Counter high
        message.push_back(0x01);  // Counter low
        
        return message;
    }
    
    // Ownship report creation
    std::vector<uint8_t> createOwnshipReport(const Position& pos, uint32_t icaoAddress) {
        std::vector<uint8_t> message;
        message.reserve(28);
        
        // Message ID
        message.push_back(0x0A);
        
        // Alert status and address type
        message.push_back(0x00);
        
        // ICAO address
        message.push_back((icaoAddress >> 16) & 0xFF);
        message.push_back((icaoAddress >> 8) & 0xFF);
        message.push_back(icaoAddress & 0xFF);
        
        // Encode position
        int32_t lat = encodeCoordinate(pos.latitude);
        message.push_back((lat >> 16) & 0xFF);
        message.push_back((lat >> 8) & 0xFF);
        message.push_back(lat & 0xFF);
        
        int32_t lon = encodeCoordinate(pos.longitude);
        message.push_back((lon >> 16) & 0xFF);
        message.push_back((lon >> 8) & 0xFF);
        message.push_back(lon & 0xFF);
        
        // Encode altitude
        uint16_t alt = encodeAltitude(pos.altitude);
        message.push_back((alt >> 4) & 0xFF);
        message.push_back(((alt & 0x0F) << 4) | 0x0A);
        
        // Navigation accuracy
        message.push_back(0xA0);
        message.push_back(0x00);
        
        // Ground speed
        uint16_t speed = static_cast<uint16_t>(pos.groundSpeed);
        if (speed > 0xFFE) speed = 0xFFE;
        message.push_back((speed >> 4) & 0xFF);
        
        // Track angle
        uint8_t track = static_cast<uint8_t>(pos.track * 256.0f / 360.0f);
        message.push_back(((speed & 0x0F) << 4) | ((track >> 4) & 0x0F));
        message.push_back((track & 0x0F) << 4);
        
        // Vertical velocity
        int16_t vv = pos.verticalVelocity / 64;
        if (vv > 0x1FF) vv = 0x1FF;
        if (vv < -0x200) vv = -0x200;
        message.push_back((vv >> 4) & 0xFF);
        message.push_back((vv & 0x0F) << 4);
        
        // Reserved
        message.push_back(0x00);
        message.push_back(0x00);
        
        return message;
    }
    
    // Message framing with escape bytes
    std::vector<uint8_t> frameMessage(const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> framed;
        framed.reserve(payload.size() + 10); // Estimate with overhead
        
        // Calculate CRC
        uint16_t crc = calculateCRC(payload.data(), payload.size());
        
        // Add opening flag
        framed.push_back(0x7E);
        
        // Add payload with escaping
        for (uint8_t byte : payload) {
            if (byte == 0x7E || byte == 0x7D) {
                framed.push_back(0x7D);
                framed.push_back(byte ^ 0x20);
            } else {
                framed.push_back(byte);
            }
        }
        
        // Add CRC with escaping
        uint8_t crc_high = (crc >> 8) & 0xFF;
        uint8_t crc_low = crc & 0xFF;
        
        if (crc_high == 0x7E || crc_high == 0x7D) {
            framed.push_back(0x7D);
            framed.push_back(crc_high ^ 0x20);
        } else {
            framed.push_back(crc_high);
        }
        
        if (crc_low == 0x7E || crc_low == 0x7D) {
            framed.push_back(0x7D);
            framed.push_back(crc_low ^ 0x20);
        } else {
            framed.push_back(crc_low);
        }
        
        // Add closing flag
        framed.push_back(0x7E);
        
        return framed;
    }
}

// Benchmark CRC calculation performance
static void BM_CRCCalculation(benchmark::State& state) {
    std::vector<uint8_t> data(state.range(0));
    
    // Fill with pseudo-random data
    std::iota(data.begin(), data.end(), 0);
    
    for (auto _ : state) {
        uint16_t crc = gdl90_bench::calculateCRC(data.data(), data.size());
        benchmark::DoNotOptimize(crc);
    }
    
    state.SetBytesProcessed(state.iterations() * data.size());
}
BENCHMARK(BM_CRCCalculation)->Range(8, 1024);

// Benchmark coordinate encoding
static void BM_CoordinateEncoding(benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> lat_dist(-90.0, 90.0);
    std::uniform_real_distribution<double> lon_dist(-180.0, 180.0);
    
    for (auto _ : state) {
        double lat = lat_dist(gen);
        double lon = lon_dist(gen);
        
        int32_t encoded_lat = gdl90_bench::encodeCoordinate(lat);
        int32_t encoded_lon = gdl90_bench::encodeCoordinate(lon);
        
        benchmark::DoNotOptimize(encoded_lat);
        benchmark::DoNotOptimize(encoded_lon);
    }
    
    state.SetItemsProcessed(state.iterations() * 2); // 2 coordinates per iteration
}
BENCHMARK(BM_CoordinateEncoding);

// Benchmark altitude encoding
static void BM_AltitudeEncoding(benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int32_t> alt_dist(-1000, 50000);
    
    for (auto _ : state) {
        int32_t altitude = alt_dist(gen);
        uint16_t encoded = gdl90_bench::encodeAltitude(altitude);
        benchmark::DoNotOptimize(encoded);
    }
}
BENCHMARK(BM_AltitudeEncoding);

// Benchmark heartbeat message creation
static void BM_HeartbeatCreation(benchmark::State& state) {
    uint32_t timestamp = 3661; // Sample timestamp
    
    for (auto _ : state) {
        auto message = gdl90_bench::createHeartbeat(timestamp);
        benchmark::DoNotOptimize(message);
        timestamp++;
    }
}
BENCHMARK(BM_HeartbeatCreation);

// Benchmark ownship report creation
static void BM_OwnshipReportCreation(benchmark::State& state) {
    gdl90_bench::Position position{
        37.524, -122.063, 1000, 150.0f, 90.0f, 500
    };
    uint32_t icaoAddress = 0xABCDEF;
    
    for (auto _ : state) {
        auto message = gdl90_bench::createOwnshipReport(position, icaoAddress);
        benchmark::DoNotOptimize(message);
        
        // Vary position slightly for realism
        position.latitude += 0.0001;
        position.longitude += 0.0001;
        position.altitude += 10;
    }
}
BENCHMARK(BM_OwnshipReportCreation);

// Benchmark message framing
static void BM_MessageFraming(benchmark::State& state) {
    // Create a sample message
    gdl90_bench::Position position{37.524, -122.063, 1000, 150.0f, 90.0f, 500};
    auto payload = gdl90_bench::createOwnshipReport(position, 0xABCDEF);
    
    for (auto _ : state) {
        auto framed = gdl90_bench::frameMessage(payload);
        benchmark::DoNotOptimize(framed);
    }
}
BENCHMARK(BM_MessageFraming);

// Benchmark complete message pipeline
static void BM_CompleteMessagePipeline(benchmark::State& state) {
    gdl90_bench::Position position{
        37.524, -122.063, 1000, 150.0f, 90.0f, 500
    };
    uint32_t icaoAddress = 0xABCDEF;
    
    for (auto _ : state) {
        // Create ownship report
        auto payload = gdl90_bench::createOwnshipReport(position, icaoAddress);
        
        // Frame the message
        auto framed = gdl90_bench::frameMessage(payload);
        
        benchmark::DoNotOptimize(framed);
        
        // Simulate changing position
        position.latitude += 0.0001;
        position.longitude += 0.0001;
    }
}
BENCHMARK(BM_CompleteMessagePipeline);

// Benchmark batch message creation (multiple traffic targets)
static void BM_BatchMessageCreation(benchmark::State& state) {
    const int num_targets = state.range(0);
    std::vector<gdl90_bench::Position> positions(num_targets);
    
    // Initialize positions
    for (int i = 0; i < num_targets; i++) {
        positions[i] = {
            37.524 + i * 0.001,
            -122.063 + i * 0.001, 
            1000 + i * 100,
            150.0f + i * 5.0f,
            90.0f + i * 2.0f,
            500
        };
    }
    
    for (auto _ : state) {
        std::vector<std::vector<uint8_t>> messages;
        messages.reserve(num_targets);
        
        for (int i = 0; i < num_targets; i++) {
            auto payload = gdl90_bench::createOwnshipReport(positions[i], 0x100000 + i);
            auto framed = gdl90_bench::frameMessage(payload);
            messages.push_back(std::move(framed));
        }
        
        benchmark::DoNotOptimize(messages);
    }
    
    state.SetItemsProcessed(state.iterations() * num_targets);
}
BENCHMARK(BM_BatchMessageCreation)->Range(1, 63);

// Benchmark memory allocation patterns
static void BM_MemoryAllocationPattern(benchmark::State& state) {
    gdl90_bench::Position position{37.524, -122.063, 1000, 150.0f, 90.0f, 500};
    
    for (auto _ : state) {
        // Test different allocation strategies
        std::vector<uint8_t> message1 = gdl90_bench::createOwnshipReport(position, 0xABCDEF);
        
        std::vector<uint8_t> message2;
        message2.reserve(28); // Pre-allocate
        // Would call createOwnshipReport with reserved vector
        
        benchmark::DoNotOptimize(message1);
        benchmark::DoNotOptimize(message2);
    }
}
BENCHMARK(BM_MemoryAllocationPattern);

// Benchmark with different compiler optimizations
static void BM_OptimizationComparison(benchmark::State& state) {
    volatile double lat = 37.524;  // Prevent constant folding
    volatile double lon = -122.063;
    volatile int32_t alt = 1000;
    
    for (auto _ : state) {
        int32_t encoded_lat = gdl90_bench::encodeCoordinate(lat);
        int32_t encoded_lon = gdl90_bench::encodeCoordinate(lon);
        uint16_t encoded_alt = gdl90_bench::encodeAltitude(alt);
        
        benchmark::DoNotOptimize(encoded_lat);
        benchmark::DoNotOptimize(encoded_lon);
        benchmark::DoNotOptimize(encoded_alt);
        
        // Vary inputs slightly
        lat += 0.0001;
        lon += 0.0001;
        alt += 10;
    }
}
BENCHMARK(BM_OptimizationComparison);

// Benchmark realistic flight scenario
static void BM_RealisticFlightScenario(benchmark::State& state) {
    // Simulate a 10-minute flight with 2Hz updates
    const int total_updates = 10 * 60 * 2; // 10 minutes * 60 seconds * 2Hz
    std::vector<gdl90_bench::Position> flightPath;
    flightPath.reserve(total_updates);
    
    // Generate realistic flight path
    for (int i = 0; i < total_updates; i++) {
        double t = static_cast<double>(i) / total_updates;
        flightPath.push_back({
            37.524 + sin(t * 2 * M_PI) * 0.01,  // Circular pattern
            -122.063 + cos(t * 2 * M_PI) * 0.01,
            1000 + static_cast<int32_t>(sin(t * M_PI) * 500), // Climb/descend
            150.0f + static_cast<float>(sin(t * 4 * M_PI) * 50.0),  // Speed variation
            static_cast<float>(t * 360.0), // Complete rotation
            static_cast<int16_t>(cos(t * M_PI) * 1000) // Vertical speed
        });
    }
    
    size_t update_index = 0;
    
    for (auto _ : state) {
        const auto& pos = flightPath[update_index % flightPath.size()];
        
        // Create and frame ownship report
        auto payload = gdl90_bench::createOwnshipReport(pos, 0xABCDEF);
        auto framed = gdl90_bench::frameMessage(payload);
        
        benchmark::DoNotOptimize(framed);
        update_index++;
    }
    
    state.SetItemsProcessed(state.iterations());
    state.counters["UpdatesPerSecond"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
