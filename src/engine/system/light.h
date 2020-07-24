#pragma once
#include "gameobject.h"

namespace engine
{
	enum class LIGHT_TYPE
	{
		DIRECT,
		AREA
	};

	class Light : public GameObject
	{
		friend SceneManager;

		float intensity_{1.0f};
		LIGHT_TYPE lightType_{ LIGHT_TYPE::DIRECT};

		Light();
	protected:
		virtual void Copy(GameObject *original) override;
		virtual void SaveYAML(void *yaml) override;
		virtual void LoadYAML(void *yaml) override;

	public:
		auto X12_API virtual GetIntensity() -> float { return intensity_; }
		auto X12_API virtual SetIntensity(float v) -> void { intensity_ = v; }
		auto X12_API virtual SetLightType(LIGHT_TYPE value) -> void { lightType_ = value; }
		auto X12_API virtual GetLightType() const -> LIGHT_TYPE { return lightType_; }

		// GameObject
		auto X12_API virtual Clone() -> GameObject* override;
	};
}
