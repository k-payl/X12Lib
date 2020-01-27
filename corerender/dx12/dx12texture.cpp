#include "pch.h"
#include "dx12texture.h"
#include "dx12render.h"
#include "dx12descriptorheap.h"

void Dx12CoreTexture::InitFromExistingResource(ID3D12Resource* resource_)
{
	resource.Attach(resource_);
	desc = resource->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	descriptorAllocation = CR_GetDescriptorAllocator()->Allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorAllocation.descriptor;

	CR_GetD3DDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, handle);
}

void Dx12CoreTexture::Release()
{
	--refs;
	assert(refs > 0);

	if (refs == 1)
		CR_ReleaseResource(refs, this);
}
