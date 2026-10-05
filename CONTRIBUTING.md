# Contributing

Thanks for considering a contribution. This is a small library, so the bar for
what gets merged is mostly about *keeping it small and correct*.

## Scope

Contributions are welcome for:

- **New portable modules** in `src/` — pure C99, no vendor headers, no dynamic
  allocation, no blocking calls.
- **New platform ports** in `port/<platform>/` — implementations of the four
  functions in `include/ekt/ekt_port.h`.
- **Test coverage** for existing modules.
- **Documentation fixes.**

Contributions that will not be merged:

- Anything that pulls a vendor SDK or RTOS header into `src/`. That belongs
  behind a port.
- Anything that allocates. The core must run without a heap.
- Anything that blocks. If an operation takes time, return a status code.

## Requirements for a new module

1. Header goes in `include/ekt/ekt_<name>.h`, implementation in
   `src/ekt_<name>.c`.
2. Use the `ekt_` prefix.
3. Validate every input at the boundary. Return an error, never a silent
   default.
4. No `delay`, no `while` loop waiting on hardware, no `malloc`.
5. Static buffers only, with a compile-time size cap.
6. A `/* SPDX-License-Identifier: MIT */` line at the top of every file.

## Requirements for a new port

1. Implement all four functions declared in `include/ekt/ekt_port.h`.
2. Document which pin, peripheral, or timing assumptions the port bakes in.
3. Every blocking operation must be bounded by a timeout and return a
   failure code rather than spinning forever.

## Before you open a pull request

```bat
run_tests.bat        :: Windows
./run_tests.sh       :: Linux / macOS
```

The suite must build clean under `-Wall -Wextra -Wpedantic -Werror` and every
assertion must pass. There are no exceptions to the warning flags — that is
the whole point of the `-Werror` in the build script.

## Commit messages

Use a conventional prefix:

```
feat: add 74HC595 chain helper
fix: correct checksum when frame contains a comma
docs: clarify bit-timing return values
test: cover the empty-payload frame case
```
