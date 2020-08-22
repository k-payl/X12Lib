
#include "dx12vertexbuffer.h"
#include "dx12render.h"
#include "dx12commandlist.h"
#include "core.h"
#include "dx12memory.h"

static void UpdateBufferResource(LPCWSTR name, ID3D12GraphicsCommandList* pCmdList,
								 ID3D12Resource* dest, ID3D12Resource** intermediate,
								 UINT64 size, const void* data, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

x12::Dx12CoreVertexBuffer::~Dx12CoreVertexBuffer()
{
}

bool x12::Dx12CoreVertexBuffer::GetReadBarrier(UINT* numBarrires, D3D12_RESOURCE_BARRIER* barriers)
{
	UINT num = 0;

	if (vbState != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER && memoryType & MEMORY_TYPE::GPU_READ)
	{
		barriers[num++] = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), vbState, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
		vbState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	}

	if (indexBuffer && ibState != D3D12_RESOURCE_STATE_INDEX_BUFFER && memoryType & MEMORY_TYPE::GPU_READ)
	{
		barriers[num++] = CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer.Get(), ibState, D3D12_RESOURCE_STATE_INDEX_BUFFER);
		ibState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	}

	if (num > 0)
		*numBarrires = num;

	return num > 0;
}

void x12::Dx12CoreVertexBuffer::Init(LPCWSTR name_, const void* vbData, const VeretxBufferDesc* vbDesc,
									 const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE memoryType_)
{
	name = name_;
	memoryType = memoryType_;

	const bool hasIndexBuffer = idxData != nullptr && idxDesc != nullptr;

	UINT64 bufferSize = getBufferSize(vbDesc);

	UINT64 idxBufferSize;

	D3D12_HEAP_TYPE d3dheapProperties;

	if (memoryType & MEMORY_TYPE::GPU_READ)
	{
		d3dheapProperties = D3D12_HEAP_TYPE_DEFAULT;
		vbState = D3D12_RESOURCE_STATE_COPY_DEST; // for copy cmd list
		ibState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else if (memoryType & MEMORY_TYPE::CPU)
	{
		d3dheapProperties = D3D12_HEAP_TYPE_UPLOAD;
		vbState = D3D12_RESOURCE_STATE_GENERIC_READ; // for upload heap
		ibState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	x12::memory::CreateCommittedBuffer(vertexBuffer.GetAddressOf(), bufferSize, vbState, d3dheapProperties);

	x12::d3d12::set_name(vertexBuffer.Get(), L"Vertex buffer '%s' %u bytes", name.c_str(), bufferSize);

	if (hasIndexBuffer)
	{
		idxBufferSize = (UINT64)formatInBytes(idxDesc->format) * idxDesc->vertexCount;
		x12::memory::CreateCommittedBuffer(indexBuffer.GetAddressOf(), idxBufferSize, ibState, d3dheapProperties);

		x12::d3d12::set_name(vertexBuffer.Get(), L"Index buffer for veretx buffer '%s' %u bytes", name.c_str(), idxBufferSize);
	}

	if (vbData)
		SetData(vbData, bufferSize, 0, idxData, idxBufferSize, 0);

	if (hasIndexBuffer)
	{
		indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();

		switch (idxDesc->format)
		{
			case INDEX_BUFFER_FORMAT::UNSIGNED_16: indexBufferView.Format = DXGI_FORMAT_R16_UINT; break;
			case INDEX_BUFFER_FORMAT::UNSIGNED_32: indexBufferView.Format = DXGI_FORMAT_R32_UINT; break;
			default: assert(0);
		}
		indexBufferView.SizeInBytes = (UINT)idxBufferSize;
	}
	else
		memset(&indexBufferView, 0, sizeof(indexBufferView));

	inputLayout.resize(vbDesc->attributesCount);

	for (int i = 0; i < vbDesc->attributesCount; ++i)
	{
		D3D12_INPUT_ELEMENT_DESC& v = inputLayout[i];

		v.SemanticName = vbDesc->attributes[i].semanticName;
		v.SemanticIndex = 0;
		v.Format = x12::d3d12::engineToDXGIFormat(vbDesc->attributes[i].format);
		v.InputSlot = i;
		v.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		v.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		v.InstanceDataStepRate = 0;
	}

	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = (UINT)bufferSize;
	vertexBufferView.StrideInBytes = getBufferStride(vbDesc);

	vertexCount = vbDesc->vertexCount;
	indexCount = hasIndexBuffer ? idxDesc->vertexCount : 0;
}

void x12::Dx12CoreVertexBuffer::SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset)
{
	if (memoryType & MEMORY_TYPE::GPU_READ)
	{
		assert(vbOffset == 0 && "Not impl");
		assert(idxOffset == 0 && "Not impl");

		auto cmdList = GetCoreRender()->GetCopyCommandContext();
		Dx12CopyCommandList* dx12ctx = static_cast<Dx12CopyCommandList*>(cmdList);
		auto d3dcommandList = dx12ctx->GetD3D12CmdList();

		ComPtr<ID3D12Resource> uploadVertexBuffer;
		ComPtr<ID3D12Resource> uploadIndexBuffer;

		cmdList->CommandsBegin();

		UpdateBufferResource(name.c_str(), d3dcommandList, vertexBuffer.Get(), &uploadVertexBuffer, vbSize, vbData);

		if (indexBuffer)
			UpdateBufferResource(name.c_str(), d3dcommandList, indexBuffer.Get(), &uploadIndexBuffer, idxSize, idxData);

		// Default heap
		// For a default heap, you need to use a fence because the GPU can be doing copying from an 
		// upload heap to a default heap for example at the same time it is drawing

		cmdList->CommandsEnd();

		ICoreRenderer* renderer = engine::GetCoreRenderer();
		renderer->ExecuteCommandList(cmdList);

		renderer->WaitGPUAll(); // wait GPU copying upload -> default heap
	}
	else if (memoryType & MEMORY_TYPE::CPU)
	{
		{
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			throwIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin + vbOffset, vbData, vbSize);
		}
		if (indexBuffer)
		{
			UINT8* pIdxDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			throwIfFailed(indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIdxDataBegin)));
			memcpy(pIdxDataBegin + idxOffset, idxData, idxSize);
		}

		vertexBuffer->Unmap(0, nullptr);
		if (indexBuffer)
			indexBuffer->Unmap(0, nullptr);

	}
}

static void UpdateBufferResource(LPCWSTR name, ID3D12GraphicsCommandList* pCmdList,
								 ID3D12Resource* dest, ID3D12Resource** intermediate,
								 UINT64 size, const void* data, D3D12_RESOURCE_FLAGS flags)
{
	// Upload heap
	// No need fence to track copying RAM -> VRAM
	x12::memory::CreateCommittedBuffer(intermediate, size, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

	x12::d3d12::set_name(*intermediate, L"Intermediate buffer (gpu read) '%s' %u bytes", name, size);

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = data;
	subresourceData.RowPitch = size;
	subresourceData.SlicePitch = subresourceData.RowPitch;

	UpdateSubresources(pCmdList, dest, *intermediate, 0, 0, 1, &subresourceData);
}
