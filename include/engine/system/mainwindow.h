#pragma once
#include "common.h"

namespace engine
{
	class MainWindow
	{
		static MainWindow *thisPtr;

		HWND hwnd;
		void(*mainLoop)() {nullptr};
		Signal<HWND, WINDOW_MESSAGE, uint32_t, uint32_t, void*> onWindowEvent;
		int passiveMainLoop{};

		static LRESULT CALLBACK sWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

		void invokeMesage(WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void *pData);
		LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	public:

		MainWindow(void(*main_loop)());
		~MainWindow();

		HWND *handle() { return &hwnd; }

		void X12_API SendCloseMesage();
		void X12_API Create();
		void X12_API StartMainLoop();
		void X12_API Destroy();
		void X12_API GetClientSize(int& w, int& h);
		void X12_API AddMessageCallback(WindowCallback c);
		void X12_API SetCaption(const wchar_t* text);
		void X12_API SetPassiveMainLoop(int value) { passiveMainLoop = value; }
	};
}

