#pragma once
#include "common.h"

struct TransformConstantBuffer
{
	float y;
	float _align[3];
};

struct FontChar
{
	float x, y;
	float w, h;
	float xoffset, yoffset;
	float xadvance;
	int _align;
};

extern vec4 graphData[4096];
extern std::vector<FontChar> fontData;


struct RenderPerfomanceData
{
	float cpu_;
	float gpu_;

	bool extended;
	uint64_t uniformUpdates;
	uint64_t stateChanges;
	uint64_t triangles;
	uint64_t draws;
};

struct Graph
{
	uint32_t graphRingBufferOffset{ 0 };
	vec4 lastGraphValue;
	vec4 lastColor;

	virtual ~Graph() = default;
	virtual void Render(void* c, vec4 color, float value, unsigned w, unsigned h) = 0;
	virtual void RecreateVB(unsigned w) = 0;
};

struct RenderProfilerRecord
{
	std::string text;
	size_t textChecksum;
	uint32_t size;

	virtual ~RenderProfilerRecord() = default;
	virtual void UpdateBuffer(void* data) = 0;
	virtual void CreateBuffer() = 0;
};

constexpr inline int recordsNum = 7;
constexpr inline int GraphsCount = 2;


class GpuProfiler
{
protected:
	const char* fontDataPath = "..//font//1.fnt";
	const wchar_t* fontTexturePath = L"..//font//1_0.dds";

	unsigned w, h;
	const int rectSize = 100;
	const int rectPadding = 1;
	const int fontMarginInPixels = 5;
	float fntLineHeight = 20;
	float viewport[4];
	unsigned lastWidth{ 0 };
	unsigned lastHeight{ 0 };	
	std::vector<Graph*> graphs;
	RenderProfilerRecord* records[recordsNum];

	bool updateViewport(unsigned w, unsigned h);
	void loadFont();
	void free();

	virtual void Begin() = 0;
	virtual void BeginGraph() = 0;
	virtual void UpdateViewportConstantBuffer() = 0;
	virtual void DrawFont(int maxRecords) = 0;
	void RenderGraph(Graph* g, float value, const vec4& color);
	virtual void* getContext() = 0;

public:
	virtual ~GpuProfiler() = default;

	void Render(const RenderPerfomanceData& ctx);

	virtual void Init() = 0;
	virtual void Free() = 0;

};



