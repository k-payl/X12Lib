#include "pch.h"
#include "gpuprofiler.h"
#include "dx12render.h"
#include "dx12vertexbuffer.h"
#include "dx12shader.h"
#include "dx12texture.h"
#include "dx12structuredbuffer.h"
#include "dx12uniformbuffer.h"
#include "dx12context.h"
#include "3rdparty/DirectXTex/DDSTextureLoader12.h"
#include <fstream>
#include <string>
#include <cstdio>
#include <cinttypes>

#define GPU_PROFILER_DIR "..//"
#define GPU_PROFILER_LDIR L"..//"
const auto fontDataPath = GPU_PROFILER_DIR"font//1.fnt";
const auto fontTexturePath = GPU_PROFILER_LDIR"font//1_0.dds";
static vec4 graphData[4096];

struct Graph
{
	uint32_t graphRingBufferOffset{ 0 };
	vec4 lastGraphValue;
	intrusive_ptr<Dx12CoreVertexBuffer> graphVertexBuffer;
	Dx12UniformBuffer* graphOffsetUniformBuffer;
	Dx12UniformBuffer* graphColorUniformBuffer;
	vec4 lastColor;
};

constexpr int GrpahsCount = 2;
std::vector<Graph> graphs;


struct GpuProfiler::Impl
{
	intrusive_ptr<Dx12CoreShader> fontShader;
	intrusive_ptr<Dx12CoreShader> graphShader;

	intrusive_ptr<Dx12CoreStructuredBuffer> fontDataStructuredBuffer;

	intrusive_ptr<Dx12CoreTexture> fontTexture;

	Dx12UniformBuffer *viewportUniformBuffer;
	Dx12UniformBuffer *transformUniformBuffer;
};

struct RenderProfilerRecord
{
	std::string text;
	size_t textChecksum;
	uint32_t size;
	Dx12CoreVertexBuffer *vertexBuffer;
};

constexpr int recordsNum = 7;
static RenderProfilerRecord records[recordsNum];

struct FontChar
{
	float x, y;
	float w, h;
	float xoffset, yoffset;
	float xadvance;
	int _align;
};

struct TransformConstantBuffer
{
	float y;
	float _align[3];
};

static std::vector<FontChar> fontData;

