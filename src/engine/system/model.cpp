
#include "model.h"
#include "core.h"
#include "resourcemanager.h"
#include "materialmanager.h"

#include "yaml-cpp/yaml.h"
using namespace YAML;

void engine::Model::Copy(GameObject * original)
{
	GameObject::Copy(original);

	Model *original_model = static_cast<Model*>(original);
}

engine::Model::Model(const char* name)
{
	type_ = OBJECT_TYPE::MODEL;
	meshPtr = engine::GetResourceManager()->CreateStreamMesh(name);
	//meshPtr.get();// force load
}

auto engine::Model::Clone() -> GameObject *
{
	Model *m = new Model(meshPtr);
	m->Copy(this);
	return m;
}

void engine::Model::SaveYAML(void * yaml)
{
	GameObject::SaveYAML(yaml);

	YAML::Emitter *_n = static_cast<YAML::Emitter*>(yaml);
	YAML::Emitter& n = *_n;

	if (!meshPtr.path().empty())
		n << YAML::Key << "mesh" << YAML::Value << meshPtr.path();

	//if (mat_)
	//	n << YAML::Key << "material" << YAML::Value << mat_->GetId();
}

void engine::Model::LoadYAML(void * yaml)
{
	GameObject::LoadYAML(yaml);

	YAML::Node *_n = static_cast<YAML::Node*>(yaml);
	YAML::Node& n = *_n;

	if (n["material"])
	{
		std::string mat = n["material"].as<std::string>();
		mat_ = GetMaterialManager()->FindMaterial(mat.c_str());
	}
}

engine::Model::Model(StreamPtr<Mesh> mesh)
{
	meshPtr = mesh;
}

