
#include "core.h"
#include "console.h"
#include "gameobject.h"

#include "yaml-cpp/yaml.h"
#include "yaml.inl"
using namespace YAML;

using engine::OBJECT_TYPE;

IdGenerator<int32_t> engine::GameObject::idGen;

engine::GameObject::GameObject()
{
	id_ = idGen.getId();
}

void engine::GameObject::Copy(GameObject * original)
{
	//Log("GameObject.Copy()");
	enabled_ = original->enabled_;
	name_ = original->name_;
	pos_ = original->pos_;
	rot_ = original->rot_;
	scale_ = original->scale_;
	localTransform_ = original->localTransform_;
	worldPos_ = original->worldPos_;
	worldRot_ = original->worldRot_;
	worldScale_ = original->worldScale_;
	worldTransform_ = original->worldTransform_;
	parent_ = original->parent_;

	// TODO: childs
}

void engine::GameObject::Update(float dt)
{
	worldTransformPrev_ = worldTransform_;

	for(int i = 0; i < childs_.size(); i++)
		childs_[i]->Update(dt);
}

void engine::GameObject::SaveYAML(void *yaml)
{
	//YAML::Emitter *_n = static_cast<YAML::Emitter*>(yaml);
	//YAML::Emitter& n = *_n;

	//n << YAML::Key << "id" << YAML::Value << id_;
	//n << YAML::Key << "enabled" << YAML::Value << enabled_;
	//n << YAML::Key << "type" << YAML::Value << getNameByType(type_);
	//n << YAML::Key << "worldTransform" << YAML::Value << worldTransform_;
}

void engine::GameObject::LoadYAML(void * yaml)
{
	YAML::Node *_n = static_cast<YAML::Node*>(yaml);
	YAML::Node& n = *_n;

	id_ = n["id"].as<int>();
	enabled_ = n["enabled"].as<bool>();

	YAML::Node wt = n["worldTransform"];
	mat4 transform;
	loadMat4(wt, transform);
	SetWorldTransform(transform);
}

engine::GameObject::~GameObject()
{
	for(int i = 0; i < childs_.size(); i++)
		delete childs_[i];

	//Log("destory ~GameObject() %i", id_);
}

auto engine::GameObject::SetLocalPosition(vec3 pos) -> void
{
	pos_ = pos;
	compositeTransform(localTransform_, pos_, rot_, scale_);
	SetLocalTransform(localTransform_);
}

auto engine::GameObject::GetLocalPosition() -> vec3
{
	return pos_;
}

auto engine::GameObject::SetLocalRotation(quat rot) -> void
{
	rot_ = rot;
	compositeTransform(localTransform_, pos_, rot_, scale_);
	SetLocalTransform(localTransform_);
}

auto engine::GameObject::GetLocalRotation() -> quat
{
	return rot_;
}

auto engine::GameObject::SetLocalScale(vec3 scale) -> void
{
	scale_ = scale;
	compositeTransform(localTransform_, pos_, rot_, scale_);
	SetLocalTransform(localTransform_);
}

auto engine::GameObject::GetLocalScale() -> vec3
{
	return scale_;
}

auto engine::GameObject::SetLocalTransform(const mat4& m) -> void
{
	localTransform_ = m;
	decompositeTransform(localTransform_, pos_, rot_, scale_);

	if (parent_)
		worldTransform_ = parent_->GetWorldTransform() * localTransform_;
	else
		worldTransform_ = localTransform_;
	decompositeTransform(worldTransform_, worldPos_, worldRot_, worldScale_);
	
	for(int i = 0; i < childs_.size(); i++) // update childs
		childs_[i]->SetLocalTransform(childs_[i]->localTransform_);
}

auto engine::GameObject::GetLocalTransform() -> mat4
{
	return localTransform_;
}

auto engine::GameObject::SetWorldPosition(vec3 pos) -> void
{
	worldPos_ = pos;
	compositeTransform(worldTransform_, worldPos_, worldRot_, worldScale_);
	SetWorldTransform(worldTransform_);
}

