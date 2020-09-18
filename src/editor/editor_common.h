#ifndef COMMON_H
#define COMMON_H
#undef min
#undef max

#include "vmath.h"
#include <QString>
#include <QPoint>
#include <QRect>

#define ROOT_PATH "../../" // relative working directory
#define LIGHT_THEME ROOT_PATH "resources/editor/light.qss"
#define DARK_THEME ROOT_PATH "resources/editor/dark.qss"

struct CameraData
{
	math::vec3 pos;
	math::quat rot;
	float fovInDegrees;
	float fovInRads;
	float aspect;
	math::mat4 WorldTransform;
	math::mat4 ViewMat;
	math::mat4 ProjectionMat;
	math::mat4 ViewProjMat;
	math::vec3 ViewWorldDirection;
};


QString vec3ToString(const math::vec3& v);
QString quatToString(const math::quat& v);
QString vec2ToString(const math::vec2& v);
float clamp(float f);

template<typename T>
T lerp(const T& l, const T& r, float v)
{
	v = clamp(v);
	return l * (1.0f - v) + r * v;
}

inline float clamp(float n, float lower, float upper)
{
	return std::max(lower, std::min(n, upper));
}

struct Spherical
{
	float r;
	float theta;
	float phi;
};

QString sphericalToString(const Spherical& v);

Spherical ToSpherical(const math::vec3& pos);
math::vec3 ToCartesian(const Spherical& pos);

float axisScale(const math::vec4& worldPos, const math::mat4& View, const math::mat4& Proj, const QPoint& screenSize);

void lookAtCamera(math::mat4& Result, const math::vec3 &eye, const math::vec3 &center);

bool PointInTriangle(const math::vec2& pt, const math::vec2& v1, const math::vec2& v2, const math::vec2& v3);

struct Plane
{
	math::vec3 origin;
	math::vec3 normal;

	Plane() = default;
	Plane(const math::vec3& normalIn, const math::vec3& originIn) :
		origin(originIn), normal(normalIn){}
};


struct Ray
{
	math::vec3 origin;
	math::vec3 direction;

	Ray() = default;
	Ray(const math::vec3& directionlIn, const math::vec3& originIn) :
		origin(originIn), direction(directionlIn){}

	math::vec3 projectPoint(math::vec3 &worldPos);
};

class QLabel;
void setLabel(QLabel *l, float val);

inline bool isTexture(const QString& str)
{
	return str.endsWith(".dds");
}


bool RayPlaneIntersection(math::vec3& intersection, const Plane& plane, const Ray& line);
Ray MouseToRay(const math::mat4& cameraModelMatrix, float fov, float aspect, const math::vec2& normalizedMousePos);
math::vec2 WorldToNdc(const math::vec3& pos, const math::mat4& ViewProj);
float PointToSegmentDistance(const math::vec2& p0, const math::vec2& p1, const math::vec2& ndc);
math::vec2 NdcToScreen(const math::vec2& pos, const QRect& screen);
float DistanceTo(const math::mat4& ViewProj, const math::mat4& worldTransform);
#endif // COMMON_H
