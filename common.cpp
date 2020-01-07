#include "pch.h"
#include "common.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

std::shared_ptr<char[]> loadShader(const char* path)
{
	std::fstream file(path, std::ofstream::in);

	size_t size = (size_t)fs::file_size(path);
	std::shared_ptr<char[]> ptr(new char[size + 1]);

	file.read((char*)ptr.get(), size);
	ptr[size] = '\0';

	file.close();
	return ptr;
}