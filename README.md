# CPSC 410/611 - Operating Systems 

A series of machine problems building a functional OS kernel from scratch in C++ and x86 Assembly, developed as part of the Operating Systems course at Texas A&M University.

---

## Overview

This repository contains incrementally built kernel components, starting from low-level memory management and progressing through virtual memory, thread scheduling, and device drivers. Each machine problem (MP) builds on the previous one.

---

## Machine Problems

### MP2 — Frame Manager
Implements a physical memory frame pool manager (`ContFramePool`) for a 32MB system.
- Bitmap-based tracking of free/used frames
- Support for multiple frame pools (kernel pool: 2–4MB, process pool: above 4MB)
- Contiguous frame allocation and release
- External management info storage for process pool frames

### MP3 — Page Table Management
Sets up x86 paging infrastructure for a single address space.
- Two-level page table (page directory + page table pages)
- Direct mapping of first 4MB (kernel space)
- Demand paging via page fault handler (Exception 14) for memory above 4MB
- CR0/CR2/CR3 register manipulation to enable paging

### MP4 — Virtual Memory Management
Extends the memory system to support large address spaces and dynamic allocation.
- Recursive page table lookup (last PDE points back to page directory)
- Page table pages stored in virtual (mapped) memory via process frame pool
- Virtual memory pool (`VMPool`) with lazy frame allocation
- Hooks into C++ `new`/`delete` operators for dynamic memory allocation

### MP5 — Kernel-Level Thread Scheduling
Adds multi-threading and a FIFO scheduler to the kernel.
- Thread creation with explicit stack management
- Context switching via low-level dispatcher (`dispatch_to`)
- FIFO ready queue with `yield()`, `add()`, `resume()`, and `terminate()`
- Support for cleanly terminating thread functions (zombie queue pattern)

### MP6 — Primitive Disk Device Driver
Implements a non-busy-waiting disk driver on top of a programmed-I/O ATA controller.
- `SimpleDisk`: baseline driver using busy-wait polling
- `NonBlockingDisk`: derived class that yields the CPU while waiting for I/O
- Integrates with the MP5 scheduler to block/unblock threads around disk operations

---

## Tech Stack

- **Language:** C++, x86 Assembly (NASM)
- **Architecture:** x86 (32-bit protected mode)
- **Emulator:** QEMU / Bochs
- **Build:** GNU Make

---

## Build & Run

```bash
make
# Then boot the kernel image in QEMU or Bochs as configured in the Makefile
```

Each MP directory contains its own `Makefile`. Unzip the corresponding `mpX.zip`, run `make`, and boot the resulting kernel binary.

---

## Repository Structure

```
mp2/   # Frame Manager
mp3/   # Page Table Management
mp4/   # Virtual Memory Management
mp5/   # Thread Scheduling
mp6/   # Disk Device Driver
```

---

## Course

**CPSC 410/611/613 — Operating Systems**  
Texas A&M University
