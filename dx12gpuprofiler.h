#pragma once
#include "gpuprofiler.h"

class Dx12GpuProfiler : public GpuProfiler
{
	Dx12GraphicCommandContext* context;

	Dx12CoreShader* fontShader;
	Dx12CoreShader* graphShader;
	Dx12CoreBuffer* fontDataStructuredBuffer;
	Dx12CoreTexture* fontTexture;

	Dx12UniformBuffer* viewportUniformBuffer;
	Dx12UniformBuffer* transformUniformBuffer;

	void Begin() override;
	void BeginGraph() override;
	void UpdateViewportConstantBuffer() override;
	void DrawFont(int maxRecords) override;
	void* getContext() override { return context; }

public:
	void Init() override;
	void Free() override;
};

