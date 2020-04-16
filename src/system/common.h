#pragma once

#include <SDKDDKVer.h>

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
			inline void CheckForDuplicateEntries();
		}
	}
}
#endif
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
//#include <DirectXMath.h>

// D3D12 extension library.
#include "d3dx12.h"

#include <stdint.h>
#include <cassert>
#include <vector>
#include <stack>
#include <queue>
#include <set>
#include <functional>
#include <map>
#include <mutex>
#include <memory>

#include "vmath.h"

// TODO: split to private an public part

inline constexpr int DeferredBuffers = 3;

#define DATA_DIR "..//resources//"
#define WDATA_DIR L"..//resources//"
#define SHADER_DIR "shaders//"

enum class WINDOW_MESSAGE;
class MainWindow;
class Core;
class Input;
class Console;
class GpuProfiler;
class FileSystem;

namespace x12 {
	class Dx12CoreRenderer;
	struct Dx12CoreShader;
	struct Dx12WindowSurface;
	struct Dx12CoreVertexBuffer;
	struct Dx12CoreBuffer;
	class Dx12BaseCommandList;
	class Dx12GraphicCommandContext;
	class Dx12CopyCommandContext;
	struct Dx12ResourceSet;
	struct Dx12CoreTexture;
}

#define DEFINE_ENUM_OPERATORS(ENUM_NAME) \
inline ENUM_NAME operator|(ENUM_NAME a, ENUM_NAME b) \
{ \
	using T = std::underlying_type_t <ENUM_NAME>; \
	return static_cast<ENUM_NAME>(static_cast<T>(a) | static_cast<T>(b)); \
} \
inline bool operator&(ENUM_NAME a, ENUM_NAME b) \
{ \
	return static_cast<bool>(static_cast<int>(a) & static_cast<int>(b)); \
}

namespace x12::descriptorheap
{
	struct Alloc;
	class Allocator;
}

typedef ID3D12Device2 device_t;
typedef IDXGIAdapter4 adapter_t;
typedef IDXGISwapChain4 swapchain_t;
typedef IDXGIFactory4 dxgifactory_t;

typedef void (*WindowCallback)(HWND, WINDOW_MESSAGE, uint32_t, uint32_t, void*);
typedef void (*RenderProcedure)();
typedef void (*InitProcedure)();
typedef void (*InitRendererProcedure)(void*);
typedef void (*UpdateProcedure)(float dt);
typedef void (*FreeProcedure)();

typedef void (*FinishFrameBroadcast)(uint64_t);

template<class T>
void Release(T*& ptr)
{
	if (ptr)
	{
		ptr->Release();
		ptr = nullptr;
	}
}

template<class T>
class IdGenerator
{
	T id{1};
public:
	T getId() { return id++; }
};

#define verify(f)          ((void)(f))

inline void unreacheble()
{
	throw std::runtime_error("Mustn't happen");
}
inline void notImplemented()
{
	throw std::runtime_error("Not implemented");
}

inline math::vec3 getRightDirection(const math::mat4& ModelMat) { return math::vec3(ModelMat.Column(0)); } // Returns local X vector in world space
inline math::vec3 getForwardDirection(const math::mat4& ModelMat) { return math::vec3(ModelMat.Column(1)); }
inline math::vec3 getBackDirection(const math::mat4& ModelMat) { return -math::vec3(ModelMat.Column(2)); }

template<typename... Signature>
class Signal
{
	typedef void(*SignalCallback)(Signature...);
	std::vector<SignalCallback> functions;

public:
	void Add(SignalCallback func)
	{
		functions.push_back(func);
	}

	void Erase(SignalCallback func)
	{
		functions.erase(std::remove(functions.begin(), functions.end(), func), functions.end());
	}

	template<typename ... Args>
	void Invoke(Args&& ...args) const
	{
		for (auto f : functions)
			f(std::forward<Args>(args)...);
	}
};

inline void ThrowIfFailed(HRESULT hr)
{
	assert((hr) == S_OK);
}

template<class T>
inline constexpr T alignConstnatBufferSize(T size)
{
	return (size + 255) & ~255;
}

inline UINT alignResource(UINT size, UINT alignment)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

std::string ConvertFromUtf16ToUtf8(const std::wstring& wstr);
std::wstring ConvertFromUtf8ToUtf16(const std::string& str);

struct RuntimeDescriptor
{
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
	uint64_t lastFrame;
	uint32_t lastDrawCall;
};
struct RuntimeDescriptorsVec
{
	std::vector<RuntimeDescriptor> handles;
};

static ID3D12DescriptorHeap* CreateDescriptorHeap(device_t *device, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, bool gpu = false)
{
	ID3D12DescriptorHeap* heap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;
	desc.Flags = gpu ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

	return heap;
}

