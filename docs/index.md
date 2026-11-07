---
hide:
  - toc
---

# `libsoa`

[![Linux x86 build](https://github.com/quasar-chunawala/qc-soa/actions/workflows/linux.yml/badge.svg?branch=main)](https://github.com/quasar-chunawala/qc-soa/actions/workflows/linux.yml)
[![MacOS build](https://github.com/quasar-chunawala/qc-soa/actions/workflows/macos.yml/badge.svg?branch=main)](https://github.com/quasar-chunawala/qc-soa/actions/workflows/macos.yml)
[![Win32/64 build](https://github.com/quasar-chunawala/qc-soa/actions/workflows/windows.yml/badge.svg?branch=main)](https://github.com/quasar-chunawala/qc-soa/actions/workflows/windows.yml)

`libsoa` is modern C++ (C++26) library providing a full spectrum of SoA(Structure-Of-Arrays) data-structures.

## Data Oriented Design Philosophy

According to Data-Oriented-Design(DoD) principles, *the purpose of all programs and all parts of those programs, is to transform data from one form to another*. If you don't understand the data, you don't understand the problem. 

Data oriented design is all about reorganizing your data-structures around for better performance. Andrew Kelley’s talk on the topic—especially his use of Zig’s `MultiArrayList` — offers a compelling real-world example.

## Array-of-Structures(AoS)

In a game-engine, imagine a scene where a rocket flies through the air, leaving behind a smoke trail. Suddenly, the rocket explodes leaving sparks flying everywhere. Out of the disintegrating rocket, a creature is jettisoned towards you. What do most of these elements have in common? In terms of technology, such a scene in a game would benefit from a `Particle` system. A `Particle` system is typically defined as follows:

```cpp
// 64 bytes
struct Particle{
  float lifetime_;  // 4 bytes, 4 bytes padding  
  Vec3 position_;   // 24 bytes
  Vec3 velocity_;   // 24 bytes
  uint32_t color_;  // 4 bytes, 4 bytes padding
};

std::vector<Particle> particles;
```

The above is Array-of-Structs(AoS) layout. AoS layout keeps each complete `Particle` object contigous, which is convenient and can be efficient during iteration when a one pass needs the whole object. 

Consider, however an update loop, which reads only position and velocity. Cache lines fetched while traversing the vector may also contain `color_`, `lifetime_`, and padding that the loop does not use.

```cpp
void update_positions(std::vector<Particle>& particles, float dt){
  for(auto& particle : particles){
    particle.position_ += particle.velocity_ * dt;
  }
}
```

Thus, the loop is wasteful. For particle systems, ECS components, and vertex processing, grouping the same fields across many objects can better match a hot loop’s access pattern.

## Structure-of-Arrays(SoA) approach

```cpp
struct ParticleSystem
{
  std::vector<float> lifetimes_;
  std::vector<Vec3> positions_;
  std::vector<Vec3> velocities_;
  std::vector<uint32_t> colors_;
};
```

Positions and velocities now occupy separate, tightly traverse arrays. The update streams through the fields it needs while leaving colors and lifetimes untouched.

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


