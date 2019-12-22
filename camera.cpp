#include "pch.h"

#include "camera.h"
#include "common.h"
#include "core.h"
#include "input.h"

static Camera* instance;

void Camera::sUpdate(float dt)
{
	instance->Update(dt);
}

Camera::Camera()
{
	assert(instance == nullptr && "One instance of camera can be created");
	instance = this;

	CORE->AddUpdateProcedure(sUpdate);
}

void Camera::Update(float dt)
{
	Input* input = CORE->GetInput();

	mat4 transform;
	compositeTransform(transform, position, rotation, vec3(1, 1, 1));

	vec3 rightDirection = getRightDirection(transform);
	vec3 forwardDirection = getBackDirection(transform);

	float dS = moveSpeed * dt;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_A))
		position -= rightDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_D))
		position += rightDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_W))
		position += forwardDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_S))
		position -= forwardDirection * dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_Q))
		position.z -= dS;

	if (input->IsKeyPressed(KEYBOARD_KEY_CODES::KEY_E))
		position.z += dS;

	if (input->IsMoisePressed(MOUSE_BUTTON::LEFT))
	{
		vec2 deltaMouse = input->GetMouseDeltaPos();

		quat dxRot = quat(-deltaMouse.y * rotateSpeed, 0.0f, 0.0f);
		quat dyRot = quat(0.0f, 0.0f, -deltaMouse.x * rotateSpeed);

		rotation = dyRot * rotation * dxRot;
	}
}

void Camera::GetPerspectiveMat(mat4& p, float aspect)
{
	p = perspectiveRH_ZO(fov * DEGTORAD, aspect, znear, zfar);
}

void Camera::GetViewMat(mat4& m)
{
	mat4 transform;
	compositeTransform(transform, position, rotation, vec3(1, 1, 1));
	m = transform.Inverse();
}

