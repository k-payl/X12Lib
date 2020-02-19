#pragma once
#include "pch.h"
#include "common.h"
#include "icorerender.h"

inline constexpr unsigned D3D12MaxRootParameters = 16;

DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format);

namespace x12::memory
{
	void CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state,
							   D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
							   D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE);

	void CreateCommitted2DTexture(ID3D12Resource** out, UINT w, UINT h, UINT mipmaps, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS texFlags,
								  D3D12_RESOURCE_STATES state,
								  D3D12_CLEAR_VALUE *clear = nullptr,
								  D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
								  D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE);
}

