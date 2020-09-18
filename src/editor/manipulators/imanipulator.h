#ifndef MANIPULATORABSTRACT_H
#define MANIPULATORABSTRACT_H

#include <QObject>
#include "vmath.h"
#include "../editor_common.h"

enum class MANIPULATOR_ELEMENT
{
	NONE = -1,
	X,
	Y,
	Z,
	XY,
	YZ,
	ZX
};

inline bool isAxisMovement(MANIPULATOR_ELEMENT e)
{
	return MANIPULATOR_ELEMENT::NONE < e && e <= MANIPULATOR_ELEMENT::Z;
}

inline bool isPlaneMovement(MANIPULATOR_ELEMENT e)
{
	return MANIPULATOR_ELEMENT::XY <= e && e <= MANIPULATOR_ELEMENT::ZX;
}

class IManupulator : public QObject
{
	Q_OBJECT

public:
	explicit IManupulator(QObject *parent = nullptr);
	virtual ~IManupulator() = default;

	void virtual render(const CameraData& cam, const math::mat4& selectionTransform, const QRect& screen) = 0;
	void virtual updateMouse(const CameraData& cam, const math::mat4& selectionTransform, const QRect& screen, const math::vec2 &normalizedMousePos) = 0;
	bool virtual isMouseIntersect(const math::vec2 &normalizedMousePos) = 0;
	void virtual mousePress(const CameraData& cam, const math::mat4& selectionTransform,const QRect &screen, const math::vec2 &normalizedMousePos) = 0;
	void virtual mouseRelease() = 0;
};

extern float SelectionThresholdInPixels;
extern math::vec4 AxesColors[3];
extern math::vec4 ColorYellow;
extern math::vec4 ColorTransparent;
extern const char* PrimitiveShaderName;
extern math::vec3 AxesEndpoints[3];
extern float MaxDistanceInPixels;
extern float SelectionThresholdInPixels;

struct AxisIntersection
{
	float minDistToAxes;
	math::vec3 worldPos;
};

AxisIntersection intersectMouseWithAxis(const CameraData& cam,
										const math::mat4 selectionWS,
										const QRect &screen,
										const math::vec2 &normalizedMousePos,
										const math::vec3 &axisDirWS,
										MANIPULATOR_ELEMENT type);

#endif // MANIPULATORABSTRACT_H
