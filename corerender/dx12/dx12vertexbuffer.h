#pragma once
#include "common.h"
#include "icorerender.h"

struct Dx12CoreVertexBuffer : public ICoreVertexBuffer
{
	void Init(const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_USAGE usage = BUFFER_USAGE::GPU_READ);
	void SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset);

	~Dx12CoreVertexBuffer();

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

	int refs{};

	static IdGenerator<uint16_t> idGen;
	uint16_t id;
	BUFFER_USAGE usage;
};
