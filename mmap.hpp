#pragma once

#include "pod.hpp"
#include <atomic>

struct alignas(64) MarketMMAP {
    alignas(64) std::atomic<uint64_t> write_idx{0};
    alignas(64) std::atomic<uint64_t> read_idx{0};
    alignas(64) MarketUpdatePOD buffer[1024];
};
