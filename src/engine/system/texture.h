#pragma once
#include "common.h"
#include "icorerender.h"

namespace engine
{
	class Texture final
	{
		intrusive_ptr<x12::ICoreTexture> coreTexture_;
		std::string path_;
		x12::TEXTURE_CREATE_FLAGS flags_;

	public:
		Texture(const std::string& path, x12::TEXTURE_CREATE_FLAGS flags);
		Texture(intrusive_ptr<x12::ICoreTexture> tex);
		~Texture();

		bool Load();

		/*
		auto X12_API GetCoreTexture()->x12::ICoreTexture*;
		auto X12_API GetVideoMemoryUsage()->size_t;
		auto X12_API GetWidth() -> int;
		auto X12_API GetHeight() -> int;
		auto X12_API GetMipmaps() -> int;
		auto X12_API ReadPixel2D(void* data, int x, int y) -> int;
		auto X12_API GetData(uint8_t* pDataOut, size_t length) -> void;
		auto X12_API CreateMipmaps() -> void;
		*/
	};
}
