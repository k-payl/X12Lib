#include "pch.h"
#include "dx12memory.h"
#include "dx12render.h"

void x12::memory::CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state,
										D3D12_HEAP_TYPE heap, D3D12_HEAP_FLAGS flags,
										D3D12_RESOURCE_FLAGS resFlags)
{
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&CD3DX12_RESOURCE_DESC::Buffer(size, resFlags),
		state,
		nullptr,
		IID_PPV_ARGS(out)));
}

void x12::memory::CreateCommitted2DTexture(ID3D12Resource** out, UINT width, UINT height,
										   UINT mipCount, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS resFlags,
										   D3D12_RESOURCE_STATES state, D3D12_CLEAR_VALUE* clear, D3D12_HEAP_TYPE heap, D3D12_HEAP_FLAGS flags)
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = static_cast<UINT16>(mipCount);
	desc.DepthOrArraySize = 1;
	desc.Format = format;
	desc.Flags = resFlags;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&desc,
		state,
		clear,
		IID_PPV_ARGS(out)));
}

