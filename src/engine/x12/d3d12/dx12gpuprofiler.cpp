
#include "dx12gpuprofiler.h"
#include "intrusiveptr.h"
#include "dx12render.h"
#include "dx12vertexbuffer.h"
#include "dx12shader.h"
#include "dx12texture.h"
#include "dx12buffer.h"
#include "dx12commandlist.h"
#include "filesystem.h"
#include "core.h"
#include "DDSTextureLoader12.h"

using namespace x12;
using namespace math;
using namespace engine;

struct Dx12GraphRenderer : public GraphRenderer
{
	intrusive_ptr<ICoreVertexBuffer> graphVertexBuffer;
	intrusive_ptr<ICoreBuffer> offsetUniformBuffer;
	intrusive_ptr<IResourceSet> graphResourceSet;
	size_t transformGraphIndex;
	size_t colorGraphIndex;
	intrusive_ptr<ICoreShader> graphShader;
	intrusive_ptr<ICoreBuffer> viewportUniformBuffer;

	Dx12GraphRenderer(intrusive_ptr<ICoreShader> graphShader_, intrusive_ptr<ICoreBuffer> viewportUniformBuffer_)
		: graphShader(graphShader_),
		viewportUniformBuffer(viewportUniformBuffer_)
	{
		GetCoreRender()->CreateConstantBuffer(offsetUniformBuffer.getAdressOf(), L"Dx12GraphRenderer offsetUniformBuffer", 16);
	}

	void Render(void* c, vec4 color, float value, unsigned w, unsigned h) override
	{
		Dx12GraphicCommandList* cmdList = (Dx12GraphicCommandList*)c;

		if (!graphResourceSet.get())
		{
			GetCoreRender()->CreateResourceSet(graphResourceSet.getAdressOf(), graphShader.get());

			graphResourceSet->BindConstantBuffer("ViewportCB", viewportUniformBuffer.get());
			cmdList->CompileSet(graphResourceSet.get());

			transformGraphIndex = graphResourceSet->FindInlineBufferIndex("GraphTransform");
			colorGraphIndex = graphResourceSet->FindInlineBufferIndex("GraphColor");
		}

		cmdList->BindResourceSet(graphResourceSet.get());
		cmdList->SetVertexBuffer(graphVertexBuffer.get());
		cmdList->UpdateInlineConstantBuffer((uint32_t)colorGraphIndex, &color, 16);

		vec4 cpuv[2];
		cpuv[0] = lastGraphValue;
		cpuv[1] = vec4((float)graphRingBufferOffset, h - value, 0, 0);

		graphVertexBuffer->SetData(&cpuv, sizeof(cpuv), graphRingBufferOffset * sizeof(cpuv), nullptr, 0, 0);

		auto Draw = [this, cmdList](float offset, uint32_t len, uint32_t o)
		{
			cmdList->UpdateInlineConstantBuffer((uint32_t)transformGraphIndex, &offset, 4);
			cmdList->Draw(graphVertexBuffer.get(), len, o);
		};

		if (graphRingBufferOffset)
			Draw(float(graphRingBufferOffset), graphRingBufferOffset * 2, 0);

		Draw(float(graphRingBufferOffset + w), (w - graphRingBufferOffset) * 2, graphRingBufferOffset * 2);
	}

	void RecreateVB(unsigned w) override
	{
		lastGraphValue = {};
		lastGraphValue.y = (float)w;
		graphRingBufferOffset = 0;

		// Vertex buffer
		VertexAttributeDesc attr[1];
		attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
		attr[0].offset = 0;
		attr[0].semanticName = "POSITION";

		VeretxBufferDesc desc;
		desc.attributesCount = _countof(attr);
		desc.attributes = attr;
		desc.vertexCount = w * 2;

		for (uint32_t i = 0; i < desc.vertexCount; i++)
			graphData[i] = vec4(float(i / 2), 100, 0, 0);

		if (graphVertexBuffer)
			graphVertexBuffer = nullptr;

		WCHAR name[256];
		wsprintf(name, L"profiler graph vb screen width=%u", w);

		GetCoreRender()->CreateVertexBuffer(graphVertexBuffer.getAdressOf(), name, graphData, &desc, nullptr, nullptr, MEMORY_TYPE::CPU);
	}
};

struct Dx12RenderProfilerRecord : public RenderProfilerRecord
{
	ICoreVertexBuffer* vertexBuffer{};

	Dx12RenderProfilerRecord(const char* format, bool isFloat, bool renderGraph_)
		: RenderProfilerRecord(format, isFloat, renderGraph_) {}

	~Dx12RenderProfilerRecord()
	{
		if (vertexBuffer)
			vertexBuffer->Release();
	}

	void CreateBuffer() override
	{
		if (vertexBuffer)
			return;

		VertexAttributeDesc attr[1];
		attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
		attr[0].offset = 0;
		attr[0].semanticName = "POSITION";

		VeretxBufferDesc desc;
		desc.attributesCount = _countof(attr);
		desc.attributes = attr;
		desc.vertexCount = 6 * ((uint32_t)text.size() + 10); // for numbers

		WCHAR name[256];
		wsprintf(name, L"profiler text buffer '%s'", format.c_str());

		GetCoreRender()->CreateVertexBuffer(&vertexBuffer, name, nullptr, &desc, nullptr, nullptr, MEMORY_TYPE::CPU);
	}

