#pragma once
#include "common.h"
#include "icorerender.h"
#include "dx12descriptorheap.h"

struct Dx12CoreBuffer final : public ICoreBuffer
{
	uint16_t ID() { return id; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return descriptorAllocation.descriptor; }

	void InitStructuredBuffer(size_t structureSize, size_t num, const void *data);

private:

	ComPtr<ID3D12Resource> resource;

	static IdGenerator<uint16_t> idGen;
	uint16_t id;

	x12::descriptorheap::Alloc descriptorAllocation;
};
