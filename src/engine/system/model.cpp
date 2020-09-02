
#include "model.h"
#include "core.h"
#include "resourcemanager.h"

#include "yaml-cpp/include/yaml-cpp/yaml.h"
using namespace YAML;

void engine::Model::Copy(GameObject * original)
{
	GameObject::Copy(original);

	Model *original_model = static_cast<Model*>(original);
	//meshPtr = original_model->meshPtr;
	//mat_ = original_model->mat_;
}

engine::Model::Model(const char* name)
{
	type_ = OBJECT_TYPE::MODEL;
	meshPtr = engine::GetResourceManager()->CreateStreamMesh(name);
	meshPtr.get();// force load
}

//engine::Model::Model(StreamPtr<Mesh> mesh) : Model()
//{
//	meshPtr = mesh;
//}
/*
std::shared_ptr<RaytracingData> engine::Model::GetRaytracingData(uint mat)
{
	vector<GPURaytracingTriangle>& dataIn = meshPtr.get()->GetRaytracingData()->triangles;

	if (!trianglesDataPtrWorldSpace)
	{
		trianglesDataPtrWorldSpace = shared_ptr<RaytracingData>(new RaytracingData());
		trianglesDataPtrWorldSpace->triangles.resize(dataIn.size());
		trianglesDataTransform = {};
	}

	if (memcmp(&trianglesDataTransform, &worldTransform_, sizeof(mat4)) != 0 || raytracingMaterial != mat)
	{
		raytracingMaterial = mat;

		vector<GPURaytracingTriangle>& dataOut = trianglesDataPtrWorldSpace->triangles;
		mat4 NM = worldTransform_.Inverse().Transpose();

		for (int i = 0; i < dataIn.size(); ++i)
		{
			GPURaytracingTriangle& ti = dataIn[i];
			GPURaytracingTriangle& to = dataOut[i];

			to.p0 = worldTransform_ * ti.p0;
			to.p1 = worldTransform_ * ti.p1;
			to.p2 = worldTransform_ * ti.p2;

			to.n = NM * ti.n;

			to.materialID = mat;
		}

		trianglesDataTransform = worldTransform_;
	}

	return trianglesDataPtrWorldSpace;
}

auto engine::Model::GetMesh() -> Mesh *
{
	return meshPtr.get();
}
auto engine::Model::GetMeshPath() -> const char *
{
	return meshPtr.path().c_str();
}

auto engine::Model::GetWorldCenter() -> vec3
{
	vec3 center = meshPtr.isLoaded() ? meshPtr.get()->GetCenter() : vec3();
	vec4 centerWS = worldTransform_ * vec4(center);
	return (vec3)centerWS;
}

auto engine::Model::GetTrinaglesWorldSpace(std::unique_ptr<vec3[]>& out, uint* trinaglesNum) -> void
{
}
*/
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

	//if (n["material"])
	//{
	//	string mat = n["material"].as<string>();
	//	mat_ = MAT_MAN->GetMaterial(mat.c_str());
	//}
}

engine::Model::Model(StreamPtr<Mesh> mesh)
{
	meshPtr = mesh;
}

