#include "pch.h"
#include "texture.h"
#include "core.h"
#include "console.h"
#include "filesystem.h"
#include "icorerender.h"

using namespace engine;

Texture::Texture(const std::string& path, x12::TEXTURE_CREATE_FLAGS flags) : path_(path), flags_(flags)
{
}

Texture::Texture(intrusive_ptr<x12::ICoreTexture> tex)
{
	coreTexture_ = std::move(tex);
}

Texture::~Texture()
{
	if (!path_.empty())
		Log("Texture unloaded: '%s'", path_.c_str());
}

bool Texture::Load()
{
	Log("Texture loading: '%s'", path_.c_str());

	if (!GetFS()->FileExist(path_.c_str()))
	{
		LogCritical("Texture::Load(): file '%s' not found", path_.c_str());
		return false;
	}

	const std::string ext = engine::fileExtension(path_.c_str());
	if (ext != "dds")
	{
		LogCritical("Texture::Load(): extension %s is not supported", ext.c_str());
		return false;
	}

	File file = GetFS()->OpenFile(path_.c_str(), FILE_OPEN_MODE::READ | FILE_OPEN_MODE::BINARY);
	size_t fileSize = file.FileSize();

	if (fileSize == 0)
	{
		LogCritical("Texture::Load(): file is empty");
		return false;
	}

	std::unique_ptr<uint8_t[]> data(new uint8_t[fileSize]);
	file.Read(data.get(), fileSize);

	//x12::ICoreTexture* coreTex = createFromDDS(std::move(data), fileSize, flags_);

//	coreTexture_ = std::unique_ptr<x12::ICoreTexture>(coreTex);

	if (!coreTexture_)
	{
		LogCritical("Texture::Load(): some error occured");
		return false;
	}
	
	return true;
}
/*
auto X12_API Texture::GetCoreTexture() -> x12::ICoreTexture *
{
	return coreTexture_.get();
}

auto X12_API Texture::GetVideoMemoryUsage() -> size_t
{
	if (!coreTexture_)
		return 0;

	return coreTexture_->GetVideoMemoryUsage();
}

auto X12_API Texture::GetWidth() -> int
{
	return coreTexture_->GetWidth();
}

auto X12_API Texture::GetHeight() -> int
{
	return coreTexture_->GetHeight();
}

auto X12_API Texture::GetMipmaps() -> int
{
	return coreTexture_->GetMipmaps();
}

auto X12_API Texture::ReadPixel2D(void *data, int x, int y) -> int
{
	return coreTexture_->ReadPixel2D(data, x, y);
}

auto X12_API Texture::GetData(uint8_t* pDataOut, size_t length) -> void
{
	return coreTexture_->GetData(pDataOut, length);
}

auto X12_API Texture::CreateMipmaps() -> void
{
	coreTexture_->CreateMipmaps();
}*/
