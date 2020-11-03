#define _CRT_SECURE_NO_WARNINGS
#include "console.h"

#include <string>
#include <sstream>

using std::string;
using std::string;
using std::unordered_map;
using std::wstring;
using std::to_string;
using namespace engine;

WNDPROC Console::oldEditWndProc;
static HWND hwndEdit0;

#define C_WND_EDIT_HEIGHT 16
#define C_WND_LISTBOX_HEIGHT 100

// All comands aand variables
namespace
{
	std::vector<const ConsoleBoolVariable*>& ConsoleVariables()
	{
		static std::vector<const ConsoleBoolVariable*> variables_;
		return variables_;
	}

	std::vector<_ConsoleCommand>& ConsoleCommands()
	{
		static std::vector<_ConsoleCommand> cmds_;
		return cmds_;
	}

	const ConsoleBoolVariable* FindConsoleVariable(const std::string& name)
	{
		auto it = std::find_if(ConsoleVariables().begin(), ConsoleVariables().end(), [name](const ConsoleBoolVariable* r)->bool {return r->name == name; });
		if (it == ConsoleVariables().end())
			return nullptr;
		return *it;
	}

	const _ConsoleCommand* FindCommand(const std::string& name)
	{
		auto it = std::find_if(ConsoleCommands().begin(), ConsoleCommands().end(), [name](const _ConsoleCommand& r)->bool {return r.name == name; });
		if (it == ConsoleCommands().end())
			return nullptr;
		return &*it;
	}
}

engine::ConsoleBoolVariable::ConsoleBoolVariable(std::string name_, bool value_) :
	name(name_), value(value_)
{
	wname = ConvertFromUtf8ToUtf16(name_);
	RegisterConsoleVariable(this);
}

void engine::RegisterConsoleVariable(const ConsoleBoolVariable* cmd)
{
	if (FindConsoleVariable(cmd->name) != nullptr)
		return;

	ConsoleVariables().push_back(cmd);
}

void engine::RegisterConsoleCommand(const std::string& name, ConsoleCallback callback)
{
	if (FindCommand(name) != nullptr)
		return;

	ConsoleCommands().push_back({ name, callback });
}

void engine::Console::PrintAllRegisteredVariables()
{
	OutputTxt("registered variables:");
	for (auto* c : ConsoleVariables())
	{
		OutputTxt(c->name.c_str());
	}
}
void engine::Console::PrintAllRegisteredCommands()
{
	OutputTxt("registered commands:");
	for (auto& c : ConsoleCommands())
	{
		OutputTxt(c.name.c_str());
	}
}

LRESULT CALLBACK Console::wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Console* this_ptr = (Console*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (message)
	{

	case WM_SIZE:
	{
		this_ptr->width_ = LOWORD(lParam);
		this_ptr->height_ = HIWORD(lParam);
		RECT rect;
		GetClientRect(this_ptr->hWnd_, &rect);
		MoveWindow(this_ptr->hMemo_, 0, 0, rect.right, rect.bottom - 0, true);
		MoveWindow(this_ptr->hMemo_, 0, 0, rect.right, rect.bottom - C_WND_EDIT_HEIGHT, true);
		MoveWindow(this_ptr->hEdit_, 0, rect.bottom - C_WND_EDIT_HEIGHT, rect.right, C_WND_EDIT_HEIGHT, true);
		//MoveWindow(this_ptr->hListBox_, 0, rect.bottom - C_WND_LISTBOX_HEIGHT - C_WND_EDIT_HEIGHT, rect.right, C_WND_LISTBOX_HEIGHT, true);
	}
	break;

	case WM_KEYUP:
		switch (wParam)
		{
			case 192: //tilda
				this_ptr->Toggle();
				SetWindowText(this_ptr->hEdit_, TEXT(""));
				break;
		}
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}

	return 0;
}

