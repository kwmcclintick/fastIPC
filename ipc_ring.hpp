#pragma once

#include "pod.hpp"
#include <atomic>

// Single producer stuff
constexpr size_t BUFFER_SIZE = 1024;
struct alignas(64) MarketMMAP {
    // stuff from first version
    alignas(64) std::atomic<uint64_t> write_idx_{0};
    alignas(64) std::atomic<uint64_t> read_idx_{0};
    alignas(64) MarketUpdatePOD buffer_[BUFFER_SIZE];
    // future proofing / things added since start
    alignas(64) uint32_t producer_pid_{0};
    alignas(64) std::atomic<bool> busy_{false}; // claimed by a producer or not TODO carefully consider placement of this, it's going to get hammered
    alignas(64) char padding_[(2<<16)-64]; // future proofing minus things we've added since future proofing
};
static_assert(sizeof(MarketMMAP) == 196'800 ); // always and forever! Otherwise assume you'll break stuff

// Multi producer stuff
constexpr size_t MAX_PRODUCERS = 16;
struct alignas(64) RingMatrix {
    // stuff from first version
    MarketMMAP rings_[MAX_PRODUCERS];
    alignas(64) size_t version_{2};
    // future proofing / things added since start
    alignas(64) char padding_[2<<16];
};
static_assert(sizeof(RingMatrix) == 3'279'936); // always and forever!
