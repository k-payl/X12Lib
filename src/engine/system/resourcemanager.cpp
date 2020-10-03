#include "resourcemanager.h"
#include "console.h"

using namespace engine;

class MeshResource;
class TextureResource;
class ShaderResource;

static std::unordered_map<std::string, MeshResource*> streamMeshesMap;
static std::unordered_map<std::string, TextureResource*> streamTexturesMap;
static std::unordered_map<std::string, ShaderResource*> shaders;

static void ShadersReloadCommand();

namespace {
	static struct CommandEmplacer
	{
		CommandEmplacer()
		{
			engine::RegisterConsoleCommand("shaders_reload", ShadersReloadCommand);
		}
	}c;
}

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

class ShaderResource : public Resource<Shader>
{
	std::vector<Shader::SafeConstantBuffersDesc> descs;
	bool compute_{ false };

protected:
	Shader* create() override
	{
		return new Shader(path_, descs, compute_);
	}

public:
	ShaderResource(const std::string& path, const x12::ConstantBuffersDesc* buffersdesc, int numdesc, bool compute = false) : Resource(path), compute_(compute)
	{
		if (buffersdesc && numdesc)
		{
			descs.resize(numdesc);
			for (int i = 0; i < numdesc; i++)
			{
				const char *ptr = descs[i].name.c_str();
				descs[i].mode = buffersdesc[i].mode;
				descs[i].name = buffersdesc[i].name;
			}
		}
	}
};

static void ShadersReloadCommand()
{
	for (auto& s : shaders)
		s.second->Reload();
}

void engine::ResourceManager::Free()
{
	for (auto& m : streamMeshesMap)
		delete m.second;
	streamMeshesMap.clear();

	for (auto& m : streamTexturesMap)
		delete m.second;
	streamTexturesMap.clear();

	for (auto& m : shaders)
		delete m.second;
	shaders.clear();
}

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

	TextureResource* resource = new TextureResource(path, flags | x12::TEXTURE_CREATE_FLAGS::USAGE_SHADER_RESOURCE);
	streamTexturesMap[path] = resource;
	return StreamPtr<Texture>(resource);
}

X12_API StreamPtr<Shader> engine::ResourceManager::CreateGraphicShader(const char* path, const x12::ConstantBuffersDesc* buffersdesc, int numdesc)
{
	auto it = shaders.find(path);
	if (it != shaders.end())
		return StreamPtr<Shader>(it->second);

	ShaderResource* resource = new ShaderResource(path, buffersdesc, numdesc, false);
	shaders[path] = resource;
	return StreamPtr<Shader>(resource);
}

X12_API StreamPtr<Shader> engine::ResourceManager::CreateComputeShader(const char* path, const x12::ConstantBuffersDesc* buffersdesc, int numdesc)
{
	auto it = shaders.find(path);
	if (it != shaders.end())
		return StreamPtr<Shader>(it->second);

	ShaderResource* resource = new ShaderResource(path, buffersdesc, numdesc, true);
	shaders[path] = resource;
	return StreamPtr<Shader>(resource);
}

X12_API auto engine::ResourceManager::ReloadShaders() -> void
{
	ShadersReloadCommand();
}

