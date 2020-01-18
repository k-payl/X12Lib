#pragma once
#include "common.h"

extern Core* core__;
#define CORE core__

enum class INIT_FLAGS
{
	NONE = 0,
	SHOW_CONSOLE = 1 << 1,
};
inline bool operator &(INIT_FLAGS a, INIT_FLAGS b)
{
    return (static_cast<int>(a) & static_cast<int>(b));
}

class Core
{
	MainWindow* window{};
	Dx12CoreRenderer* renderer{};
	Input* input;
	Console* console;

	Signal<> onRender;
	Signal<> onInit;
	Signal<> onFree;
	Signal<float> onUpdate;

	void setWindowCaption(int is_paused, int fps);
	void engineUpdate();
	void mainLoop();
	void messageCallback(WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData);

	static void sMainLoop();
	static void sMessageCallback(WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData);

public:
	Core();

	float dt{};
	int fps{};
	int64_t frame{};

	void Init(INIT_FLAGS flags = INIT_FLAGS::NONE);
	void Free();
	void Start();
	void AddRenderProcedure(RenderProcedure fn);
	void AddInitProcedure(InitProcedure fn);
	void AddUpdateProcedure(UpdateProcedure fn);
	void AddFreeProcedure(FreeProcedure fn);

	Dx12CoreRenderer* GetCoreRenderer() const { return renderer; }
	MainWindow* GetWindow() { return window; }
	Input* GetInput() { return input; }

	void Log(const char* str);
	void LogProfiler(const char* key, const char* value);
	void LogProfiler(const char* key, float value);
	void LogProfiler(const char* key, int value);
};

