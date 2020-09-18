#include "imanipulator.h"

math::vec4 AxesColors[3] = {{1,0,0,1},{0,1,0,1},{0,0,1,1}};
extern const char* PrimitiveShaderName = "primitive.hlsl";
math::vec3 AxesEndpoints[3] = {math::vec3(1, 0, 0), math::vec3(0, 1, 0), math::vec3(0, 0, 1)};
float MaxDistanceInPixels = 1000000.0f;
float SelectionThresholdInPixels = 8.0f;

IManupulator::IManupulator(QObject *parent) : QObject(parent)
{
}

AxisIntersection intersectMouseWithAxis(const CameraData& cam, const math::mat4 selectionWS, const QRect &screen, const math::vec2 &normalizedMousePos, const math::vec3 &axisDirWS, MANIPULATOR_ELEMENT type)
{
	AxisIntersection out;

	math::vec3 center = selectionWS.Column3(3);
	math::vec4 center4 = math::vec4(selectionWS.Column3(3));

	math::vec3 V = (center - cam.pos).Normalized();
	math::vec3 VcrossAxis = V.Cross(axisDirWS).Normalized();
	math::vec3 N = axisDirWS.Cross(VcrossAxis).Normalized();

	Plane plane(N, center);

	Ray ray = MouseToRay(cam.WorldTransform, cam.fovInDegrees, cam.aspect, normalizedMousePos);

	if (RayPlaneIntersection(out.worldPos, plane, ray))
	{
	   math::vec2 A = NdcToScreen(WorldToNdc(center, cam.ViewProjMat), screen);

	   math::vec4 axisEndpointLocal = math::vec4(AxesEndpoints[(int)type] * axisScale(center4, cam.ViewMat, cam.ProjectionMat, QPoint(screen.width(), screen.height())));
	   math::vec4 axisEndpointWorld = selectionWS * axisEndpointLocal;
	   math::vec2 B = NdcToScreen(WorldToNdc(math::vec3(axisEndpointWorld), cam.ViewProjMat), screen);

	   math::vec2 I = NdcToScreen(WorldToNdc(out.worldPos, cam.ViewProjMat), screen);

	   out.minDistToAxes = PointToSegmentDistance(A, B, I);
	   return out;
	}

	out.minDistToAxes = MaxDistanceInPixels;

	return out;
}
