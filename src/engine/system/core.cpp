
#include "core.h"
#include "mainwindow.h"
#include "input.h"
#include "dx12render.h"
#include "console.h"
#include "filesystem.h"
#include "render.h"
#include "scenemanager.h"
#include "camera.h"
#include "dx12gpuprofiler.h"
#include "resourcemanager.h"

#if VK_ENABLE
#include "vkrender.h"
#endif

#include <chrono>
#include <string>
#include <inttypes.h>

using namespace std;
using namespace engine;

#define UPD_INTERVAL 0.25f

static std::chrono::steady_clock::time_point start;
Core* core__;

static ConsoleBoolCommand commandGPUProfiler("gpu_profiler", true);

void Core::mainLoop()
{
	input->Update();
	sceneManager->Update(dt);
	engineUpdate();

	onUpdate.Invoke(dt);

	// Render code begins here

	int64_t frameEmit = (frame) % QueryNum;
	int64_t frameGet = (frame + 1) % QueryNum;

	if (!queryCmdListsStart[frameEmit])
	{
		x12::ICoreGraphicCommandList* cmdList = queryCmdListsStart[frameEmit] = renderer->GetGraphicCommandList(frameEmit);
		cmdList->CommandsBegin();
		cmdList->StartQuery(queries[frameEmit].get());
		cmdList->CommandsEnd();
	}
	renderer->ExecuteCommandList(queryCmdListsStart[frameEmit]);

	// high level render
	if (render && window)
	{
		int w, h;
		window->GetClientSize(w, h);

		engine::ViewportData viewportData;
		viewportData.width = w;
		viewportData.height = h;
		viewportData.id = 0;
		HWND *hwnd = GetWindow()->handle();
		viewportData.hwnd = hwnd;

		engine::CameraData camData{};
		camData.camera = camera;
		if (camera)
		{
			float aspect = (float)w / h;
			camera->GetProjectionMatrix(camData.ProjMat, aspect);
			camera->GetViewMatrix(camData.ViewMat);
			camData.verFullFovInRadians = camera->GetFullVertFOVInRadians();
		}

		render->RenderFrame(viewportData, camData);
	}

	// custom render code
	onRender.Invoke();

	float gpuFrameTime = queries[frameGet]->GetTime();

	// render profiler
	if (window && commandGPUProfiler.value)
	{
		int w, h;
		window->GetClientSize(w, h);
		RenderProfiler(gpuFrameTime, dt * 1000.f, w, h);
	}

	if (!queryCmdListsStop[frameEmit])
	{
		x12::ICoreGraphicCommandList* cmdList = queryCmdListsStop[frameEmit] = renderer->GetGraphicCommandList(QueryNum + frameEmit);
		cmdList->CommandsBegin();
		cmdList->StopQuery(queries[frameEmit].get());
		cmdList->CommandsEnd();
	}
	renderer->ExecuteCommandList(queryCmdListsStop[frameEmit]);

	renderer->PresentSurfaces();
	renderer->FrameEnd();
	renderer->WaitGPU();

	endFrame();
}

void Core::messageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	if (type == WINDOW_MESSAGE::SIZE)
	{
		if (renderer)
			renderer->RecreateBuffers(hwnd, param1, param2);
	}
	else if (type == WINDOW_MESSAGE::KEY_UP)
	{
		KEYBOARD_KEY_CODES key = static_cast<KEYBOARD_KEY_CODES>(param1);
		if (key == KEYBOARD_KEY_CODES::KEY_ESCAPE)
			window->SendCloseMesage();
		else if (key == KEYBOARD_KEY_CODES::KEY_TILDA && console)
			console->Toggle();
	}
}

void Core::sMainLoop()
{
	core__->mainLoop();
}

void Core::sMessageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	core__->messageCallback(hwnd, type, param1, param2, pData);
}

Core::Core()
{
	core__ = this;
}

Core::~Core() = default;

