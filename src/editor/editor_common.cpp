#include "editor_common.h"
#include "common.h"
#include <algorithm>
#include <QLabel>

QString vec3ToString(const math::vec3& v)
{
	return QString('{') +
			QString::number(v.x) + QString(", ") +
			QString::number(v.y) + QString(", ") +
			QString::number(v.z) + QString('}');
}

QString vec2ToString(const math::vec2& v)
{
	return QString('{') +
			QString::number(v.x) + QString(", ") +
			QString::number(v.y) + QString('}');
}

float clamp(float f)
{
	return std::min(1.0f, std::max(0.0f, f));
}

Spherical ToSpherical(const math::vec3& pos)
{
	float r = sqrt(pos.Dot(pos));
	float theta = acos(pos.z / r);
	float phi = atan2(pos.y, pos.x);

	return Spherical{r, theta, phi};
}

math::vec3 ToCartesian(const Spherical &pos)
{
	return math::vec3(
				pos.r * sin(pos.theta) * cos(pos.phi),
				pos.r * sin(pos.theta) * sin(pos.phi),
				pos.r * cos(pos.theta));
}


QString sphericalToString(const Spherical &v)
{
	return QString("{r: ") +
			QString::number(v.r) + QString(", theta: ") +
			QString::number(v.theta) + QString(", phi: ") +
			QString::number(v.phi) + QString('}');
}

float axisScale(const math::vec4& worldPos, const math::mat4& View, const math::mat4& Proj, const QPoint& screenSize)
{
	math::vec4 p0 = Proj * View * worldPos;
	math::vec4 xx = math::vec4(View.el_2D[0][0], View.el_2D[0][1], View.el_2D[0][2], 0.0f);
	math::vec4 p1 = Proj * View * (worldPos + xx);
	p0 = p0 / abs(p0.w);
	p1 = p1 / abs(p1.w);

	float x = (p1.x - p0.x) * 0.5f * screenSize.x();
	float y = (p1.y - p0.y) * 0.5f * screenSize.y();

	return float(120.0f / (math::vec3(x, y, 0.0f).Lenght()));
}

void lookAtCamera(math::mat4& Result, const math::vec3 &eye, const math::vec3 &center)
{
	Result = math::mat4(1.0f);
	math::vec3 Z = (eye - center).Normalize();
	math::vec3 X = math::vec3(0.0f, 0.0f, 1.0f).Cross(Z).Normalize();
	math::vec3 Y(Z.Cross(X));
	Y.Normalize();
	Result.el_2D[0][0] = X.x;
	Result.el_2D[0][1] = X.y;
	Result.el_2D[0][2] = X.z;
	Result.el_2D[1][0] = Y.x;
	Result.el_2D[1][1] = Y.y;
	Result.el_2D[1][2] = Y.z;
	Result.el_2D[2][0] = Z.x;
	Result.el_2D[2][1] = Z.y;
	Result.el_2D[2][2] = Z.z;
	Result.el_2D[0][3] = -X.Dot(eye);
	Result.el_2D[1][3] = -Y.Dot(eye);
	Result.el_2D[2][3] = -Z.Dot(eye);
}

QString quatToString(const math::quat &q)
{
	return QString("{x: ") +
			QString::number(q.x) + QString(", y: ") +
			QString::number(q.y) + QString(", z: ") +
			QString::number(q.z) + QString(", w: ") +
			QString::number(q.w) + QString('}');
}

bool RayPlaneIntersection(math::vec3& intersection, const Plane& plane, const Ray& line)
{
	math::vec3 R = line.direction.Normalized();
	math::vec3 N = plane.normal.Normalized();

	float d = N.Dot(plane.origin);

	float denom = N.Dot(R);
	if (abs(denom) < 0.00001f) return false;

	float x = (d - N.Dot(line.origin)) / denom;

	intersection = line.origin + R * x;

	return true;
}

Ray MouseToRay(const math::mat4& cameraModelMatrix, float fovInDegree, float aspect, const math::vec2& ndc)
{
	math::vec3 forwardN = -cameraModelMatrix.Column3(2).Normalized();

	float y = tan(math::DEGTORAD * fovInDegree * 0.5f);
	float x = y;

	math::vec3 rightN = cameraModelMatrix.Column3(0).Normalized();
	math::vec3 right = rightN * x * aspect;

	math::vec3 upN = cameraModelMatrix.Column3(1).Normalized();
	math::vec3 up = upN * y;

	math::vec2 mousePos = ndc * 2.0f - math::vec2(1.0f, 1.0f);

	math::vec3 dir = (forwardN + right * mousePos.x + up * mousePos.y).Normalized();
	math::vec3 origin = cameraModelMatrix.Column3(3);

	return Ray(dir, origin);
}

math::vec2 WorldToNdc(const math::vec3& pos, const math::mat4& ViewProj)
{
	math::vec4 screenPos = ViewProj * math::vec4(pos);
	screenPos /= screenPos.w;
	return math::vec2(screenPos.x, screenPos.y);
}

float PointToSegmentDistance(const math::vec2& p0, const math::vec2& p1, const math::vec2& point)
{
	math::vec2 direction = math::vec2(p1.x - p0.x, p1.y - p0.y);
	math::vec2 p00 = p0;
	return (p00 + direction * clamp(direction.Dot(point - p00) / direction.Dot(direction), 0.0f, 1.0f) - point).Lenght();
}

math::vec2 NdcToScreen(const math::vec2 &ndc, const QRect& screen)
{
	math::vec2 tmp = ndc * 0.5f + math::vec2(0.5f, 0.5f);
	return math::vec2(tmp.x * screen.width(), tmp.y * screen.height());
}

float DistanceTo(const math::mat4& ViewProj, const math::mat4& worldTransform)
{
	math::vec4 view4 = ViewProj * math::vec4(worldTransform.el_2D[0][3], worldTransform.el_2D[1][3], worldTransform.el_2D[2][3], 1.0f);
	math::vec3 view(view4);
	return view.Lenght();
}

static float sig(const math::vec2& p1, const math::vec2& p2, const math::vec2& p3)
{
	return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool PointInTriangle(const math::vec2& pt, const math::vec2& v1, const math::vec2& v2, const math::vec2& v3)
{
	float d1, d2, d3;
	bool has_neg, has_pos;

	d1 = sig(pt, v1, v2);
	d2 = sig(pt, v2, v3);
	d3 = sig(pt, v3, v1);

	has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
	has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

	return !(has_neg && has_pos);
}

math::vec3 Ray::projectPoint(math::vec3 &worldPos)
{
	math::vec3 AP = worldPos - origin;
	math::vec3 AB = direction;
	return origin + direction * AP.Dot(AB);
}

void setLabel(QLabel *l, float val)
{
	QString text;
	text.sprintf("%6.2f", val);
	l->setText(text);
}
