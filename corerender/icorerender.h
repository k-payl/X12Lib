#pragma once
#include "common.h"

inline constexpr size_t MaxBindedResourcesPerFrame = 100'000;
inline constexpr unsigned MaxResourcesPerShader = 8;

enum class VERTEX_BUFFER_FORMAT
{
	FLOAT4,
};

enum class INDEX_BUFFER_FORMAT
{
	UNSIGNED_16,
	UNSIGNED_32,
};

struct VertexAttributeDesc
{
	uint32_t offset;
	VERTEX_BUFFER_FORMAT format;
	const char* semanticName;
};

struct VeretxBufferDesc
{
	uint32_t vertexCount;
	int attributesCount;
	VertexAttributeDesc* attributes;
};

struct IndexBufferDesc
{
	uint32_t vertexCount;
	INDEX_BUFFER_FORMAT format;
};

enum class CONSTANT_BUFFER_UPDATE_FRIQUENCY
{
	PER_FRAME = 0,
	PER_DRAW
};

struct ConstantBuffersDesc
{
	const char* name; 
	CONSTANT_BUFFER_UPDATE_FRIQUENCY mode;
};

struct IResourceUnknown
{
	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual int GetRefs() = 0;
	virtual ~IResourceUnknown() = default;
};

struct ICoreShader : public IResourceUnknown
{
};

struct ICoreVertexBuffer : public IResourceUnknown
{
};

struct ICoreTexture : public IResourceUnknown
{
};

struct ICoreStructuredBuffer : public IResourceUnknown
{
};

enum class PRIMITIVE_TOPOLOGY
{
	UNDEFINED = 0,
	POINT = 1,
	LINE = 2,
	TRIANGLE = 3,
	PATCH = 4
};

struct PipelineState
{
	ICoreShader* shader;
	ICoreVertexBuffer* vb;
	PRIMITIVE_TOPOLOGY primitiveTopology;
};

enum BUFFER_USAGE
{
	CPU_WRITE,
	GPU_READ
};

enum class SHADER_TYPE
{
	SHADER_VERTEX,
	SHADER_FRAGMENT,
	NUM
};

enum RESOURCE_BIND_FLAGS
{
	NONE = 0,
	UNIFORM_BUFFER = 1 << 1,
	TEXTURE_SRV = 1 << 2,
	STRUCTURED_BUFFER_SRV = 1 << 3,
};

static UINT formatInBytes(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return 16;
		default: assert(0);
	}
	return 0;
}
static UINT formatInBytes(INDEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case INDEX_BUFFER_FORMAT::UNSIGNED_16: return 2;
		case INDEX_BUFFER_FORMAT::UNSIGNED_32: return 4;
		default: assert(0);
	}
	return 0;
}


