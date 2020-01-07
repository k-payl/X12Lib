#pragma once
#include "common.h"

class GpuProfiler
{
	Dx12CoreVertexBuffer *quad;
	Dx12CoreShader *shader;
	Dx12UniformBuffer *viewportCB;
	Dx12UniformBuffer *transformCB;
	Dx12CoreTexture *fontTexture;
	Dx12CoreStructuredBuffer *fontDataGPUBuffer;

public:

	void Init();
	void Free();
	void Render();
};

