# X12Lib
Library for experiments with modern graphic API. VS 2019, and windows SDK 1809 (build 17763) are required.

## Terminology:
ResourceSet - prebuild set of static resources that binds to pipeline fast at once call. \
CommandContext - class that can record gpu commands.

## Constant buffer workflows:
1) __Rare updates__ (manual): Create separate ICoreBuffer with BUFFER_FLAGS::GPU_READ flags (fast GPU access). For common engine parameters, settings, viewport... Further updates are made through __ICoreBuffer::SetData()__
2) __Per-frame updates__: Create separate ICoreBuffer with BUFFER_FLAGS::CPU_WRITE flags (fast uploading). For camera MVP matrix, positions... Further updates are made through __ICoreBuffer::SetData()__
3) __Per-draw updates__: No need create separate buffer. Send to creation shader options { "[constant buffer name]", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW}. Then update constant buffer through __CommandContext::UpdateInlineConstantBuffer()__

[![Build status](https://ci.appveyor.com/api/projects/status/cyhlpnavp2su9440?svg=true)](https://ci.appveyor.com/project/k-payl/x12lib)
![Vector math benchmark](https://github.com/k-payl/X12Lib/workflows/Vector%20math%20benchmark/badge.svg)
https://k-payl.github.io/X12Lib/dev/bench/

![Alt text](d3d12.png?raw=true "d3d12")
![Alt text](d3d11.png?raw=true "d3d11")

