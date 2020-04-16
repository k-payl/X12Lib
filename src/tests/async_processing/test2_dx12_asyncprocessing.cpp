#include <windows.h>
#include <commctrl.h>
#include "resource.h"
#include <thread>
#include <chrono>
#include <condition_variable>
#include <queue>

using namespace std;

static HINSTANCE hInstance;
static HWND hWnd;
static HWND hWndButton;
static HWND hWndProgressBar;
static HWND hWndLabel;

static bool isexit;
static std::mutex mutex_;
static condition_variable var;
static queue<string> tasks;

void setProgress(int i);
void setMessage(const char* msg);
std::wstring ConvertFromUtf8ToUtf16(const char* str);

void processGPUImagesThread()
{
	while (true)
	{
		queue<string> tasks_thread;
		{
			std::unique_lock<std::mutex> lck(mutex_);
			var.wait(lck, [] {return isexit || !tasks.empty(); });
			tasks_thread = std::move(tasks);
			tasks = {};
		}

		setProgress(0);
		setMessage("Start processing");

		size_t task_num = tasks_thread.size();
		int task_processed = 0;

		while (!tasks_thread.empty())
		{
			const auto val = tasks_thread.front();
			tasks_thread.pop();
		
			std::this_thread::sleep_for(100ms);
		
			setMessage(val.c_str());
		
			float progress = 100.0f * float(++task_processed) / task_num;
			setProgress(int(progress));
			setMessage(val.c_str());
		}

		if (isexit)
			break;
	}
}

void addTasks(const vector<string>& tasks_)
{
	std::lock_guard<std::mutex> L{ mutex_ };

	for (auto& t : tasks_)
		tasks.push(t);

	// Tell the worker
	var.notify_one();
}

void setProgress(int i)
{
	SendMessage(hWndProgressBar, PBM_SETPOS, i, 0);
	UpdateWindow(hWndProgressBar);
}

void setMessage(const char* msg)
{
	auto name = ConvertFromUtf8ToUtf16(msg);
	SetWindowText(hWndLabel, name.c_str());
	UpdateWindow(hWndLabel);
}

void addDefaultTasks()
{
	addTasks({ "aaaa", "bbb", "ccc", "ddd" });
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_PAINT:
			
			break;

		case WM_KEYUP:
		{
			if (wParam == 27)
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			else if (wParam == 116)
				addDefaultTasks();
		}
		break;

		case WM_CREATE:
		{
			hWndButton = CreateWindowEx(0, L"button", L"Start", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 200, 20, hWnd,
				(HMENU)IDB_TEST, hInstance, NULL);

			hWndProgressBar = CreateWindowEx(0, PROGRESS_CLASS, (LPCWSTR)NULL, WS_VISIBLE | WS_CHILD, 10, 40, 200, 20,
											 hWnd, (HMENU)IDPB_PROGRESS_BAR, hInstance, NULL);

			hWndLabel = CreateWindowEx(0, L"static", L"ST_U", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
									 10, 70, 200, 20, hWnd, (HMENU)(501), hInstance, NULL);

			SendMessage(hWndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
			SendMessage(hWndProgressBar, PBM_SETSTEP, (WPARAM)1, 0);
		}
		break;

		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case IDB_TEST:
				{
					switch (HIWORD(wParam))
					{
						case BN_CLICKED:
						{
							addDefaultTasks();
						}
						break;
					}
				}
				break;
			}
			return 0;
		}
		break;

		case WM_CLOSE:
			DestroyWindow(hWnd);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return (DefWindowProc(hWnd, Msg, wParam, lParam));
	}

	return 0;
}

int centerWindow(RECT& rect, HWND parent_window, int width, int height)
{
	GetClientRect(parent_window, &rect);
	rect.left = (rect.right / 2) - (width / 2);
	rect.top = (rect.bottom / 2) - (height / 2);
	return 0;
}

INT WINAPI WinMain(HINSTANCE  hInstance_ , HINSTANCE  hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	hInstance = hInstance_;

	InitCommonControls();

	RECT rect;
	const int width = 240;
	const int height = 170;
	centerWindow(rect, GetDesktopWindow(), width, height);

	WNDCLASSEX wc;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = (WNDPROC)WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"a";

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, L"Failed To Register The Window Class.", L"Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	hWnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"a", L"Progress Bars", WS_OVERLAPPEDWINDOW, rect.left, rect.top,
		width, height, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		MessageBox(NULL, L"Window Creation Failed.", L"Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	std::thread t = thread(processGPUImagesThread);

	MSG Msg;

	while (GetMessage(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	{
		std::lock_guard<std::mutex> lck(mutex_);
		isexit = true;
		var.notify_one();
	}

	t.join();

	return (int)Msg.wParam;
}

std::wstring ConvertFromUtf8ToUtf16(const char* str)
{
	std::wstring res;
	int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
	if (size > 0)
	{
		std::vector<wchar_t> buffer(size);
		MultiByteToWideChar(CP_UTF8, 0, str, -1, &buffer[0], size);
		res.assign(buffer.begin(), buffer.end() - 1);
	}
	return res;
}
