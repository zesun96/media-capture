# media-capture

Cross-platform media-device capture abstractions with isolated third-party backends.

Implemented components:

- `media_capture::audio`, backed by miniaudio 0.11.25;
- `media_capture::camera`, backed by CameraCapture 1.7.4, with device enumeration, BGRA capture,
  and transactional device switching;
- `media_capture::screen`, backed by screen_capture_lite 17.1.2745, with monitor/window enumeration
  and contiguous BGRA frame capture with optional cursor composition.

Public headers do not expose backend types or include third-party headers. Windows builds the
screen component by default. On Linux, enable it with `-DMEDIA_CAPTURE_BUILD_SCREEN=ON` after
installing the X11, XFixes, XTest, and Xinerama development packages.

## Local build

```powershell
cmake -S . -B out/build/vs2022-x64-debug `
  -DMEDIA_CAPTURE_MINIAUDIO_ROOT=E:/workspace/cpp/lk-sdk/others/miniaudio `
  -DMEDIA_CAPTURE_CAMERA_CAPTURE_ROOT=E:/workspace/cpp/lk-sdk/others/CameraCapture `
  -DMEDIA_CAPTURE_SCREEN_CAPTURE_LITE_ROOT=E:/workspace/cpp/lk-sdk/others/screen_capture_lite
cmake --build out/build/vs2022-x64-debug --config Debug
ctest --test-dir out/build/vs2022-x64-debug -C Debug --output-on-failure
```

Without the local root overrides, CMake downloads pinned release archives and verifies their
SHA-256 hashes.

## Install and consume

On MSVC, build the static libraries with the same runtime as the consumer. For example,
`livekit-client-cpp` uses the static `/MT` runtime, so configure this project with:

```powershell
cmake -S . -B out/build/vs2022-x64-release `
  '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>' `
  -DMEDIA_CAPTURE_MINIAUDIO_ROOT=E:/workspace/cpp/lk-sdk/others/miniaudio `
  -DMEDIA_CAPTURE_CAMERA_CAPTURE_ROOT=E:/workspace/cpp/lk-sdk/others/CameraCapture `
  -DMEDIA_CAPTURE_SCREEN_CAPTURE_LITE_ROOT=E:/workspace/cpp/lk-sdk/others/screen_capture_lite
```

```powershell
cmake --install out/build/vs2022-x64-release --config Release `
  --prefix out/install/vs2022-x64-release
```

Consumers can then use `find_package(media-capture CONFIG REQUIRED)` and link only the components
they need: `media_capture::audio`, `media_capture::camera`, or `media_capture::screen`.

List sources or capture one monitor frame with:

```powershell
out/build/vs2022-x64-debug/examples/Debug/media_capture_screen_sources.exe
out/build/vs2022-x64-debug/examples/Debug/media_capture_capture_screen.exe
```
