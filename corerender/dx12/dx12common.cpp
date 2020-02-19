#include "pch.h"
#include "dx12common.h"
#include "dx12render.h"

static std::mutex resourcesMutex;
std::vector<IResourceUnknown*> IResourceUnknown::resources;

IResourceUnknown::IResourceUnknown()
{
	AddRef();
	std::scoped_lock guard(resourcesMutex);
	resources.push_back(this);
}

void IResourceUnknown::ReleaseResource(int& refs, IResourceUnknown* ptr)
{
	assert(refs == 1);

	std::scoped_lock guard(resourcesMutex);

	auto it = std::find_if(resources.begin(), resources.end(), [ptr](const IResourceUnknown* r) -> bool
	{
		return r == ptr;
	});

	assert(it != resources.end());

	resources.erase(it);
	delete ptr;
}

DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: assert(0);
	}
	return DXGI_FORMAT_UNKNOWN;
}

void IResourceUnknown::Release()
{
	--refs;
	assert(refs > 0);

	if (refs == 1)
		ReleaseResource(refs, this);
}

void IResourceUnknown::CheckResources()
{
	for (auto& r : resources)
	{
		if (r->GetRefs() != 1)
			throw std::exception("Resource is not released properly");
	}
}

void x12::memory::CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state, D3D12_HEAP_TYPE heap, D3D12_HEAP_FLAGS flags)
{
	return ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
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

	return ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heap),
		flags,
		&desc,
		state,
		clear,
		IID_PPV_ARGS(out)));
}
