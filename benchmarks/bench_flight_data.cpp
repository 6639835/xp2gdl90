/**
 * XP2GDL90 Benchmarks - Flight Data Performance Tests
 */

#include <benchmark/benchmark.h>
#include "../mocks/XPLMMock.h"
#include <vector>
#include <random>

namespace flight_bench {
    struct FlightData {
        double latitude;
        double longitude;
        double elevation;
        float groundSpeed;
        float heading;
    };
    
    FlightData readFlightData() {
        FlightData data;
        XPLMDataRef latRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
        XPLMDataRef lonRef = XPLMFindDataRef("sim/flightmodel/position/longitude");
        
        data.latitude = XPLMGetDatad(latRef);
        data.longitude = XPLMGetDatad(lonRef);
        data.elevation = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/elevation"));
        data.groundSpeed = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/groundspeed"));
        data.heading = XPLMGetDataf(XPLMFindDataRef("sim/flightmodel/position/psi"));
        
        return data;
    }
}

// Benchmark dataref reading
static void BM_DataRefReading(benchmark::State& state) {
    XPLMockState::getInstance().reset();
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", 37.524);
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", -122.063);
    
    for (auto _ : state) {
        auto data = flight_bench::readFlightData();
        benchmark::DoNotOptimize(data);
    }
}
BENCHMARK(BM_DataRefReading);
