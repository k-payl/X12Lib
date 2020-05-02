#include "pch.h"
#include "icorerender.h"

IdGenerator<uint16_t> x12::ICoreShader::idGen;
IdGenerator<uint16_t> x12::ICoreVertexBuffer::idGen;
IdGenerator<uint16_t> x12::ICoreTexture::idGen;
IdGenerator<uint16_t> x12::ICoreBuffer::idGen;
IdGenerator<uint16_t> x12::IResourceSet::idGen;

x12::ICoreShader::ICoreShader() : IResourceUnknown(idGen.getId())
{
}

x12::ICoreVertexBuffer::ICoreVertexBuffer() : IResourceUnknown(idGen.getId())
{
}

x12::ICoreTexture::ICoreTexture() : IResourceUnknown(idGen.getId())
{
}

x12::ICoreBuffer::ICoreBuffer() : IResourceUnknown(idGen.getId())
{
}

x12::IResourceSet::IResourceSet() : IResourceUnknown(idGen.getId())
{
}

psomap_checksum_t x12::CalculateChecksum(const x12::GraphicPipelineState& pso)
{
	// 0: 15 (16)  vb ID
	// 16:31 (16)  shader ID
	// 32:34 (3)   PRIMITIVE_TOPOLOGY
	// 35:38 (4)   src blend
	// 39:42 (4)   dst blend

	static_assert(11 == static_cast<int>(BLEND_FACTOR::NUM));

	assert(pso.shader.get() != nullptr);
	assert(pso.vb.get() != nullptr);

	uint64_t checksum = uint64_t(pso.shader->ID() << 0);
	checksum |= uint64_t(pso.vb->ID() << 16);
	checksum |= uint64_t(pso.primitiveTopology) << 32;
	checksum |= uint64_t(pso.src) << 35;
	checksum |= uint64_t(pso.dst) << 39;

	return checksum;
}
psomap_checksum_t x12::CalculateChecksum(const x12::ComputePipelineState& pso)
{
	assert(pso.shader != nullptr);
	uint64_t checksum = uint64_t(pso.shader->ID() << 0);

	return checksum;
}

