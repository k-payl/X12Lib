#include "pch.h"

#define _CRT_SECURE_NO_WARNINGS
#include "console.h"
#include <string>

using std::string;
using std::string;
using std::unordered_map;
using std::wstring;
using std::to_string;

static HWND hwndEdit0;
HWND Console::hwndEdit1;
unordered_map<string, string> Console::_profiler;

HWND create_edit(HWND parentHwnd, int x, int w, int h)
{
	return CreateWindowEx(
		0, TEXT("EDIT"),   // predefined class 
		NULL,         // no window title 
		WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
		x, 0, w, h,   // set size in WM_SIZE message 
		parentHwnd,         // parent window 
		(HMENU)nullptr,   // edit control ID 
		(HINSTANCE)GetModuleHandle(NULL),
		NULL);        // pointer not needed 
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		RECT r;
		GetWindowRect(hwnd, &r);
		int w = r.right - r.left;
		int h = r.bottom - r.top;

		hwndEdit0 = create_edit(hwnd, 0, w / 2, h);
		Console::hwndEdit1 = create_edit(hwnd, w / 2, w / 2, h);

	}
	break;

	case WM_SIZE:
	{
		int w = LOWORD(lParam);
		int h = HIWORD(lParam);
		MoveWindow(hwndEdit0, 0, 0, w / 2, h, true);
		MoveWindow(Console::hwndEdit1, w / 2, 0, w / 2, h, true);
	}
	break;

	//case  WM_CLOSE:
	//{
	//	PostQuitMessage(0);
	//	return 0;
	//}
	//break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

void Console::Create()
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = wndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = GetModuleHandle(NULL);
	wcex.hIcon = nullptr;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"My window class";
	wcex.hIconSm = nullptr;

	if (!RegisterClassExW(&wcex)) return;

	hwnd = CreateWindowEx(WS_EX_TOOLWINDOW /*0*/, TEXT("My window class"), TEXT("Log"),
		WS_OVERLAPPEDWINDOW, 50, 50, 850, 600, nullptr, nullptr, GetModuleHandle(NULL), nullptr);

	ShowWindow(hwnd, SW_SHOW);

}

void Console::Destroy()
{
	DestroyWindow(hwnd);
}

wstring ConvertFromUtf8ToUtf16(const string& str)
{
	wstring res;
	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, 0, 0);
	if (size > 0)
	{
		std::vector<wchar_t> buffer(size);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &buffer[0], size);
		res.assign(buffer.begin(), buffer.end() - 1);
	}

	return res;
}

void Console::log(const char* str)
{
	string _str(str);
	_str += "\r\n\n";
	wstring wstr = ConvertFromUtf8ToUtf16(_str);
	SendMessage(hwndEdit0, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(wstr.c_str()));
}




string to_stringf(int i)
{
	return std::to_string(i);
}

string to_stringf(float f)
{
	char buf[20];
	sprintf(buf, "%.5f", f);
	return string(buf);
}

string to_stringf(double d)
{
	char buf[20];
	sprintf(buf, "%.2lf", d);
	return string(buf);
}

string to_stringf(const vec3& v)
{
	return "(" + to_stringf(v.x) + ", " + to_stringf(v.y) + ", " + to_stringf(v.z) + ")";
}

string to_stringf(const vec4& v)
{
	return "(" + to_stringf(v.x) + ", " + to_stringf(v.y) + ", " + to_stringf(v.z) + ", " + to_stringf(v.w) + ")";
}

template<typename T, size_t M, size_t N>
string _to_string_mat(const T& m)
{
	string res("\r\n\n\t");
	for (int i = 0; i < M; i++)
	{
		for (int j = 0; j < N; j++)
		{
			res += to_stringf(m.el_2D[i][j]);
			if (j != N - 1) res += ", ";
		}
		if (i != M - 1) res += "\r\n\n\t";
	}
	return res;
}

string to_stringf(const mat3& m)
{
	return _to_string_mat<mat3, 3, 3>(m);
}

string to_stringf(const mat4& m)
{
	return _to_string_mat<mat4, 4, 4>(m);
}
