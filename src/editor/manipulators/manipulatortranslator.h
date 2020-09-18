#ifndef MANIPULATORTRANSLATOR_H
#define MANIPULATORTRANSLATOR_H
#include "imanipulator.h"

class ManipulatorTranslator : public IManupulator
{
	enum class STATE
	{
		NONE,
		MOVING_ARROW_HANDLE,
		MOVING_PLANE_HANDLE
	};

	MANIPULATOR_ELEMENT underMouse = MANIPULATOR_ELEMENT::NONE;
	STATE state = STATE::NONE;
	math::vec3 worldDelta;
	math::vec2 oldNormalizedMousePos;
	Ray movesAlongLine;
	Plane movesAlongPlane;

public:
	ManipulatorTranslator();
	virtual ~ManipulatorTranslator();

	void render(const CameraData& cam, const math::mat4& selectionTransform, const QRect& screen) override;
	void updateMouse(const CameraData& cam, const math::mat4& selectionTransform, const QRect& screen, const math::vec2& normalizedMousePos) override;
	bool isMouseIntersect(const math::vec2 &normalizedMousePos) override;
	void mousePress(const CameraData& cam, const math::mat4& selectionTransform, const QRect &screen, const math::vec2& normalizedMousePos) override;
	void mouseRelease() override;
};

#endif // MANIPULATORTRANSLATOR_H
