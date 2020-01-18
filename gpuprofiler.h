#pragma once
#include "common.h"

class GpuProfiler
{
	// Common
	float viewport[4];
	unsigned lastWidth{ 0 };
	unsigned lastHeight{ 0 };

	// Font
	Dx12CoreShader *shaderFont;
	Dx12UniformBuffer *viewportCB;
	Dx12UniformBuffer *transformCB;
	Dx12CoreTexture *fontTexture;
	Dx12CoreStructuredBuffer *fontDataStructuredBuffer;

	// Graph
	uint32_t graphRingBufferOffset{0};
	vec4 lastGraphValue;
	Dx12CoreVertexBuffer* graphVertexBuffer;
	Dx12CoreShader* graphShader;
	Dx12UniformBuffer* graphOffsetUniformBuffer;

	void recreateGraphBuffer(int width);

public:

	void Init();
	void Free();
	void Render(float cpu_, float gpu_);
};

