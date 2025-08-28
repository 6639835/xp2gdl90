/**
 * XP2GDL90 Benchmarks - Flight Data Performance Tests
 * 
 * Pure performance benchmarks for flight data processing operations
 */

#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <cstdint>
#include <map>
#include <string>

namespace flight_bench {
    struct FlightData {
        double latitude;
        double longitude;
        double elevation;
        float groundSpeed;
        float heading;
    };
    
    // Mock data for benchmarking purposes only
    class MockDataStore {
    private:
        std::map<std::string, double> doubleValues;
        std::map<std::string, float> floatValues;
        
    public:
        MockDataStore() {
            // Initialize with realistic flight data
            doubleValues["sim/flightmodel/position/latitude"] = 37.7749;
            doubleValues["sim/flightmodel/position/longitude"] = -122.4194;
            floatValues["sim/flightmodel/position/elevation"] = 1000.0f;
            floatValues["sim/flightmodel/position/groundspeed"] = 150.0f;
            floatValues["sim/flightmodel/position/psi"] = 90.0f;
        }
        
        double getDouble(const std::string& name) { return doubleValues[name]; }
        float getFloat(const std::string& name) { return floatValues[name]; }
    };
    
    static MockDataStore dataStore;
    
    FlightData readFlightData() {
        FlightData data;
        // Simulate the overhead of dataref lookups and data access
        data.latitude = dataStore.getDouble("sim/flightmodel/position/latitude");
        data.longitude = dataStore.getDouble("sim/flightmodel/position/longitude");
        data.elevation = dataStore.getFloat("sim/flightmodel/position/elevation");
        data.groundSpeed = dataStore.getFloat("sim/flightmodel/position/groundspeed");
        data.heading = dataStore.getFloat("sim/flightmodel/position/psi");
        
        return data;
    }
}

// Benchmark dataref reading
static void BM_DataRefReading(benchmark::State& state) {
    for (auto _ : state) {
        auto data = flight_bench::readFlightData();
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_DataRefReading);
