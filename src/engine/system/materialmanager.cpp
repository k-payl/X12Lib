#include "pch.h"
#include "materialmanager.h"
#include "core.h"
#include "console.h"
#include "filesystem.h"
#include "material.h"
#include <unordered_map>

static std::unordered_map<std::string, engine::Material*> materials;

auto X12_API engine::MaterialManager::LoadMaterial(const char* path) -> Material*
{
	if (!engine::GetFS()->FileExist(path))
		return nullptr;

	auto mat = new Material(path);
	mat->LoadYAML();
	materials[mat->GetName()] = mat;
	return mat;
}

auto X12_API engine::MaterialManager::FindMaterial(const char* name) -> Material*
{
	auto it = materials.find(name);
	if (it != materials.end())
		return it->second;

	auto l = LoadMaterial(name);

	return l;
}

auto X12_API engine::MaterialManager::CreateMaterial(const char* path) -> Material*
{
	auto mat = new Material(path);
	materials[mat->GetName()] = mat;
	return mat;
}

auto X12_API engine::MaterialManager::GetDefaultMaterial() -> Material*
{
	return defaultMat;
}

auto engine::MaterialManager::Free() -> void
{
	for (auto m : materials)
		delete m.second;
	materials.clear();
}

void engine::MaterialManager::Init()
{
	defaultMat = CreateMaterial("___default_material");
	//defaultMat->SetValue(Material::Params::Albedo, math::vec4(1, 0, 0, 1));

	std::vector<std::string> paths = GetFS()->FilterPaths("materials", ".mat");

	for (auto& p : paths)
	{
		LoadMaterial(p.c_str());
	}
}