void Core::Init(INIT_FLAGS flags, GpuProfiler* gpuprofiler_, InitRendererProcedure initRenderer)
{
#define FLAG(arg) (flags & arg)
#define NOT(arg) (!(flags & arg))

	fs = std::make_unique<FileSystem>(DATA_DIR);
	sceneManager = std::make_unique<SceneManager>();

	if (NOT(INIT_FLAGS::NO_CONSOLE))
	{
		console = std::make_unique<Console>();
		console->Create();
	}

	if (NOT(INIT_FLAGS::NO_WINDOW))
	{
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		window = std::make_unique<MainWindow>(&Core::sMainLoop);
		window->Create();
		window->AddMessageCallback(sMessageCallback);
		window->SetCaption(L"Test DX12");
	}

	if (NOT(INIT_FLAGS::NO_INPUT))
	{
		input = std::make_unique<Input>();
		input->Init();
	}

	if (FLAG(INIT_FLAGS::DIRECTX12_RENDERER))
	{
		renderer = std::make_unique<x12::Dx12CoreRenderer>();
	}
	else if (FLAG(INIT_FLAGS::VULKAN_RENDERER))
	{
	#if VK_ENABLE
		renderer = std::make_unique<x12::VkCoreRenderer>();
	#else
		throw "Vulkan disabled. Use -DVK_ENABLE=ON in cmake";
	#endif
	}

	if (renderer)
		renderer->Init();

	if (renderer && FLAG(INIT_FLAGS::DIRECTX12_RENDERER))
	{
		renderprofiler = std::unique_ptr<GpuProfiler>(new Dx12GpuProfiler({ 0.6f, 0.6f, 0.6f, 1.0f }, 0.0f));
		renderprofiler->Init();
		renderprofiler->AddRecord("=== D3D12 Render ===",				true, false);
		renderprofiler->AddRecord("CPU: % 0.2f ms.",					true, true);
		renderprofiler->AddRecord("GPU: % 0.2f ms.",					true, true);
		renderprofiler->AddRecord("State changes: %" PRId64,			false, false);
		renderprofiler->AddRecord("Triangles: %" PRId64,				false, false);
		renderprofiler->AddRecord("Draw calls: %" PRId64,				false, false);
		renderprofiler->SetRecordColor(2, { 1,0,0,1 });
	}

	if (!window && initRenderer)
		notImplemented();
	else
	if (window && initRenderer)
		initRenderer(window->handle());

	if (gpuprofiler_)
	{
		gpuprofiler_->Init();
		renderprofiler.reset(gpuprofiler_);
	}

	if (renderer && FLAG(INIT_FLAGS::DIRECTX12_RENDERER))
	{
		memoryprofiler = std::unique_ptr<GpuProfiler>(new Dx12GpuProfiler({ 0.6f, 0.6f, 0.6f, 1.0f }, 140.0f));
		memoryprofiler->Init();
		memoryprofiler->AddRecord("=== GraphicMemory ===",			false, false);
		memoryprofiler->AddRecord("committedMemory: %zu bytes",		false, false);		// Bytes of memory currently committed/in-flight
		memoryprofiler->AddRecord("totalMemory: %zu bytes",			false, false);		// Total bytes of memory used by the allocators
		memoryprofiler->AddRecord("totalPages: %zu",				false, false);		// Total page count
		memoryprofiler->AddRecord("peakCommitedMemory: %zu bytes",	false, false);		// Peak commited memory value since last reset
		memoryprofiler->AddRecord("peakTotalMemory: %zu bytes",		false, false);		// Peak total bytes
		memoryprofiler->AddRecord("peakTotalPages: %zu",			false, false);		// Peak total page count
	}

	if (FLAG(INIT_FLAGS::HIGH_LEVEL_RENDER))
	{
		render = std::make_unique<Render>();
		render->Init();
	}

	queries.resize(QueryNum);
	for (int i = 0; i < QueryNum; i++)
	{
		renderer->CreateQuery(queries[i].getAdressOf());
	}

	onInit.Invoke();

#undef FLAG
#undef NOT
}

void Core::Free()
{
	queries.clear();

	onFree.Invoke();

	if (render)
	{
		render->Free();
		render = nullptr;
	}

	if (renderprofiler)
	{
		renderprofiler->Free();
		renderprofiler = nullptr;
	}

	if (memoryprofiler)
	{
		memoryprofiler->Free();
		memoryprofiler = nullptr;
	}

	if (renderer)
	{
		renderer->Free();
		renderer = nullptr;
	}

	if (console)
	{
		console->Destroy();
		console = nullptr;
	}

	if (window)
	{
		window->Destroy();
		window = nullptr;
	}

	input->Free();
	input = nullptr;

	fs = nullptr;
	sceneManager = nullptr;
}

