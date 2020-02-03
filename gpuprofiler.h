#pragma once
#include "common.h"
#include "intrusiveptr.h"

struct RenderContext
{
	float cpu_;
	float gpu_;
};

class GpuProfiler
{
	const int rectSize = 100;
	const int rectPadding = 1;
	const int fontMarginInPixels = 5;
	float fntLineHeight = 20;

	// Common
	float viewport[4];
	unsigned lastWidth{ 0 };
	unsigned lastHeight{ 0 };
	
	uint32_t graphRingBufferOffset{0};
	vec4 lastGraphValue;

	struct Impl;
	Impl* impl{};

	void recreateGraphBuffer(int width);

public:
	GpuProfiler();
	~GpuProfiler();

	void loadFont();

	void Init();
	void Free();
	void Render(const RenderContext& ctx);
};

