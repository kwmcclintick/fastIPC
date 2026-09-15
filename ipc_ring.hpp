#pragma once

#include "pod.hpp"
#include <atomic>

constexpr size_t kringSize = 1024;

struct alignas(64) MarketMMAP {
    alignas(64) std::atomic<uint64_t> write_idx_{0};
    alignas(64) std::atomic<uint64_t> read_idx_{0};
    alignas(64) MarketUpdatePOD buffer[kringSize];
};
