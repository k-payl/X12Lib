#include "pch.h"
#include "common.h"
#include <fstream>
#include <filesystem>

using std::string;
namespace fs = std::filesystem;

string ConvertFromUtf16ToUtf8(const std::wstring& wstr)
{
	if (wstr.empty())
		return string();

	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	string strTo(size_needed, 0);

	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

	return strTo;
}

std::wstring ConvertFromUtf8ToUtf16(const string& str)
{
	std::wstring res;
	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, 0, 0);
	if (size > 0)
	{
		std::vector<wchar_t> buffer(size);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &buffer[0], size);
		res.assign(buffer.begin(), buffer.end() - 1);
	}
	return res;
}
