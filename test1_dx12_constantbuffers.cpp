//#include "pch.h"

#include "common.h"
#include "core.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12commandlist.h"
#include "dx12vertexbuffer.h"
#include "camera.h"
#include "test1_shared.h"

using namespace std::chrono;

struct Resources
{
	Dx12CoreShader* shader;
	Dx12CoreVertexBuffer *vertexBuffer;
	std::unique_ptr<Camera> cam;
	Dx12UniformBuffer* mvpCB;
	Dx12UniformBuffer* transformCB;

	Resources()
	{
		cam = std::make_unique<Camera>();
	}

	~Resources()
	{
		shader->Release();
		vertexBuffer->Release();
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
	core->Init();

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

	PipelineState pso;
	pso.shader = res->shader;
	pso.vb = res->vertexBuffer;

	context->SetPipelineState(pso);
	
	context->SetVertexBuffer(res->vertexBuffer);

	mat4 P;
	res->cam->GetPerspectiveMat(P, static_cast<float>(w) / h);

	MVPcb mvpcb;

	mat4 V;
	res->cam->GetViewMat(V);

	mvpcb.MVP = P * V;

	context->BindUniformBuffer(SHADER_TYPE::SHADER_VERTEX, 0, res->mvpCB);
	context->BindUniformBuffer(SHADER_TYPE::SHADER_VERTEX, 3, res->transformCB);

	context->UpdateUnifromBuffer(res->mvpCB, &mvpcb, 0, sizeof(MVPcb));

	for (int i = 0; i < numCubesX; i++)
	{
		for( int j = 0; j < numCubesY; j++)
		{
			ColorCB transformCB;
			transformCB.color_out = cubeColor(i, j);
			transformCB.transform = cubePosition(i, j);

			context->UpdateUnifromBuffer(res->transformCB, &transformCB, 0, sizeof(ColorCB));

			context->Draw(res->vertexBuffer);
		}
	}

	//renderer->GPUProfileRender();
	
	context->TimerEnd(0);

	auto duration = high_resolution_clock::now() - start;
	auto micrs = duration_cast<microseconds>(duration).count();
	float frameCPU = micrs * 1e-3f;
	float frameGPU = context->TimerGetTimeInMs(0);

#ifdef USE_PROFILER_REALTIME
	static float accum;
	if (accum > UPD_INTERVAL)
	{
		accum = 0;
		CORE->LogProfiler("Render GPU (in ms)", frameGPU);
		CORE->LogProfiler("Render CPU (in ms)", frameCPU);
		CORE->LogProfiler("Frame CPU (in ms)", CORE->dt * 1e3f);
		CORE->LogProfiler("FPS", CORE->fps);
	}

	accum += CORE->dt;
#endif

#ifdef USE_PROFILE_TO_CSV
	// stat
	if (CORE->frame > StartFrame && CORE->frame % SkipFrames == 0 && curFrame < Frames)
	{
		data[curFrame].f = CORE->frame;
		data[curFrame].CPU = frameCPU;
		data[curFrame].GPU = frameGPU;
		curFrame++;
	}
	if (curFrame == Frames)
	{
		CORE->Log("Statistic compltetd");
		{
			std::ofstream file("dx12.csv", std::ios::out);
			for (size_t i = 0; i < Frames; ++i)
			{
				file << data[i].f << ", " << data[i].CPU << ", " << data[i].GPU << "\n";
			}
			file.close();
		}

		std::sort(data.begin(), data.end(), [](const Stat& l, const Stat& r) -> int
		{
			return l.CPU < r.CPU;
		});

		char buf[50];
		sprintf_s(buf, "Median CPU: %f", data[Frames / 2].CPU);
		CORE->Log(buf);

		std::sort(data.begin(), data.end(), [](const Stat& l, const Stat& r) -> int
		{
			return l.GPU < r.GPU;
		});

		sprintf_s(buf, "Median GPU: %f", data[Frames / 2].GPU);
		CORE->Log(buf);

		curFrame = Frames + 1;
	}
#endif

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

	res->vertexBuffer = renderer->CreateVertexBuffer(vertexData, &desc, indexData, &idxDesc);

	auto text = loadShader("mesh.shader");

	const ConstantBuffersDesc buffersdesc[] =
	{
		"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
	};

	res->shader = renderer->CreateShader(text.get(), text.get(), buffersdesc, 
										 _countof(buffersdesc));

	res->mvpCB = renderer->CreateUniformBuffer(sizeof(MVPcb));
	res->transformCB = renderer->CreateUniformBuffer(sizeof(ColorCB));

#ifdef USE_PROFILE_TO_CSV
	data.resize(Frames);
#endif
}