void Core::setWindowCaption(int is_paused, int fps)
{
	if (!window)
		return;

	std::wstring title = std::wstring(L"DirecX12Test");

	if (is_paused)
		title += std::wstring(L" [Paused]");
	else
	{
		string fps_str = std::to_string(fps);
		title += std::wstring(L" [") + std::wstring(fps_str.begin(), fps_str.end()) + std::wstring(L"]");
	}

	window->SetCaption(title.c_str());
}

void Core::engineUpdate()
{
	static float accum = 0.0f;

	std::chrono::duration<float> _durationSec = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::steady_clock::now() - start);

	dt = _durationSec.count();
	fps = static_cast<int>(1.0f / dt);

	accum += dt;

	if (accum > UPD_INTERVAL)
	{
		accum = 0;
		setWindowCaption(0, fps);
	}

	start = std::chrono::steady_clock::now();
}

void engine::Core::endFrame()
{
	++frame;
	frameIndex = (frameIndex + 1u) % DeferredBuffers;
}

void Core::Start(Camera* cam)
{
	camera = cam;
	window->StartMainLoop();
}

void Core::RenderProfiler(float gpu_, float cpu_, int width, int height)
{
	renderprofiler->UpdateRecord(1, cpu_);
	renderprofiler->UpdateRecord(2, gpu_);

	if (dynamic_cast<Dx12GpuProfiler*>(renderprofiler.get()))
	{
		x12::CoreRenderStat s;
		renderer->GetStat(s);

		renderprofiler->UpdateRecord(3, s.StateChanges);
		renderprofiler->UpdateRecord(4, s.Triangles);
		renderprofiler->UpdateRecord(5, s.DrawCalls);

		if (memoryprofiler)
		{
			memoryprofiler->UpdateRecord(1, s.committedMemory);
			memoryprofiler->UpdateRecord(2, s.totalMemory);
			memoryprofiler->UpdateRecord(3, s.totalPages);
			memoryprofiler->UpdateRecord(4, s.peakCommitedMemory);
			memoryprofiler->UpdateRecord(5, s.peakTotalMemory);
			memoryprofiler->UpdateRecord(6, s.peakTotalPages);
		}
	}

	HWND hwnd = *GetWindow()->handle();

	x12::surface_ptr surface = renderer->GetWindowSurface(hwnd);
	x12::ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	cmdList->BindSurface(surface);
	cmdList->SetViewport(width, height);
	cmdList->SetScissor(0, 0, width, height);

	renderprofiler->Render(cmdList, width, height);

	if (memoryprofiler)
		memoryprofiler->Render(cmdList, width, height);

	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);
}

void Core::AddRenderProcedure(RenderProcedure fn)
{
	onRender.Add(fn);
}

void Core::AddInitProcedure(InitProcedure fn)
{
	onInit.Add(fn);
}

void Core::AddUpdateProcedure(UpdateProcedure fn)
{
	onUpdate.Add(fn);
}

void Core::AddFreeProcedure(FreeProcedure fn)
{
	onFree.Add(fn);
}

void Core::Log(const char* str)
{
	console->log(str);
}

void Core::LogProfiler(const char* key, const char* value)
{
//	console->log_profiler(key, value);
}

void Core::LogProfiler(const char* key, float value)
{
	//console->log_profiler(key, value);
}

void Core::LogProfiler(const char* key, int value)
{
	//console->log_profiler(key, value);
}

X12_API Core* engine::CreateCore()
{
	return new Core();
}

X12_API void engine::DestroyCore(Core* core)
{
	delete core;
}

x12::ICoreRenderer* engine::GetCoreRenderer()
{
	return core__->GetCoreRenderer();
}

MainWindow* engine::GetWindow()
{
	return core__->GetWindow();
}

Input* engine::GetInput()
{
	return core__->GetInput();
}

FileSystem* engine::GetFS()
{
	return core__->GetFS();
}

SceneManager* engine::GetSceneManager()
{
	return core__->GetSceneManager();
}

ResourceManager* engine::GetResourceManager()
{
	return core__->GetResourceManager();
}

X12_API Console* engine::GetConsole()
{
	return nullptr;
}

X12_API void engine::Log(const char* str)
{
	core__->GetConsole()->OutputTxt(str);
}
