#pragma once
#include "common.h"
#include "resource.h"
#include "mesh.h"
#include "texture.h"
#include "shader.h"

namespace engine
{
	class ResourceManager final
	{
	public:
		void Free();

	public:
		X12_API auto CreateStreamMesh(const char* path) -> StreamPtr<Mesh>;
		X12_API auto CreateStreamTexture(const char* path, x12::TEXTURE_CREATE_FLAGS flags) -> StreamPtr<Texture>;
		X12_API auto CreateGraphicShader(const char *path, const x12::ConstantBuffersDesc *buffersdesc, int numdesc) -> StreamPtr<Shader>;
		X12_API auto ReloadShaders() -> void;
	};
}
