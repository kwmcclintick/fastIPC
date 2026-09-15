# fastIPC

I wanted to learn some mmap / shm stuff, especially regarding pointer casting and placement new.

This repo started as the core mechanic that popular IPC libraries use: viewing an atomic SPSC ring buffer struct over a shared mmap, with cached indices to send POD structs from a writer(producer) to a reader(consumer).

importantly, the producer constructs data in-place in the mmap, and the reader views directly into the mmap

This simple program can be ran as:

```bash
./main write
# in another window:
./main read
```

Actually using the data is out of scope, right now it's just optionally read into a print statement.

## Additions since Init

 - Versioning for the ring buffer and a check that aborts if the version is not what's expected
 - Future-proofing buffer and static assert for the ring buffer
 - Simple check + logging in the reader to see if the writer's sequence number jumps ahead
 - Producer writes PID to ring buffer and consumer checks status when there's a drop

## Future Work

- UDP re-ordering given sequence number without redundant copys to a staging buffer
- RAII for mmap and shm. Right now they're carefully cleaned up at all returns, but would be cool to not have to manage that.
- MPSC (multiple market feeds)

# Build

I like to use `std::print` so I'm compiling with c++23.

```bash
g++ main.cpp -std=c++23 -o main
```
