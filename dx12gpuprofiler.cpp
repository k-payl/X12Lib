#include "pch.h"
#include "dx12gpuprofiler.h"
#include "intrusiveptr.h"
#include "dx12render.h"
#include "dx12vertexbuffer.h"
#include "dx12shader.h"
#include "dx12texture.h"
#include "dx12structuredbuffer.h"
#include "dx12uniformbuffer.h"
#include "dx12context.h"
#include "filesystem.h"
#include "core.h"
#include "3rdparty/DirectXTex/DDSTextureLoader12.h"

struct Dx12Graph : public Graph
{
	intrusive_ptr<Dx12CoreVertexBuffer> graphVertexBuffer;
	Dx12UniformBuffer* offsetUniformBuffer;
	Dx12UniformBuffer* colorUniformBuffer;

	Dx12Graph()
	{
		GetCoreRender()->CreateUniformBuffer(&offsetUniformBuffer, 16);
		GetCoreRender()->CreateUniformBuffer(&colorUniformBuffer, 16);
	}

	void Render(void* c, vec4 color, float value, unsigned w, unsigned h) override
	{
		Dx12GraphicCommandContext* context = (Dx12GraphicCommandContext*)c;

		context->SetVertexBuffer(graphVertexBuffer.get());
		context->BindUniformBuffer(0, colorUniformBuffer, SHADER_TYPE::SHADER_FRAGMENT);
		context->BindUniformBuffer(1, offsetUniformBuffer, SHADER_TYPE::SHADER_VERTEX);

		if (!lastColor.Aproximately(color))
		{
			context->UpdateUniformBuffer(colorUniformBuffer, &color, 0, 16);
			lastColor = color;
		}

		vec4 cpuv[2];
		cpuv[0] = lastGraphValue;
		cpuv[1] = vec4((float)graphRingBufferOffset, h - value, 0, 0);

		graphVertexBuffer->SetData(&cpuv, sizeof(cpuv), graphRingBufferOffset * sizeof(cpuv), nullptr, 0, 0);

		auto Draw = [this, context](float offset, uint32_t len, uint32_t o)
		{
			context->UpdateUniformBuffer(offsetUniformBuffer, &offset, 0, 4);
			context->Draw(graphVertexBuffer.get(), len, o);
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

		GetCoreRender()->CreateVertexBuffer(graphVertexBuffer.getAdressOf(), graphData, &desc, nullptr, nullptr, BUFFER_USAGE::CPU_WRITE);
	}
};

struct Dx12RenderProfilerRecord : public RenderProfilerRecord
{
	Dx12CoreVertexBuffer* vertexBuffer{};

	~Dx12RenderProfilerRecord()
	{
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
		desc.vertexCount = 6 * (uint32_t)text.size();

		GetCoreRender()->CreateVertexBuffer(&vertexBuffer, nullptr, &desc, nullptr, nullptr, BUFFER_USAGE::CPU_WRITE);
	}

	void UpdateBuffer(void* data) override
	{
		vertexBuffer->SetData(data, size * sizeof(vec4), 0, nullptr, 0, 0);
	}
};
void Dx12GpuProfiler::Free()
{
	free();
	fontShader->Release();
	graphShader->Release();
	fontDataStructuredBuffer->Release();
	fontTexture->Release();
}
void Dx12GpuProfiler::Init()
{
	FileSystem* fs = CORE->GetFS();

	// Font shader
	{
		auto text = fs->LoadFile("gpuprofiler_font.shader");
		const ConstantBuffersDesc buffersdesc[1] =
		{
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		GetCoreRender()->CreateShader(&fontShader, text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));
	}

	// Graph
	graphs.resize(GraphsCount);
	for (int i = 0; i < GraphsCount; ++i)
		graphs[i] = new Dx12Graph;

	for (int i = 0; i < recordsNum; ++i)
		records[i] = new Dx12RenderProfilerRecord;

	{
		const ConstantBuffersDesc buffersdesc[1] =
		{
			"GraphTransform", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		auto text = fs->LoadFile("gpuprofiler_graph.shader");
		GetCoreRender()->CreateShader(&graphShader, text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));
	}

	GetCoreRender()->CreateUniformBuffer(&viewportUniformBuffer, 16);
	GetCoreRender()->CreateUniformBuffer(&transformUniformBuffer, sizeof(TransformConstantBuffer));

	// Texture
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ID3D12Resource* d3dtexture;
	DirectX::LoadDDSTextureFromFile(CR_GetD3DDevice(), fontTexturePath, &d3dtexture, ddsData, subresources);

	GetCoreRender()->CreateTexture(&fontTexture, std::move(ddsData), subresources, d3dtexture);

	// Font
	loadFont();
	GetCoreRender()->CreateStructuredBuffer(&fontDataStructuredBuffer, sizeof(FontChar), fontData.size(), &fontData[0]);
}
void Dx12GpuProfiler::Begin()
{
	context = GetCoreRender()->GetGraphicCommmandContext();

	GetCoreRender()->GetSurfaceSize(w, h);
}

void Dx12GpuProfiler::BeginGraph()
{
	PipelineState pso{};
	pso.src = BLEND_FACTOR::SRC_ALPHA;
	pso.dst = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::LINE;
	pso.shader = graphShader;
	pso.vb = static_cast<Dx12Graph*>(graphs[0])->graphVertexBuffer.get();
	context->SetPipelineState(pso);

	context->BindUniformBuffer(0, viewportUniformBuffer, SHADER_TYPE::SHADER_VERTEX);
}

void Dx12GpuProfiler::UpdateViewportConstantBuffer()
{
	context->UpdateUniformBuffer(viewportUniformBuffer, viewport, 0, 16);
}

void Dx12GpuProfiler::DrawFont(int maxRecords)
{
	PipelineState pso{};
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
	pso.shader = fontShader;
	pso.vb = static_cast<Dx12RenderProfilerRecord*>(records[0])->vertexBuffer;
	pso.src = BLEND_FACTOR::SRC_ALPHA;
	pso.dst = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
	context->SetPipelineState(pso);

	context->BindUniformBuffer(0, viewportUniformBuffer, SHADER_TYPE::SHADER_VERTEX);
	context->BindUniformBuffer(1, transformUniformBuffer, SHADER_TYPE::SHADER_VERTEX);

	context->BindTexture(1, fontTexture, SHADER_TYPE::SHADER_FRAGMENT);
	context->BindStructuredBuffer(0, fontDataStructuredBuffer, SHADER_TYPE::SHADER_VERTEX);

	float t = 0;
	for (int i = 0; i < maxRecords; ++i)
	{
		auto* r = static_cast<Dx12RenderProfilerRecord*>(records[i]);

		context->SetVertexBuffer(r->vertexBuffer);
		t += fntLineHeight;
		context->UpdateUniformBuffer(transformUniformBuffer, &t, 0, 4);
		context->Draw(r->vertexBuffer, r->size);
	}
}


