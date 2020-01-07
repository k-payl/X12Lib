#pragma once
#include "common.h"
#include "icorerender.h"

struct Dx12CoreVertexBuffer : public ICoreVertexBuffer
{
	void Init(const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData = nullptr, const IndexBufferDesc* idxDesc = nullptr);

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