# C-projects

A collection of low-level C/C++ projects completed for university coursework, focused on memory management, filesystem parsing, signal-driven logging, and process scheduling on POSIX systems.

## Repository Structure

### `own-malloc/`

Custom implementation of `malloc`/`calloc`/`realloc`/`free` built on top of raw system calls for memory acquisition (`custom_sbrk()`), without relying on the standard library allocator.

### `file-reader-lib/`

A FAT12/FAT16 file container parser. Provides an API to open/read block devices, mount FAT volumes, and traverse, search, and read files and directories — read-only, no write support.

### `signal-log-lib/`

A thread-safe logging library controlled entirely via real-time POSIX signals. Supports asynchronous signal handling, on-demand state dump files, toggling logging on/off, and switching between three logging detail levels (MIN / STANDARD / MAX) at runtime.

### `cron-like-app/`

A functional clone of Unix `cron`. Implements interval-timer-based job scheduling with a client-server architecture (single running instance, message-queue-based IPC), support for relative/absolute/recurring schedules, job listing/cancellation, and logging via the `signal-log-lib` project.

## Tech Stack

C · POSIX APIs (signals, message queues, system calls) · Linux

---

Each folder contains its own source code.
