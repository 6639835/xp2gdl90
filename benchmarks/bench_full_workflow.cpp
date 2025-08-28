/**
 * XP2GDL90 Benchmarks - Full Workflow Performance Tests
 */

#include <benchmark/benchmark.h>
#include "../mocks/XPLMMock.h"
#include <vector>
#include <cstdint>

// Simple integrated benchmark
static void BM_FullWorkflow(benchmark::State& state) {
    XPLMockState::getInstance().reset();
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", 37.524);
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", -122.063);
    
    for (auto _ : state) {
        // Simulate reading flight data
        XPLMDataRef latRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
        double lat = XPLMGetDatad(latRef);
        
        // Simulate encoding (simplified)
        std::vector<uint8_t> message(28, 0);
        message[0] = 0x0A; // Message ID
        
        benchmark::DoNotOptimize(message);
        benchmark::DoNotOptimize(lat);
    }
}
BENCHMARK(BM_FullWorkflow);
