# EngineEngine

EngineEngine is a C++20 real-time rendering playground built around Vulkan, SDL3, CMake, and Conan. It contains forward and deferred renderers, GLTF asset loading, PBR/material experiments, terrain rendering, shadow mapping, and a small shader playground.

The project is under active development, so expect renderer internals and sample apps to change as features are explored.

## Features

- Vulkan rendering backend with SDL3 windowing
- Forward and deferred rendering sample applications
- Directional and point shadow maps, including experimental cascaded directional shadows
- GLTF mesh/material loading through TinyGLTF
- PBR-oriented material path and debug material views
- Terrain sample with streaming chunks and water rendering experiments
- ImGui debug UI, frame timing tools, and shader/debug visualizations
- GoogleTest-based test target

## Repository Layout

```text
assets/        Runtime assets and GLSL shaders
src/           Engine modules and sample applications
test/          Unit/integration test target
third_party/   Vendored third-party code used directly by CMake
CMake/         Shared CMake helpers
```

Important app targets include:

- `forward`
- `deferred`
- `terrain`
- `shader_playground`
- `Test`

## Requirements

- Visual Studio 2022 with MSVC
- Windows SDK
- Vulkan SDK
- Conan
- CMake

Dependencies are declared in `conanfile.txt`.

## Build

Debug build:

```bat
GenerateAndBuild.bat
```

Or manually:

```bat
conan install . --build=missing -pr=debug20
cmake --preset=conan-default
cmake --build --preset=conan-debug
```

Build just the tests:

```bat
cmake --build build --config Debug --target Test
```

## Notes

The Visual Studio solution is generated into `build/`. Runtime binaries are emitted under `x64/Debug` or `x64/Release` depending on configuration.

Some features are intentionally experimental, especially the active rendering research paths such as cascaded shadows and terrain/water rendering.
