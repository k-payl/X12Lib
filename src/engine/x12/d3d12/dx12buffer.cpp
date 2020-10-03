
#include "dx12buffer.h"
#include "dx12commandlist.h"
#include "dx12render.h"
#include "core.h"

using namespace x12;

void x12::Dx12CoreBuffer::_GPUCopyToStaging(ICoreGraphicCommandList* cmdList)
{
	if (!stagingResource)
	{
		x12::memory::CreateCommittedBuffer(stagingResource.GetAddressOf(), size, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
		x12::d3d12::set_name(stagingResource.Get(), L"Staging buffer for gpu->cpu copying %u bytes for '%s'", size, name.c_str());
	}

	{
		Dx12GraphicCommandList* dx12ctx = static_cast<Dx12GraphicCommandList*>(cmdList);
		auto d3dCmdList = dx12ctx->GetD3D12CmdList(); // TODO: avoid

		if (resourceState() != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), resourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE));
			resourceState() = D3D12_RESOURCE_STATE_COPY_SOURCE;
		}
		d3dCmdList->CopyResource(stagingResource.Get(), GetResource());
	}
}

void x12::Dx12CoreBuffer::_GetStagingData(void* data)
{
	void* ptr;
	stagingResource->Map(0, nullptr, &ptr);

	memcpy(data, ptr, size);

	stagingResource->Unmap(0, nullptr);
}

void x12::Dx12CoreBuffer::Map()
{
	resource->Map(0, nullptr, &ptr);
}

void x12::Dx12CoreBuffer::Unmap()
{
	if (memoryType & MEMORY_TYPE::CPU)
	{
		if (ptr)
		{
			resource->Unmap(0, nullptr);
			ptr = 0;
		}
	}
}

void x12::Dx12CoreBuffer::GetData(void* data)
{
	ICoreRenderer* renderer = engine::GetCoreRenderer();
	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	_GPUCopyToStaging(cmdList);
	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);

	renderer->WaitGPUAll(); // execute current GPU work and wait

	_GetStagingData(data);
}

void x12::Dx12CoreBuffer::SetData(const void* data, size_t dataSize)
{
	if (!data)
		return;

	if (memoryType & MEMORY_TYPE::CPU)
	{
		Map();
		memcpy(ptr, data, size);
		Unmap();
	}
	else if (memoryType & MEMORY_TYPE::GPU_READ)
	{
		stagingState = D3D12_RESOURCE_STATE_GENERIC_READ;

		ComPtr<ID3D12Resource> uploadResource;
		x12::memory::CreateCommittedBuffer(uploadResource.GetAddressOf(), this->size,
										   stagingState, D3D12_HEAP_TYPE_UPLOAD);

		x12::d3d12::set_name(uploadResource.Get(), L"Upload buffer for cpu->gpu copying %u bytes for '%s'", this->size, name);

		D3D12_SUBRESOURCE_DATA initData = {};
		initData.pData = data;
		initData.RowPitch = dataSize;
		initData.SlicePitch = initData.RowPitch;

		auto* cmdList = GetCoreRender()->GetGraphicCommandList();
		cmdList->CommandsBegin();

		Dx12GraphicCommandList* dx12ctx = static_cast<Dx12GraphicCommandList*>(cmdList);

		if (state != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST)
			dx12ctx->GetD3D12CmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), state, D3D12_RESOURCE_STATE_COPY_DEST));

		UpdateSubresources<1>(dx12ctx->GetD3D12CmdList(), resource.Get(),
							  uploadResource.Get(), 0, 0, data ? 1 : 0, data ? &initData : nullptr);

		if (state != D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST)
			dx12ctx->GetD3D12CmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, state));

		cmdList->CommandsEnd();

		ICoreRenderer* renderer = engine::GetCoreRenderer();
		renderer->ExecuteCommandList(cmdList);

		renderer->WaitGPUAll(); // wait GPU copying upload -> default heap
	}
}

x12::Dx12CoreBuffer::~Dx12CoreBuffer()
{
}

x12::Dx12CoreBuffer::Dx12CoreBuffer(size_t size_, const void* data, MEMORY_TYPE memoryType_, BUFFER_FLAGS flags_, LPCWSTR name_)
{
	name = name_;
	flags = flags_;
	memoryType = memoryType_;
	size = size_;

	//if (flags_ & BUFFER_FLAGS::CONSTANT_BUFFER)
	//	size = alignConstantBufferSize(structureSize);
	//else
	//	size = structureSize * num;

	D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

	state = D3D12_RESOURCE_STATE_COPY_DEST;

	if (memoryType_ & MEMORY_TYPE::CPU)
	{
		state = D3D12_RESOURCE_STATE_GENERIC_READ;
		heap = D3D12_HEAP_TYPE_UPLOAD;
	}
	else if (memoryType_ & MEMORY_TYPE::GPU_READ)
	{
		state = D3D12_RESOURCE_STATE_GENERIC_READ;
		heap = D3D12_HEAP_TYPE_DEFAULT;
	}
	else if (memoryType_ & MEMORY_TYPE::READBACK)
	{
		state = D3D12_RESOURCE_STATE_GENERIC_READ;
		heap = D3D12_HEAP_TYPE_READBACK;
	}

	if (flags_ & BUFFER_FLAGS::UNORDERED_ACCESS)
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	x12::memory::CreateCommittedBuffer(resource.GetAddressOf(), size, state, heap,
									   D3D12_HEAP_FLAG_NONE, flags);

	x12::d3d12::set_name(resource.Get(), name.c_str());

	SetData(data, size);
}

void x12::Dx12CoreBuffer::initSRV(UINT num, UINT structireSize, bool raw)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = raw? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = raw ? num / 4 : num;
	srvDesc.Buffer.StructureByteStride = structireSize;
	srvDesc.Buffer.Flags = raw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

	SRVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticDescriptor();

	d3d12::CR_GetD3DDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, SRVdescriptor.descriptor);
}

void x12::Dx12CoreBuffer::initUAV(UINT num, UINT structireSize, bool raw)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = raw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.NumElements = raw ? num/4 : num;
	uavDesc.Buffer.StructureByteStride = structireSize;
	uavDesc.Buffer.Flags = raw? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

	UAVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticDescriptor();

	d3d12::CR_GetD3DDevice()->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, UAVdescriptor.descriptor);
}

void x12::Dx12CoreBuffer::initCBV(UINT size)
{
	CBVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticDescriptor();

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.SizeInBytes = alignConstantBufferSize(size);
	desc.BufferLocation = resource->GetGPUVirtualAddress();

	d3d12::CR_GetD3DDevice()->CreateConstantBufferView(&desc, CBVdescriptor.descriptor);
}
