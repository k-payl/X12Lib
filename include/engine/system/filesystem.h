#pragma once
#include "common.h"
#include <filesystem>
#include <fstream>
#include <utility>

namespace engine
{
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
		X12_API FileMapping() = default;
		X12_API ~FileMapping();
		X12_API FileMapping& operator=(const FileMapping&) = delete;
		X12_API FileMapping(const FileMapping&) = delete;
		X12_API FileMapping& operator=(FileMapping&&);
		X12_API FileMapping(FileMapping&&);
	};

	class File final
	{
		std::fstream file_;
		std::filesystem::path fsPath_;

	public:
		X12_API File(const std::ios_base::openmode& fileMode, const std::filesystem::path& path);
		X12_API ~File();
		X12_API File& operator=(const File&) = delete;
		X12_API File(const File&) = delete;
		X12_API File& operator=(const File&&);
		X12_API File(const File&&);

		auto X12_API Read(void* pMem, size_t bytes) -> void;
		auto X12_API Write(const void* pMem, size_t bytes) -> void;
		auto X12_API WriteStr(const char* str) -> void;
		auto X12_API FileSize()->size_t;
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
		auto X12_API FileExist(const char *path) -> bool;
		auto X12_API DirectoryExist(const char *path) -> bool;
		auto X12_API CreateDirectory_(const char* name) -> void;
		auto X12_API GetWorkingPath(const char *path) -> std::string;
		auto X12_API IsRelative(const char *path) -> bool;
		auto X12_API OpenFile(const char *path, FILE_OPEN_MODE mode = FILE_OPEN_MODE::WRITE) -> File;
		auto X12_API ClearFile(const char *path) -> void;
		auto X12_API FilterPaths(const char* path, const char *ext) -> std::vector<std::string>;
		auto X12_API CreateMemoryMapedFile(const char *path) -> FileMapping;
		auto X12_API GetFileName(const std::string& filePath, bool withExtension = true) -> std::string;
		auto X12_API IsValid(const std::string& filePath) -> bool;
		auto X12_API ToValid(std::string& path) -> void;
		auto X12_API GetTime(std::string& path) -> int64_t;
		auto X12_API LoadFile(const char* path) ->std::shared_ptr<char[]>;
		auto X12_API FileExtension(const std::string& path) ->std::string;
	};
}
