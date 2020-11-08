#include "pch.h"
#include "material.h"

#include <sstream>

#include "core.h"
#include "console.h"
#include "resourcemanager.h"
#include "materialmanager.h"
#include "filesystem.h"

#include "yaml-cpp/yaml.h"
#include "yaml.inl"

using namespace math;
using std::string;

struct ParamInfo
{
	engine::Material::Params index;
	vec4 defaultValue;
};

std::map<std::string, ParamInfo> nameToIndex =
{
	{"base_color", {engine::Material::Params::Albedo, vec4(1,1,1,1)}},
	{"roughness", {engine::Material::Params::Roughness, vec4(0,0,0,0)}},
};

engine::Material::Material(const std::string& path) : path_(path)
{
	for (auto& a : nameToIndex)
	{
		parameters[a.second.index].value = a.second.defaultValue;
	}
}

void engine::Material::LoadYAML()
{
	using namespace YAML;
	using namespace math;

	engine::File f = GetFS()->OpenFile("materials//mesh.yaml", FILE_OPEN_MODE::READ | FILE_OPEN_MODE::BINARY);

	size_t fileSize = f.FileSize();

	std::unique_ptr<char[]> tmp = std::unique_ptr<char[]>(new char[fileSize + 1L]);
	tmp[fileSize] = '\0';

	f.Read((uint8_t*)tmp.get(), fileSize);

	YAML::Node mat_yaml = YAML::Load(tmp.get());
	auto t = mat_yaml.Type();

	if (!mat_yaml["name"])
	{
		LogCritical("LoadWorld(): invalid file");
		return;
	}

	auto roots_yaml = mat_yaml["name"];
	name_ = roots_yaml.as<string>();

	auto objects_yaml = mat_yaml["parameters"];
	if (objects_yaml.Type() != NodeType::Sequence)
	{
		LogCritical("LoadWorld(): invalid file");
		return;
	}

	for (int i = 0; i < objects_yaml.size(); i++)
	{
		auto obj_yaml = objects_yaml[i];

		if (!obj_yaml["name"])
			continue;

		std::string n = obj_yaml["name"].as<string>();
		auto it = nameToIndex.find(n);

		if (it == nameToIndex.end())
			continue;

		int index = it->second.index;

		if (obj_yaml["value"])
		{
			if (obj_yaml["value"].Type() == NodeType::Scalar)
			{
				parameters[index].value = vec4(obj_yaml["value"].as<float>());
			}
			else if (obj_yaml["value"].Type() == NodeType::Sequence)
			{
				loadVec4(obj_yaml, "value", parameters[index].value);
			}
		}

		if (obj_yaml["texture"] && !obj_yaml["texture"].as<string>().empty())
		{
			parameters[index].texture = engine::GetResourceManager()->CreateStreamTexture(obj_yaml["texture"].as<string>().c_str(), x12::TEXTURE_CREATE_FLAGS::NONE);
			//parameters[index].texture.get();
		}
	}
}

math::vec4 engine::Material::GetValue(Params p)
{
	return parameters[p].value;
}

engine::Texture* engine::Material::GetTexture(Params p)
{
	return parameters[p].texture.get();
}

void engine::Material::SaveYAML()
{
}

