#pragma once
#include "common.h"
#include "icorerender.h"


struct Dx12CoreVertexBuffer : public ICoreVertexBuffer
{
	Dx12CoreVertexBuffer(
		uint32_t vertexCount_,
		ComPtr<ID3D12Resource> vertexBufer_,
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView_,
		uint32_t indexCount_,
		ComPtr<ID3D12Resource> indexBuffer_,
		D3D12_INDEX_BUFFER_VIEW indexBufferView_,
		std::vector<D3D12_INPUT_ELEMENT_DESC>&& inputLayout_) :

		vertexCount(vertexCount_),
		vertexBuffer(vertexBufer_),
		vertexBufferView(vertexBufferView_),
		indexCount(indexCount_),
		indexBuffer(indexBuffer_),
		indexBufferView(indexBufferView_),
		inputLayout(std::move(inputLayout_))
	{
		id = idGen.getId();
	}

	inline void AddRef() override { refs++; }
	inline int GetRefs() override { return refs; }
	void Release() override;

	uint32_t vertexCount;
	ComPtr<ID3D12Resource> vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

	uint32_t indexCount;
	ComPtr<ID3D12Resource> indexBuffer;
	D3D12_INDEX_BUFFER_VIEW indexBufferView;

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

	uint16_t ID() { return id; }

private:
	~Dx12CoreVertexBuffer() override = default;

	int refs{};

	static IdGenerator<uint16_t> idGen;
	uint16_t id;
};