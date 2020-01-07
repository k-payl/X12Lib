#pragma once
#include "common.h"
#include "icorerender.h"
#include "dx12descriptorheap.h"

struct Dx12CoreStructuredBuffer : public ICoreStructuredBuffer
{
	inline void AddRef() override { refs++; }
	inline int GetRefs() override { return refs; }
	void Release() override;

	uint16_t ID() { return id; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return descriptorAllocation.descriptor; }

	void Init(size_t structureSize, size_t num, const void *data);

private:
	~Dx12CoreStructuredBuffer() override = default;

	ComPtr<ID3D12Resource> resource;
	int refs{};

	static IdGenerator<uint16_t> idGen;
	uint16_t id;

	DescriptorHeap::Alloc descriptorAllocation;
};