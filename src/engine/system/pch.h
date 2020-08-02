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

// Workaround for error C2039: 'CheckForDuplicateEntries': is not a member of 'Microsoft::WRL::Details'
#if !defined(_DEBUG)
namespace Microsoft {
	namespace WRL {
		namespace Details {
			template <typename T>
			inline void CheckForDuplicateEntries() {}
		}
	}
}
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

// Vulkan specific headers.
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

// D3D12 extension library.
#include "d3dx12.h"

#if VK_ENABLE
#include "vulkan.h"
#include "vk_mem_alloc.h"
#endif

#include <stdint.h>

#include <cassert>
#include <vector>
#include <stack>
#include <queue>
#include <set>
#include <functional>
#include <memory>
#include <mutex>
#include <memory>

#include "vmath.h"
