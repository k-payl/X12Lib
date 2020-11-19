#pragma once
#include "common.h"
#include "icorerender.h"

namespace engine
{
	struct TextureData
	{
		uint8_t *rgbaBuffer;
		uint32_t mipmaps;
		int width, height;
		size_t bufferInBytes;
	};

	X12_API void saveDDS(const TextureData& data, const char* path);

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
		auto X12_API GetCoreTexture()->x12::ICoreTexture*;
	};
}
