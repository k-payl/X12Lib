# X12Lib
Library for experiments with modern graphic API. Cmake 3.16, VS 2019, and windows SDK 2004 (build 19041), DXR-compatible GPU tier 1.1 are required.

![Alt text](docs/sponza_1.png?raw=true "Sponza")
![Alt text](docs/sponza_2.png?raw=true "Sponza")
![Alt text](docs/sponza_3.png?raw=true "Sponza")

## Build
* Visual studio 2019: run generate_vs_solution.bat then open x12lib.sln<br />
* VS code: install extensions C/C++, Cmake Tools. Open root folder in editor.<br />
Default engine application is engine_main project.

## Current progress
* Path tracing with one bounce, area lights support
* GLTF scene converter to internal formats (scene, mesh, texture, material)

## Console
Press ~ to open console. <br />
Commands:<br />
* __gpu_profiler__ <1 or 0> - show or hide gpu profiler info 
* __load__ <scene.yaml> - load new scene (relative resources folder)
* __shaders_reload__ - compile modifed shaders

[![Build status](https://ci.appveyor.com/api/projects/status/cyhlpnavp2su9440?svg=true)](https://ci.appveyor.com/project/k-payl/x12lib)
![Benchmark](https://github.com/k-payl/X12Lib/workflows/Benchmark/badge.svg?branch=master)
https://k-payl.github.io/X12Lib/dev/bench/