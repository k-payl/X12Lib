#pragma once
#include "intrusiveptr.h"
#include "icorerender.h"
#include "gpuprofiler.h"

class Dx12GpuProfiler : public GpuProfiler
{
	x12::Dx12GraphicCommandContext* context;
	intrusive_ptr<ICoreShader> fontShader;
	intrusive_ptr<ICoreShader> graphShader;
	intrusive_ptr<ICoreBuffer> fontDataStructuredBuffer;
	intrusive_ptr<ICoreTexture> fontTexture;
	intrusive_ptr<ICoreBuffer> COLORCBUniformBuffer;
	intrusive_ptr<ICoreBuffer> viewportUniformBuffer;
	intrusive_ptr<IResourceSet> fontResourceSet;
	size_t transformFontIndex;
	intrusive_ptr<ICoreVertexBuffer> dymmyVertexBuffer;

	void Begin() override;
	void BeginGraph() override;
	void UpdateViewportConstantBuffer() override;
	void DrawRecords(size_t maxRecords) override;
	void* getContext() override { return context; }
	void AddRecord(const char* format, bool isFloat, bool renderGraph_) override;

public:
	Dx12GpuProfiler(math::vec4 color_, float verticalOffset_) : GpuProfiler(color_, verticalOffset_) {}

	void Init() override;
	void Free() override;
};