void GpuProfiler::Render(const RenderContext& ctx)
{
	Dx12GraphicCommandContext* context = GetCoreRender()->GetGraphicCommmandContext();

	unsigned w, h;
	GetCoreRender()->GetSurfaceSize(w, h);

	if (lastWidth != w || lastHeight != h)
	{
		viewport[0] = float(w);
		viewport[1] = float(h);
		viewport[2] = 1.0f / w;
		viewport[3] = 1.0f / h;

		context->UpdateUniformBuffer(impl->viewportUniformBuffer, viewport, 0, 16);
	}

	records[0].text = "====Core Render Profiler====";

	char float_buff[100];

	sprintf_s(float_buff, "CPU: %0.2f ms.", ctx.cpu_);
	records[1].text = float_buff;

	sprintf_s(float_buff, "GPU: %0.2f ms.", ctx.gpu_);
	records[2].text = float_buff;

	sprintf_s(float_buff, "Uniform buffer updates: %" PRId64, GetCoreRender()->UniformBufferUpdates());
	records[3].text = float_buff;

	sprintf_s(float_buff, "State changes: %" PRId64, GetCoreRender()->StateChanges());
	records[4].text = float_buff;

	sprintf_s(float_buff, "Triangles: %" PRId64, GetCoreRender()->Triangles());
	records[5].text = float_buff;

	sprintf_s(float_buff, "Draw calls: %" PRId64, GetCoreRender()->DrawCalls());
	records[6].text = float_buff;

	for (int i = 0; i < recordsNum; ++i)
	{
		RenderProfilerRecord& r = records[i];
		if (!r.vertexBuffer)
		{
			VertexAttributeDesc attr[1];
			attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
			attr[0].offset = 0;
			attr[0].semanticName = "POSITION";

			VeretxBufferDesc desc;
			desc.attributesCount = _countof(attr);
			desc.attributes = attr;
			desc.vertexCount = 6 * (uint32_t)r.text.size();

			GetCoreRender()->CreateVertexBuffer(&r.vertexBuffer, nullptr, &desc, nullptr, nullptr, BUFFER_USAGE::CPU_WRITE);
		}

		float offsetInPixels = float(fontMarginInPixels);

		static std::hash<std::string> hashFn;
		size_t newChecksum = hashFn(r.text);

		if (r.textChecksum != newChecksum)
		{
			r.textChecksum = newChecksum;

			r.size = (uint32_t)r.text.size() * 6;
			int i = 0;
			for (uint32_t j = 0; j < r.size; j+=6, ++i)
			{
				int ascii = r.text[i];
				FontChar& char_ = fontData[ascii];

				graphData[j + 0] = vec4(0.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 1] = vec4(1.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 2] = vec4(0.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 3] = vec4(0.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 4] = vec4(1.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 5] = vec4(1.0f, 1.0f, (float)ascii, offsetInPixels);
				offsetInPixels += char_.xadvance + rectPadding;
			}
			r.vertexBuffer->SetData(graphData, r.size * sizeof(vec4), 0, nullptr, 0, 0);
		}
	}

	PipelineState pso{};
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
	pso.shader = impl->fontShader.get();
	pso.vb = records[0].vertexBuffer;
	pso.src = BLEND_FACTOR::SRC_ALPHA;
	pso.dst = BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
	context->SetPipelineState(pso);
	
	context->BindUniformBuffer(0, impl->viewportUniformBuffer, SHADER_TYPE::SHADER_VERTEX);
	context->BindUniformBuffer(1, impl->transformUniformBuffer, SHADER_TYPE::SHADER_VERTEX);
	context->BindTexture(0, impl->fontTexture.get(), SHADER_TYPE::SHADER_FRAGMENT);
	context->BindStructuredBuffer(0, impl->fontDataStructuredBuffer.get(), SHADER_TYPE::SHADER_VERTEX);

	float t = 0;
	for (int i = 0; i < recordsNum; ++i)
	{
		context->SetVertexBuffer(records[i].vertexBuffer);
		t += fntLineHeight;
		context->UpdateUniformBuffer(impl->transformUniformBuffer, &t, 0, 4);
		context->Draw(records[i].vertexBuffer, records[i].size);
	}
	

	// Graphs.

	if (w != lastWidth)
		for(int i = 0; i < GrpahsCount; ++i)
			recreateGraphBuffer(graphs[i], w);

	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::LINE;
	pso.shader = impl->graphShader.get();
	pso.vb = graphs[0].graphVertexBuffer.get();
	context->SetPipelineState(pso);

	context->BindUniformBuffer(0, impl->viewportUniformBuffer, SHADER_TYPE::SHADER_VERTEX);

	auto renderGraph = [context, this, w, h](Graph& g, float value, const vec4& color)
	{
		context->SetVertexBuffer(g.graphVertexBuffer.get());
		
		context->BindUniformBuffer(1, g.graphOffsetUniformBuffer, SHADER_TYPE::SHADER_VERTEX);

		if (!g.lastColor.Aproximately(color))
		{
			context->UpdateUniformBuffer(g.graphColorUniformBuffer, &color, 0, 16);
			g.lastColor = color;
		}
		context->BindUniformBuffer(0, g.graphColorUniformBuffer, SHADER_TYPE::SHADER_FRAGMENT);

		vec4 cpuv[2];
		cpuv[0] = g.lastGraphValue;
		cpuv[1] = vec4((float)g.graphRingBufferOffset, h - value, 0, 0);

		g.graphVertexBuffer->SetData(&cpuv, sizeof(cpuv), g.graphRingBufferOffset * sizeof(cpuv), nullptr, 0, 0);

		float transform = (float)g.graphRingBufferOffset;
		context->UpdateUniformBuffer(g.graphOffsetUniformBuffer, &transform, 0, 4);
		if (g.graphRingBufferOffset > 0)
			context->Draw(g.graphVertexBuffer.get(), g.graphRingBufferOffset * 2);

		transform = float(g.graphRingBufferOffset + w);
		context->UpdateUniformBuffer(g.graphOffsetUniformBuffer, &transform, 0, 4);
		context->Draw(g.graphVertexBuffer.get(), (w - g.graphRingBufferOffset) * 2, g.graphRingBufferOffset * 2);

		g.lastGraphValue = cpuv[1];
		g.graphRingBufferOffset++;
		if (g.graphRingBufferOffset >= w)
		{
			g.lastGraphValue.x = 0;
			g.graphRingBufferOffset = 0;
		}
	};

	renderGraph(graphs[0], ctx.cpu_ * 30, vec4(0, 0.5f, 0.5f, 1));
	renderGraph(graphs[1], ctx.gpu_ * 30, vec4(1, 0, 0.5f, 1));

	lastWidth = w;
	lastHeight = h;
}

