#pragma once
#include "vmath.h"

class Camera
{
	const float fov = 60;
	const float zfar = 1000;
	const float znear = 0.1f;
	const float moveSpeed = 20;
	const float rotateSpeed = 0.1f;

	vec3 position{0.0f, -15.0f, 5.0f};
	quat rotation {80.0f, 0.0f, 0.0f};

	static void sUpdate(float dt);

public:

	Camera();

	void Update(float dt);
	void GetPerspectiveMat(mat4& p, float aspect);
	void GetViewMat(mat4& m);
};