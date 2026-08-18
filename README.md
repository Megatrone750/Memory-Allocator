# Memory Allocator

A small educational memory allocator for Linux that reimplements `malloc()`, `calloc()`, `realloc()`, and `free()` on top of the `sbrk()` system call.

It is meant as a walkthrough of how a heap allocator can work: each request is backed by a header plus a payload, free blocks are reused with a first-fit scan, and the last block on the heap can be returned to the OS by shrinking the program break.

## Features

- Custom `malloc`, `calloc`, `realloc`, and `free`
- First-fit search over a singly linked free/used block list
- 16-byte header alignment
- Thread locking around allocation and free (via `pthread`)
- Can be loaded into other programs with `LD_PRELOAD`

This is **not** a drop-in replacement for glibc `malloc`. It is a teaching implementation with known limits (see [Limitations](#limitations)).

## How it works

Every allocated region is preceded by a header:

| Field | Meaning |
| --- | --- |
| `size` | Usable payload size in bytes |
| `is_free` | `1` if the block is available for reuse |
| `next` | Next block in the heap linked list |

The header is a union padded to 16 bytes so the payload that follows is aligned.

```
  [ header | payload ] -> [ header | payload ] -> ...
       ^                         ^
      head                      tail
```

1. **`malloc(size)`**  
   Locks the heap, looks for a free block large enough (`get_free_block`), and reuses it if one exists. Otherwise it extends the heap with `sbrk(sizeof(header) + size)`, initializes a new header, and appends it to the list.

2. **`free(ptr)`**  
   Recovers the header in front of `ptr`. If that block sits at the current program break (it is the last block), the allocator shrinks the heap with a negative `sbrk`. Otherwise it only marks the block free so a later `malloc` can reuse it.

3. **`calloc(n, size)`**  
   Allocates `n * size` bytes (with an overflow check) and zeros the payload with `memset`.

4. **`realloc(ptr, size)`**  
   Returns `malloc(size)` if `ptr` is `NULL`, frees and returns `NULL` if `size` is `0`, keeps the same block if it is already large enough, otherwise allocates a new block, copies the old payload, and frees the old block.

`print_mem_list()` dumps the current list (addresses, sizes, free flags) for debugging. It is not part of the libc API.

## Requirements

- Linux (uses `sbrk`)
- `gcc`
- `pthread` (linked automatically when you build the shared object as shown below)

macOS and other systems that do not expose `sbrk` the same way are not supported.

## Build

Clone the repository and compile a shared library:

```bash
git clone https://github.com/Megatrone750/Memory-Allocator.git
cd Memory-Allocator

gcc -o memalloc.so -fPIC -shared memalloc.c
```

`-fPIC -shared` produces a position-independent shared object that the dynamic linker can preload.

## Use with `LD_PRELOAD`

On Linux, `LD_PRELOAD` loads your `.so` before libc, so calls to `malloc` / `free` / `calloc` / `realloc` go to this allocator:

```bash
export LD_PRELOAD=$PWD/memalloc.so
ls
unset LD_PRELOAD
```

You can also prefix a single command:

```bash
LD_PRELOAD=$PWD/memalloc.so ls
```

Many programs allocate internally, so a working preload is a practical smoke test. For a tighter test, write a small C program that calls `malloc`, writes to the buffer, `realloc`s, and `free`s.

To stop using the custom allocator, unset the variable or open a new shell:

```bash
unset LD_PRELOAD
```

## Project layout

```
.
├── memalloc.c   # Allocator implementation
└── README.md
```

## Limitations

- **First-fit only.** Free blocks are not split. A 4 KB free block can be wholly consumed by an 8-byte request.
- **No coalescing of interior free blocks.** Adjacent free blocks in the middle of the list stay separate. Only the last block can be given back to the OS.
- **No `mmap` path** for large allocations.
- **Linux / `sbrk` only.** Mixing this with glibc internals (or using it as a general-purpose production allocator) is unsafe.
- **Educational locking.** The mutex is used around heap updates; it is not initialized with `PTHREAD_MUTEX_INITIALIZER` in the source, and there is no fork-safety story.

## License

No license file is included in the repository. Treat the code as the author’s unless they add one.
