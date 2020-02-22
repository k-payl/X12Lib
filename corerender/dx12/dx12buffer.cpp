#include "pch.h"
#include "dx12buffer.h"
#include "dx12context.h"
#include "dx12render.h"
#include "core.h"

IdGenerator<uint16_t> Dx12CoreBuffer::idGen;

void Dx12CoreBuffer::_GPUCopyToStaging()
{
	if (!stagingResource)
	{
		x12::memory::CreateCommittedBuffer(stagingResource.GetAddressOf(), size, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
		stagingResource->SetName(L"Staging buffer");
	}

	{
		Dx12GraphicCommandContext* ctx = GetCoreRender()->GetGraphicCommmandContext();
		auto d3dCmdList = ctx->GetD3D12CmdList();

		if (resourceState() != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), resourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE));
			resourceState() = D3D12_RESOURCE_STATE_COPY_SOURCE;
		}
		d3dCmdList->CopyResource(stagingResource.Get(), GetResource());
	}
}

void Dx12CoreBuffer::_GetStagingData(void* data)
{
	void* ptr;
	stagingResource->Map(0, nullptr, &ptr);

	memcpy(data, ptr, size);

	stagingResource->Unmap(0, nullptr);
}

void Dx12CoreBuffer::GetData(void* data)
{
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();
	Dx12GraphicCommandContext* context = renderer->GetGraphicCommmandContext();

	context->PushState();

	_GPUCopyToStaging();

	context->CommandsEnd();
	context->Submit();
	context->WaitGPUAll(); // execute current GPU work and wait

	_GetStagingData(data);

	context->CommandsBegin();

	context->PopState();
}

Dx12CoreBuffer::Dx12CoreBuffer()
{
	id = idGen.getId();
}

// TODO: BUFFER_FLAGS::CPU_WRITE
void Dx12CoreBuffer::InitStructuredBuffer(size_t structureSize, size_t num, const void* data, BUFFER_FLAGS flags)
{
	size = structureSize * num;

	D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;
	if (flags & BUFFER_FLAGS::UNORDERED_ACCESS)
		resFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	state = D3D12_RESOURCE_STATE_COPY_DEST;

	x12::memory::CreateCommittedBuffer(resource.GetAddressOf(), size, state,
									   D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_NONE,
									   resFlags);

	resource->SetName(L"structured buffer");

	// upload data
	{
		stagingState = D3D12_RESOURCE_STATE_GENERIC_READ;

		ComPtr<ID3D12Resource> uploadResource;
		x12::memory::CreateCommittedBuffer(uploadResource.GetAddressOf(), size,
										   stagingState, D3D12_HEAP_TYPE_UPLOAD);
	
		D3D12_SUBRESOURCE_DATA initData = {};
		initData.pData = data;
		initData.RowPitch = size;
		initData.SlicePitch = initData.RowPitch;

		Dx12CopyCommandContext *copyContext = GetCoreRender()->GetCopyCommandContext();
		copyContext->CommandsBegin();
	
		UpdateSubresources<1>(copyContext->GetD3D12CmdList(), resource.Get(),
							  uploadResource.Get(), 0, 0, data?1:0, data? &initData : nullptr);

		copyContext->CommandsEnd();
		copyContext->Submit();
		copyContext->WaitGPUAll(); // wait GPU copying upload -> default heap
	}

	initSRV((UINT)num, (UINT)structureSize, D3D12_BUFFER_SRV_FLAG_NONE, DXGI_FORMAT_UNKNOWN);

	if (flags & BUFFER_FLAGS::UNORDERED_ACCESS)
		initUAV((UINT)num, (UINT)structureSize, D3D12_BUFFER_UAV_FLAG_NONE, DXGI_FORMAT_UNKNOWN);
}
void Dx12CoreBuffer::InitRawBuffer(size_t size_)
{
	size = size_;

	x12::memory::CreateCommittedBuffer(resource.GetAddressOf(), size_,
									   D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_DEFAULT);

	initSRV((UINT)size_, 1, D3D12_BUFFER_SRV_FLAG_RAW, DXGI_FORMAT_UNKNOWN); //crash wtf?
}

void Dx12CoreBuffer::initSRV(UINT num, UINT structireSize, D3D12_BUFFER_SRV_FLAGS flag, DXGI_FORMAT format)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = num;
	srvDesc.Buffer.StructureByteStride = structireSize;
	srvDesc.Buffer.Flags = flag;

	SRVdescriptor = GetCoreRender()->AllocateDescriptor();

	CR_GetD3DDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, SRVdescriptor.descriptor);
}

void Dx12CoreBuffer::initUAV(UINT num, UINT structireSize, D3D12_BUFFER_UAV_FLAGS flag, DXGI_FORMAT format)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = num;
	srvDesc.Buffer.StructureByteStride = structireSize;
	srvDesc.Buffer.Flags = flag;

	UAVdescriptor = GetCoreRender()->AllocateDescriptor();

	CR_GetD3DDevice()->CreateUnorderedAccessView(resource.Get(), nullptr, &srvDesc, UAVdescriptor.descriptor);
}
