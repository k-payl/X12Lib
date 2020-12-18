#pragma once
#include "common.h"
#include "icorerender.h"
#include "console.h"

extern engine::Core* core__;

namespace engine
{
	X12_API Core*					CreateCore();
	X12_API void					DestroyCore(Core* core);

	X12_API x12::ICoreRenderer*		GetCoreRenderer();
	X12_API MainWindow*				GetWindow();
	X12_API Input*					GetInput();
	X12_API FileSystem*				GetFS();
	X12_API SceneManager*			GetSceneManager();
	X12_API ResourceManager*		GetResourceManager();
	X12_API MaterialManager*		GetMaterialManager();
	X12_API Console*				GetConsole();
	X12_API Renderer*				GetRenderer();
	X12_API void					Log(const char* str);

	enum class INIT_FLAGS
	{
		NONE,
		NO_CONSOLE = 1 << 0,
		NO_WINDOW = 1 << 1,
		NO_INPUT = 1 << 2,
		DIRECTX12_RENDERER = 1 << 3,
		VULKAN_RENDERER = 1 << 4,
		HIGH_LEVEL_RENDERER = 1 << 5,
	};
	DEFINE_ENUM_OPERATORS(INIT_FLAGS)

	class Core
	{
		// Subsystems
		std::unique_ptr<MainWindow> window;
		std::unique_ptr<Input> input;
		std::unique_ptr<Console> console;
		std::unique_ptr<GpuProfiler> renderprofiler;
		std::unique_ptr<GpuProfiler> memoryprofiler;
		std::unique_ptr<FileSystem> fs;
		std::unique_ptr<Renderer> render;
		std::unique_ptr<SceneManager> sceneManager;
		std::unique_ptr<ResourceManager> resourceManager;
		std::unique_ptr<MaterialManager> matManager;
		std::unique_ptr<x12::ICoreRenderer> renderer;

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

		// Pathes without end backslashes
		std::string rootPath_;
		std::string workingPath_;
		std::string dataPath_;

		X12_API void Init(const char* rootPath, INIT_FLAGS flags, GpuProfiler* gpuprofiler_ = nullptr, InitRendererProcedure initRenderer = nullptr);
		X12_API void Free();
		X12_API void Start(Camera* cam = 0);
		X12_API void AddRenderProcedure(RenderProcedure fn);
		X12_API void AddInitProcedure(InitProcedure fn);
		X12_API void AddUpdateProcedure(UpdateProcedure fn);
		X12_API void AddFreeProcedure(FreeProcedure fn);
		X12_API void Log(const char* str);
		X12_API void LogProfiler(const char* key, const char* value);
		X12_API void LogProfiler(const char* key, float value);
		X12_API void LogProfiler(const char* key, int value);

		x12::ICoreRenderer* GetCoreRenderer() const { return renderer.get(); }
		MainWindow* GetWindow() { return window.get(); }
		Input* GetInput() { return input.get(); }
		FileSystem* GetFS() { return fs.get(); }
		SceneManager* GetSceneManager() { return sceneManager.get(); }
		ResourceManager* GetResourceManager() { return resourceManager.get(); }
		MaterialManager* GetMaterialManager() { return matManager.get(); }
		Console* GetConsole() { return console.get(); }
		Renderer* GetRenderer() { return render.get(); }

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