void Console::Create()
{
	HINSTANCE _hInst = GetModuleHandle(NULL);

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)Console::wndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = _hInst;
	wcex.hIcon = LoadIcon(_hInst, (LPCTSTR)"");
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_GRAYTEXT);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = TEXT("ConsoleClass");
	wcex.hIconSm = LoadIcon(_hInst, (LPCTSTR)"");

	if (FindAtom(TEXT("ConsoleClass")) == NULL && !RegisterClassEx(&wcex))
	{
		MessageBox(NULL, TEXT("Failed to register console class!"), TEXT("DGLE Console"), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return;
	}

	HWND h = 0;

	hWnd_ = CreateWindowEx(WS_EX_TOOLWINDOW, TEXT("ConsoleClass"), TEXT("Console"),
						   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX,
						   1160, 200, 350, 500, h, NULL, _hInst, NULL);

	if (!hWnd_)
	{
		MessageBox(NULL, TEXT("Failed to create console window!"), TEXT("Console"), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return;
	}

	SetWindowLongPtr(hWnd_, GWLP_USERDATA, (LONG_PTR)this);

	RECT client_rect;
	GetClientRect(hWnd_, &client_rect);
	hMemo_ = CreateWindow(TEXT("EDIT"), TEXT(""),
						  WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL |
						  ES_MULTILINE | ES_READONLY,
						  0, 0, client_rect.right - client_rect.left, client_rect.bottom - client_rect.top, hWnd_, 0, 0, NULL);

	oldEditWndProc = (WNDPROC)SetWindowLongPtr(hMemo_, GWLP_WNDPROC, (LONG_PTR)(WNDPROC)Console::_s_EditProc);
	SetWindowLongPtr(hMemo_, GWLP_USERDATA, (LONG_PTR)this);

	//SetWindowText(_hMemo, L"Console created\r\n");	

	LOGFONT LF = {12, 0, 0, 0, 0, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, TEXT("Lucida Console")};
	hFont_ = CreateFontIndirect(&LF);

	SendMessage(hMemo_, WM_SETFONT, (WPARAM)hFont_, MAKELPARAM(TRUE, 0));
	SendMessage(hMemo_, EM_LIMITTEXT, 200000, 0);

	hEdit_ = CreateWindow(TEXT("EDIT"), TEXT(""), WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd_, 0, 0, NULL);
	oldEditProc = (void*)SetWindowLongPtr(hEdit_, GWLP_WNDPROC, (LONG_PTR)(WNDPROC)Console::_s_WndEditProc);
	SendMessage(hEdit_, WM_SETFONT, (WPARAM)hFont_, MAKELPARAM(TRUE, 0));

	SetWindowLongPtr(hEdit_, GWLP_USERDATA, (LONG_PTR)this);

	Show();
}

void Console::Show()
{
	isVisible = 1;
	ShowWindow(hWnd_, SW_SHOW);
	UpdateWindow(hWnd_);
}

void Console::Hide()
{
	BringToFront();
	isVisible = 0;
	ShowWindow(hWnd_, SW_HIDE);
}

void engine::Console::Toggle()
{
	if (isVisible)
		Hide();
	else
		Show();
}

void Console::BringToFront()
{
	SetActiveWindow(hWnd_);
}

void engine::Console::CreateHintWindow(const std::string& word_)
{
	if (hint)
		return;

	size_t count = 0;
	std::vector<bool> needToAdd;
	std::vector<int> indexes;
	indexes.reserve(ConsoleVariables().size());
	needToAdd.resize(ConsoleVariables().size());

	for (int i = 0; i < ConsoleVariables().size(); ++i)
	{
		const ConsoleBoolVariable* cmd = ConsoleVariables()[i];

		if (cmd->name.find(word_)  != string::npos)
		{
			++count;
			needToAdd[i] = true;
			indexes.push_back(i);
		}
	}

	if (count == 0)
		return;

	hint = new Console::Hint;
	hint->commandIndexes = std::move(indexes);
	hint->count = (int32_t)count;
	hint->hListBox_ = CreateWindow(TEXT("LISTBOX"), TEXT(""), WS_CHILD | WS_VISIBLE, 0, 0, 400, 17 * (int)count, hMemo_, 0, 0, NULL);

	for (int i = 0; i < ConsoleVariables().size(); ++i)
	{
		if (needToAdd[i])
			SendMessage(hint->hListBox_, LB_ADDSTRING, 0, (LPARAM)(LPSTR)ConsoleVariables()[i]->wname.c_str());
	}

	SendMessage(hint->hListBox_, LB_SETCURSEL, (WPARAM)0, (LPARAM)-1);
}

void engine::Console::DestroyHintWindow()
{
	if (hint)
	{
		SendMessage(hint->hListBox_, LB_RESETCONTENT, 0, 0);
		DestroyWindow(hint->hListBox_);
		delete hint;
		hint = {};
	}
}

void engine::Console::ExecuteCommand(const wchar_t* str)
{
	string tmp = ConvertFromUtf16ToUtf8(str);
	OutputTxt(tmp.c_str());

	int value = -1;
	string name_;
	std::stringstream ss(tmp);

	ss >> name_;
	ss >> value;

	if (ConsoleBoolVariable* v = const_cast<ConsoleBoolVariable*>(FindConsoleVariable(name_)))
	{
		v->value = value > 0;
	}
	else if (_ConsoleCommand *cmd = const_cast<_ConsoleCommand*>(FindCommand(name_)))
	{
		cmd->callback();
	}
	else
	{
		OutputTxt(("Unknow command '" + name_ + "'").c_str());
		PrintAllRegisteredVariables();
		PrintAllRegisteredCommands();
	}

	if (history.empty() || history.back() != str)
		history.push_back(str);

	historyIndex = -1;
}

void engine::Console::SetCursorToEnd()
{
	DWORD TextSize;
	TextSize = GetWindowTextLength(hEdit_);
	SendMessage(hEdit_, EM_SETSEL, TextSize, TextSize);
};

void engine::Console::SetEditText(const wchar_t* str)
{
	SetWindowText(hEdit_, str);
	SetCursorToEnd();
}

void engine::Console::CompleteNextHistory()
{
	if (history.empty())
		return;

	if (historyIndex == -1)
		historyIndex = (int32_t)history.size() - 1;
	else if (history.size() > 1)
	{
		if (historyIndex == 0)
			historyIndex = (int32_t)history.size() - 1;
		else
			historyIndex--;
	}

	SetEditText(history[historyIndex].c_str());
}

void engine::Console::CompletePrevHistory()
{
	if (history.empty())
		return;

	if (historyIndex == -1)
		historyIndex = 0;
	else if (history.size() > 1)
	{
		if (historyIndex == history.size() - 1)
			historyIndex = 0;
		else
			historyIndex++;
	}

	SetEditText(history[historyIndex].c_str());
}

LRESULT CALLBACK Console::_s_WndEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Console* this_ptr = (Console*)GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);

	wchar_t wtmp[300];

	switch (message)
	{
		case WM_KEYUP:
		{
			if ((0x30 <= wParam && wParam <= 0x5A) || // ascii
				(VK_OEM_PLUS <= wParam && wParam <= VK_OEM_PERIOD) ||
				wParam == VK_SPACE ||
				wParam == 8 /*return*/)
				if (GetWindowTextLength(this_ptr->hEdit_) > 0)
				{
					this_ptr->DestroyHintWindow();

					GetWindowText(this_ptr->hEdit_, wtmp, 300);
					string tmp = ConvertFromUtf16ToUtf8(wtmp);

					this_ptr->CreateHintWindow(tmp);
				}

			switch (wParam)
			{
				case 192: //tilda
					this_ptr->Toggle();
					SetWindowText(this_ptr->hEdit_, L"");
					break;

					//case 38: //up
					//{
					//	if (this_ptr->_completion_cmd_idx <= 0)
					//		break;

					//	if (this_ptr->_completion_cmd_idx == this_ptr->_completion_commands.size())
					//	{
					//		GetWindowText(this_ptr->_hEdit, wtmp, 300);
					//		this_ptr->_typed_command = NativeToUTF8(wtmp);
					//	}

					//	this_ptr->_completion_cmd_idx--;

					//	string mem_cmd = this_ptr->_completion_commands[this_ptr->_completion_cmd_idx];
					//	mstring wmem_cmd = UTF8ToNative(mem_cmd);
					//	SetWindowText(this_ptr->_hEdit, wmem_cmd.c_str());
					//	SetCursorToEnd();
					//}
					break;

				case 8:
					if (GetWindowTextLength(this_ptr->hEdit_) == 0)
						this_ptr->DestroyHintWindow();
					break;

					//case 40: // down
					//{
					//	if (this_ptr->_completion_cmd_idx > (int) (this_ptr->_completion_commands.size() - 1) || this_ptr->_completion_commands.empty())				
					//		break;

					//	if (this_ptr->_completion_cmd_idx == this_ptr->_completion_commands.size() - 1)
					//	{
					//		wstring wtyped = UTF8ToNative(this_ptr->_typed_command);
					//		this_ptr->_typed_command = "";
					//		SetWindowText(this_ptr->_hEdit, wtyped.c_str());
					//		SetCursorToEnd();

					//		
					//	} else
					//	{
					//		string mem_cmd = this_ptr->_completion_commands[this_ptr->_completion_cmd_idx + 1];
					//		mstring wmem_cmd = UTF8ToNative(mem_cmd);
					//		SetWindowText(this_ptr->_hEdit, wmem_cmd.c_str());
					//		SetCursorToEnd();
					//	}

					//	this_ptr->_completion_cmd_idx++;
					//}
					break;
			}
		}
		break;
		case WM_CHAR:
			if (wParam == 96 /*tilda*/)
				break;

			if (GetWindowTextLength(this_ptr->hEdit_) > 0)
			{
				if (wParam == 9 /*tab*/)
				{
					GetWindowText(this_ptr->hEdit_, wtmp, 300);
					string tmp = ConvertFromUtf16ToUtf8(wtmp);
					this_ptr->CompleteCommand(tmp);
					break;
				}
				else
					if (wParam == 13 /* enter */)
					{
						GetWindowText(this_ptr->hEdit_, wtmp, 300);
						SetWindowText(this_ptr->hEdit_, NULL);
						this_ptr->ExecuteCommand(wtmp);
						this_ptr->DestroyHintWindow();
						break;
					}
			}
			goto callDefWndPros;

		case WM_KEYDOWN:
		{
			GetWindowText(this_ptr->hEdit_, wtmp, 300);
			string tmp = ConvertFromUtf16ToUtf8(wtmp);

			if (this_ptr->hint)
			{
				if (wParam == 38)
					this_ptr->hint->SelectNext();
				else if (wParam == 40)
					this_ptr->hint->SelectPrev();
			}
			else
			{
				if (wParam == 38)
				{
					this_ptr->CompleteNextHistory();
				}
				else if (wParam == 40)
				{
					this_ptr->CompletePrevHistory();
				}
			}
			break;
				goto callDefWndPros;
		}
		break;
		default:
			goto callDefWndPros;
	}

	return 0;

	DefWindowProc(hWnd, message, wParam, lParam);

