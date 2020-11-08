
#include "common.h"
#include <fstream>
#include <filesystem>

using std::string;
namespace fs = std::filesystem;

string engine::ConvertFromUtf16ToUtf8(const std::wstring& wstr)
{
	if (wstr.empty())
		return string();

	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	string strTo(size_needed, 0);

	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

	return strTo;
}

std::wstring engine::ConvertFromUtf8ToUtf16(const string& str)
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

static const char* names[] = { "GameObject", "Model", "Light", "Camera" };
static std::map<std::string, engine::OBJECT_TYPE> types =
{
	{"GameObject", engine::OBJECT_TYPE::GAMEOBJECT},
	{"Model", engine::OBJECT_TYPE::MODEL},
	{"Light", engine::OBJECT_TYPE::LIGHT},
	{"Camera", engine::OBJECT_TYPE::CAMERA}
};

const char* engine::getNameByType(OBJECT_TYPE type)
{
	int i = static_cast<int>(type);
	return names[i];
}

engine::OBJECT_TYPE engine::getTypeByName(const std::string& name)
{
	return types[name];
}
