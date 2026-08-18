# media-capture

Cross-platform media-device capture abstractions with isolated third-party backends.

The first implemented component is `media_capture::audio`, backed by miniaudio 0.11.25. Public
headers do not expose miniaudio types or include `miniaudio.h`.

## Local build

```powershell
cmake -S . -B out/build/vs2022-x64-debug `
  -DMEDIA_CAPTURE_MINIAUDIO_ROOT=E:/workspace/cpp/lk-sdk/others/miniaudio
cmake --build out/build/vs2022-x64-debug --config Debug
ctest --test-dir out/build/vs2022-x64-debug -C Debug --output-on-failure
```

Without `MEDIA_CAPTURE_MINIAUDIO_ROOT`, CMake downloads the pinned 0.11.25 source archive and
verifies its SHA-256 hash.