	void UpdateBuffer(void* data) override
	{
		vertexBuffer->SetData(data, size * sizeof(vec4), 0, nullptr, 0, 0);
	}
};
void Dx12GpuProfiler::Free()
{
	GpuProfiler::free();

	fontShader.Reset();
	graphShader.Reset();
	fontDataStructuredBuffer.Reset();
	fontTexture.Reset();
	COLORCBUniformBuffer.Reset();
	viewportUniformBuffer.Reset();
	fontResourceSet.Reset();
}
void Dx12GpuProfiler::Init()
{
	FileSystem* fs = engine::GetFS();

	// Font shader
	{
		auto text = fs->LoadFile(SHADER_DIR "gpuprofiler_font.hlsl");
		const ConstantBuffersDesc buffersdesc[] =
		{
			{"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW},
		};
		GetCoreRender()->CreateShader(fontShader.getAdressOf(), L"gpuprofiler_font.hlsl", text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));
	}

	{
		const ConstantBuffersDesc buffersdesc[] =
		{
			{"GraphTransform", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW},
			{"GraphColor", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW}
		};

		auto text = fs->LoadFile(SHADER_DIR"gpuprofiler_graph.hlsl");
		GetCoreRender()->CreateShader(graphShader.getAdressOf(), L"gpuprofiler_graph.hlsl", text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));
	}

	GetCoreRender()->CreateConstantBuffer(viewportUniformBuffer.getAdressOf(), L"Dx12GpuProfiler viewport", 16);
	GetCoreRender()->CreateConstantBuffer(COLORCBUniformBuffer.getAdressOf(), L"Dx12GpuProfiler COLORCBUniformBuffer", 16);
	COLORCBUniformBuffer->SetData(&color, 16);

	// Texture
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ID3D12Resource* d3dtexture;
	DirectX::LoadDDSTextureFromFile(d3d12::CR_GetD3DDevice(), fontTexturePath, &d3dtexture, ddsData, subresources);

	GetCoreRender()->CreateTextureFrom(fontTexture.getAdressOf(), fontTexturePath, subresources, d3dtexture);

	// Font
	loadFont();
	GetCoreRender()->CreateStructuredBuffer(fontDataStructuredBuffer.getAdressOf(), L"font's data", sizeof(FontChar), fontData.size(), &fontData[0],
											BUFFER_FLAGS::SHADER_RESOURCE);

	// Vertex buffer
	VertexAttributeDesc attr[1];
	attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
	attr[0].offset = 0;
	attr[0].semanticName = "POSITION";

	VeretxBufferDesc desc;
	desc.attributesCount = _countof(attr);
	desc.attributes = attr;
	desc.vertexCount = 1;

	GetCoreRender()->CreateVertexBuffer(dymmyVertexBuffer.getAdressOf(), L"Dx12GpuProfiler dymmyVertexBuffer",
										nullptr, &desc, nullptr, nullptr, MEMORY_TYPE::CPU);

}
void Dx12GpuProfiler::Begin(void* c)
{
	context = (ICoreGraphicCommandList*)c;
	//context->CommandsBegin();
}

void engine::Dx12GpuProfiler::End()
{
	//context->CommandsEnd();
	//GetCoreRender()->ExecuteCommandList(context);
}

void Dx12GpuProfiler::BeginGraph()
{
	GraphicPipelineState pso{};
	pso.src = BLEND_FACTOR::SRC_ALPHA;
	pso.dst = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::LINE;
	pso.shader = graphShader.get();
	pso.vb = static_cast<ICoreVertexBuffer*>(dymmyVertexBuffer.get());
	context->SetGraphicPipelineState(pso);
}

void Dx12GpuProfiler::UpdateViewportConstantBuffer()
{
	viewportUniformBuffer->SetData(&viewport, 16);
}

void Dx12GpuProfiler::DrawRecords(size_t maxRecords)
{
	if (!fontResourceSet.get())
	{
		GetCoreRender()->CreateResourceSet(fontResourceSet.getAdressOf(), fontShader.get());

		fontResourceSet->BindConstantBuffer("ViewportCB", viewportUniformBuffer.get());
		fontResourceSet->BindStructuredBufferSRV("character_buffer", fontDataStructuredBuffer.get());
		fontResourceSet->BindTextueSRV("texture_font", fontTexture.get());
		fontResourceSet->BindConstantBuffer("COLORCB", COLORCBUniformBuffer.get());
		context->CompileSet(fontResourceSet.get());

		transformFontIndex = fontResourceSet->FindInlineBufferIndex("TransformCB");
	}

	GraphicPipelineState pso{};
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
	pso.shader = fontShader.get();
	pso.vb = static_cast<Dx12RenderProfilerRecord*>(records[0])->vertexBuffer;
	pso.src = BLEND_FACTOR::SRC_ALPHA;
	pso.dst = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
	context->SetGraphicPipelineState(pso);

	context->BindResourceSet(fontResourceSet.get());

	float t = verticalOffset;
	for (int i = 0; i < maxRecords; ++i)
	{
		auto* r = static_cast<Dx12RenderProfilerRecord*>(records[i]);

		context->SetVertexBuffer(r->vertexBuffer);
		t += fntLineHeight;
		context->UpdateInlineConstantBuffer((uint32_t)transformFontIndex, &t, 4);

		context->Draw(r->vertexBuffer, r->size);
	}
}

void Dx12GpuProfiler::AddRecord(const char* format, bool isFloat, bool renderGraph_)
{
	size_t index = records.size();

	records.push_back(nullptr);
	records.back() = new Dx12RenderProfilerRecord(format, isFloat, renderGraph_);

	renderRecords.resize(records.size());
	renderRecords[index] = renderGraph_;

	graphs.resize(records.size());
	if (renderGraph_)
		graphs[index] = new Dx12GraphRenderer(graphShader, viewportUniformBuffer);
}