enum class WINDOW_MESSAGE
{
	SIZE,
	KEY_DOWN,
	KEY_UP,
	MOUSE_MOVE,
	MOUSE_DOWN,
	MOUSE_UP,
	WINDOW_DEACTIVATED,
	WINDOW_ACTIVATED,
	APPLICATION_DEACTIVATED,
	APPLICATION_ACTIVATED,
	WINDOW_MINIMIZED,
	WINDOW_UNMINIMIZED,
	WINDOW_REDRAW,
	WINDOW_CLOSE
};

enum class KEYBOARD_KEY_CODES
{
	KEY_UNKNOWN = 0x0,

	KEY_ESCAPE = 0x01,
	KEY_TAB = 0x0F,
	KEY_GRAVE = 0x29,
	KEY_CAPSLOCK = 0x3A,
	KEY_BACKSPACE = 0x0E,
	KEY_RETURN = 0x1C,
	KEY_SPACE = 0x39,
	KEY_SLASH = 0x35,
	KEY_BACKSLASH = 0x2B,

	KEY_SYSRQ = 0xB7,
	KEY_SCROLL = 0x46,
	KEY_PAUSE = 0xC5,

	KEY_INSERT = 0xD2,
	KEY_DELETE = 0xD3,
	KEY_HOME = 0xC7,
	KEY_END = 0xCF,
	KEY_PGUP = 0xC9,
	KEY_PGDN = 0xD1,

	KEY_LSHIFT = 0x2A,
	KEY_RSHIFT = 0x36,
	KEY_LALT = 0x38,
	KEY_RALT = 0xB8,
	KEY_LWIN_OR_CMD = 0xDB,
	KEY_RWIN_OR_CMD = 0xDC,
	KEY_LCONTROL = 0x1D,
	KEY_RCONTROL = 0x9D,

	KEY_UP = 0xC8,
	KEY_RIGHT = 0xCD,
	KEY_LEFT = 0xCB,
	KEY_DOWN = 0xD0,

	KEY_1 = 0x02,
	KEY_2 = 0x03,
	KEY_3 = 0x04,
	KEY_4 = 0x05,
	KEY_5 = 0x06,
	KEY_6 = 0x07,
	KEY_7 = 0x08,
	KEY_8 = 0x09,
	KEY_9 = 0x0A,
	KEY_0 = 0x0B,

	KEY_F1 = 0x3B,
	KEY_F2 = 0x3C,
	KEY_F3 = 0x3D,
	KEY_F4 = 0x3E,
	KEY_F5 = 0x3F,
	KEY_F6 = 0x40,
	KEY_F7 = 0x41,
	KEY_F8 = 0x42,
	KEY_F9 = 0x43,
	KEY_F10 = 0x44,
	KEY_F11 = 0x57,
	KEY_F12 = 0x58,

	KEY_Q = 0x10,
	KEY_W = 0x11,
	KEY_E = 0x12,
	KEY_R = 0x13,
	KEY_T = 0x14,
	KEY_Y = 0x15,
	KEY_U = 0x16,
	KEY_I = 0x17,
	KEY_O = 0x18,
	KEY_P = 0x19,
	KEY_A = 0x1E,
	KEY_S = 0x1F,
	KEY_D = 0x20,
	KEY_F = 0x21,
	KEY_G = 0x22,
	KEY_H = 0x23,
	KEY_J = 0x24,
	KEY_K = 0x25,
	KEY_L = 0x26,
	KEY_Z = 0x2C,
	KEY_X = 0x2D,
	KEY_C = 0x2E,
	KEY_V = 0x2F,
	KEY_B = 0x30,
	KEY_N = 0x31,
	KEY_M = 0x32,

	KEY_MINUS = 0x0C,
	KEY_PLUS = 0x0D,
	KEY_LBRACKET = 0x1A,
	KEY_RBRACKET = 0x1B,

	KEY_SEMICOLON = 0x27,
	KEY_APOSTROPHE = 0x28,

	KEY_COMMA = 0x33,
	KEY_PERIOD = 0x34,

	KEY_NUMPAD0 = 0x52,
	KEY_NUMPAD1 = 0x4F,
	KEY_NUMPAD2 = 0x50,
	KEY_NUMPAD3 = 0x51,
	KEY_NUMPAD4 = 0x4B,
	KEY_NUMPAD5 = 0x4C,
	KEY_NUMPAD6 = 0x4D,
	KEY_NUMPAD7 = 0x47,
	KEY_NUMPAD8 = 0x48,
	KEY_NUMPAD9 = 0x49,
	KEY_NUMPADPERIOD = 0x53,
	KEY_NUMPADENTER = 0x9C,
	KEY_NUMPADSTAR = 0x37,
	KEY_NUMPADPLUS = 0x4E,
	KEY_NUMPADMINUS = 0x4A,
	KEY_NUMPADSLASH = 0xB5,
	KEY_NUMLOCK = 0x45,
};

enum class MOUSE_BUTTON
{
	LEFT,
	RIGHT,
	MIDDLE
};
