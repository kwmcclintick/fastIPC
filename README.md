# fastIPC

I wanted to learn some mmap / shm stuff, especially regarding pointer casting and placement new.

This repo started as the core mechanic that popular IPC libraries use: viewing an atomic SPSC ring buffer struct over a shared mmap, with cached indices to send POD structs from a writer(producer) to a reader(consumer).

importantly, the producer constructs data in-place in the mmap, and the reader views directly into the mmap

This simple program can be ran as:

```bash
# consumer process
./main read
# in some other windows, run some producer processes
./main write
./main write
./main write
```

Current `main` is hard coded to expect three writes to run before the owning reader exits.

Producers mark a ring as busy, but the consumer separately keeps track of that as well. Killing a producer and starting a new one is ok.

Actually using the data is out of scope, right now it's just optionally read into a print statement.

## Additions since Init

 - Versioning for the ring buffer and a check that aborts if the version is not what's expected
 - Future-proofing buffer and static assert for the ring buffer
 - MPSC (multiple market feeds)

## Future Work

 - Simple check + logging in the reader to see if the writer's sequence number jumps ahead
 - UDP re-ordering given sequence number without redundant copys to a staging buffer
 - RAII for mmap, shm, and busy. Right now they're carefully cleaned up at all returns, but would be cool to not have to manage that. Right now if you kill a writer, the ring is permanently marked as busy.

# Build

I like to use `std::print` so I'm compiling with c++23.

```bash
g++ main.cpp -std=c++23 -o main
```