void GpuProfiler::recreateGraphBuffer(Graph &g, int w)
{
	if (w == lastWidth)
		return;

	g.lastGraphValue = {};
	g.lastGraphValue.y = (float)w;
	g.graphRingBufferOffset = 0;

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
		graphData[i] = vec4(float(i/2), 100, 0, 0);

	if (g.graphVertexBuffer)
		g.graphVertexBuffer = nullptr;

	GetCoreRender()->CreateVertexBuffer(g.graphVertexBuffer.getAdressOf(), graphData , &desc, nullptr, nullptr, BUFFER_USAGE::CPU_WRITE);
}

GpuProfiler::GpuProfiler()
{
	impl = new Impl();
}

GpuProfiler::~GpuProfiler()
{
	delete impl;
}

void GpuProfiler::loadFont()
{
	std::fstream file(fontDataPath, std::ofstream::in);

	char buffer[4096];
	file.getline(buffer, 4096);
	file.getline(buffer, 4096);
	sscanf_s(buffer, "common lineHeight=%f", &fntLineHeight);
	file.getline(buffer, 4096);
	file.getline(buffer, 4096);

	int charsNum;
	sscanf_s(buffer, "chars count=%d", &charsNum);

	fontData.resize(256);

	for (int i = 0; i < charsNum; ++i)
	{
		file.getline(buffer, 4096);

		int id;
		sscanf_s(buffer, "char id=%d ", &id);

		if (id > 255)
			continue;

		FontChar& item = fontData[id];
		sscanf_s(buffer, "char id=%d x=%f y=%f width=%f height=%f xoffset=%f yoffset=%f xadvance=%f", &id, &item.x, &item.y, &item.w, &item.h, &item.xoffset, &item.yoffset, &item.xadvance);
	}

	file.close();
}

void GpuProfiler::Init()
{
	
	// Font shader
	{
		auto text = loadShader(GPU_PROFILER_DIR"gpuprofiler_font.shader");
		const ConstantBuffersDesc buffersdesc[1] =
		{
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		GetCoreRender()->CreateShader(impl->fontShader.getAdressOf(), text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));
	}

	// Graph
	graphs.resize(GrpahsCount);
	{
		const ConstantBuffersDesc buffersdesc[1] =
		{
			"GraphTransform", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		auto text = loadShader(GPU_PROFILER_DIR"gpuprofiler_graph.shader");
		GetCoreRender()->CreateShader(impl->graphShader.getAdressOf(), text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));

		Dx12GraphicCommandContext* context = GetCoreRender()->GetGraphicCommmandContext();
		unsigned w, h;
		GetCoreRender()->GetSurfaceSize(w, h);
	}

	GetCoreRender()->CreateUniformBuffer(&impl->viewportUniformBuffer, 16);
	GetCoreRender()->CreateUniformBuffer(&impl->transformUniformBuffer, sizeof(TransformConstantBuffer));

	for(int i = 0; i < GrpahsCount; ++i)
	{
		GetCoreRender()->CreateUniformBuffer(&graphs[i].graphOffsetUniformBuffer, 16);
		GetCoreRender()->CreateUniformBuffer(&graphs[i].graphColorUniformBuffer, 16);
	}

	//
	// Texture
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ID3D12Resource* d3dtexture;
	DirectX::LoadDDSTextureFromFile(CR_GetD3DDevice(), fontTexturePath, &d3dtexture, ddsData, subresources);

	GetCoreRender()->CreateTexture(impl->fontTexture.getAdressOf(), std::move(ddsData), subresources, d3dtexture);

	//
	// Font data
	loadFont();

	GetCoreRender()->CreateStructuredBuffer(impl->fontDataStructuredBuffer.getAdressOf(), sizeof(FontChar), fontData.size(), &fontData[0]);
}

void GpuProfiler::Free()
{
	for (int i = 0; i < recordsNum; ++i)
	{
		RenderProfilerRecord& r = records[i];
		if (r.vertexBuffer)
			r.vertexBuffer->Release();
	}
	graphs.clear();
}

