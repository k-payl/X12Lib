#pragma once
#include "pch.h"
#include "common.h"
#include "icorerender.h"

using psomap_checksum_t = uint64_t;

inline constexpr unsigned D3D12MaxRootParameters = 16;

DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format);

// Resources associated with window
struct Dx12WindowSurface
{
	void Init(HWND hwnd, ID3D12CommandQueue* queue);

	unsigned width, height;

	ComPtr<swapchain_t> swapChain;

	ComPtr<ID3D12Resource> colorBuffers[DeferredBuffers];
	ComPtr<ID3D12Resource> depthBuffer;

	ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
	ComPtr<ID3D12DescriptorHeap> descriptorHeapDSV;

	D3D12_RESOURCE_STATES state{};

	void ResizeBuffers(unsigned width_, unsigned height_);
	void Present();
};

using surface_ptr = std::shared_ptr<Dx12WindowSurface>;

namespace x12::memory
{
	void CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state,
							   D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
							   D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE,
							   D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE);

	void CreateCommitted2DTexture(ID3D12Resource** out, UINT w, UINT h, UINT mipmaps, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS texFlags,
								  D3D12_RESOURCE_STATES state,
								  D3D12_CLEAR_VALUE *clear = nullptr,
								  D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
								  D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE);
}

