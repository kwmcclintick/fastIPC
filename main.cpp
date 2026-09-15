#include "pod.hpp"
#include "ipc_ring.hpp"

#include <signal.h>
#include <cstring>
#include <print>
#include <cstddef>
#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h> // Required for close(int fd)
#include <string_view>
#include <cstdint>

const char* kshmName = "/shared_memory";
constexpr size_t kshmSize = sizeof(RingMatrix);

constexpr uint32_t knLoops = 1'000'000; // how much data each writer will write to their ring
constexpr uint32_t n_producers_loop = 3; // how many writers will we read from? After, reader will end.
constexpr size_t kexpectedVersion = 2; // ipc buffer version check

int writerMain() {
        // open shm, not owned (created or destroyed) by writer, but by single reader
        const int kfd = shm_open(kshmName, O_RDWR, 0666);
        if( kfd == -1 ) {
            std::println(std::cerr, "Failed to oepn_shm");
            return 1; // no cleanup to do
        }

        // mmap
        void* const kmmPtr = mmap(NULL, kshmSize, PROT_WRITE | PROT_READ, MAP_SHARED, kfd, 0);
        if( kmmPtr == MAP_FAILED) {
            std::println(std::cerr, "Failed to mmap");
            close(kfd); // cleanup
            return 1;
        }
        RingMatrix *ring_mat = reinterpret_cast<RingMatrix *>( kmmPtr );
        if( ring_mat->version_ != kexpectedVersion ) {
            std::println(std::cerr, "Expected ring buffer version v{}, instead got v{}", kexpectedVersion, ring_mat->version_);
            munmap(kmmPtr, kshmSize); close(kfd); // cleanup
            return 1;
        }
        // Claim a ring for this producer! Functions as a spin if none available yet
        bool claimed = false;
        MarketMMAP* my_ring = nullptr;
        while( !claimed ) {
            for( size_t i = 0; i < MAX_PRODUCERS; ++i ) {
                bool expected = false;
                if( ring_mat->rings_[i].busy_.compare_exchange_strong(expected, true,
                                                                std::memory_order_acquire, // success needs acquire to pair with release of any producers saying they don't need this anymore
                                                                std::memory_order_relaxed) ) { // failure doesn't need memory fence at all
                    my_ring = &ring_mat->rings_[i];
                    const uint32_t my_pid = getpid();
                    my_ring->producer_pid_ = my_pid;
                    std::println(std::cout,"PID {} is claiming ring {}/{}", my_pid, i, MAX_PRODUCERS-1);
                    claimed = true; break; // don't need to check anymore
                } // otherwise just try next ring
            }
        }

        // write
        uint64_t w_idx = my_ring->write_idx_.load(std::memory_order_relaxed);        
        uint64_t r_idx_cache = my_ring->read_idx_.load(std::memory_order_acquire);
        size_t ring_size = std::size(my_ring->buffer_);
        for( uint32_t i = 0; i < knLoops; ++i ) {

            // if our ring is full, that means the consumer couldn't keep up, very bad!
            // overflow sanity check example: w_idx=0, r_idx=~((uint64_t)0)-10, result should be false, don't spin. Actual: 0 - (max-10) overflows to 10, which is less than ring size.
            // we get false, then, which is correct!
            if( w_idx - r_idx_cache >= ring_size ) {
                while( w_idx - (r_idx_cache = my_ring->read_idx_.load(std::memory_order_acquire) ) >= ring_size ) {
                    asm volatile("pause" ::: "memory");
                }
            }

            // since there's room, do the write (in-place!)
            uint64_t slot = w_idx & (ring_size-1);
            my_ring->buffer_[slot] = MarketUpdatePOD{1994,i,{'A','P','P','L','\0','\0','\0','\0'},1,24,'B',1}; // dummy market data
            // update the write index
            w_idx++;
            my_ring->write_idx_.store(w_idx, std::memory_order_release);
        }
        // cleanup: unmap the mmap, close the fd, and set busy to false for this ring
        my_ring->busy_.store(false, std::memory_order_release); my_ring->producer_pid_ = 0;
        munmap(kmmPtr, kshmSize); close(kfd);
        return 0;
}

