#pragma once
#include "common.h"

namespace engine
{
	class ConsoleBoolVariable
	{
	public:
		ConsoleBoolVariable(std::string name_, bool value_);
		std::string name;
		std::wstring wname;
		bool value{};
	};

	struct _ConsoleCommand
	{
		std::string name;
		ConsoleCallback callback;
	};

	void RegisterConsoleVariable(const ConsoleBoolVariable* v);
	void RegisterConsoleCommand(const std::string& name, ConsoleCallback callback);

	std::wstring ConvertFromUtf8ToUtf16(const std::string& str);

	inline std::string to_stringf(const std::string& str) { return str; }
	std::string to_stringf(int i);
	std::string to_stringf(float f);
	std::string to_stringf(double d);
	std::string to_stringf(const math::vec3& v);
	std::string to_stringf(const math::vec4& v);
	std::string to_stringf(const math::mat3& m);
	std::string to_stringf(const math::mat4& m);

	class Console
	{
		void CreateHintWindow(const std::string& word_);
		void DestroyHintWindow();
		void SetCursorToEnd();
		void CompleteNextHistory();
		void CompletePrevHistory();
		void CompleteCommand(std::string& tmp);
		void SetEditText(const wchar_t* str);
		void PrintAllRegisteredVariables();
		void PrintAllRegisteredCommands();

	public:
		struct Hint
		{
			HWND hListBox_{};
			int32_t count{};
			int32_t selected{};
			std::vector<int> commandIndexes;

			void _select();
			void SelectNext();
			void SelectPrev();
		}*hint;

		HWND hWnd_{};
		HWND hMemo_{};
		HWND hEdit_{};
		HFONT hFont_;

		int	x_, y_, width_, height_;
		int	prevLineSize_;
		int isVisible{};
		void* oldEditProc;

		std::vector<std::wstring> history;
		int32_t historyIndex{-1};

		static WNDPROC oldEditWndProc;

		static LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		static LRESULT CALLBACK _s_EditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
		static LRESULT CALLBACK _s_WndEditProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

		void Create();
		void Destroy();

		void log(const char* str);
		void OutputTxt(const char* pStr);
		void Show();
		int IsVisible() { return isVisible; }
		void Hide();
		void Toggle();
		void BringToFront();

		void ExecuteCommand(const wchar_t* str);
	};
}

