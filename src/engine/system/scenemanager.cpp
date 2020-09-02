
#include "scenemanager.h"
#include "core.h"
#include "gameobject.h"
#include "model.h"
#include "camera.h"
#include "light.h"
#include "filesystem.h"

#include "yaml-cpp/include/yaml-cpp/yaml.h"

using engine::GameObject;
using engine::Model;
using engine::Camera;
using engine::Light;

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
	using namespace YAML;

	if(rootObjectsVec.size())
	{

		LogCritical("ResourceManager::LoadWorld(): scene loaded");
		return;
	}

	engine::File f = GetFS()->OpenFile("scene.yaml", FILE_OPEN_MODE::READ | FILE_OPEN_MODE::BINARY);

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
}

auto engine::SceneManager::GetNumObjects() -> size_t
{
	return rootObjectsVec.size();
}

auto engine::SceneManager::GetObject_(size_t i) -> GameObject*
{
	return rootObjectsVec[i];
}
