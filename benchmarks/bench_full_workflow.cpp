/**
 * XP2GDL90 Benchmarks - Full Workflow Performance Tests
 * 
 * Pure performance benchmark for complete data-to-message workflow
 */

#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>
#include <cmath>

namespace workflow_bench {
    struct FlightData {
        double latitude = 37.524;
        double longitude = -122.063;
        float altitude = 1000.0f;
        float groundSpeed = 150.0f;
        float heading = 90.0f;
    };
    
    // Simulate data reading overhead
    FlightData readSimulatedData() {
        FlightData data;
        // Simulate some computational overhead
        data.latitude += std::sin(0.1) * 0.0001;
        data.longitude += std::cos(0.1) * 0.0001;
        return data;
    }
    
    // Simulate GDL-90 message encoding
    std::vector<uint8_t> encodeMessage(const FlightData& data) {
        std::vector<uint8_t> message(28, 0);
        
        // Message ID for Ownship Report
        message[0] = 0x0A;
        
        // Encode latitude (simplified)
        uint32_t lat_encoded = static_cast<uint32_t>((data.latitude / 180.0 + 0.5) * 0xFFFFFF);
        message[4] = (lat_encoded >> 16) & 0xFF;
        message[5] = (lat_encoded >> 8) & 0xFF;
        message[6] = lat_encoded & 0xFF;
        
        // Encode longitude (simplified)
        uint32_t lon_encoded = static_cast<uint32_t>((data.longitude / 360.0 + 0.5) * 0xFFFFFF);
        message[7] = (lon_encoded >> 16) & 0xFF;
        message[8] = (lon_encoded >> 8) & 0xFF;
        message[9] = lon_encoded & 0xFF;
        
        // Add some processing overhead
        uint16_t checksum = 0;
        for (size_t i = 0; i < message.size() - 2; ++i) {
            checksum += message[i];
        }
        
        return message;
    }
}

// Benchmark the complete workflow: data reading + encoding + output
static void BM_FullWorkflow(benchmark::State& state) {
    for (auto _ : state) {
        // Read simulated flight data
        auto flightData = workflow_bench::readSimulatedData();
        
        // Encode GDL-90 message
        auto message = workflow_bench::encodeMessage(flightData);
        
        // Prevent optimization
        benchmark::DoNotOptimize(message);
        benchmark::DoNotOptimize(flightData);
    }
}
BENCHMARK(BM_FullWorkflow);

// Benchmark just the data reading part
static void BM_DataReading(benchmark::State& state) {
    for (auto _ : state) {
        auto data = workflow_bench::readSimulatedData();
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_DataReading);

// Benchmark just the encoding part
static void BM_MessageEncoding(benchmark::State& state) {
    workflow_bench::FlightData testData;
    
    for (auto _ : state) {
        auto message = workflow_bench::encodeMessage(testData);
        benchmark::DoNotOptimize(message);
    }
}
BENCHMARK(BM_MessageEncoding);
