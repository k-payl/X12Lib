#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "filesystem.h"
#include "icorerender.h"
#include "test1_shared.h"

using namespace std::chrono;
using namespace x12;

//#define TEST_PUSH_POP // define to test push/pop states
#define CAMERA_SEPARATE_BUFFER
#define VIDEO_API INIT_FLAGS::DIRECTX12_RENDERER

void Init();
void Render();

static struct Resources
{
	std::unique_ptr<Camera> cam;
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<ICoreVertexBuffer> vertexBuffer;
	intrusive_ptr<IResourceSet> cubeResources;
	intrusive_ptr<ICoreBuffer> cameraBuffer;

#ifdef TEST_PUSH_POP
	intrusive_ptr<ICoreShader> comp;
	intrusive_ptr<IResourceSet> compSB;
	intrusive_ptr<ICoreBuffer> compBuffer;
#endif

	Resources()
	{
		cam = std::make_unique<Camera>();
	}

} *res;
constexpr inline UINT float4chunks = 10;
static HWND hwnd;
static size_t mvpIdx;
static size_t transformIdx;
static size_t chunkIdx;
static steady_clock::time_point start;


// ----------------------------
// Main
// ----------------------------

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	Core *core = new Core();

	core->AddRenderProcedure(Render);
	core->AddInitProcedure(Init);

	res = new Resources();
	core->Init(nullptr, nullptr, INIT_FLAGS::NO_CONSOLE | VIDEO_API);
	hwnd = *core->GetWindow()->handle();

	core->Start();

	delete res;

	core->Free();
	delete core;

	return 0;
}

void Render()
{
	ICoreRenderer* renderer = CORE->GetCoreRenderer();
	surface_ptr surface = renderer->GetWindowSurface(hwnd);
	ICoreGraphicCommandList* context = renderer->GetGraphicCommandContext();

	context->CommandsBegin();
	context->BindSurface(surface);
	context->Clear();
	context->TimerBegin(0);
	start = high_resolution_clock::now();

	unsigned w, h;
	surface->GetSubstents(w, h);
	float aspect = float(w) / h;

	context->SetViewport(w, h);
	context->SetScissor(0, 0, w, h);
	GraphicPipelineState pso{};
	pso.shader = res->shader.get();
	pso.vb = res->vertexBuffer.get();
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;

	context->SetGraphicPipelineState(pso);

	context->SetVertexBuffer(res->vertexBuffer.get());

	mat4 MVP;
	res->cam->GetMVP(MVP, aspect);

	res->cameraBuffer->SetData(&MVP, sizeof(MVP));

	if (!res->cubeResources)
	{
		renderer->CreateResourceSet(res->cubeResources.getAdressOf(), res->shader.get());
		res->cubeResources->BindConstantBuffer("CameraCB", res->cameraBuffer.get());
		context->BuildResourceSet(res->cubeResources.get());
		transformIdx = res->cubeResources->FindInlineBufferIndex("TransformCB");
	}

	context->BindResourceSet(res->cubeResources.get());

	auto drawCubes = [context](float x)
	{
		for (int i = 0; i < numCubesX; ++i)
		{
			for (int j = 0; j < numCubesY; ++j)
			{
				static_assert(sizeof(DynamicCB) == 4 * 8);

				DynamicCB dynCB;
				dynCB.transform = cubePosition(i, j);
				dynCB.color_out = cubeColor(i, j);
				dynCB.transform.z += x;

				context->UpdateInlineConstantBuffer(transformIdx, &dynCB, sizeof(dynCB));
				context->Draw(res->vertexBuffer.get());
			}
		}
	};

	drawCubes(-4.f);

#ifdef TEST_PUSH_POP
	context->PushState();
	{
		ComputePipelineState cpso{};
		cpso.shader = res->comp.get();

		if (!res->compSB)
		{
			renderer->CreateResourceSet(res->compSB.getAdressOf(), res->comp.get());
			res->compSB->BindStructuredBufferUAV("tex_out", res->compBuffer.get());
			context->BuildResourceSet(res->compSB.get());
			chunkIdx = res->compSB->FindInlineBufferIndex("ChunkNumber");
		}

		context->SetComputePipelineState(cpso);

		context->BindResourceSet(res->compSB.get());

		for (UINT i = 0; i < float4chunks; ++i)
		{
			context->UpdateInlineConstantBuffer(chunkIdx, &i, 4);
			context->Dispatch(1, 1);
			context->EmitUAVBarrier(res->compBuffer.get());
		}

		float ss[4 * float4chunks];
		memset(ss, 0, sizeof(ss));
	
		res->compBuffer->GetData(ss);
		int y = 0;
	}
	context->PopState();
	
#endif

	drawCubes(4.f);

	context->TimerEnd(0);
	float frameGPU = context->TimerGetTimeInMs(0);

	auto frameCPU = duration_cast<microseconds>(high_resolution_clock::now() - start).count() * 1e-3f;

	CORE->RenderProfiler(frameGPU, frameCPU);

	context->CommandsEnd();
	context->Submit();
	renderer->PresentSurfaces();

	context->WaitGPUFrame();
}

void Init()
{
	ICoreRenderer* renderer = CORE->GetCoreRenderer();

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

	renderer->CreateVertexBuffer(res->vertexBuffer.getAdressOf(), L"cube", vertexData, &desc, indexData, &idxDesc);

	{
		auto text = CORE->GetFS()->LoadFile(SHADER_DIR "mesh.shader");

		const ConstantBuffersDesc buffersdesc[] =
		{
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		renderer->CreateShader(res->shader.getAdressOf(), L"mesh.shader", text.get(), text.get(), buffersdesc,
											 _countof(buffersdesc));
	}
	renderer->CreateConstantBuffer(res->cameraBuffer.getAdressOf(), L"Camera constant buffer", sizeof(mat4));

#ifdef TEST_PUSH_POP
	{
		auto text = CORE->GetFS()->LoadFile(SHADER_DIR "uav.shader");

		const ConstantBuffersDesc buffersdesc[] =
		{
			"ChunkNumber",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		renderer->CreateComputeShader(res->comp.getAdressOf(), L"uav.shader", text.get(), buffersdesc,
									  _countof(buffersdesc));
	}
	renderer->CreateStructuredBuffer(res->compBuffer.getAdressOf(), L"Unordered buffer for test barriers", 16, float4chunks, nullptr, BUFFER_FLAGS::UNORDERED_ACCESS);
#endif
}


