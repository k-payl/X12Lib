#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "filesystem.h"
#include "icorerender.h"
#include "constantbuffers_shared.h"
#include "scenemanager.h"

#include "resourcemanager.h"
#include "texture.h"

using namespace x12;

#define CAMERA_SEPARATE_BUFFER
#define VIDEO_API engine::INIT_FLAGS::DIRECTX12_RENDERER

void Init();
void Render();

static struct Resources
{
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<IResourceSet> cubeResources;
	intrusive_ptr<ICoreBuffer> cameraBuffer;
	engine::StreamPtr<engine::Texture> tex;
	engine::StreamPtr<engine::Mesh> teapot;
} *res;

constexpr inline UINT float4chunks = 10;
static HWND hwnd;
static size_t mvpIdx;
static size_t transformIdx;
static size_t chunkIdx;
static engine::Camera* cam;

// ----------------------------
// Main
// ----------------------------

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	engine::Core *core = engine::CreateCore();

	core->AddRenderProcedure(Render);
	core->AddInitProcedure(Init);

	res = new Resources();
	core->Init(VIDEO_API);
	cam = engine::GetSceneManager()->CreateCamera();
	hwnd = *core->GetWindow()->handle();

	core->Start();

	delete res;

	core->Free();
	engine::DestroyCore(core);

	return 0;
}

void Render()
{
	ICoreRenderer* renderer = engine::GetCoreRenderer();
	surface_ptr surface = renderer->GetWindowSurface(hwnd);
	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	cmdList->BindSurface(surface);
	cmdList->Clear();

	unsigned w, h;
	surface->GetSubstents(w, h);
	float aspect = float(w) / h;

	cmdList->SetViewport(w, h);
	cmdList->SetScissor(0, 0, w, h);
	GraphicPipelineState pso{};
	pso.shader = res->shader.get();
	pso.vb = res->teapot.get()->RenderVertexBuffer();
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;

	cmdList->SetGraphicPipelineState(pso);

	cmdList->SetVertexBuffer(res->teapot.get()->RenderVertexBuffer());

	mat4 MVP;
	cam->GetModelViewProjectionMatrix(MVP, aspect);

	res->cameraBuffer->SetData(&MVP, sizeof(MVP));

	if (!res->cubeResources)
	{
		renderer->CreateResourceSet(res->cubeResources.getAdressOf(), res->shader.get());
		res->cubeResources->BindConstantBuffer("CameraCB", res->cameraBuffer.get());
		res->cubeResources->BindTextueSRV("texture_", res->tex.get()->GetCoreTexture());
		cmdList->CompileSet(res->cubeResources.get());
		transformIdx = res->cubeResources->FindInlineBufferIndex("TransformCB");
	}

	cmdList->BindResourceSet(res->cubeResources.get());

	auto drawCubes = [cmdList](float x)
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

				cmdList->UpdateInlineConstantBuffer(transformIdx, &dynCB, sizeof(dynCB));
				cmdList->Draw(res->teapot.get()->RenderVertexBuffer());
			}
		}
	};

	drawCubes(-3);

	drawCubes(3);

	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);
}

void Init()
{
	ICoreRenderer* renderer = engine::GetCoreRenderer();

	{
		auto text = engine::GetFS()->LoadFile(SHADER_DIR "mesh.shader");

		const ConstantBuffersDesc buffersdesc[] =
		{
			"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};

		renderer->CreateShader(res->shader.getAdressOf(), L"mesh.shader", text.get(), text.get(), buffersdesc,
											 _countof(buffersdesc));
	}
	renderer->CreateConstantBuffer(res->cameraBuffer.getAdressOf(), L"Camera constant buffer", sizeof(mat4));

	res->tex = engine::GetResourceManager()->CreateStreamTexture(TEXTURES_DIR"chipped-paint-metal-albedo_3_512x512.dds", TEXTURE_CREATE_FLAGS::NONE);
	res->tex.get(); // force load

	res->teapot = engine::GetResourceManager()->CreateStreamMesh(MESH_DIR "Teapot.002.mesh");
	res->teapot.get();
}


