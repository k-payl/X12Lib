
#include "scenemanager.h"
#include "core.h"
#include "gameobject.h"
#include "model.h"
#include "camera.h"
#include "light.h"
#include "filesystem.h"
#include "cpp_hlsl_shared.h"
#include "console.h"
#include "render.h"

#include "yaml-cpp/yaml.h"

using engine::GameObject;
using engine::Model;
using engine::Camera;
using engine::Light;

namespace {
	static struct CommandEmplacer
	{
		CommandEmplacer()
		{
			engine::RegisterConsoleCommand("load", engine::SceneManager::LoadSceneCommand);
		}
	}c;
}


void engine::SceneManager::Update(float dt)
{
	for (GameObject* g : rootObjectsVec)
		g->Update(dt);
}

GameObject* engine::SceneManager::CreateGameObject()
{
	GameObject* g = new GameObject;
	rootObjectsVec.push_back(g);
	onObjectAdded.Invoke(g);
	return g;
}

auto engine::SceneManager::CreateCamera() -> Camera*
{
	Camera* c = new Camera;
	rootObjectsVec.push_back(c);
	onObjectAdded.Invoke(c);
	return c;
}

auto engine::SceneManager::CreateModel(const char* path) -> Model*
{
	//StreamPtr<Mesh> meshPtr = CreateStreamMesh(path);

	Model* m = new Model(path);
	rootObjectsVec.push_back(m);
	onObjectAdded.Invoke(m);
	return m;
}

auto engine::SceneManager::CreateLight() -> Light*
{
	Light* l = new Light;
	rootObjectsVec.push_back(l);
	onObjectAdded.Invoke(l);
	return l;
}

auto engine::SceneManager::DestroyObject(GameObject* obj) -> void
{
	if (auto childs = obj->GetNumChilds())
	{
		for (int i = int(childs - 1); i >= 0; i--)
			DestroyObject(obj->GetChild(i));
	}

	if (obj->GetParent() == nullptr)
	{
		auto it = std::find(rootObjectsVec.begin(), rootObjectsVec.end(), obj);
		assert(it != rootObjectsVec.end());
		GameObject* g = *it;
		rootObjectsVec.erase(it);
	}
	else
		obj->GetParent()->RemoveChild(obj);

	onObjectDestroy.Invoke(obj);

	delete obj;
}

void engine::SceneManager::destroyObjects()
{
	if (const int roots = (int)rootObjectsVec.size())
	{
		for (int i = roots - 1; i >= 0; i--)
			DestroyObject(rootObjectsVec[i]);
	}
}

engine::SceneManager::~SceneManager()
{
	destroyObjects();
}

void engine::SceneManager::LoadSceneCommand(const char* arg)
{
	GetRender()->WaitForGpuAll();
	engine::GetCoreRenderer()->WaitGPUAll();
	GetSceneManager()->DestroyObjects();
	GetSceneManager()->LoadScene(arg);
}

auto engine::SceneManager::CloneObject(GameObject* obj) -> GameObject*
{
	GameObject* ret = obj->Clone();

	if (std::find(rootObjectsVec.begin(), rootObjectsVec.end(), obj) != rootObjectsVec.end())
		rootObjectsVec.emplace_back(ret);
	else
	{
		GameObject* p = obj->GetParent();
		p->InsertChild(ret);
	}

	onObjectAdded.Invoke(ret);

	return ret;
}

static void loadObj(std::vector<GameObject*>& rootObjectsVec, YAML::Node& objects_yaml, int* i, GameObject* parent, engine::Signal<GameObject*>& sig)
{
	using namespace engine;

	if (*i >= objects_yaml.size())
		return;

	auto obj_yaml = objects_yaml[*i];
	*i = *i + 1;

	int childs = obj_yaml["childs"].as<int>();
	engine::OBJECT_TYPE type = getTypeByName(obj_yaml["type"].as<std::string>());

	GameObject* g = nullptr;

	switch (type)
	{
	case OBJECT_TYPE::GAMEOBJECT: g = GetSceneManager()->CreateGameObject(); break;
	case OBJECT_TYPE::MODEL:
	{
		std::string mesh = obj_yaml["mesh"].as<std::string>();
		g = GetSceneManager()->CreateModel(mesh.c_str());
	}
	break;
	case OBJECT_TYPE::LIGHT: g = GetSceneManager()->CreateLight(); break;
	case OBJECT_TYPE::CAMERA: g = GetSceneManager()->CreateCamera(); break;
	default:
		assert("unknown type");
		break;
	}

	//if (parent)
	//	parent->InsertChild(g);
	//else
	//	rootObjectsVec.push_back(g);

	g->LoadYAML(static_cast<void*>(&obj_yaml));

	//sig.Invoke(g);

	for (int j = 0; j < childs; j++)
		loadObj(rootObjectsVec, objects_yaml, i, g, sig);
}

