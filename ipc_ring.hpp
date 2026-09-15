#pragma once

#include "pod.hpp"
#include <atomic>

struct alignas(64) MarketMMAP {
    MarketMMAP(uint32_t pid) : producer_pid_(pid) {};
    // metadata
    alignas(64) size_t version_{2};
    // core, functional members
    alignas(64) std::atomic<uint64_t> write_idx_{0};
    alignas(64) std::atomic<uint64_t> read_idx_{0};
    alignas(64) MarketUpdatePOD buffer_[1024];
    // future proofing / things added since start
    alignas(64) uint32_t producer_pid_{0};
    alignas(64) bool busy{false};
    alignas(64) char padding[(2<<16)-(2*64)]; // future proofing minus things we've added since future proofing
};

static_assert(sizeof(MarketMMAP) == 196'800 ); // always and forever! Otherwise assume you'll break stuff

constexpr size_t MAX_PRODUCERS = 16;

struct alignas(64) RingMatrix {
    MarketMMAP rings[MAX_PRODUCERS];
};
