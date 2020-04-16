#include "pch.h"
#include "vmath.h"

void math::orthonormalize(mat3& ret, const mat3& m)
{
	vec3 x = vec3(m.el_2D[0][0], m.el_2D[1][0], m.el_2D[2][0]);
	vec3 y = vec3(m.el_2D[0][1], m.el_2D[1][1], m.el_2D[2][1]);
	vec3 z = x.Cross(y);
	y = z.Cross(x);
	x.Normalize();
	y.Normalize();
	z.Normalize();
	ret.el_2D[0][0] = x.x;
	ret.el_2D[0][1] = y.x;
	ret.el_2D[0][2] = z.x;
	ret.el_2D[1][0] = x.y;
	ret.el_2D[1][1] = y.y;
	ret.el_2D[1][2] = z.y;
	ret.el_2D[2][0] = x.z;
	ret.el_2D[2][1] = y.z;
	ret.el_2D[2][2] = z.z;
}

void math::decompositeTransform(const mat4& transform, vec3& t, quat& r, vec3& s)
{
	mat3 rotate, rotation = mat3(transform);
	orthonormalize(rotate, rotation);
	t.x = transform.el_2D[0][3];
	t.y = transform.el_2D[1][3];
	t.z = transform.el_2D[2][3];
	r = quat(rotate);
	s.x = rotate.el_2D[0][0] * rotation.el_2D[0][0] + rotate.el_2D[1][0] * rotation.el_2D[1][0] + rotate.el_2D[2][0] * rotation.el_2D[2][0];
	s.y = rotate.el_2D[0][1] * rotation.el_2D[0][1] + rotate.el_2D[1][1] * rotation.el_2D[1][1] + rotate.el_2D[2][1] * rotation.el_2D[2][1];
	s.z = rotate.el_2D[0][2] * rotation.el_2D[0][2] + rotate.el_2D[1][2] * rotation.el_2D[1][2] + rotate.el_2D[2][2] * rotation.el_2D[2][2];
}

void math::compositeTransform(mat4& transform, const vec3& t, const quat& r, const vec3& s)
{
	mat4 R;
	mat4 T;
	mat4 S;

	R = r.ToMatrix();

	T.el_2D[0][3] = t.x;
	T.el_2D[1][3] = t.y;
	T.el_2D[2][3] = t.z;

	S.el_2D[0][0] = s.x;
	S.el_2D[1][1] = s.y;
	S.el_2D[2][2] = s.z;

	transform = T * R * S;
}

math::mat4 math::perspectiveRH_ZO(float fovRad, float aspect, float zNear, float zFar)
{
	float const tanHalfFovy = tan(fovRad / 2);

	mat4 Result;

	Result.el_2D[3][3] = 0.0f;
	Result.el_2D[0][0] = 1.0f / (aspect * tanHalfFovy);
	Result.el_2D[1][1] = 1.0f / (tanHalfFovy);
	Result.el_2D[2][2] = -zFar / (zFar - zNear);
	Result.el_2D[3][2] = -1.0f;
	Result.el_2D[2][3] = -(zFar * zNear) / (zFar - zNear);

	return Result;
}
