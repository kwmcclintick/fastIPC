#include "pod.hpp"
#include "ipc_ring.hpp"

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
constexpr size_t kshmSize = sizeof(MarketMMAP);
constexpr uint32_t knLoops = 1'000'000;
constexpr size_t kexpectedVersion = 1;

int writerMain() {
        // open shm
        const int kfd = shm_open(kshmName, O_CREAT | O_RDWR, 0666);
        if( kfd == -1 ) {
            std::println(std::cerr, "Failed to oepn_shm");
            return 1;
        }
        // truncate
        ftruncate(kfd, kshmSize);
        // mmap
        void* const kmmPtr = mmap(NULL, kshmSize, PROT_WRITE | PROT_READ, MAP_SHARED, kfd, 0);
        if( kmmPtr == MAP_FAILED) {
            std::println(std::cerr, "Failed to mmap");
            return 1;
        }
        MarketMMAP* ring = ::new (kmmPtr) MarketMMAP{};
        if( ring->version_ != kexpectedVersion ) {
            std::println(std::cerr, "Expected ring buffer version v{}, instead got v{}", kexpectedVersion, ring->version_);
            return 1;
        }
        size_t ring_size = std::size(ring->buffer_);
        // write
        uint64_t w_idx = ring->write_idx_.load(std::memory_order_relaxed);        
        uint64_t r_idx_cache = ring->read_idx_.load(std::memory_order_acquire);
        for( uint32_t i = 0; i < knLoops; ++i ) {

            // wait for reader to catch up
            if( w_idx - r_idx_cache >= ring_size ) {
                while( w_idx - (r_idx_cache = ring->read_idx_.load(std::memory_order_acquire) ) >= ring_size ) asm volatile("pause" ::: "memory");
            }
            // do the write
            uint64_t slot = w_idx & (ring_size-1);
	    ring->buffer_[slot] = MarketUpdatePOD{100*i,i,{'A','P','P','L','\0','\0','\0','\0'},1,24,'B',1}; // dummy market data
            w_idx++;
            ring->write_idx_.store(w_idx, std::memory_order_release);
        }
        // cleanup
        munmap(kmmPtr, kshmSize);
        close(kfd);
        shm_unlink(kshmName);
        return 0;
}

int readerMain() {
        // open shm
        const int kfd = shm_open(kshmName, O_RDWR, 0666);
        if( kfd == -1 ) {
            std::println(std::cerr, "Failed to shm_open");
            return 1;
        }
        // mmap
        void* const kmmPtr = mmap(NULL, kshmSize, PROT_WRITE | PROT_READ, MAP_SHARED, kfd, 0);
        if( kmmPtr == MAP_FAILED) {
            std::println(std::cerr, "Failed to mmap");
            return 1;
        }
        MarketMMAP *ring = reinterpret_cast<MarketMMAP *>( kmmPtr );
        if( ring->version_ != kexpectedVersion ) {
            std::println(std::cerr, "Expected ring buffer version v{}, instead got v{}", kexpectedVersion, ring->version_);
            return 1;
        }
        size_t ring_size = std::size(ring->buffer_);
        // read
        uint64_t r_idx = ring->read_idx_.load(std::memory_order_relaxed);
        uint64_t w_idx_cache = ring->write_idx_.load(std::memory_order_acquire);
        for( int i = 0; i < knLoops; ++i ) {

            // wait for writes
            if( w_idx_cache == r_idx) {
                while( (w_idx_cache = ring->write_idx_.load(std::memory_order_acquire) ) == r_idx ) asm volatile("pause" ::: "memory");
            }
            uint64_t slot = r_idx & (ring_size-1);

            // Process market data here
            MarketUpdatePOD mu_pod = ring->buffer_[slot];
            std::println(std::cout, "POD {}: {}", i, mu_pod);
            // end process here

            r_idx++;
            ring->read_idx_.store(r_idx, std::memory_order_release);
        }
        // cleanup
        munmap(kmmPtr, kshmSize);
        close(kfd);
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