callDefWndPros:
	return CallWindowProc((WNDPROC)this_ptr->oldEditProc, hWnd, message, wParam, lParam);
}

void engine::Console::CompleteCommand(std::string& tmp)
{
	if (hint)
	{
		const ConsoleBoolVariable* cmd = ConsoleVariables()[hint->selected];
		if (tmp != cmd->name)
			SetEditText(cmd->wname.c_str());
		else
		{
			hint->SelectPrev();
			const ConsoleBoolVariable* cmd = ConsoleVariables()[hint->selected];
		SetEditText(cmd->wname.c_str());
		}
	}
}

void Console::OutputTxt(const char* pStr)
{
	if (!hMemo_)
		return;

	int cur_l = GetWindowTextLength(hMemo_);

	SendMessage(hMemo_, EM_SETSEL, cur_l, cur_l);
	SendMessage(hMemo_, EM_REPLACESEL, false, (LPARAM)(std::wstring(ConvertFromUtf8ToUtf16(pStr)) + std::wstring(L"\r\n")).c_str());

	prevLineSize_ = (int)strlen(pStr);
}

LRESULT CALLBACK Console::_s_EditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	Console* this_ptr = (Console*)GetWindowLongPtr(GetParent(hWnd), GWLP_USERDATA);

	//wchar_t wtmp[300];

	auto SetCursorToEnd = [=]()
	{
		DWORD TextSize;
		TextSize = GetWindowTextLength(this_ptr->hEdit_);
		SendMessage(this_ptr->hEdit_, EM_SETSEL, TextSize, TextSize);
	};

	switch (uMsg)
	{
		case WM_KEYUP:
			switch (wParam)
			{
				case 192: //tilda
					this_ptr->Toggle();
					SetWindowText(this_ptr->hEdit_, L"");
					break;
			}
			break;
	}
	//if ((uMsg == WM_KEYUP) && (wParam == 192))
	//{
	//	Console* this_ptr = (Window*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	//	if (this_ptr->isVisible)
	//		this_ptr->Hide();
	//	else
	//		this_ptr->Show();

	//	return 0;
	//}
	return CallWindowProc(oldEditWndProc, hWnd, uMsg, wParam, lParam);
}

