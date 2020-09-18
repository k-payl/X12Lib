#ifndef MANIPULATORSCALE_H
#define MANIPULATORSCALE_H
#include "imanipulator.h"

class ManipulatorScale : public IManupulator
{
	enum class STATE
	{
		NONE,
		SCALING_ARROW_HANDLE
	};

	STATE state{STATE::NONE};
	MANIPULATOR_ELEMENT underMouse = MANIPULATOR_ELEMENT::NONE;
	math::vec3 worldDelta;
	float initProjectedAxisDistance{1.0f};
	Ray movesAlongLine;
	math::vec3 originalLocalScale;
	math::vec3 scaleVector{1.f, 1.f, 1.f};

public:
	ManipulatorScale();
	~ManipulatorScale();

	void render(const CameraData& cam, const math::mat4& selectionTransform, const QRect& screen) override;
	void updateMouse(const CameraData& cam, const math::mat4& selectionTransform, const QRect& screen, const math::vec2 &normalizedMousePos) override;
	bool isMouseIntersect(const math::vec2 &normalizedMousePos) override;
	void mousePress(const CameraData& cam, const math::mat4& selectionTransform, const QRect &screen, const math::vec2 &normalizedMousePos) override;
	void mouseRelease() override;

};

#endif // MANIPULATORSCALE_H
