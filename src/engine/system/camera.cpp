
#include "camera.h"
#include "console.h"
#include "core.h"
#include "input.h"

#define MOVE_SPEED 30.f
#define ROTATE_SPEED 0.2f

using math::vec2;
using math::vec3;
using math::mat4;

namespace
{
	vec3 GetRightDirection(const mat4& ModelMat) { return vec3(ModelMat.Column(0)); } // Returns local X vector in world space
	vec3 GetForwardDirection(const mat4& ModelMat) { return vec3(ModelMat.Column(1)); } // Returns local Y vector in world space
	vec3 GetBackDirection(const mat4& ModelMat) { return -vec3(ModelMat.Column(2)); } // Returns local -Z vector in world space
}

void engine::Camera::Update(float dt)
{
	GameObject::Update(dt);

	Input* input = GetInput();

	mat4 transform;
	compositeTransform(transform, pos_, rot_, vec3(1, 1, 1));

	vec3 rightDirection = getRightDirection(transform);
	vec3 forwardDirection = getBackDirection(transform);

	float dS = MOVE_SPEED * dt;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_A))
		pos_ -= rightDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_D))
		pos_ += rightDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_W))
		pos_ += forwardDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_S))
		pos_ -= forwardDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_Q))
		pos_.z -= dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_E))
		pos_.z += dS;

	if (input->IsMoisePressed(MOUSE_BUTTON::LEFT))
	{
		vec2 deltaMouse = input->GetMouseDeltaPos();

		quat dxRot = quat(-deltaMouse.y * ROTATE_SPEED, 0.0f, 0.0f);
		quat dyRot = quat(0.0f, 0.0f, -deltaMouse.x * ROTATE_SPEED);

		rot_ = dyRot * rot_ * dxRot;
	}
}

void engine::Camera::SaveYAML(void *yaml)
{
	GameObject::SaveYAML(yaml);

	//YAML::Emitter *_n = static_cast<YAML::Emitter*>(yaml);
	//YAML::Emitter& n = *_n;

	//n << YAML::Key << "zNear" << YAML::Value << znear;
	//n << YAML::Key << "zFar" << YAML::Value << zfar;
	//n << YAML::Key << "fovAngle" << YAML::Value << fovAngle;
}

void engine::Camera::LoadYAML(void *yaml)
{
	GameObject::LoadYAML(yaml);

	//YAML::Node *_n = static_cast<YAML::Node*>(yaml);
	//YAML::Node& n = *_n;

	//if (n["zNear"]) znear = n["zNear"].as<float>();
	//if (n["zFar"]) zfar = n["zFar"].as<float>();
	//if (n["fovAngle"]) fovAngle = n["fovAngle"].as<float>();
}

void engine::Camera::Copy(GameObject * original)
{
	GameObject::Copy(original);
	Camera *original_cam = static_cast<Camera*>(original);
	fovAngle = original_cam->fovAngle;
	zfar = original_cam->zfar;
	znear = original_cam->znear;
}

engine::Camera::Camera()
{
	type_ = OBJECT_TYPE::CAMERA;
	SetWorldPosition(vec3(0.0f, -5.0f, 5.0f));
	SetLocalRotation(quat(65.0f, 0.0f, 0.0f));
}

auto engine::Camera::Clone() -> GameObject *
{
	Camera *l = new Camera;
	l->Copy(this);
	return l;
}

void engine::Camera::GetProjectionMatrix(mat4& p, float aspect)
{
	p = math::perspectiveRH_ZO(fovAngle * math::DEGTORAD, aspect, znear, zfar);
}

void engine::Camera::GetModelViewProjectionMatrix(mat4& mvp, float aspect)
{
	mat4 V;
	GetViewMatrix(V);

	mat4 P;
	GetProjectionMatrix(P, aspect);

	mvp = P * V;
}

void engine::Camera::GetViewMatrix(mat4& m)
{
	mat4 transform;
	compositeTransform(transform, pos_, rot_, vec3(1, 1, 1));
	m = transform.Inverse();
}

auto engine::Camera::GetFullVerFOVInDegrees() -> float
{
	return fovAngle;
}

auto engine::Camera::GetFullVertFOVInRadians() -> float
{
	return fovAngle * math::DEGTORAD;
}

