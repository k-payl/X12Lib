#pragma once
#include "common.h"

namespace engine
{
	using math::vec3;
	using math::quat;
	using math::mat4;

	class GameObject
	{
		friend SceneManager;

		static IdGenerator<int32_t> idGen;

	protected:
		GameObject();
		virtual ~GameObject();

		int id_;
		std::string name_{"GameObject"};
		bool enabled_{true};
		OBJECT_TYPE type_{OBJECT_TYPE::GAMEOBJECT};

		// Local (relative parent)
		vec3 pos_;
		quat rot_;
		vec3 scale_{1.0f, 1.0f, 1.0f};
		mat4 localTransform_; // transforms Local -> Parent coordinates

		// World (relative world)
		vec3 worldPos_;
		quat worldRot_;
		vec3 worldScale_{1.0f, 1.0f, 1.0f};
		mat4 worldTransform_; // transforms Local -> World coordinates

		mat4 worldTransformPrev_;

		GameObject *parent_{nullptr};
		std::vector<GameObject*> childs_;

		virtual void Copy(GameObject *original);

	public:
		// Interanl API
		virtual void Update(float dt);
		virtual void SaveYAML(void *yaml);
		virtual void LoadYAML(void *yaml);

	public:
		auto X12_API GetName() -> const char* { return name_.c_str(); }
		auto X12_API SetName(const char *name) -> void { name_ = name; }
		auto X12_API GetId() -> int { return id_; }
		auto X12_API SetId(int id) -> void { id_ = id; }
		auto X12_API SetEnabled(bool v) -> void { enabled_ = v; }
		auto X12_API IsEnabled() -> bool { return enabled_; }
		auto X12_API GetType() -> OBJECT_TYPE { return type_; }

		// Transformation
		auto X12_API SetLocalPosition(vec3 pos) -> void;
		auto X12_API GetLocalPosition() -> vec3;
		auto X12_API SetLocalRotation(quat pos) -> void;
		auto X12_API GetLocalRotation() -> quat;
		auto X12_API SetLocalScale(vec3 pos) -> void;
		auto X12_API GetLocalScale() -> vec3;
		auto X12_API SetLocalTransform(const mat4& m) -> void;
		auto X12_API GetLocalTransform() -> mat4;
		auto X12_API SetWorldPosition(vec3 pos) -> void;
		auto X12_API GetWorldPosition() -> vec3;
		auto X12_API SetWorldRotation(const quat& r) -> void;
		auto X12_API GetWorldRotation() -> quat;
		auto X12_API SetWorldScale(vec3 s) -> void;
		auto X12_API GetWorldScale() -> vec3;
		auto X12_API SetWorldTransform(const mat4& m) -> void;
		auto X12_API GetWorldTransform() -> mat4;
		auto X12_API GetInvWorldTransform() -> mat4;
		auto X12_API GetWorldTransformPrev()->mat4;

		// Hierarchy
		auto X12_API GetParent() -> GameObject* { return parent_; }
		auto X12_API GetNumChilds() -> size_t { return childs_.size(); }
		auto X12_API GetChild(size_t i) -> GameObject*;
		auto X12_API RemoveChild(GameObject *obj) -> void;
		auto X12_API InsertChild(GameObject *obj, int row = -1) -> void;
		auto X12_API virtual Clone() -> GameObject*;

		// debug
		void print_local();
		void print_global();
	};
}
