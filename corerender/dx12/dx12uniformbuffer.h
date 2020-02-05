#pragma once
#include "common.h"
#include "icorerender.h"
#include "dx12uploadheap.h"

struct Dx12UniformBuffer
{
	Dx12UniformBuffer(UINT size_, size_t idx_);
	~Dx12UniformBuffer();

	Dx12UniformBuffer(const Dx12UniformBuffer& r) = delete;
	Dx12UniformBuffer& operator=(const Dx12UniformBuffer& r) = delete;
	
	Dx12UniformBuffer& operator=(Dx12UniformBuffer&& r) = delete;
	Dx12UniformBuffer(Dx12UniformBuffer&& r);

	size_t idx;
	const UINT alignedSize; // aligned to 256 byte
	const UINT dataSize; // real size
	uint8_t *cache;
	bool dirty{ true };
};



