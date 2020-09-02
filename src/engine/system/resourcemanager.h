#pragma once
#include "common.h"
#include "resource.h"
#include "mesh.h"
#include "texture.h"

namespace engine
{
	class ResourceManager final
	{
	public:
		X12_API StreamPtr<Mesh> CreateStreamMesh(const char* path);
		X12_API StreamPtr<Texture> CreateStreamTexture(const char* path, x12::TEXTURE_CREATE_FLAGS flags);
	};
}
