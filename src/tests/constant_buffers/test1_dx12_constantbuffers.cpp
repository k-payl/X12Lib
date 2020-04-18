#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "filesystem.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12context.h"
#include "dx12vertexbuffer.h"
#include "dx12buffer.h"
#include "test1_shared.h"

#define TEST_PUSH_POP // define to test push/pop states
#define CAMERA_SEPARATE_BUFFER

using namespace std::chrono;
using namespace x12;

constexpr inline UINT float4chunks = 10;

static HWND hwnd;
static size_t mvpIdx;
static size_t transformIdx;
static size_t chunkIdx;

static struct Resources
{
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<ICoreVertexBuffer> vertexBuffer;
	std::unique_ptr<Camera> cam;
	intrusive_ptr<IResourceSet> cubeResources;

#ifdef CAMERA_SEPARATE_BUFFER
	intrusive_ptr<ICoreBuffer> cameraBuffer;
#endif

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

void Init();
void Render();

static steady_clock::time_point start;

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	Core *core = new Core();

	core->AddRenderProcedure(Render);
	core->AddInitProcedure(Init);

	res = new Resources();
	core->Init(nullptr, nullptr, INIT_FLAGS::NO_CONSOLE | INIT_FLAGS::BUILT_IN_DX12_RENDERER);
	hwnd = *core->GetWindow()->handle();

	core->Start();

	delete res;

	core->Free();
	delete core;

	return 0;
}

void Render()
{
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();
	surface_ptr surface = renderer->GetWindowSurface(hwnd);
	Dx12GraphicCommandContext* context = renderer->GetGraphicCommmandContext();

	context->CommandsBegin();
	context->BindSurface(surface);
	context->Clear();
	context->TimerBegin(0);
	start = high_resolution_clock::now();

	unsigned w = surface->width;
	unsigned h = surface->height;
	float aspect = float(w) / h;

	context->SetViewport(w, h);
	context->SetScissor(0, 0, w, h);
#if _MSC_VER >= 1921
	GraphicPipelineState pso = {
		.shader = res->shader.get(),
		.vb = res->vertexBuffer.get(),
		.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE,
	};
#else
	GraphicPipelineState pso{};
	pso.shader = res->shader.get();
	pso.vb = res->vertexBuffer.get();
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
#endif
	context->SetGraphicPipelineState(pso);

	context->SetVertexBuffer(res->vertexBuffer.get());

	mat4 MVP;
	res->cam->GetMVP(MVP, aspect);

#ifdef CAMERA_SEPARATE_BUFFER
	res->cameraBuffer->SetData(&MVP, sizeof(MVP));
#endif

	if (!res->cubeResources)
	{
		renderer->CreateResourceSet(res->cubeResources.getAdressOf(), res->shader.get());
#ifdef CAMERA_SEPARATE_BUFFER
		res->cubeResources->BindConstantBuffer("CameraCB", res->cameraBuffer.get());
#endif
		context->BuildResourceSet(res->cubeResources.get());

#ifndef CAMERA_SEPARATE_BUFFER
		mvpIdx = res->cubeResources->FindInlineBufferIndex("CameraCB");
#endif
		transformIdx = res->cubeResources->FindInlineBufferIndex("TransformCB");
	}

#ifndef CAMERA_SEPARATE_BUFFER
	context->UpdateInlineConstantBuffer(mvpIdx, &MVP, sizeof(MVP));
#endif
	context->BindResourceSet(res->cubeResources.get());

	auto drawCubes = [context](float x)
	{
		for (int i = 0; i < numCubesX; ++i)
		{
			for (int j = 0; j < numCubesY; ++j)
			{
				static_assert(sizeof(DynamicCB) == 4 * 8);

#if _MSC_VER >= 1921
				DynamicCB dynCB {
					.transform = cubePosition(i, j) /*+ vec4(0, 0, 0, x)*/, // fatal error C1001: Internal compiler error.
					.color_out = cubeColor(i, j)
				};
				dynCB.transform.z += x;
#else
				DynamicCB dynCB;
				dynCB.transform = cubePosition(i, j);
				dynCB.color_out = cubeColor(i, j);
				dynCB.transform.z += x;
#endif

				context->UpdateInlineConstantBuffer(transformIdx, &dynCB, sizeof(dynCB));
				context->Draw(res->vertexBuffer.get());
			}
		}
	};

	drawCubes(-5.f);

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

	drawCubes(5.f);

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
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();

#if _MSC_VER >= 1921
	VertexAttributeDesc attr[2] = {
		{
			.offset = 0,
			.format = VERTEX_BUFFER_FORMAT::FLOAT4,
			.semanticName = "POSITION"
		},
		{
			.offset = 16,
			.format = VERTEX_BUFFER_FORMAT::FLOAT4,
			.semanticName = "TEXCOORD"
		}
	};

	VeretxBufferDesc desc =	{
		.vertexCount = veretxCount,
		.attributesCount = 2,
		.attributes = attr
	};	

	IndexBufferDesc idxDesc = {
		.vertexCount = idxCount,
		.format = INDEX_BUFFER_FORMAT::UNSIGNED_16
	};
#else
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
#endif
	renderer->CreateVertexBuffer(res->vertexBuffer.getAdressOf(), L"cube", vertexData, &desc, indexData, &idxDesc);

	{
		auto text = CORE->GetFS()->LoadFile(SHADER_DIR "mesh.shader");

		const ConstantBuffersDesc buffersdesc[] =
		{
		#ifndef CAMERA_SEPARATE_BUFFER
			"CameraCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW,
		#endif
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		renderer->CreateShader(res->shader.getAdressOf(), L"mesh.shader", text.get(), text.get(), buffersdesc,
											 _countof(buffersdesc));
	}
#ifdef CAMERA_SEPARATE_BUFFER
	renderer->CreateConstantBuffer(res->cameraBuffer.getAdressOf(), L"Camera constant buffer", sizeof(mat4));
#endif

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


