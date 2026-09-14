# fastIPC

I wanted to learn some mmap / shm stuff, especially regarding pointer casting and placement new.

This simple program can be ran as:

```bash
./main write
./main read
```

The writer will write Plain Old Data (POD) structs to a shared mmap, and the reader will read them.

The shared memory is interpreted as a SPSC ring buffer where the writer caches the readers index and the reader caches the writers index.

Actually using the data is out of scope, right now it's just read into a print statement.

# Build

I like to use `std::print` so I'm compiling with c++23.

```bash
g++ main.cpp -std=c++23 -o main
```
