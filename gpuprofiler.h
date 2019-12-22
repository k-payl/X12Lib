#pragma once
#include "common.h"

class gpuProfiler
{
	Dx12CoreVertexBuffer* quad;
	Dx12CoreShader* shader;
	Dx12UniformBuffer* viewportCB;
	Dx12UniformBuffer* transformCB;

public:
	gpuProfiler() = default;

	void Init();
	void Free();
	void Render();
};

