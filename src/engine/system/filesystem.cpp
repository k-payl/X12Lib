
#include "filesystem.h"
#include "core.h"
#include <filesystem>
#include <assert.h>

using std::string;
using namespace engine;
namespace fs = std::filesystem;

#ifdef _WIN32
typedef std::wstring mstring;
inline std::string NativeToUTF8(const std::wstring& wstr)
{
	return ConvertFromUtf16ToUtf8(wstr);
}
inline mstring UTF8ToNative(const std::string& str)
{
	return ConvertFromUtf8ToUtf16(str);
}
#else
typedef std::string mstring;
#define NativeToUTF8(ARG) ARG
#define UTF8ToNative(ARG) ARG
#endif

bool FileSystem::_exist(const char *path)
{
	fs::path fsPath = fs::path(path);

	if (fsPath.is_relative())
	{
		fs::path fsDataPath = fs::path(dataPath);
		fsPath = fsDataPath / fsPath;
	}

	return fs::exists(fsPath);
}

bool FileSystem::FileExist(const char *path)
{
	return _exist(path);
}

bool FileSystem::DirectoryExist(const char *path)
{
	return _exist(path);
}

std::string FileSystem::GetWorkingPath(const char *path)
{
	if (strlen(path)==0)
		return fs::current_path().string();
	return canonical(fs::path(path)).string();
}

bool FileSystem::IsRelative(const char *path)
{
	fs::path fsPath = fs::path(path);
	return fsPath.is_relative();
}

auto FileSystem::OpenFile(const char *path, FILE_OPEN_MODE mode) -> File
{
	const bool read = mode & FILE_OPEN_MODE::READ;
	
	fs::path fsPath = fs::path(path);

	if (fsPath.is_relative())
	{
		fs::path fsDataPath = fs::path(dataPath);
		fsPath = fsDataPath / fsPath;
	}

	assert(read && fs::exists(fsPath) || !read);

	std::ios_base::openmode cpp_mode = read ? std::ofstream::in : cpp_mode = std::ofstream::out;

	if (FILE_OPEN_MODE::APPEND & mode)
		cpp_mode |= std::ofstream::out | std::ofstream::app;

	if (bool(mode & FILE_OPEN_MODE::BINARY))
		cpp_mode |= std::ofstream::binary;

	return File(cpp_mode, fsPath);
}

auto FileSystem::ClearFile(const char *path) -> void
{
	fs::path fsPath = fs::path(path);

	if (fsPath.is_relative())
	{
		fs::path fsDataPath = fs::path(dataPath);
		fsPath = fsDataPath / fsPath;
	}

	if (fs::exists(fsPath))
	{
		std::ofstream ofs;
		mstring mPath = UTF8ToNative(fsPath.string());
		ofs.open(mPath, std::ofstream::out | std::ofstream::trunc);
		ofs.close();
	}
}

auto FileSystem::FilterPaths(const char *ext) -> std::vector<std::string>
{
	std::vector<std::string> files;
	std::string extension(ext);
	fs::path path = fs::path(dataPath);
	fs::recursive_directory_iterator it(path);
	fs::recursive_directory_iterator endit;

	while (it != endit)
	{
		if(fs::is_regular_file(*it) && (extension=="") ? true : it->path().extension() == extension)
		{
			fs::path rel = fs::relative(*it, path);
			files.push_back(rel.string());
		}
		++it;
	}

	return files;
}

