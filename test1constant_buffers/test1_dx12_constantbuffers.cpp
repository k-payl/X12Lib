#include "common.h"
#include "core.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12context.h"
#include "dx12vertexbuffer.h"
#include "dx12buffer.h"
#include "camera.h"
#include "mainwindow.h"
#include "filesystem.h"
#include "test1_shared.h"

using namespace std::chrono;

constexpr inline UINT chunks = 10;

struct Resources
{
	intrusive_ptr<Dx12CoreShader> shader;
	intrusive_ptr<Dx12CoreVertexBuffer> vertexBuffer;
	std::unique_ptr<Camera> cam;
	Dx12UniformBuffer* mvpCB;
	Dx12UniformBuffer* transformCB;
	HWND hwnd{};

	//dbg
	intrusive_ptr<Dx12CoreShader> comp;
	Dx12UniformBuffer* compCB;
	intrusive_ptr<Dx12CoreBuffer> compSB;

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

	res = new Resources();
	core->Init(nullptr, nullptr, /*INIT_FLAGS::SHOW_CONSOLE |*/ INIT_FLAGS::BUILT_IN_DX12_RENDERER);
	res->hwnd = *core->GetWindow()->handle();

	core->Start();

	delete res;

	core->Free();
	delete core;

	return 0;
}

void Render()
{
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();
	surface_ptr surface = renderer->MakeCurrent(res->hwnd);
	Dx12GraphicCommandContext* context = renderer->GetGraphicCommmandContext();

	context->CommandsBegin(surface);

	context->Clear();

	context->TimerBegin(0);
	start = high_resolution_clock::now();

	unsigned w = surface->width;
	unsigned h = surface->height;

	context->SetViewport(w, h);
	context->SetScissor(0, 0, w, h);

	GraphicPipelineState pso{};
	pso.shader = res->shader.get();
	pso.vb = res->vertexBuffer.get();
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;

	context->SetGraphicPipelineState(pso);
	
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

	auto drawCubes = [context](vec4 color, float x)
	{
		for (int i = 0; i < numCubesX; i++)
		{
			for (int j = 0; j < numCubesY; j++)
			{
				ColorCB transformCB;
				transformCB.color_out = cubeColor(i, j);
				transformCB.transform = cubePosition(i, j);
				transformCB.transform.z += x;

				context->UpdateUniformBuffer(res->transformCB, &transformCB, 0, sizeof(ColorCB));

				context->Draw(res->vertexBuffer.get());
			}
		}
	};

	drawCubes(vec4(1, 0, 0, 1), -6.0f);	

	{
		context->PushState();

		ComputePipelineState cpso{};
		cpso.shader = res->comp.get();

		context->SetComputePipelineState(cpso);
		context->BindUniformBuffer(0, res->compCB, SHADER_TYPE::SHADER_COMPUTE);
		context->BindUAVStructuredBuffer(1, res->compSB.get(), SHADER_TYPE::SHADER_COMPUTE);

		for (UINT i = 0; i< chunks; ++i)
		{
			context->UpdateUniformBuffer(res->compCB, &i, 0, 4);
			context->Dispatch(1, 1);
			context->EmitUAVBarrier(res->compSB.get());
		}

		float ss[4 * chunks];
		memset(ss, 0, sizeof(ss));

		res->compSB->GetData(ss);
		int y = 0;

		context->PopState();
	}

	drawCubes(vec4(1, 0, 0, 1), 6.0f);

	context->TimerEnd(0);
	float frameGPU = context->TimerGetTimeInMs(0);

	auto frameCPU = duration_cast<microseconds>(high_resolution_clock::now() - start).count() * 1e-3f;


	CORE->RenderProfiler(frameGPU, frameCPU, true);

	context->CommandsEnd();
	context->Submit();
	renderer->PresentSurfaces();
	context->WaitGPUFrame();

	context->FrameEnd();
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

	{
		auto text = CORE->GetFS()->LoadFile("mesh.shader");

		const ConstantBuffersDesc buffersdesc[] =
		{
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		renderer->CreateShader(res->shader.getAdressOf(), text.get(), text.get(), buffersdesc,
											 _countof(buffersdesc));
	}

	{
		auto text = CORE->GetFS()->LoadFile("mipmap.shader");

		
		renderer->CreateComputeShader(res->comp.getAdressOf(), text.get());
	}

	renderer->CreateUniformBuffer(&res->mvpCB, sizeof(MVPcb));
	renderer->CreateUniformBuffer(&res->transformCB, sizeof(ColorCB));
	renderer->CreateUniformBuffer(&res->compCB, 4);

	renderer->CreateStructuredBuffer(res->compSB.getAdressOf(), 16, chunks, nullptr, BUFFER_FLAGS::UNORDERED_ACCESS);
}


