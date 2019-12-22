#pragma once
#include "common.h"

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

struct PipelineState
{
	ICoreShader* shader;
	ICoreVertexBuffer* vb;
};

enum class SHADER_TYPE
{
	SHADER_VERTEX,
	SHADER_FRAGMENT,
	NUM
};