auto FileSystem::CreateMemoryMapedFile(const char* path) -> FileMapping
{
	FileMapping mapping;

	fs::path fsPath = fs::path(path);

	if (fsPath.is_relative())
	{
		fs::path fsDataPath = fs::path(dataPath);
		fsPath = fsDataPath / fsPath;
	}

	std::wstring wpath = ConvertFromUtf8ToUtf16(fsPath.string());
	mapping.hFile = CreateFile(wpath.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	assert(mapping.hFile != INVALID_HANDLE_VALUE);

	mapping.fsize = GetFileSize(mapping.hFile, nullptr);
	assert(mapping.fsize != INVALID_FILE_SIZE);

	mapping.hMapping = CreateFileMapping(mapping.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	assert(mapping.hMapping);

	mapping.ptr = (unsigned char*)MapViewOfFile(mapping.hMapping, FILE_MAP_READ, 0, 0, mapping.fsize);
	assert(mapping.ptr);

	return mapping;
}

auto FileSystem::GetFileName(const std::string& filePath, bool withExtension) -> std::string
{
	// Create a Path object from File Path
	fs::path pathObj(filePath);

	// Check if file name is required without extension
	if (withExtension == false)
	{
		// Check if file has stem i.e. filename without extension
		if (pathObj.has_stem())
		{
			// return the stem (file name without extension) from path object
			return pathObj.stem().string();
		}
		return "";
	}
	else
	{
		// return the file name with extension from path object
		return pathObj.filename().string();
	}
}

bool FileSystem::isInvalidSymbol(char c)
{
	return
		c == '/' ||
		c == ':' ||
		c == '*' ||
		c == '?' ||
		c == '"' ||
		c == '<' ||
		c == '>' ||
		c == '>';
}

auto FileSystem::IsValid(const std::string& filePath) -> bool
{
	auto f = std::bind(std::mem_fn(&FileSystem::isInvalidSymbol), this, std::placeholders::_1);
	return !std::any_of(std::begin(filePath), std::end(filePath), f);
}

auto FileSystem::ToValid(std::string& filePath) -> void
{
	auto f = std::bind(std::mem_fn(&FileSystem::isInvalidSymbol), this, std::placeholders::_1);
	filePath.erase(std::remove_if(filePath.begin(), filePath.end(), f));
}

auto FileSystem::GetTime(std::string& path) -> int64_t
{
	fs::path fsPath = fs::path(path);
	auto timestampt = fs::last_write_time(fsPath);

	using namespace std::chrono_literals;
	auto ftime = fs::last_write_time(fsPath);
	auto a = ftime.time_since_epoch();
	return a.count();
}

auto FileSystem::LoadFile(const char* path) -> std::shared_ptr<char[]>
{
	File file = OpenFile(path, FILE_OPEN_MODE::READ | FILE_OPEN_MODE::BINARY);
	size_t size = file.FileSize();
	std::shared_ptr<char[]> ptr(new char[size + 1]);

	file.Read((char*)ptr.get(), size);
	ptr[size] = 0;

	return ptr;
}

File::File(const std::ios_base::openmode & fileMode, const std::filesystem::path & path)
{
	fsPath_ = path;
	mstring mPath = UTF8ToNative(path.string());
	file_.open(mPath, fileMode);
}

File::~File()
{
	if (file_.is_open())
		file_.close();
}

File& File::operator=(const File && r)
{
	std::fstream tmp;
	file_.swap(tmp);
	fsPath_ = std::move(r.fsPath_);
	return *this;
}

File::File(const File && r)
{
	std::fstream tmp;
	file_.swap(tmp);
	fsPath_ = std::move(r.fsPath_);
}

auto File::Read(void *pMem, size_t bytes) -> void
{
	file_.read((char *)pMem, bytes);
}

auto File::Write(const void *pMem, size_t bytes) -> void
{
	file_.write((const char *)pMem, bytes);
}

auto File::WriteStr(const char *str) -> void
{
	file_.write(str, strlen(str));
}

auto File::FileSize() -> size_t
{
	return fs::file_size(fsPath_);
}

FileMapping::~FileMapping()
{
	if (ptr)
	{
		UnmapViewOfFile(ptr);
		CloseHandle(hMapping);
		CloseHandle(hFile);
		ptr = nullptr;
	}
}

FileMapping& FileMapping::operator=(FileMapping&& r)
{
	hFile = r.hFile;
	hMapping = r.hMapping;
	fsize = r.fsize;
	ptr = r.ptr;
	r.ptr = nullptr;
	return *this;
}

FileMapping::FileMapping(FileMapping&& r)
{
	hFile = r.hFile;
	hMapping = r.hMapping;
	fsize = r.fsize;
	ptr = r.ptr;
	r.ptr = nullptr;
}
