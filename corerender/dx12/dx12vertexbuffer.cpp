#include "pch.h"
#include "dx12vertexbuffer.h"
#include "dx12render.h"

IdGenerator<uint16_t> Dx12CoreVertexBuffer::idGen;

void Dx12CoreVertexBuffer::Release()
{
	CR_ReleaseResource(refs, this);
}