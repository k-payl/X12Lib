#pragma once
#include "common.h"
#include "icorerender.h"

namespace engine
{
	class SceneManager final
	{
		std::vector<GameObject*> rootObjectsVec;
		Signal<GameObject*> onObjectAdded;
		Signal<GameObject*> onObjectDestroy;
		intrusive_ptr<x12::ICoreBuffer> lightsBuffer;

		void destroyObjects();

		template<typename T>
		void addObjectsRecursive(std::vector<T*>& ret, engine::GameObject* root, engine::OBJECT_TYPE type)
		{
			size_t childs = root->GetNumChilds();

			for (size_t i = 0; i < childs; i++)
			{
				engine::GameObject* g = root->GetChild(i);
				addObjectsRecursive<T>(ret, g, type);
			}

			if (root->GetType() == type && root->IsEnabled())
				ret.push_back(static_cast<T*>(root));
		}

		template<typename T>
		void getObjectsOfType(std::vector<T*>& vec, engine::OBJECT_TYPE type)
		{
			size_t objects = rootObjectsVec.size();

			for (size_t i = 0; i < objects; i++)
			{
				T* g = static_cast<T*>(engine::GetSceneManager()->GetObject_(i));
				addObjectsRecursive<T>(vec, g, type);
			}
		}

	public:
		~SceneManager();

		void Update(float dt);

		auto X12_API GetNumObjects()->size_t;
		auto X12_API GetObject_(size_t i)->GameObject*;

		auto X12_API CreateGameObject() -> GameObject*;
		auto X12_API CreateCamera() -> Camera*;
		auto X12_API CreateModel(const char* path) -> Model*;
		auto X12_API CreateLight() -> Light*;

		auto X12_API DestroyObject(GameObject* obj) -> void;
		auto X12_API CloneObject(GameObject* obj) -> GameObject*;

		auto X12_API LoadScene(const char* name) -> void;

		X12_API x12::ICoreBuffer* LightsBuffer() { return lightsBuffer.get(); }
	};
}
