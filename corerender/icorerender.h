#pragma once
#include "common.h"

inline constexpr size_t MaxBindedResourcesPerFrame = 1'024;
inline constexpr unsigned MaxResourcesPerShader = 8;

enum class TEXTURE_FORMAT
{
	// normalized
	R8,
	RG8,
	RGBA8,
	BGRA8,

	// float
	R16F,
	RG16F,
	RGBA16F,
	R32F,
	RG32F,
	RGBA32F,

	// integer
	R32UI,

	// compressed
	DXT1,
	DXT3,
	DXT5,

	// depth/stencil
	D24S8,

	UNKNOWN
};

enum class TEXTURE_CREATE_FLAGS : uint32_t
{
	NONE = 0x00000000,

	FILTER = 0x0000000F,
	FILTER_POINT = 1, // magn = point,	min = point,	mip = point
	FILTER_BILINEAR = 2, // magn = linear,	min = linear,	mip = point
	FILTER_TRILINEAR = 3, // magn = linear,	min = linear,	mip = lenear
	FILTER_ANISOTROPY_2X = 4,
	FILTER_ANISOTROPY_4X = 5,
	FILTER_ANISOTROPY_8X = 6,
	FILTER_ANISOTROPY_16X = 7,

	COORDS = 0x00000F00,
	COORDS_WRAP = 1 << 8,
	//COORDS_MIRROR
	//COORDS_CLAMP
	//COORDS_BORDER

	USAGE = 0x0000F000,
	USAGE_RENDER_TARGET = 1 << 12,
	USAGE_UNORDRED_ACCESS = 1 << 13,

	MSAA = 0x000F0000,
	MSAA_2x = 2 << 16,
	MSAA_4x = 3 << 16,
	MSAA_8x = 4 << 16,

	MIPMAPS = 0xF0000000,
	GENERATE_MIPMAPS = 1 << 28,
	MIPMPAPS_PRESENTED = (1 << 28) + 1,
};
enum class TEXTURE_TYPE
{
	TYPE_2D = 0x00000001,
	//TYPE_3D					= 0x00000001,
	TYPE_CUBE = 0x00000002,
	//TYPE_2D_ARRAY			= 0x00000003,
	//TYPE_CUBE_ARRAY			= 0x00000004
};

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
	mutable int refs{};

	static std::vector<IResourceUnknown*> resources;
	static void ReleaseResource(int& refs, IResourceUnknown* ptr);

public:
	IResourceUnknown();

	void AddRef() const { refs++; }
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
	virtual void SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset) = 0;
};

struct ICoreTexture : public IResourceUnknown
{
};

struct ICoreBuffer : public IResourceUnknown
{
	virtual void GetData(void* data) = 0;
	virtual void SetData(const void* data, size_t size) = 0;
};

struct IResourceSet : public IResourceUnknown
{
	virtual void BindConstantBuffer(const char* name, ICoreBuffer* buffer) = 0;
	virtual void BindStructuredBufferSRV(const char* name, ICoreBuffer* buffer) = 0;
	virtual void BindStructuredBufferUAV(const char* name, ICoreBuffer* buffer) = 0;
	virtual void BindTextueSRV(const char* name, ICoreTexture* texture) = 0;
	virtual size_t FindInlineBufferIndex(const char* name) = 0;
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
	UNORDERED_ACCESS = 1 << 2,
	CONSTNAT_BUFFER = 1 << 3,
	RAW_BUFFER = 1 << 4,
};
DEFINE_ENUM_OPERATORS(BUFFER_FLAGS)

enum class SHADER_TYPE
{
	SHADER_VERTEX,
	SHADER_FRAGMENT,
	SHADER_COMPUTE,
	NUM
};

enum RESOURCE_DEFINITION
{
	RBF_NO_RESOURCE = 0,

	// Shader constants
	RBF_UNIFORM_BUFFER = 1 << 1,

	// Shader read-only resources
	RBF_TEXTURE_SRV = 1 << 2,
	RBF_BUFFER_SRV = 1 << 3,
	RBF_SRV = RBF_TEXTURE_SRV | RBF_BUFFER_SRV,

	// Shader unordered access resources
	RBF_TEXTURE_UAV = 1 << 4,
	RBF_BUFFER_UAV = 1 << 5,
	RBF_UAV = RBF_TEXTURE_UAV | RBF_BUFFER_UAV,
};
DEFINE_ENUM_OPERATORS(RESOURCE_DEFINITION)

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


