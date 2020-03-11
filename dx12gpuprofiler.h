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
	Dx12UniformBuffer* colorUniformBuffer;

	void Begin() override;
	void BeginGraph() override;
	void UpdateViewportConstantBuffer() override;
	void DrawRecords(int maxRecords) override;
	void* getContext() override { return context; }
	void AddRecord(const char* format) override;

public:
	Dx12GpuProfiler(vec4 color_, float verticalOffset_) : GpuProfiler(color_, verticalOffset_) {}
	void Init() override;
	void Free() override;
};

