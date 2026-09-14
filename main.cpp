#include "pod.hpp"
#include "mmap.hpp"

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


const char* shm_name = "/shared_memory";
constexpr size_t shm_size = sizeof(MarketMMAP);

int main(int argc, const char* argv[]) {
    if( argc < 2 ) {
        std::println(std::cerr, "Usage: {} read|write", argv[0]); 
        return 1;
    }

    if( !strcmp(argv[1],"write") ) {

        // open shm
        const int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
        // truncate
        ftruncate(fd, shm_size);
        // mmap
        void* const mm_ptr = mmap(0, shm_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        MarketMMAP* ring = ::new (mm_ptr) MarketMMAP{};
        // write
        uint64_t w_idx = ring->write_idx.load(std::memory_order_relaxed);        
        uint64_t r_idx_cache = ring->read_idx.load(std::memory_order_acquire);
        for( uint32_t i = 0; i < 1'000'000; ++i ) {

            // wait for reader to catch up
            if( w_idx - r_idx_cache >= 1024 ) {
                while( w_idx - (r_idx_cache = ring->read_idx.load(std::memory_order_acquire) ) >= 1024 ) asm volatile("pause" ::: "memory");
            }
            // do the write
            uint64_t slot = w_idx & 1023;
	    ring->buffer[slot] = MarketUpdatePOD{12742934,i,{'A','P','P','L','\0','\0','\0','\0'},1,24,'B',1}; 
            w_idx++;
            ring->write_idx.store(w_idx, std::memory_order_release);
        }
        // cleanup
        munmap(mm_ptr, shm_size);
        close(fd);
        shm_unlink(shm_name);

    } else if( !strcmp(argv[1],"read") ) {

        // open shm
        const int fd = shm_open(shm_name, O_RDWR, 0666);
        // mmap
        void* const mm_ptr = mmap(0, shm_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
        MarketMMAP *ring = reinterpret_cast<MarketMMAP *>( mm_ptr );
        // read
        uint64_t r_idx = ring->read_idx.load(std::memory_order_relaxed);
        uint64_t w_idx_cache = ring->write_idx.load(std::memory_order_acquire);
        for( int i = 0; i < 1'000'000; ++i ) {

            // wait for writes
            if( w_idx_cache == r_idx) {
                while( (w_idx_cache = ring->write_idx.load(std::memory_order_acquire) ) == r_idx ) asm volatile("pause" ::: "memory");
            }
            uint64_t slot = r_idx & 1023;
            MarketUpdatePOD* mu_pod = &ring->buffer[slot];
            //std::println(std::cout, "POD {}: {}", i, ring->buffer[slot]);
            r_idx++;
            ring->read_idx.store(r_idx, std::memory_order_release);
        }
        // cleanup
        munmap(mm_ptr, shm_size);
        close(fd);

    } else {
        std::println(std::cerr, "Invalid arg: {}. Choose read|write", argv[1]);
    }
    return 0;
}
