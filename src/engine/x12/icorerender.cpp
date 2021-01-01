
#include "icorerender.h"

x12::ICoreRenderer* x12::_coreRender;

#define DEFINE_RESOURCE(type) \
	IdGenerator<uint16_t> x12::type::idGen; \
	x12::type::type() : IResourceUnknown(idGen.getId()){}

DEFINE_RESOURCE(ICoreShader)
DEFINE_RESOURCE(ICoreVertexBuffer)
DEFINE_RESOURCE(ICoreTexture)
DEFINE_RESOURCE(ICoreBuffer)
DEFINE_RESOURCE(IResourceSet)
DEFINE_RESOURCE(ICoreQuery)

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

UINT64 x12::VbSizeFromDesc(const VeretxBufferDesc* vbDesc)
{
	return getBufferStride(vbDesc) * vbDesc->vertexCount;
}

UINT64 x12::getBufferStride(const VeretxBufferDesc* vbDesc)
{
	UINT vertexStride = 0;
	for (int i = 0; i < vbDesc->attributesCount; ++i)
	{
		VertexAttributeDesc& attr = vbDesc->attributes[i];
		vertexStride += formatInBytes(attr.format);
	}

	return vertexStride;
}

