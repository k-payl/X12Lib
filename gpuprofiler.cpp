#include "pch.h"
#include "gpuprofiler.h"
#include "dx12render.h"
#include "dx12vertexbuffer.h"
#include "dx12shader.h"
#include "dx12texture.h"
#include "dx12structuredbuffer.h"
#include "dx12context.h"
#include "3rdparty/DirectXTex/DDSTextureLoader12.h"
#include <fstream>

const int rectSize = 100;
const int rectPadding = 1;
const int guiMargin = 5;
const auto fontDataPath = "font//1.fnt";
const auto fontTexturePath = L"font//1_0.dds";

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
	float x, y, w, h;
	int id;
};

static std::vector<FontChar> fontData;

void GpuProfiler::Init()
{
	//
	// Vertex buffer
	VertexAttributeDesc attr[1];
	attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
	attr[0].offset = 0;
	attr[0].semanticName = "POSITION";

	const uint32_t veretxCount = 6;
	vec4 vertexData[veretxCount] = {
		{ vec4( 0.0f, 0.0f, 0.0f, 1.0f) },
		{ vec4(1.0f,  1.0f, 0.0f, 1.0f) },
		{ vec4( 0.0f, 1.0f, 0.0f, 1.0f) },
		{ vec4(0.0f, 0.0f, 0.0f, 1.0f) },
		{ vec4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ vec4(1.0f,  1.0f, 0.0f, 1.0f) },
	};

	VeretxBufferDesc desc;
	desc.attributesCount = _countof(attr);
	desc.attributes = attr;
	desc.vertexCount = veretxCount;

	quad = GetCoreRender()->CreateVertexBuffer(vertexData, &desc);
	
	//
	// Shader
	auto text = loadShader("gpuprofiler.shader");

	const ConstantBuffersDesc buffersdesc[1] =
	{
		"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
	};

	shader = GetCoreRender()->CreateShader(text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));

	viewportCB = GetCoreRender()->CreateUniformBuffer(16);
	transformCB = GetCoreRender()->CreateUniformBuffer(sizeof(TransformCB));

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

	fontDataGPUBuffer = GetCoreRender()->CreateStructuredBuffer(sizeof(FontChar), fontData.size(), &fontData[0]);
}

void GpuProfiler::Free()
{
	quad->Release();
	shader->Release();
	fontTexture->Release();
	fontDataGPUBuffer->Release();
}

void GpuProfiler::Render()
{
	Dx12GraphicCommandContext* context = GetCoreRender()->GetMainCommmandContext();

	PipelineState pso;
	pso.shader = shader;
	pso.vb = quad;

	context->SetPipelineState(pso);

	context->SetVertexBuffer(quad);

	context->BindUniformBuffer(0, viewportCB, SHADER_TYPE::SHADER_VERTEX);
	context->BindUniformBuffer(1, transformCB, SHADER_TYPE::SHADER_VERTEX);

	context->BindTexture(0, fontTexture, SHADER_TYPE::SHADER_FRAGMENT);
	context->BindStructuredBuffer(0, fontDataGPUBuffer, SHADER_TYPE::SHADER_VERTEX);

	unsigned w, h;
	context->GetBufferSize(w, h);
	float viewport[4] =
	{
		float(w),
		float(h),
		1.0f / w,
		1.0f / h
	};

	context->UpdateUniformBuffer(viewportCB, viewport, 0, 16);

	float offset = guiMargin;

	for (int i = 0; i < 76; ++i)
	{
		FontChar& item = fontData[48 + i];

		TransformCB t = 
		{
			offset + item.xoffset,
			guiMargin + item.yoffset,
			item.w,
			item.h,
			i
		};

		context->UpdateUniformBuffer(transformCB, &t, 0, sizeof(TransformCB));

		context->Draw(quad);

		offset += item.xadvance;
	}
}