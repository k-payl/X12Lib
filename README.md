# X12Lib

Library for experiments with modern graphic API. Build with VS 2019 (or 2017), C++17 and windows sdk 1809 (build 17763) are required.

## Terminology:
ResourceSet - prebuild set of static resources that binds to pipeline fast at once call.
CommandContext - class that can record gpu commands.

## Constant buffer workflows:
1) Rare updates (manual): Create separate ICoreBuffer with BUFFER_FLAGS::GPU_READ flags (fast GPU access). For common engine parameters, settings, viewport,...
2) More often updates (per-frame): Create ICoreBuffer with BUFFER_FLAGS::CPU_WRITE flags (fast uploading). For camera MVP matrix, positions...
3) Often updates (per-draw): No need create separate buffer. Send to creation shader options { "[name constant buffer], "CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW}. Then update constant buffer through CommandContext.

## Project structure:
* __[root]__ - utility code: windows, main loop, filesystem, input, gpu profiler... (not need for library)
* __corerender__ - main libraray code. Both cpp and headers are here
* __3rdparty__ - third-party code necessary for the library to work
* __dx11__ - helper DirectX 11 library (for comparison performance)
* __font__ - generated bitmap font .dds and meta files 
* __test*__ - shows how to use library