int readerMain() {
        // open shm, owned by single reader (created / unlinked by reader)
        const int kfd = shm_open(kshmName, O_CREAT | O_RDWR, 0666);
        if( kfd == -1 ) {
            std::println(std::cerr, "Failed to shm_open");
            return 1; // no cleanup to do
        }
        ftruncate(kfd, kshmSize);
        // mmap
        void* const kmmPtr = mmap(NULL, kshmSize, PROT_WRITE | PROT_READ, MAP_SHARED, kfd, 0);
        if( kmmPtr == MAP_FAILED) {
            std::println(std::cerr, "Failed to mmap");
            close(kfd); shm_unlink(kshmName); // cleanup
            return 1;
        }
        RingMatrix* ring_matrix = ::new (kmmPtr) RingMatrix{};
        // version check
        if( ring_matrix->version_ != kexpectedVersion ) {
            std::println(std::cerr, "Expected ring buffer version v{}, instead got v{}", kexpectedVersion, ring_matrix->version_);
            munmap(kmmPtr, kshmSize); close(kfd); shm_unlink(kshmName); // cleanup
            return 1;
        }

        // look for a ring to read from 
        uint32_t reads = 0;        
        const size_t ring_size = std::size(ring_matrix->rings_[0].buffer_);
        uint64_t r_idx[MAX_PRODUCERS] = {0};
        uint64_t w_idx_cache[MAX_PRODUCERS] = {0};
        bool ring_initialized[MAX_PRODUCERS] = {false};

        while( reads < knLoops * n_producers_loop ) {
            for( size_t i = 0; i < MAX_PRODUCERS; ++i ) {
                // set vars for our ring of choice
                MarketMMAP *ring = &ring_matrix->rings_[i];

                // sync with this producer
                if( !ring_initialized[i] ) {
                    if( ring->busy_.load(std::memory_order_acquire) ) {
                        ring_initialized[i] = true;
                        r_idx[i] = ring->read_idx_.load(std::memory_order_relaxed);
                    } else continue; // otherwise continue, there's no producer there
                }

                w_idx_cache[i] = ring->write_idx_.load(std::memory_order_acquire);

                // if we're empty, skip to next ring (used to spin and wait). Effectively this will spin if all rings are empty
                // it's tempting to use <= here to catch cases where the reader is not just at the writer but ahead of it
                // if we did that, we need to consider overflow. Example: w_idx = ~((uint64_t)0) - 10, r_idx = 0, reader is 10 ahead of the writer! We want true, to spin
                // Actual: max-10 <= 0 is false, so we don't spin. Doesn't work! We need to use == and make sure our atomics don't allow races 
                if( w_idx_cache[i] == r_idx[i]) {
                    if( !ring->busy_.load(std::memory_order_acquire) ) {
                        ring_initialized[i] = false; 
                    }
                    continue;
                }
                uint64_t slot = r_idx[i] & (ring_size-1);

                // Process market data here
                MarketUpdatePOD* mu_pod = &ring->buffer_[slot];
                reads++;
                std::println(std::cout, "read #{} from ring #{}: {}", reads, i, *mu_pod);
                // end processing of data here

                r_idx[i]++;
                ring->read_idx_.store(r_idx[i], std::memory_order_release);
            }
        }
        // cleanup. single consumer (reader) owns shm
        munmap(kmmPtr, kshmSize); close(kfd); shm_unlink(kshmName);
        return 0;
}

int main(int argc, const char* argv[]) {
    if( argc < 2 ) {
        std::println(std::cerr, "Usage: {} read|write", argv[0]); 
        return 1;
    }

    if( !strcmp(argv[1],"write") ) {
        return writerMain();
    } else if( !strcmp(argv[1],"read") ) {
        return readerMain();
    } else {
        std::println(std::cerr, "Invalid arg: {}. Choose read|write", argv[1]);
        return 1;
    }
}