auto X12_API engine::SceneManager::LoadScene(const char* name) -> void
{
	if(rootObjectsVec.size())
	{
		LogCritical("ResourceManager::LoadWorld(): scene loaded");
		return;
	}

	auto ext = engine::GetFS()->FileExtension(name);
	if (ext == "yaml")
	{
		loadSceneYAML(name);
		onSceneLoaded.Invoke();
	}
	else
	{
		LogCritical("ResourceManager::LoadWorld(): inavlid name");
		return;
	}
}

void saveObj(YAML::Emitter& out, GameObject* o)
{
	if (!o)
		return;

	out << YAML::BeginMap;
	out << YAML::Key << "childs" << YAML::Value << o->GetNumChilds();
	o->SaveYAML(static_cast<void*>(&out));
	out << YAML::EndMap;

	for (int i = 0; i < o->GetNumChilds(); i++)
		saveObj(out, o->GetChild(i));
}

auto X12_API engine::SceneManager::SaveScene(const char* name) -> void
{
	using namespace YAML;
	YAML::Emitter out;

	out << YAML::BeginMap;

	out << Key << "roots" << Value << rootObjectsVec.size();

	out << Key << "objects" << Value;
	out << YAML::BeginSeq;

	for (int i = 0; i < rootObjectsVec.size(); i++)
		saveObj(out, rootObjectsVec[i]);

	out << YAML::EndSeq;
	out << YAML::EndMap;

	File f = GetFS()->OpenFile(name, FILE_OPEN_MODE::WRITE | FILE_OPEN_MODE::BINARY);

	f.WriteStr(out.c_str());
}

void engine::SceneManager::loadSceneYAML(const char* name)
{
	using namespace YAML;
	using namespace math;

	engine::File f = GetFS()->OpenFile(name, FILE_OPEN_MODE::READ | FILE_OPEN_MODE::BINARY);

	size_t fileSize = f.FileSize();

	std::unique_ptr<char[]> tmp = std::unique_ptr<char[]>(new char[fileSize + 1L]);
	tmp[fileSize] = '\0';

	f.Read((uint8_t*)tmp.get(), fileSize);

	YAML::Node model_yaml = YAML::Load(tmp.get());
	auto t = model_yaml.Type();

	if (!model_yaml["roots"])
	{
		LogCritical("LoadWorld(): invalid file");
		return;
	}

	auto roots_yaml = model_yaml["roots"];

	if (roots_yaml.Type() != NodeType::Scalar)
	{
		LogCritical("LoadWorld(): invalid file");
		return;
	}

	auto roots = roots_yaml.as<int>();
	if (roots <= 0)
	{
		Log("LoadWorld(): world is empty");
		return;
	}

	auto objects_yaml = model_yaml["objects"];
	if (objects_yaml.Type() != NodeType::Sequence)
	{
		LogCritical("LoadWorld(): invalid file");
		return;
	}

	int i = 0;
	for (int j = 0; j < roots; j++)
		loadObj(rootObjectsVec, objects_yaml, &i, nullptr, onObjectAdded);

	createLightsGPUBuffer();
}

auto X12_API engine::SceneManager::DestroyObjects() -> void
{
	if (const int roots = (int)rootObjectsVec.size())
	{
		for (int i = roots - 1; i >= 0; i--)
			DestroyObject(rootObjectsVec[i]);
	}
}

void engine::SceneManager::createLightsGPUBuffer()
{
	using namespace math;

	std::vector<Light*> lights;
	GetObjectsOfType(lights, OBJECT_TYPE::LIGHT);

	std::vector<engine::Shaders::Light> lightsData;
	lightsData.resize(lights.size());

	for (auto i = 0; i < lights.size(); i++)
	{
		math::mat4 transform = lights[i]->GetWorldTransform();
		memcpy(lightsData[i].worldTransform, &transform.el_1D[1], sizeof(float) * 16);

		vec4 p0 = transform * (vec4(-1, 1, 0, 1));
		vec4 p1 = transform * (vec4(-1, -1, 0, 1));
		vec4 p2 = transform * (vec4(1, -1, 0, 1));
		vec4 p3 = transform * (vec4(1, 1, 0, 1));
		lightsData[i].T_world = (p1 - p0) * .5f;
		lightsData[i].B_world = (p3 - p0) * .5f;

		lightsData[i].center_world = vec4(transform.Column3(3));
		lightsData[i].center_world.w = 1;

		lightsData[i].color = math::vec4(1, 1, 1, 1) * lights[i]->GetIntensity();
		lightsData[i].size = math::vec4(1, 1, 1, 1);
		lightsData[i].normal = math::triangle_normal(p0, p1, p2);
	}

	if (!lights.empty())
	GetCoreRenderer()->CreateStructuredBuffer(lightsBuffer.getAdressOf(), L"Lights buffer",
		sizeof(engine::Shaders::Light), lights.size(), &lightsData[0], x12::BUFFER_FLAGS::SHADER_RESOURCE);
}

auto engine::SceneManager::GetNumObjects() -> size_t
{
	return rootObjectsVec.size();
}

auto engine::SceneManager::GetObject_(size_t i) -> GameObject*
{
	return rootObjectsVec[i];
}
