#pragma once
#include "common.h"

class MainWindow
{
	static MainWindow *thisPtr;

	HWND hwnd;
	void(*mainLoop)() {nullptr};
	Signal<WINDOW_MESSAGE, uint32_t, uint32_t, void*> onWindowEvent;
	int passiveMainLoop{};

	static LRESULT CALLBACK sWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	void invokeMesage(WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void *pData);
	LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:

	MainWindow(void(*main_loop)());
	~MainWindow();

	HWND handle() { return hwnd; }

	void SendCloseMesage();
	void Create();
	void StartMainLoop();
	void Destroy();
	void GetClientSize(int& w, int& h);
	void AddMessageCallback(WindowCallback c);
	void SetCaption(const wchar_t* text);
	void SetPassiveMainLoop(int value) { passiveMainLoop = value; }
};

