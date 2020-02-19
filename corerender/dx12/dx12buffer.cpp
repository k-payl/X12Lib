#include "pch.h"
#include "dx12buffer.h"
#include "dx12context.h"
#include "dx12render.h"

IdGenerator<uint16_t> Dx12CoreBuffer::idGen;

void Dx12CoreBuffer::InitStructuredBuffer(size_t structureSize, size_t num, const void* data)
{
	const UINT64 sizeUploadBuffer = structureSize * num;
	
	x12::memory::CreateCommittedBuffer(resource.GetAddressOf(), sizeUploadBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_DEFAULT);

	ComPtr<ID3D12Resource> uploadResource;
	x12::memory::CreateCommittedBuffer(uploadResource.GetAddressOf(), sizeUploadBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);
	
	D3D12_SUBRESOURCE_DATA particlesData = {};
	particlesData.pData = data;
	particlesData.RowPitch = sizeUploadBuffer;
	particlesData.SlicePitch = particlesData.RowPitch;

	auto copyContext = GetCoreRender()->GetCopyCommandContext();
	auto d3dcommandList = copyContext->GetD3D12CmdList();

	copyContext->Begin();
	
	UpdateSubresources<1>(d3dcommandList, resource.Get(), uploadResource.Get(), 0, 0, 1, &particlesData);

	copyContext->End();
	copyContext->Submit();
	copyContext->WaitGPUAll(); // wait GPU copying upload -> default heap

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = (UINT)num;
	srvDesc.Buffer.StructureByteStride = (UINT)structureSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	descriptorAllocation = GetCoreRender()->AllocateDescriptor();
	
	CR_GetD3DDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, descriptorAllocation.descriptor);}