auto engine::GameObject::GetWorldPosition() -> vec3
{
	return worldPos_;
}

auto engine::GameObject::SetWorldRotation(const quat & r) -> void
{
	worldRot_ = r;
	compositeTransform(worldTransform_, worldPos_, worldRot_, worldScale_);
	SetWorldTransform(worldTransform_);
}

auto engine::GameObject::GetWorldRotation() -> quat
{
	return worldRot_;
}

auto engine::GameObject::SetWorldScale(vec3 s) -> void
{
	worldScale_ = s;
	compositeTransform(worldTransform_, worldPos_, worldRot_, worldScale_);
	SetWorldTransform(worldTransform_);
}

auto engine::GameObject::GetWorldScale() -> vec3
{
	return worldScale_;
}

auto engine::GameObject::SetWorldTransform(const mat4& m) -> void
{
	worldTransform_ = m;
	decompositeTransform(worldTransform_, worldPos_, worldRot_, worldScale_);

	if (parent_)
		localTransform_ = parent_->GetWorldTransform().Inverse() * worldTransform_;
	else
		localTransform_ = worldTransform_;
	decompositeTransform(localTransform_, pos_, rot_, scale_);

	for(int i = 0; i < childs_.size(); i++) // update childs
		childs_[i]->SetLocalTransform(childs_[i]->localTransform_);
}

auto engine::GameObject::GetWorldTransform() -> mat4
{
	return worldTransform_;
}

auto engine::GameObject::GetInvWorldTransform() -> mat4
{
	mat4 mat = worldTransform_.Inverse();
	return mat;
}

auto engine::GameObject::GetWorldTransformPrev() -> mat4
{
	return worldTransformPrev_;
}

auto engine::GameObject::GetChild(size_t i) -> GameObject*
{
	assert(i < childs_.size());
	return childs_[i];
}

auto engine::GameObject::RemoveChild(GameObject *obj) -> void
{
	auto it = std::find(childs_.begin(), childs_.end(), obj);
	if (it != childs_.end())
	{
		childs_.erase(it);
		obj->parent_ = nullptr;
		obj->SetWorldTransform(obj->worldTransform_);
	}
	//else
	//	LogWarning("engine::GameObject::RemoveChild(): child not found");
}

auto engine::GameObject::InsertChild(GameObject *obj, int row) -> void
{
	if (row < 0)
		childs_.push_back(obj);
	else
		childs_.insert(childs_.begin() + row, obj);
	obj->parent_ = this;
	obj->SetWorldTransform(obj->worldTransform_);
}

auto engine::GameObject::Clone() -> GameObject *
{
	GameObject *g = new GameObject;
	g->Copy(this);
	return g;
}

/*
void print_vec(vec3& m)
{
	Log("(%f %f %f)", m.x, m.y, m.z);
}

void print_quat(quat& m)
{
	Log("(%f %f %f %f)", m.x, m.y, m.z, m.z);
}

void print_mat(mat4& m)
{
	Log("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f",
	m.el_1D[0],
	m.el_1D[1],
	m.el_1D[2],
	m.el_1D[3],
	m.el_1D[4],
	m.el_1D[5],
	m.el_1D[6],
	m.el_1D[7],
	m.el_1D[8],
	m.el_1D[9],
	m.el_1D[10],
	m.el_1D[11],
	m.el_1D[12],
	m.el_1D[13],
	m.el_1D[14],
	m.el_1D[15]);
}

void engine::GameObject::print_local()
{
	Log("Object %s", name_.c_str());
	Log("pos_:");
	print_vec(pos_);

	Log("rot_:");
	print_quat(rot_);

	Log("scale_:");
	print_vec(scale_);

	Log("localTransform:");
	print_mat(localTransform_);
}

void engine::GameObject::print_global()
{
	Log("Object %s", name_.c_str());
	Log("worldPos:");
	print_vec(worldPos_);

	Log("worldRot:");
	print_quat(worldRot_);

	Log("worldScale:");
	print_vec(worldScale_);

	Log("worldTransform:");
	print_mat(worldTransform_);
}
*/
