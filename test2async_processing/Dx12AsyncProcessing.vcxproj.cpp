
// aero style
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#include <windows.h>
#include <commctrl.h>
#include "resource.h"

HWND hWndButton;
HWND hWndProgressBar;

LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
		case WM_CREATE:
		{
			hWndButton = CreateWindowEx(0,L"BUTTON", L"Start", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, 100, 20, hWnd,
				(HMENU)IDB_TEST, (HINSTANCE)GetModuleHandle(NULL),NULL);

			hWndProgressBar = CreateWindowEx(0, PROGRESS_CLASS, (LPCWSTR)NULL, WS_VISIBLE | WS_CHILD, 10, 50, 200, 20,
				hWnd, (HMENU)IDPB_PROGRESS_BAR, (HINSTANCE)GetModuleHandle(NULL), NULL);

			assert(hWndProgressBar);
				MessageBox(NULL, L"Progress Bar Failed.", L"Error", MB_OK | MB_ICONERROR);
			
			SendMessage(hWndProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 9));
			SendMessage(hWndProgressBar, PBM_SETSTEP, (WPARAM)1, 0);
		}
		break;

		case WM_TIMER:
		{
			switch ((UINT)wParam)
			{
				case IDT_TIMER:
				{
					KillTimer(hWnd, IDT_TIMER);
					KillTimer(hWnd, IDT_PROGRESS_TIMER);
					//MessageBox(NULL, L"Timer Activated", L"Success", MB_OK | MB_ICONINFORMATION);
				}
				break;

				case IDT_PROGRESS_TIMER:
					SendMessage(hWndProgressBar, PBM_STEPIT, 0, 0);
					break;
			}
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
							if (SetTimer(hWnd, IDT_TIMER, 5000, (TIMERPROC)NULL))
							{
								SetTimer(hWnd, IDT_PROGRESS_TIMER, 500, (TIMERPROC)NULL);
								//MessageBox(NULL, L"Timer set for 5 seconds", L"Success", MB_OK | MB_ICONINFORMATION);
							}
							else
								MessageBox(NULL, L"Timer failed", L"Error", MB_OK | MB_ICONERROR);
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



INT WINAPI WinMain(HINSTANCE  hInstance, HINSTANCE  hPrevInstance, LPSTR lpCmdLine, INT nCmdShow)
{
	InitCommonControls();

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

	HWND hWnd;
	hWnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		L"a",
		L"Progress Bars",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		240,
		120,
		NULL,
		NULL,
		hInstance,
		NULL);

	if (!hWnd)
	{
		MessageBox(NULL, L"Window Creation Failed.", L"Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);
	MSG Msg;

	while (GetMessage(&Msg, NULL, 0, 0))
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}

	return Msg.wParam;
}
