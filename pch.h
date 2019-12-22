#pragma once

#include <SDKDDKVer.h>

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#ifdef _DEBUG
#include <dxgidebug.h>
#endif
//#include <DirectXMath.h>

// D3D12 extension library.
#include "d3dx12.h"

#include <stdint.h>

#include <cassert>
#include <vector>
#include <queue>
#include <set>
#include <functional>
#include <memory>

#include "vmath.h"