void Console::Destroy()
{
	DestroyWindow(hWnd_);
}

void Console::log(const char* str)
{
	string _str(str);
	_str += "\r\n\n";
	wstring wstr = ConvertFromUtf8ToUtf16(_str);
	SendMessage(hwndEdit0, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(wstr.c_str()));
}

string engine::to_stringf(int i)
{
	return std::to_string(i);
}

string engine::to_stringf(float f)
{
	char buf[20];
	sprintf(buf, "%.5f", f);
	return string(buf);
}

string engine::to_stringf(double d)
{
	char buf[20];
	sprintf(buf, "%.2lf", d);
	return string(buf);
}

string engine::to_stringf(const math::vec3& v)
{
	return "(" + engine::to_stringf(v.x) + ", " + engine::to_stringf(v.y) + ", " + engine::to_stringf(v.z) + ")";
}

string engine::to_stringf(const math::vec4& v)
{
	return "(" + engine::to_stringf(v.x) + ", " + engine::to_stringf(v.y) + ", " + engine::to_stringf(v.z) + ", " + engine::to_stringf(v.w) + ")";
}

namespace
{
	template<typename T, size_t M, size_t N>
	string _to_string_mat(const T& m)
	{
		string res("\r\n\n\t");
		for (int i = 0; i < M; i++)
		{
			for (int j = 0; j < N; j++)
			{
				res += engine::to_stringf(m.el_2D[i][j]);
				if (j != N - 1) res += ", ";
			}
			if (i != M - 1) res += "\r\n\n\t";
		}
		return res;
	}
}
string engine::to_stringf(const math::mat3& m)
{
	return _to_string_mat<math::mat3, 3, 3>(m);
}

string engine::to_stringf(const math::mat4& m)
{
	return _to_string_mat<math::mat4, 4, 4>(m);
}

void engine::Console::Hint::_select()
{
	SendMessage(hListBox_, LB_SETCURSEL, (WPARAM)selected, (LPARAM)-1);
}

void engine::Console::Hint::SelectNext()
{
	if (selected == 0)
		selected = count - 1;
	else
		--selected;

	_select();
}

void engine::Console::Hint::SelectPrev()
{
	if (selected == count - 1)
		selected = 0;
	else
		++selected;

	_select();
}
