#pragma once
#include "common.h"
#include "intrusiveptr.h"

struct RenderContext
{
	float cpu_;
	float gpu_;
};


struct Graph;

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

	struct Impl;
	Impl* impl{};

	void recreateGraphBuffer(Graph&, int width);

public:
	GpuProfiler();
	~GpuProfiler();

	void loadFont();

	void Init();
	void Free();
	void Render(const RenderContext& ctx);
};

