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
	return nullptr;
}

auto X12_API engine::MaterialManager::CreateMaterial(const char* path) -> Material*
{
	return nullptr;
}

auto engine::MaterialManager::Free() -> void
{
	for (auto m : materials)
		delete m.second;
	materials.clear();
}

void engine::MaterialManager::Init()
{
	std::vector<std::string> paths = GetFS()->FilterPaths("materials", ".yaml");

	for (auto& p : paths)
	{
		LoadMaterial(p.c_str());
	}
}
