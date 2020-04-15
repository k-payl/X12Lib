#pragma once
#include "common.h"
#include <unordered_map>
#include <string>

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
public:

	HWND hwnd;
	static HWND hwndEdit1;
	static std::unordered_map<std::string, std::string> _profiler;

	void Create();
	void Destroy();

	void log(const char* str);
	
	template <typename T>
	void log_profiler(const std::string& tag, const T& arg)
	{
		auto it = _profiler.find(tag);
		if (it == _profiler.end())
		{
			_profiler.emplace(tag, to_stringf(arg));
			it = _profiler.find(tag);
		}
		else
			it->second = to_stringf(arg);

		std::string t;

		for (auto el : _profiler)
		{
			t += (el.first + " = " + el.second + "\r\n\n");
		}

		std::wstring wstr = ConvertFromUtf8ToUtf16(t);
		SetWindowTextW(hwndEdit1, wstr.c_str());
	}

};

