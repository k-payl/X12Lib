#include "pch.h"
#include "core.h"
#include "mainwindow.h"
#include "input.h"
#include "dx12render.h"
#include "console.h"
#include "filesystem.h"
#include "dx12gpuprofiler.h"
#include <chrono>
#include <string>
#include <inttypes.h>

using namespace std;

#define UPD_INTERVAL 0.25f

static std::chrono::steady_clock::time_point start;
Core* core__;

void Core::mainLoop()
{
	input->Update();
	engineUpdate();

	onUpdate.Invoke(dt);
	onRender.Invoke();
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

void Core::Init(GpuProfiler* gpuprofiler_, InitRendererProcedure initRenderer, INIT_FLAGS flags)
{
	fs = new FileSystem("..//");

	if (!(flags & INIT_FLAGS::NO_CONSOLE))
	{
		console = new Console;
		console->Create();
	}

	if (!(flags & INIT_FLAGS::NO_WINDOW))
	{
		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		window = new MainWindow(&Core::sMainLoop);
		window->Create();
		window->AddMessageCallback(sMessageCallback);
		window->SetCaption(L"Test DX12");
	}

	if (!(flags & INIT_FLAGS::NO_INPUT))
	{
		input = new Input;
		input->Init();
	}

	if (flags & INIT_FLAGS::BUILT_IN_DX12_RENDERER)
	{
		renderer = new Dx12CoreRenderer;
		renderer->Init();

		assert(renderprofiler == nullptr);
		renderprofiler = new Dx12GpuProfiler(vec4(0.6f, 0.6f, 0.6f, 1.0f), 100.0f);
		renderprofiler->AddRecord("=== D3D12 Render ===");
		renderprofiler->AddRecord("CPU: % 0.2f ms.");
		renderprofiler->AddRecord("CPU: % 0.2f ms.");
		renderprofiler->AddRecord("Uniform buffer updates: %" PRId64);
		renderprofiler->AddRecord("State changes: %" PRId64);
		renderprofiler->AddRecord("Triangles: %" PRId64);
		renderprofiler->AddRecord("Draw calls: %" PRId64);
	}

	if (!window && initRenderer)
		throw logic_error("External renderer without window is not implemented");
	else
	if (window && initRenderer)
		initRenderer(window->handle());

	if (gpuprofiler_)
		renderprofiler = gpuprofiler_;

	if (renderprofiler)
		renderprofiler->Init();

	if (renderer)
	{
		memoryprofiler = new Dx12GpuProfiler(vec4(0, 0.7f, 0.4f, 1.0f), 250.0f);
		memoryprofiler->Init();
		memoryprofiler->AddRecord("=== Memory ===");
		memoryprofiler->AddRecord("Dynamic: %zu bytes");
		memoryprofiler->AddRecord("Dynamic pages: %zu");
	}

	onInit.Invoke();
}

void Core::Free()
{
	onFree.Invoke();

	if (renderprofiler)
	{
		renderprofiler->Free();
		delete renderprofiler;
		renderprofiler = nullptr;
	}

	if (memoryprofiler)
	{
		memoryprofiler->Free();
		delete memoryprofiler;
		memoryprofiler = nullptr;
	}

	if (renderer)
	{
		renderer->Free();
		delete renderer;
		renderer = nullptr;
	}

	if (console)
	{
		console->Destroy();
		delete console;
		console = nullptr;
	}

	if (window)
	{
		window->Destroy();
		delete window;
		window = nullptr;
	}

	input->Free();
	delete input;
	input = nullptr;

	delete fs;
	fs = nullptr;
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

	//accum += dt;

	//if (accum > UPD_INTERVAL)
	//{
	//	accum = 0;
	//	setWindowCaption(0, fps);
	//}

	start = std::chrono::steady_clock::now();

	frame++;
}

void Core::Start()
{
	window->StartMainLoop();
}

void Core::RenderProfiler(float gpu_, float cpu_)
{
	renderprofiler->UpdateRecord(1, cpu_);
	renderprofiler->UpdateRecord(2, gpu_);

	if (dynamic_cast<Dx12GpuProfiler*>(renderprofiler))
	{
		renderprofiler->UpdateRecord(3, renderer->UniformBufferUpdates());
		renderprofiler->UpdateRecord(4, renderer->StateChanges());
		renderprofiler->UpdateRecord(5, renderer->Triangles());
		renderprofiler->UpdateRecord(6, renderer->DrawCalls());
	}

	if (memoryprofiler)
	{
		memoryprofiler->UpdateRecord(1, x12::memory::dynamic::GetUsedVideoMemory());
		memoryprofiler->UpdateRecord(2, x12::memory::dynamic::GetAllocatedPagesCount());
		memoryprofiler->Render();
	}

	renderprofiler->Render();
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
	console->log_profiler(key, value);
}

void Core::LogProfiler(const char* key, float value)
{
	console->log_profiler(key, value);
}

void Core::LogProfiler(const char* key, int value)
{
	console->log_profiler(key, value);
}
