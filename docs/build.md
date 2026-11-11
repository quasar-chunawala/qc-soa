# Configure and Build

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

