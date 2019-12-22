#include "pch.h"
#include "gpuprofiler.h"
#include "dx12render.h"
#include "dx12vertexbuffer.h"
#include "dx12shader.h"
#include "dx12context.h"

const int rectSize = 10;
const int rectPadding = 7;
const int guiMargin = 20;

void gpuProfiler::Init()
{
	VertexAttributeDesc attr[1];
	attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
	attr[0].offset = 0;
	attr[0].semanticName = "POSITION";

	const uint32_t veretxCount = 4;
	vec4 vertexData[veretxCount] = {
		{ vec4( 0.0f, 0.0f, 0.0f, 1.0f) },
		{ vec4( 0.0f, 1.0f, 0.0f, 1.0f) },
		{ vec4(1.0f,  1.0f, 0.0f, 1.0f) },
		{ vec4(1.0f,  0.0f, 0.0f, 1.0f) },
	};

	const uint32_t idxCount = 6;
	static WORD indexData[idxCount] =
	{
		0, 2, 1,
		0, 3, 2
	};

	VeretxBufferDesc desc;
	desc.attributesCount = _countof(attr);
	desc.attributes = attr;
	desc.vertexCount = veretxCount;

	IndexBufferDesc idxDesc;
	idxDesc.format = INDEX_BUFFER_FORMAT::UNSIGNED_16;
	idxDesc.vertexCount = idxCount;

	quad = GetCoreRender()->CreateVertexBuffer(vertexData, &desc, indexData, &idxDesc);

	auto text = loadShader("gpuprofiler.shader");

	const ConstantBuffersDesc buffersdesc[1] =
	{
		"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
	};

	shader = GetCoreRender()->CreateShader(text.get(), text.get(), &buffersdesc[0], _countof(buffersdesc));

	viewportCB = GetCoreRender()->CreateUniformBuffer(16);
	transformCB = GetCoreRender()->CreateUniformBuffer(16);
}

void gpuProfiler::Free()
{
	quad->Release();
	shader->Release();
}

void gpuProfiler::Render()
{
	Dx12GraphicCommandContext* context = GetCoreRender()->GetMainCommmandContext();

	PipelineState pso;
	pso.shader = shader;
	pso.vb = quad;

	context->SetPipelineState(pso);

	context->SetVertexBuffer(quad);

	context->BindUniformBuffer(SHADER_TYPE::SHADER_VERTEX, 0, viewportCB);
	context->BindUniformBuffer(SHADER_TYPE::SHADER_VERTEX, 1, transformCB);

	unsigned w, h;
	context->GetBufferSize(w, h);
	float viewport[4];
	viewport[0] = w;
	viewport[1] = h;
	viewport[2] = 1.0f / w;
	viewport[3] = 1.0f / h;

	context->UpdateUnifromBuffer(viewportCB, viewport, 0, 16);

	for (int i = 0; i < 10; ++i)
	{
		float transform[4];
		transform[0] = guiMargin + i * (rectSize + rectPadding);
		transform[1] = guiMargin;
		transform[2] = rectSize;
		transform[3] = rectSize;

		context->UpdateUnifromBuffer(transformCB, transform, 0, 16);

		context->Draw(quad);
	}
}