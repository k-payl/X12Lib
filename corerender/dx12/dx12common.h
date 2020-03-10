#pragma once
#include "common.h"
#include "icorerender.h"

using psomap_checksum_t = uint64_t;

inline constexpr unsigned D3D12MaxRootParameters = 16;

DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format);

// Resources associated with window
struct Dx12WindowSurface
{
	void Init(HWND hwnd, ID3D12CommandQueue* queue);
	void ResizeBuffers(unsigned width_, unsigned height_);
	void Present();

	unsigned width, height;

	ComPtr<swapchain_t> swapChain;

	ComPtr<ID3D12Resource> colorBuffers[DeferredBuffers];
	ComPtr<ID3D12Resource> depthBuffer;

	ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
	ComPtr<ID3D12DescriptorHeap> descriptorHeapDSV;

	D3D12_RESOURCE_STATES state{};
};

using surface_ptr = std::shared_ptr<Dx12WindowSurface>;

template<typename... Arguments>
void set_name(ID3D12Object *obj, LPCWSTR format, Arguments ...args)
{
	WCHAR wstr[256];
	wsprintf(wstr, format, args...);
	obj->SetName(wstr);
}




