#pragma once
#include "common.h"

class GpuProfiler;
class FileSystem;

extern Core* core__;
#define CORE core__

enum class INIT_FLAGS
{
	NONE = 0,
	NO_CONSOLE = 1 << 1,
	NO_WINDOW = 1 << 2,
	NO_INPUT = 1 << 2,
	BUILT_IN_DX12_RENDERER = 1 << 3,
};
inline bool operator&(INIT_FLAGS a, INIT_FLAGS b)
{
    return (static_cast<int>(a) & static_cast<int>(b));
}

inline INIT_FLAGS operator|(INIT_FLAGS a, INIT_FLAGS b)
{
    return static_cast<INIT_FLAGS>(static_cast<int>(a) | static_cast<int>(b));
}

class Core
{
	MainWindow* window{};
	x12::Dx12CoreRenderer* renderer{};
	Input* input{};
	Console* console{};
	GpuProfiler* renderprofiler{};
	GpuProfiler* memoryprofiler{};
	FileSystem* fs{};

	Signal<> onRender;
	Signal<> onInit;
	Signal<> onFree;
	Signal<float> onUpdate;

	void setWindowCaption(int is_paused, int fps);
	void engineUpdate();
	void mainLoop();
	void messageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData);

	static void sMainLoop();
	static void sMessageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData);

public:
	Core();

	float dt{};
	int fps{};
	int64_t frame{};

	void Init(GpuProfiler* gpuprofiler_, InitRendererProcedure initRenderer, INIT_FLAGS flags = INIT_FLAGS::NONE);
	void Free();
	void Start();
	void RenderProfiler(float gpu_, float cpu_);

	void AddRenderProcedure(RenderProcedure fn);
	void AddInitProcedure(InitProcedure fn);
	void AddUpdateProcedure(UpdateProcedure fn);
	void AddFreeProcedure(FreeProcedure fn);

	x12::Dx12CoreRenderer* GetCoreRenderer() const { return renderer; }
	MainWindow* GetWindow() { return window; }
	Input* GetInput() { return input; }
	FileSystem* GetFS() { return fs; }

	void Log(const char* str);
	void LogProfiler(const char* key, const char* value);
	void LogProfiler(const char* key, float value);
	void LogProfiler(const char* key, int value);
};

