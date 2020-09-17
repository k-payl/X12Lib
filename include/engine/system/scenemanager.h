#pragma once
#include "common.h"

namespace engine
{
	class SceneManager final
	{
		std::vector<GameObject*> rootObjectsVec;
		Signal<GameObject*> onObjectAdded;
		Signal<GameObject*> onObjectDestroy;

		void destroyObjects();

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

	};
}
