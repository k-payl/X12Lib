# X12Lib

Library for experiments with modern graphic API. Now I focused only on DirectX 12, but there is possibility of adding another API. Build with VS 2019 (or 2017), C++17 and windows sdk 1809 (10.0 build 17763) are required.

## Project structure:
* __[root]__ - utility code: windows, main loop, filesystem, input, gpu profiler... (not need for library)
* __corerender__ - main libraray code. Both cpp and headers are here
* __3rdparty__ - third-party code necessary for the library to work
* __dx11__ - helper DirectX 11 library (for comparison performance)
* __font__ - generated bitmap font .dds and meta files 
* __test*__ - shows how to use library
* __bin__ - binaries (generated, feel free to remove)
* __obj__ - temporary compiler files (generated, feel free to remove)