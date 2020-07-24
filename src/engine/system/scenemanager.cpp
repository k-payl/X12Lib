
#include "scenemanager.h"
#include "model.h"
#include "camera.h"
#include "light.h"

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

	Model* m = new Model(/*meshPtr*/);
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
