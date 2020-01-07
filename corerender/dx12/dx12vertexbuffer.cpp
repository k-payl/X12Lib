#include "pch.h"
#include "dx12vertexbuffer.h"
#include "dx12render.h"
#include "dx12context.h"
#include "dx12common.h"

IdGenerator<uint16_t> Dx12CoreVertexBuffer::idGen;

static void UpdateBufferResource(ID3D12GraphicsCommandList* pCmdList, 
			ID3D12Resource** dest, ID3D12Resource** intermediate,
			UINT64 size, const void* data, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
	// Default heap
	// For a default heap, you need to use a fence because the GPU can be doing copying from an 
	// upload heap to a default heap for example at the same time it is drawing
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size, flags),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(dest)));

	if (data)
	{
		// Upload heap
		// No need fence to track copying RAM->VRAM
		ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(intermediate)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = size;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(pCmdList, *dest, *intermediate, 0, 0, 1, &subresourceData);
	}
}


void Dx12CoreVertexBuffer::Init(const void* vbData, const VeretxBufferDesc* vbDesc, 
								const void* idxData, const IndexBufferDesc* idxDesc)
{
	id = idGen.getId();

	const bool hasIndexBuffer = idxData != nullptr && idxDesc != nullptr;

	UINT vertexStride = 0;
	for (int i = 0; i < vbDesc->attributesCount; ++i)
	{
		VertexAttributeDesc& attr = vbDesc->attributes[i];
		vertexStride += formatInBytes(attr.format);
	}

	UINT64 bufferSize = (UINT64)vbDesc->vertexCount * vertexStride;

	UINT64 idxBufferSize;

	if (hasIndexBuffer)
		idxBufferSize = (UINT64)formatInBytes(idxDesc->format) * idxDesc->vertexCount;

	{
		auto copyContext = GetCoreRender()->GetCopyCommandContext();
		auto d3dcommandList = copyContext->GetD3D12CmdList();

		ComPtr<ID3D12Resource> uploadVertexBuffer;
		ComPtr<ID3D12Resource> uploadIndexBuffer;

		copyContext->Begin();

		UpdateBufferResource(d3dcommandList, &vertexBuffer, &uploadVertexBuffer, bufferSize, vbData);

		if (hasIndexBuffer)
			UpdateBufferResource(d3dcommandList, &indexBuffer, &uploadIndexBuffer, idxBufferSize, idxData);

		copyContext->End();
		copyContext->Submit();
		copyContext->WaitGPUAll(); // wait GPU copying upload -> default heap
	}

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
		v.Format = engineToDXGIFormat(vbDesc->attributes[i].format);
		v.InputSlot = i;
		v.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		v.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		v.InstanceDataStepRate = 0;
	}

	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = (UINT)bufferSize;
	vertexBufferView.StrideInBytes = vertexStride;

	vertexCount = vbDesc->vertexCount;
	indexCount = hasIndexBuffer ? idxDesc->vertexCount : 0;
}

void Dx12CoreVertexBuffer::Release()
{
	CR_ReleaseResource(refs, this);
}