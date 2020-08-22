#include "resourcemanager.h"

using namespace engine;

class MeshResource : public Resource<Mesh>
{
protected:
	Mesh* create() override
	{
		return new Mesh(path_);
	}

public:
	MeshResource(const std::string& path) : Resource(path)
	{}
};

static std::unordered_map<std::string, MeshResource*> streamMeshesMap;

X12_API auto ResourceManager::CreateStreamMesh(const char* path) -> StreamPtr<Mesh>
{
	auto it = streamMeshesMap.find(path);
	if (it != streamMeshesMap.end())
		return StreamPtr<Mesh>(it->second);

	MeshResource* resource = new MeshResource(path);
	streamMeshesMap[path] = resource;
	return StreamPtr<Mesh>(resource);
}
