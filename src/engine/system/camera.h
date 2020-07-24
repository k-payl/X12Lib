#pragma once
#include "gameobject.h"

namespace engine
{
	class Camera final : public GameObject
	{
		friend SceneManager;

		float znear{0.1f};
		float zfar{1000.0f};
		float fovAngle{60.0f};

	protected:
		Camera();
		virtual void Copy(GameObject *original) override;
		virtual void SaveYAML(void *yaml) override;
		virtual void LoadYAML(void *yaml) override;

	public:
		virtual void Update(float dt);

		void X12_API GetProjectionMatrix(math::mat4& p, float aspect);
		void X12_API GetModelViewProjectionMatrix(math::mat4& mvp, float aspect);
		void X12_API GetViewMatrix(math::mat4& m);
		auto X12_API GetFullVerFOVInDegrees() -> float;
		auto X12_API GetFullVertFOVInRadians() -> float;

		// GameObject
		auto virtual Clone() -> GameObject* override;
	};
}
