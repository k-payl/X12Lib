#include "pch.h"
#include "dx12uniformbuffer.h"
#include "dx12render.h"


Dx12UniformBuffer::Dx12UniformBuffer(UINT size_) :
	alignedSize(alignConstnatBufferSize(size_)),
	dataSize(size_)
{
	cache = new uint8_t[dataSize];
	memset(cache, 0, dataSize);
}

Dx12UniformBuffer::~Dx12UniformBuffer()
{
	delete[] cache;
}

Dx12UniformBuffer::Dx12UniformBuffer(Dx12UniformBuffer&& r) :
	alignedSize(r.alignedSize),
	dataSize(r.dataSize)
{
};
