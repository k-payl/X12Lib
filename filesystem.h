#pragma once
#include "common.h"
#include <filesystem>
#include <fstream>
#include <utility>

enum class FILE_OPEN_MODE
{
	READ = 1 << 0,
	WRITE = 1 << 1,
	APPEND = 1 << 2,
	BINARY = 1 << 3,
};
DEFINE_ENUM_OPERATORS(FILE_OPEN_MODE)

struct FileMapping
{
	HANDLE hFile;
	HANDLE hMapping;
	size_t fsize;
	unsigned char* ptr;

public:
	FileMapping() = default;
	~FileMapping();
	FileMapping& operator=(const FileMapping&) = delete;
	FileMapping(const FileMapping&) = delete;
	FileMapping& operator=(FileMapping&&);
	FileMapping(FileMapping&&);
};

class File final
{
	std::fstream file_;
	std::filesystem::path fsPath_;

public:
	File(const std::ios_base::openmode& fileMode, const std::filesystem::path& path);
	~File();
	File& operator=(const File&) = delete;
	File(const File&) = delete;
	File& operator=(const File&&);
	File(const File&&);

	auto Read(void* pMem, size_t bytes) -> void;
	auto Write(const void* pMem, size_t bytes) -> void;
	auto WriteStr(const char* str) -> void;
	auto FileSize()->size_t;
};

class FileSystem final
{
	std::string dataPath;
	bool isInvalidSymbol(char c);

	bool _exist(const char* path);
public:
	FileSystem(const std::string& dataPath_) :
		dataPath(dataPath_)
	{}

public:
	auto FileExist(const char *path) -> bool;
	auto DirectoryExist(const char *path) -> bool;
	auto GetWorkingPath(const char *path) -> std::string;
	auto IsRelative(const char *path) -> bool;
	auto OpenFile(const char *path, FILE_OPEN_MODE mode = FILE_OPEN_MODE::WRITE) -> File;
	auto ClearFile(const char *path) -> void;
	auto FilterPaths(const char *ext) -> std::vector<std::string>;
	auto CreateMemoryMapedFile(const char *path) -> FileMapping;
	auto GetFileName(const std::string& filePath, bool withExtension = true) -> std::string;
	auto IsValid(const std::string& filePath) -> bool;
	auto ToValid(std::string& path) -> void;
	auto GetTime(std::string& path) -> int64_t;
	auto LoadFile(const char* path) ->std::shared_ptr<char[]>;
};





