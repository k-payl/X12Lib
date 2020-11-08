#pragma once
#include "common.h"
#include "resource.h"
#include "texture.h"

namespace engine
{
	class Material
	{
		std::string name_;
		std::string path_;

		struct Parameter
		{
			math::vec4 value;
			StreamPtr<engine::Texture> texture;
		};

	public:
		Material(const std::string& path);

		enum Params
		{
			Albedo,
			Roughness,
			Num
		};

		virtual void SaveYAML();
		virtual void LoadYAML();
		std::string GetName() const { return name_; }
		X12_API math::vec4 GetValue(Params p);
		X12_API engine::Texture* GetTexture(Params p);

	private:
		Parameter parameters[Num];

	};
}
