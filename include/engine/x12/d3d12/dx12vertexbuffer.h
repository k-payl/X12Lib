#pragma once
#include "dx12common.h"

namespace x12
{
	struct Dx12CoreVertexBuffer final : public ICoreVertexBuffer
	{
		void Init(LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE memory_type_);
		void SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset) override;

		~Dx12CoreVertexBuffer();

		MEMORY_TYPE memoryType;

		uint32_t vertexCount;
		ComPtr<ID3D12Resource> vertexBuffer;
		std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
		D3D12_RESOURCE_STATES vbState;

		uint32_t indexCount;
		ComPtr<ID3D12Resource> indexBuffer;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		D3D12_RESOURCE_STATES ibState;

		std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

		const D3D12_INDEX_BUFFER_VIEW* pIndexBufferVew() const { return indexBufferView.SizeInBytes > 0 ? &indexBufferView : nullptr; }

		bool GetReadBarrier(UINT* numBarrires, D3D12_RESOURCE_BARRIER* barriers);

	private:

		std::wstring name;
	};

}
