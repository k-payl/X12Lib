//#include "pch.h"

#include "common.h"
#include "core.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12context.h"
#include "dx12vertexbuffer.h"
#include "camera.h"
#include "test1_shared.h"

using namespace std::chrono;

struct Resources
{
	intrusive_ptr<Dx12CoreShader> shader;
	intrusive_ptr<Dx12CoreVertexBuffer> vertexBuffer;
	std::unique_ptr<Camera> cam;
	Dx12UniformBuffer* mvpCB;
	Dx12UniformBuffer* transformCB;

	Resources()
	{
		cam = std::make_unique<Camera>();
	}
} *res;

void Init();
void Render();

static steady_clock::time_point start;

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	Core *core = new Core();
	core->AddRenderProcedure(Render);
	core->AddInitProcedure(Init);
	
	res = new Resources;
	core->Init(INIT_FLAGS::SHOW_CONSOLE);

	core->Start();

	delete res;

	core->Free();
	delete core;

	return 0;
}

void Render()
{
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();

	Dx12GraphicCommandContext* context = renderer->GetMainCommmandContext();
	context->Begin();

	context->TimerBegin(0);
	start = high_resolution_clock::now();

	context->ClearBuiltinRenderTarget(vec4(0, 0, 0, 0));
	context->ClearBuiltinRenderDepthBuffer();

	context->SetBuiltinRenderTarget();

	unsigned w, h;
	context->GetBufferSize(w, h);
	context->SetViewport(w, h);

	context->SetScissor(0, 0, w, h);

	PipelineState pso{};
	pso.shader = res->shader.get();
	pso.vb = res->vertexBuffer.get();
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;

	context->SetPipelineState(pso);
	
	context->SetVertexBuffer(res->vertexBuffer.get());

	mat4 P;
	res->cam->GetPerspectiveMat(P, static_cast<float>(w) / h);

	MVPcb mvpcb;

	mat4 V;
	res->cam->GetViewMat(V);

	mvpcb.MVP = P * V;

	context->BindUniformBuffer(0, res->mvpCB, SHADER_TYPE::SHADER_VERTEX);
	context->BindUniformBuffer(3, res->transformCB, SHADER_TYPE::SHADER_VERTEX);

	context->UpdateUniformBuffer(res->mvpCB, &mvpcb, 0, sizeof(MVPcb));

	for (int i = 0; i < numCubesX; i++)
	{
		for( int j = 0; j < numCubesY; j++)
		{
			ColorCB transformCB;
			transformCB.color_out = cubeColor(i, j);
			transformCB.transform = cubePosition(i, j);

			context->UpdateUniformBuffer(res->transformCB, &transformCB, 0, sizeof(ColorCB));

			context->Draw(res->vertexBuffer.get());
		}
	}
	
	context->TimerEnd(0);
	float frameGPU = context->TimerGetTimeInMs(0);

	auto frameCPU = duration_cast<microseconds>(high_resolution_clock::now() - start).count() * 1e-3f;

	CORE->RenderProfiler(frameGPU, frameCPU);

	context->End();
	context->Submit();
	context->Present();
	context->WaitGPUFrame();
}

void Init()
{
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();
	
	VertexAttributeDesc attr[2];
	attr[0].format = VERTEX_BUFFER_FORMAT::FLOAT4;
	attr[0].offset = 0;
	attr[0].semanticName = "POSITION";
	attr[1].format = VERTEX_BUFFER_FORMAT::FLOAT4;
	attr[1].offset = 16;
	attr[1].semanticName = "TEXCOORD";

	VeretxBufferDesc desc;
	desc.attributesCount = 2;
	desc.attributes = attr;
	desc.vertexCount = veretxCount;

	IndexBufferDesc idxDesc;
	idxDesc.format = INDEX_BUFFER_FORMAT::UNSIGNED_16;
	idxDesc.vertexCount = idxCount;

	renderer->CreateVertexBuffer(res->vertexBuffer.getAdressOf(), vertexData, &desc, indexData, &idxDesc);

	auto text = loadShader("..//mesh.shader");

	const ConstantBuffersDesc buffersdesc[] =
	{
		"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
	};

	renderer->CreateShader(res->shader.getAdressOf(), text.get(), text.get(), buffersdesc,
										 _countof(buffersdesc));

	renderer->CreateUniformBuffer(&res->mvpCB, sizeof(MVPcb));
	renderer->CreateUniformBuffer(&res->transformCB, sizeof(ColorCB));
}


