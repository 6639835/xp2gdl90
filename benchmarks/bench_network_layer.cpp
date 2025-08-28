/**
 * XP2GDL90 Benchmarks - Network Layer Performance Tests
 */

#include <benchmark/benchmark.h>
#include <vector>
#include <cstdint>

// Simple network simulation for benchmarking
namespace network_bench {
    
    class MockUDPSender {
    public:
        bool send(const std::vector<uint8_t>& data) {
            // Simulate network send overhead
            volatile size_t checksum = 0;
            for (uint8_t byte : data) {
                checksum += byte;
            }
            return checksum > 0;
        }
    };
    
    std::vector<uint8_t> createTestMessage(size_t size) {
        std::vector<uint8_t> msg(size);
        for (size_t i = 0; i < size; ++i) {
            msg[i] = static_cast<uint8_t>(i % 256);
        }
        return msg;
    }
}

// Benchmark UDP send performance
static void BM_UDPSend(benchmark::State& state) {
    network_bench::MockUDPSender sender;
    auto message = network_bench::createTestMessage(state.range(0));
    
    for (auto _ : state) {
        bool result = sender.send(message);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetBytesProcessed(state.iterations() * message.size());
}
BENCHMARK(BM_UDPSend)->Range(8, 1024);

// Benchmark batch sending
static void BM_BatchUDPSend(benchmark::State& state) {
    network_bench::MockUDPSender sender;
    const int batch_size = state.range(0);
    
    std::vector<std::vector<uint8_t>> messages;
    for (int i = 0; i < batch_size; ++i) {
        messages.push_back(network_bench::createTestMessage(28)); // Typical GDL-90 size
    }
    
    for (auto _ : state) {
        for (const auto& msg : messages) {
            bool result = sender.send(msg);
            benchmark::DoNotOptimize(result);
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_BatchUDPSend)->Range(1, 63);
