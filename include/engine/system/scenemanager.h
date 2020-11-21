#pragma once
#include "common.h"
#include "icorerender.h"
#include "light.h"

namespace engine
{
	class SceneManager final
	{
		std::vector<GameObject*> rootObjectsVec;
		Signal<GameObject*> onObjectAdded;
		Signal<GameObject*> onObjectDestroy;
		Signal<> onSceneLoaded;
		intrusive_ptr<x12::ICoreBuffer> lightsBuffer;

		void destroyObjects();
		void loadSceneYAML(const char* path);
		void createLightsGPUBuffer();

	public:
		~SceneManager();

		static void LoadSceneCommand(const char *arg);

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
		void GetObjectsOfType(std::vector<T*>& vec, engine::OBJECT_TYPE type)
		{
			size_t objects = rootObjectsVec.size();

			for (size_t i = 0; i < objects; i++)
			{
				T* g = static_cast<T*>(engine::GetSceneManager()->GetObject_(i));
				addObjectsRecursive<T>(vec, g, type);
			}
		}

		void GetAreaLights(std::vector<Light*>& areaLights)
		{
			GetObjectsOfType(areaLights, engine::OBJECT_TYPE::LIGHT);
			std::remove_if(areaLights.begin(), areaLights.end(),
				[](const engine::Light* l)-> bool
				{
					return l->GetLightType() != engine::LIGHT_TYPE::AREA;
				});
		}

		void Update(float dt);

		auto X12_API GetNumObjects()->size_t;
		auto X12_API GetObject_(size_t i)->GameObject*;
		auto X12_API CreateGameObject()->GameObject*;
		auto X12_API CreateCamera()->Camera*;
		auto X12_API CreateModel(const char* path)->Model*;
		auto X12_API CreateLight()->Light*;
		auto X12_API DestroyObject(GameObject* obj) -> void;
		auto X12_API CloneObject(GameObject* obj)->GameObject*;
		auto X12_API LoadScene(const char* name) -> void;
		auto X12_API SaveScene(const char* name)->void;
		auto X12_API DestroyObjects()->void;
		auto X12_API LightsBuffer() -> x12::ICoreBuffer* { return lightsBuffer.get(); }
		auto X12_API AddCallbackOnObjAdded(ObjectCallback c) -> void { onObjectAdded.Add(c); }
		auto X12_API AddCallbackSceneLoaded(VoidProcedure c) -> void { onSceneLoaded.Add(c); }
	};
}
