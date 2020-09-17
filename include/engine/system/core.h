#pragma once
#include "common.h"
#include "icorerender.h"
#include "console.h"

extern engine::Core* core__;

namespace engine
{
	X12_API Core* CreateCore();
	X12_API void DestroyCore(Core* core);

	X12_API x12::ICoreRenderer*		GetCoreRenderer();
	X12_API MainWindow*				GetWindow();
	X12_API Input*					GetInput();
	X12_API FileSystem*				GetFS();
	X12_API SceneManager*			GetSceneManager();
	X12_API ResourceManager*		GetResourceManager();
	X12_API Console*				GetConsole();
	X12_API void					Log(const char* str);

	enum class INIT_FLAGS
	{
		NONE = 0,
		NO_CONSOLE = 1 << 1,
		NO_WINDOW = 1 << 2,
		NO_INPUT = 1 << 2,
		DIRECTX12_RENDERER = 1 << 3,
		VULKAN_RENDERER = 1 << 4,
		HIGH_LEVEL_RENDER = 1 << 5,
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
		std::unique_ptr<MainWindow> window;
		std::unique_ptr<Input> input;
		std::unique_ptr<Console> console;
		std::unique_ptr<GpuProfiler> renderprofiler;
		std::unique_ptr<GpuProfiler> memoryprofiler;
		std::unique_ptr<FileSystem> fs;
		std::unique_ptr<x12::ICoreRenderer> renderer;
		std::unique_ptr<Render> render;
		std::unique_ptr<SceneManager> sceneManager;
		std::unique_ptr<ResourceManager> resourceManager;

		std::vector<intrusive_ptr<x12::ICoreQuery>> queries;

		x12::ICoreGraphicCommandList* queryCmdListsStart[QueryNum]{};
		x12::ICoreGraphicCommandList* queryCmdListsStop[QueryNum]{};

		enum COMMAND_LIST_ID
		{
			CMD_LIST_QUERY_START = 0,
			CMD_LIST_QUERY_NUM = QueryNum * 2,
			CMD_LIST_NUM
		};

		Camera* camera{};

		Signal<> onRender;
		Signal<> onInit;
		Signal<> onFree;
		Signal<float> onUpdate;

		void setWindowCaption(int is_paused, int fps);
		void engineUpdate();
		void endFrame();
		void mainLoop();
		void messageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData);

		static void sMainLoop();
		static void sMessageCallback(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData);
		void RenderProfiler(float gpu_, float cpu_, int width, int height);

	public:
		Core();
		~Core();

		float dt{};
		int fps{};
		int64_t frame{};
		int64_t frameIndex{};

		std::mutex logMtx;

		void X12_API Init(INIT_FLAGS flags, GpuProfiler* gpuprofiler_ = nullptr, InitRendererProcedure initRenderer = nullptr);
		void X12_API Free();
		void X12_API Start(Camera* cam = 0);

		void X12_API AddRenderProcedure(RenderProcedure fn);
		void X12_API AddInitProcedure(InitProcedure fn);
		void X12_API AddUpdateProcedure(UpdateProcedure fn);
		void X12_API AddFreeProcedure(FreeProcedure fn);

		void X12_API Log(const char* str);
		void X12_API LogProfiler(const char* key, const char* value);
		void X12_API LogProfiler(const char* key, float value);
		void X12_API LogProfiler(const char* key, int value);

		x12::ICoreRenderer* GetCoreRenderer() const { return renderer.get(); }
		MainWindow* GetWindow() { return window.get(); }
		Input* GetInput() { return input.get(); }
		FileSystem* GetFS() { return fs.get(); }
		SceneManager* GetSceneManager() { return sceneManager.get(); }
		ResourceManager* GetResourceManager() { return resourceManager.get(); }
		Console* GetConsole() { return console.get(); }

		template<class T, typename... Arguments>
		void _Log(T a, Arguments ...args)
		{
			std::lock_guard<std::mutex> guard(logMtx);
			
			static char logBuffer__[5000];

			if (strlen(a) > sizeof(logBuffer__))
				abort();

			sprintf(logBuffer__, a, args...);

			if (console)
				console->OutputTxt(logBuffer__);
		}

		bool isRenderProfiler{true};
	};

	template<typename... Arguments>
	void Log(Arguments ...args)
	{
		core__->_Log(args...);
	}

	template<typename... Arguments>
	void LogCritical(Arguments ...args) // TODO
	{
		core__->_Log(args...);
	}
}

