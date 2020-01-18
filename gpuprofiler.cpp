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

#define DIR "..//"
#define LDIR L"..//"
const int rectSize = 100;
const int rectPadding = 1;
const int fontMarginInPixels = 5;
const auto fontDataPath = DIR"font//1.fnt";
const auto fontTexturePath = LDIR"font//1_0.dds";
static vec4 graphData[4096];
float fntLineHeight = 20;

struct RenderProfilerRecord
{
	std::string text;
	size_t textChecksum;
	uint32_t size;
	Dx12CoreVertexBuffer* vertexBuffer;
};

static std::vector<RenderProfilerRecord> records;

struct FontChar
{
	float x, y;
	float w, h;
	float xoffset, yoffset;
	float xadvance;
	int _align;
};

struct TransformCB
{
	float y;
	float _align[3];
};

static std::vector<FontChar> fontData;

void GpuProfiler::Render(float cpu_, float gpu_)
{
	Dx12GraphicCommandContext* context = GetCoreRender()->GetMainCommmandContext();

	unsigned w, h;
	context->GetBufferSize(w, h);

	if (lastWidth != w || lastHeight != h)
	{
		viewport[0] = float(w);
		viewport[1] = float(h);
		viewport[2] = 1.0f / w;
		viewport[3] = 1.0f / h;

		context->UpdateUniformBuffer(viewportCB, viewport, 0, 16);
	}

	records[0].text = "=====Core Render Profiler====";
	char float_buff[12];
	sprintf_s(float_buff, "%0.2f ms.", cpu_);
	records[1].text = std::string("CPU: ") + float_buff;
	sprintf_s(float_buff, "%0.2f ms.", gpu_);
	records[2].text = std::string("GPU: ") + float_buff;

	for (int i = 0; i < records.size(); ++i)
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

			r.vertexBuffer = GetCoreRender()->CreateVertexBuffer(nullptr, &desc, nullptr, nullptr, BUFFER_USAGE::CPU_WRITE);
		}

		float offsetInPixels = fontMarginInPixels;

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

	PipelineState pso;
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
	pso.shader = shaderFont;
	pso.vb = records[0].vertexBuffer;
	context->SetPipelineState(pso);
	
	context->BindUniformBuffer(0, viewportCB, SHADER_TYPE::SHADER_VERTEX);
	context->BindUniformBuffer(1, transformCB, SHADER_TYPE::SHADER_VERTEX);
	context->BindTexture(0, fontTexture, SHADER_TYPE::SHADER_FRAGMENT);
	context->BindStructuredBuffer(0, fontDataStructuredBuffer, SHADER_TYPE::SHADER_VERTEX);

	float t = 0;
	for (int i = 0; i < records.size(); ++i)
	{
		context->SetVertexBuffer(records[i].vertexBuffer);
		t += fntLineHeight;
		context->UpdateUniformBuffer(transformCB, &t, 0, 4);
		context->Draw(records[i].vertexBuffer, records[i].size);
	}
	

	// Graph.

	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::LINE;
	pso.shader = graphShader;
	pso.vb = graphVertexBuffer;
	context->SetPipelineState(pso);

	if (w != lastWidth)
		recreateGraphBuffer(w);

	context->SetVertexBuffer(graphVertexBuffer);

	context->BindUniformBuffer(0, viewportCB, SHADER_TYPE::SHADER_VERTEX);
	context->BindUniformBuffer(1, graphOffsetUniformBuffer, SHADER_TYPE::SHADER_VERTEX);

	vec4 cpuv[2];
	cpuv[0] = lastGraphValue;
	cpuv[1] = vec4((float)graphRingBufferOffset, h - cpu_ * 30, 0, 0);

	graphVertexBuffer->SetData(&cpuv, sizeof(cpuv), graphRingBufferOffset * sizeof(cpuv), nullptr, 0, 0);

	float transform = (float)graphRingBufferOffset;
	context->UpdateUniformBuffer(graphOffsetUniformBuffer, &transform, 0, 4);
	if (graphRingBufferOffset > 0)
		context->Draw(graphVertexBuffer, graphRingBufferOffset * 2);

	transform = float(graphRingBufferOffset + w);
	context->UpdateUniformBuffer(graphOffsetUniformBuffer, &transform, 0, 4);
	context->Draw(graphVertexBuffer, (w - graphRingBufferOffset) * 2, graphRingBufferOffset * 2);

	lastGraphValue = cpuv[1];
	graphRingBufferOffset++;
	if (graphRingBufferOffset >= w)
	{
		lastGraphValue.x = 0;
		graphRingBufferOffset = 0;
	}

	lastWidth = w;
	lastHeight = h;

}


void GpuProfiler::recreateGraphBuffer(int w)
{
	if (w == lastWidth)
		return;

	lastGraphValue = {};
	lastGraphValue.y = (float)w;
	graphRingBufferOffset = 0;

	if (graphVertexBuffer)
		graphVertexBuffer->Release();

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
	{
		graphData[i] = vec4(float(i/2), 100, 0, 0);
	}

	graphVertexBuffer = GetCoreRender()->CreateVertexBuffer(graphData, &desc, nullptr, nullptr, BUFFER_USAGE::CPU_WRITE);
}

void GpuProfiler::Init()
{
	
	// Font shader
	{
		auto text = loadShader(DIR"gpuprofiler_font.shader");
		const ConstantBuffersDesc buffersdesc[1] =
		{
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		shaderFont = GetCoreRender()->CreateShader(text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));
	}

	// Graph
	{
		const ConstantBuffersDesc buffersdesc[1] =
		{
			"GraphTransform", CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		auto text = loadShader(DIR"gpuprofiler_graph.shader");
		graphShader = GetCoreRender()->CreateShader(text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));

		Dx12GraphicCommandContext* context = GetCoreRender()->GetMainCommmandContext();
		unsigned w, h;
		context->GetBufferSize(w, h);

		recreateGraphBuffer(w);
	}

	viewportCB = GetCoreRender()->CreateUniformBuffer(16);
	transformCB = GetCoreRender()->CreateUniformBuffer(sizeof(TransformCB));
	graphOffsetUniformBuffer = GetCoreRender()->CreateUniformBuffer(16);

	//
	// Texture
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ID3D12Resource* d3dtexture;
	DirectX::LoadDDSTextureFromFile(CR_GetD3DDevice(), fontTexturePath, &d3dtexture, ddsData, subresources);

	fontTexture = GetCoreRender()->CreateTexture(std::move(ddsData), subresources, d3dtexture);

	//
	// Font data
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

	fontDataStructuredBuffer = GetCoreRender()->CreateStructuredBuffer(sizeof(FontChar), fontData.size(), &fontData[0]);

	records.resize(3);
}

void GpuProfiler::Free()
{
	shaderFont->Release();
	fontTexture->Release();
	fontDataStructuredBuffer->Release();
	graphVertexBuffer->Release();
	graphShader->Release();

	for (int i = 0; i < records.size(); ++i)
	{
		RenderProfilerRecord& r = records[i];
		r.vertexBuffer->Release();
	}
}

