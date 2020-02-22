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
private:
	int refs{};

	static std::vector<IResourceUnknown*> resources;
	static void ReleaseResource(int& refs, IResourceUnknown* ptr);

public:
	IResourceUnknown();

	void AddRef() { refs++; }
	int GetRefs() { return refs; }
	void Release();
	virtual ~IResourceUnknown() = default;

	static void CheckResources();
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

struct ICoreBuffer : public IResourceUnknown
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

enum class BLEND_FACTOR
{
	NONE = 0,
	ZERO,
	ONE,
	SRC_COLOR,
	ONE_MINUS_SRC_COLOR,
	SRC_ALPHA,
	ONE_MINUS_SRC_ALPHA,
	DEST_ALPHA,
	ONE_MINUS_DEST_ALPHA,
	DEST_COLOR,
	ONE_MINUS_DEST_COLOR,
	NUM
};

struct GraphicPipelineState
{
	ICoreShader* shader;
	ICoreVertexBuffer* vb;
	PRIMITIVE_TOPOLOGY primitiveTopology;
	BLEND_FACTOR src;
	BLEND_FACTOR dst;
};

struct ComputePipelineState
{
	ICoreShader* shader;
};

enum class BUFFER_FLAGS
{
	NONE = 0,
	CPU_WRITE = 1 << 0,
	GPU_READ = 1 << 1,
	UNORDERED_ACCESS = 1 << 2
};
DEFINE_ENUM_OPERATORS(BUFFER_FLAGS)

enum class SHADER_TYPE
{
	SHADER_VERTEX,
	SHADER_FRAGMENT,
	SHADER_COMPUTE,
	NUM
};

enum RESOURCE_BIND_FLAGS
{
	RBF_NO_RESOURCE = 0,
	RBF_UNIFORM_BUFFER = 1 << 1,

	RBF_TEXTURE_SRV = 1 << 2,
	RBF_BUFFER_SRV = 1 << 3,
	RBF_SRV = RBF_TEXTURE_SRV | RBF_BUFFER_SRV,

	RBF_TEXTURE_UAV = 1 << 4,
	RBF_BUFFER_UAV = 1 << 5,
	RBF_UAV = RBF_TEXTURE_UAV | RBF_BUFFER_UAV,
};
DEFINE_ENUM_OPERATORS(RESOURCE_BIND_FLAGS)

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


