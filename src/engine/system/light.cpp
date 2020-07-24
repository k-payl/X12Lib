
#include "light.h"

void engine::Light::Copy(GameObject * original)
{
	GameObject::Copy(original);

	Light *original_light = static_cast<Light*>(original);
	intensity_ = original_light->intensity_;
	lightType_ = original_light->lightType_;
}

void engine::Light::SaveYAML(void * yaml)
{
	//GameObject::SaveYAML(yaml);

	//YAML::Emitter *_n = static_cast<YAML::Emitter*>(yaml);
	//YAML::Emitter& n = *_n;

	//n << YAML::Key << "intensity" << YAML::Value << intensity_;
	//n << YAML::Key << "light_type" << YAML::Value << (int)lightType_;
}

void engine::Light::LoadYAML(void * yaml)
{
	GameObject::LoadYAML(yaml);

	//YAML::Node *_n = static_cast<YAML::Node*>(yaml);
	//YAML::Node& n = *_n;

	//if (n["intensity"]) intensity_ = n["intensity"].as<float>();
	//if (n["light_type"]) lightType_ = (LIGHT_TYPE)n["light_type"].as<int>();
}

engine::Light::Light()
{
	type_ = OBJECT_TYPE::LIGHT;
}

auto engine::Light::Clone() -> GameObject *
{
	Light *l = new Light;
	l->Copy(this);
	return l;
}
