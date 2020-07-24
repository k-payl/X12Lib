#pragma once
#include "common.h"

namespace engine
{
	class Render
	{
	public:
		void Init();
		void Free();
		void Update();
		void RenderFrame(const ViewportData& viewport, const engine::CameraData& camera);
	};
}
