# `libsoa`

[![Linux x86 build](https://github.com/quasar-chunawala/qc-soa/actions/workflows/linux.yml/badge.svg?branch=main)](https://github.com/quasar-chunawala/qc-soa/actions/workflows/linux.yml)
[![MacOS build](https://github.com/quasar-chunawala/qc-soa/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/quasar-chunawala/qc-soa/actions/workflows/macos.yml)
[![Win32/64 build](https://github.com/quasar-chunawala/qc-soa/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/quasar-chunawala/qc-soa/actions/workflows/windows.yml)

`libsoa` is modern C++ (C++26) library providing a full spectrum of SoA(Structure-Of-Arrays) data-structures.

## Configure and Build

To list all presets, execute:

```bash
cmake --list-presets
```

To configure and build the code, run:

```
cmake --preset <preset-name>
cmake --build --preset <preset-name>
```

- To install dependencies for Linux, follow the instructions in [SetupLinux.md](./SetupLinux.md).

- To install dependencies for Win64, refer [SetupWin64.md](./SetupWin64.md).

- To install dependencies for MacOS, refer [SetupMacOS.md](./SetupMacOS.md)

## Compatibility

- **Language Standard**: C++26
- **Compilers Tested**: 
  - GCC 16.2
  

## Integration

This library is header-only and can be copied directly into your project or fetched via CMake.

### CMake FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
  libasoa
  GIT_REPOSITORY https://github.com/quasar-chunawala/qc-soa.git
  GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(soa)

target_link_libraries(your_target PRIVATE soa)
```

## Configuration Macros

### Hardening and Contract Checks

Runtime precondition checks (null pointers, out-of-bounds indices, invalid iterator ranges) can be configured globally:

| Macro | Options / Default | Description |
|---|---|---|
| `HARDENING_MODE` | `HARDENING_MODE_NONE` (Default)<br>`HARDENING_MODE_FAST`<br>`HARDENING_MODE_EXTENSIVE`<br>`HARDENING_MODE_DEBUG` | Enables precondition checks across all API boundaries. |
| `ASSERT_MODE` | `ASSERT_MODE_LOG_TRAP`(Default)<br>`ASSERT_MODE_NONE`<br>`ASSERT_MODE_TRAP`<br>`ASSERT_MODE_ABORT`<br>`ASSERT_MODE_LOG_ABORT` | Defines failure action when a hardening assertion triggers. |

### Example

```cpp
#define HARDENING_MODE HARDENING_MODE_DEBUG
#define ASSERT_MODE ASSERT_MODE_LOG_TRAP
#include <soa/vector.h>
```

### Description

- `HARDENING_MODE_NONE`. When hardening mode is turned off, all checks compile away entirely with zero runtime overhead.
- `HARDENING_MODE_FAST`. A set of securitiy critical checks that can be done with relatively little overhead in constant time and intended to be used in production. 
- `HARDENING_MODE_EXTENSIVE`. Contains all the checks from the fast mode and some additional checks that incur relatively little overhead but aren't security critical. 
- `DEBUG_MODE`. This mode should be used in non-production environments(such as test-suites, CI or local development).

