#include "pch.h"

#include "core.h"
#include "mainwindow.h"
#include "input.h"
#include "dx12render.h"
#include "console.h"
#include <chrono>
#include <string>

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

void Core::messageCallback(WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	if (type == WINDOW_MESSAGE::SIZE)
		renderer->RecreateBuffers(param1, param2);
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

void Core::sMessageCallback(WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	core__->messageCallback(type, param1, param2, pData);
}

Core::Core()
{
	core__ = this;
}

void Core::Init(INIT_FLAGS flags)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	if (flags & INIT_FLAGS::SHOW_CONSOLE)
	{
		console = new Console;
		console->Create();
	}

	window = new MainWindow(&Core::sMainLoop);
	window->Create();
	window->AddMessageCallback(sMessageCallback);
	window->SetCaption(L"Test DX12");

	input = new Input;
	input->Init();

	renderer = new Dx12CoreRenderer;
	renderer->Init(window->handle());
	
	onInit.Invoke();
}

void Core::Free()
{
	onFree.Invoke();

	renderer->Free();
	delete renderer;

	if (console)
	{
		console->Destroy();
		delete console;
	}
	window->Destroy();
	delete window;

	input->Free();
	delete input;
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
