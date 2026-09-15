#pragma once

#include "pod.hpp"
#include <atomic>

struct alignas(64) MarketMMAP {
    // metadata
    size_t version_{1};
    // core, functional members
    alignas(64) std::atomic<uint64_t> write_idx_{0};
    alignas(64) std::atomic<uint64_t> read_idx_{0};
    alignas(64) MarketUpdatePOD buffer_[1024];
    alignas(64) char padding[2<<16]; // future proofing
};

static_assert(sizeof(MarketMMAP) == 196'800); // always and forever! Otherwise assume you'll break stuff
