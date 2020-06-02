#include "pch.h"
#include "dx12buffer.h"
#include "dx12context.h"
#include "dx12render.h"
#include "core.h"

using namespace x12;

void x12::Dx12CoreBuffer::_GPUCopyToStaging()
{
	if (!stagingResource)
	{
		x12::memory::CreateCommittedBuffer(stagingResource.GetAddressOf(), size, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
		x12::d3d12::set_name(stagingResource.Get(), L"Staging buffer for gpu->cpu copying %u bytes for '%s'", size, name.c_str());
	}

	{
		auto* ctx = GetCoreRender()->GetGraphicCommandContext();
		Dx12GraphicCommandContext* dx12ctx = static_cast<Dx12GraphicCommandContext*>(ctx);
		auto d3dCmdList = dx12ctx->GetD3D12CmdList();

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

void x12::Dx12CoreBuffer::GetData(void* data)
{
	ICoreRenderer* renderer = CORE->GetCoreRenderer();
	ICoreGraphicCommandList* context = renderer->GetGraphicCommandContext();

	context->PushState();

	_GPUCopyToStaging();

	context->CommandsEnd();
	context->Submit();
	context->WaitGPUAll(); // execute current GPU work and wait

	_GetStagingData(data);

	context->CommandsBegin();

	context->PopState();
}

void x12::Dx12CoreBuffer::SetData(const void* data, size_t size)
{
	if (flags & BUFFER_FLAGS::CPU_WRITE)
	{
		memcpy(ptr, data, size);
	}
	else
		notImplemented();
}

x12::Dx12CoreBuffer::~Dx12CoreBuffer()
{
	if (flags & BUFFER_FLAGS::CPU_WRITE)
	{
		if (ptr)
		{
			resource->Unmap(0, nullptr);
			ptr = 0;
		}
	}
}

void x12::Dx12CoreBuffer::InitBuffer(size_t structureSize, size_t num, const void* data, BUFFER_FLAGS flags_, LPCWSTR name_)
{
	name = name_;
	flags = flags_;

	if (flags_ & BUFFER_FLAGS::CONSTNAT_BUFFER)
		size = alignConstnatBufferSize(structureSize);
	else
		size = structureSize * num;

	D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	state = D3D12_RESOURCE_STATE_COPY_DEST;

	if (flags_ & BUFFER_FLAGS::CPU_WRITE)
	{
		state = D3D12_RESOURCE_STATE_GENERIC_READ;
		heap = D3D12_HEAP_TYPE_UPLOAD;
	}
	else if (flags_ & BUFFER_FLAGS::UNORDERED_ACCESS)
	{
		flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}

	x12::memory::CreateCommittedBuffer(resource.GetAddressOf(), size, state, heap,
									   D3D12_HEAP_FLAG_NONE, flags);

	if (flags_ & BUFFER_FLAGS::CPU_WRITE)
	{
		resource->Map(0, nullptr, &ptr);
	}

	// upload data
	if (!(flags_ & BUFFER_FLAGS::CPU_WRITE))
	{
		stagingState = D3D12_RESOURCE_STATE_GENERIC_READ;

		ComPtr<ID3D12Resource> uploadResource;
		x12::memory::CreateCommittedBuffer(uploadResource.GetAddressOf(), size,
										   stagingState, D3D12_HEAP_TYPE_UPLOAD);

		x12::d3d12::set_name(uploadResource.Get(), L"Upload buffer for cpu->gpu copying %u bytes for '%s'", size, name_);

		D3D12_SUBRESOURCE_DATA initData = {};
		initData.pData = data;
		initData.RowPitch = size;
		initData.SlicePitch = initData.RowPitch;

		auto* copyContext = GetCoreRender()->GetCopyCommandContext();
		copyContext->CommandsBegin();

		Dx12CopyCommandContext* dx12ctx = static_cast<Dx12CopyCommandContext*>(copyContext);

		UpdateSubresources<1>(dx12ctx->GetD3D12CmdList(), resource.Get(),
							  uploadResource.Get(), 0, 0, data ? 1 : 0, data ? &initData : nullptr);

		copyContext->CommandsEnd();
		copyContext->Submit();
		copyContext->WaitGPUAll(); // wait GPU copying upload -> default heap
	}

	initSRV((UINT)num, (UINT)structureSize, D3D12_BUFFER_SRV_FLAG_NONE, DXGI_FORMAT_UNKNOWN);

	if (flags_ & BUFFER_FLAGS::UNORDERED_ACCESS)
		initUAV((UINT)num, (UINT)structureSize, D3D12_BUFFER_UAV_FLAG_NONE, DXGI_FORMAT_UNKNOWN);

	if (flags_ & BUFFER_FLAGS::CONSTNAT_BUFFER)
	{
		initCBV((UINT)size);
		name = L"(constant buffer)" + name;
	}
	else if (flags_ & BUFFER_FLAGS::RAW_BUFFER)
	{
		initSRV((UINT)size, 1, D3D12_BUFFER_SRV_FLAG_RAW, DXGI_FORMAT_UNKNOWN); //crash wtf?
		name = L"(raw buffer)" + name;
	}
	else
		name = L"(structured buffer)" + name;

	x12::d3d12::set_name(resource.Get(), name.c_str());
}

void x12::Dx12CoreBuffer::initSRV(UINT num, UINT structireSize, D3D12_BUFFER_SRV_FLAGS flag, DXGI_FORMAT format)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = num;
	srvDesc.Buffer.StructureByteStride = structireSize;
	srvDesc.Buffer.Flags = flag;

	SRVdescriptor = d3d12::D3D12GetCoreRender()->AllocateDescriptor();

	d3d12::CR_GetD3DDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, SRVdescriptor.descriptor);
}

void x12::Dx12CoreBuffer::initUAV(UINT num, UINT structireSize, D3D12_BUFFER_UAV_FLAGS flag, DXGI_FORMAT format)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC srvDesc = {};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	srvDesc.Buffer.NumElements = num;
	srvDesc.Buffer.StructureByteStride = structireSize;
	srvDesc.Buffer.Flags = flag;

	UAVdescriptor = d3d12::D3D12GetCoreRender()->AllocateDescriptor();

	d3d12::CR_GetD3DDevice()->CreateUnorderedAccessView(resource.Get(), nullptr, &srvDesc, UAVdescriptor.descriptor);
}

void x12::Dx12CoreBuffer::initCBV(UINT size)
{
	CBVdescriptor = d3d12::D3D12GetCoreRender()->AllocateDescriptor();

	D3D12_CONSTANT_BUFFER_VIEW_DESC desc;
	desc.SizeInBytes = alignConstnatBufferSize(size);
	desc.BufferLocation = resource->GetGPUVirtualAddress();

	d3d12::CR_GetD3DDevice()->CreateConstantBufferView(&desc, CBVdescriptor.descriptor);
}
