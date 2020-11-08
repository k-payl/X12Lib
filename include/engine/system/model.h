#pragma once
#include "gameobject.h"
#include "resource.h"
#include "mesh.h"

namespace engine
{
	class Model final : public GameObject
	{
		friend SceneManager;

		StreamPtr<Mesh> meshPtr;
		Material* mat_{};
		//vec3 meshCeneter;
		//Material *mat_{nullptr};
		//std::shared_ptr<RaytracingData> trianglesDataPtrWorldSpace;
		//uint raytracingMaterial = 0;
		//mat4 trianglesDataTransform;
		
	protected:
		Model(const char *name);
		virtual void Copy(GameObject *original) override;
		virtual void SaveYAML(void *yaml) override;
		virtual void LoadYAML(void *yaml) override;

	public:
		
		Model(StreamPtr<Mesh> mesh);

		//std::shared_ptr<RaytracingData> GetRaytracingData(uint mat);

		auto GetMesh() -> Mesh* { return meshPtr.get(); };
		auto GetMaterial() -> Material* { return mat_; }
		//auto GetMeshPath() -> const char*;
		//auto SetMaterial(Material *mat) -> void { mat_ = mat; }
		//auto GetMaterial() -> Material* { return mat_; }
		//auto GetWorldCenter() -> vec3;
		//auto GetTrinaglesWorldSpace(std::unique_ptr<vec3[]>& out, uint* trinaglesNum) -> void;

		// GameObject
		auto X12_API virtual Clone() -> GameObject* override;
	};
}
