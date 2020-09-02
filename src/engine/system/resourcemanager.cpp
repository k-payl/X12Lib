#include "resourcemanager.h"

using namespace engine;

class MeshResource;
class TextureResource;

static std::unordered_map<std::string, MeshResource*> streamMeshesMap;
static std::unordered_map<std::string, TextureResource*> streamTexturesMap;

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

class TextureResource : public Resource<Texture>
{
	x12::TEXTURE_CREATE_FLAGS flags_;

protected:
	Texture* create() override
	{
		return new Texture(path_, flags_);
	}

public:
	TextureResource(const std::string& path, x12::TEXTURE_CREATE_FLAGS flags) : Resource(path), flags_(flags)
	{}
};

X12_API auto ResourceManager::CreateStreamMesh(const char* path) -> StreamPtr<Mesh>
{
	auto it = streamMeshesMap.find(path);
	if (it != streamMeshesMap.end())
		return StreamPtr<Mesh>(it->second);

	MeshResource* resource = new MeshResource(path);
	streamMeshesMap[path] = resource;
	return StreamPtr<Mesh>(resource);
}

X12_API auto ResourceManager::CreateStreamTexture(const char* path, x12::TEXTURE_CREATE_FLAGS flags) -> StreamPtr<Texture>
{
	auto it = streamTexturesMap.find(path);
	if (it != streamTexturesMap.end())
		return StreamPtr<Texture>(it->second);

	TextureResource* resource = new TextureResource(path, flags);
	streamTexturesMap[path] = resource;
	return StreamPtr<Texture>(resource);
}

