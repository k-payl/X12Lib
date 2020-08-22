#pragma once
#include "common.h"
#include "resource.h"
#include "mesh.h"

namespace engine
{
	class ResourceManager final
	{
	public:
		X12_API StreamPtr<Mesh> CreateStreamMesh(const char* path);
	};
}
