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
        // write
        uint64_t w_idx = ring->write_idx_.load(std::memory_order_relaxed);        
        uint64_t r_idx_cache = ring->read_idx_.load(std::memory_order_acquire);
        for( uint32_t i = 0; i < knLoops; ++i ) {

            // wait for reader to catch up
            if( w_idx - r_idx_cache >= kringSize ) {
                while( w_idx - (r_idx_cache = ring->read_idx_.load(std::memory_order_acquire) ) >= kringSize ) asm volatile("pause" ::: "memory");
            }
            // do the write
            uint64_t slot = w_idx & (kringSize-1);
	    ring->buffer[slot] = MarketUpdatePOD{10000+i,i,{'A','P','P','L','\0','\0','\0','\0'},1,24,'B',1}; 
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
        // read
        uint64_t r_idx = ring->read_idx_.load(std::memory_order_relaxed);
        uint64_t w_idx_cache = ring->write_idx_.load(std::memory_order_acquire);
        for( int i = 0; i < knLoops; ++i ) {

            // wait for writes
            if( w_idx_cache == r_idx) {
                while( (w_idx_cache = ring->write_idx_.load(std::memory_order_acquire) ) == r_idx ) asm volatile("pause" ::: "memory");
            }
            uint64_t slot = r_idx & (kringSize-1);
            MarketUpdatePOD* mu_pod = &ring->buffer[slot];
            //std::println(std::cout, "POD {}: {}", i, ring->buffer[slot]);
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
