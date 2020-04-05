#pragma once
#include "common.h"
#include <array>

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
	uint64_t uniformUpdates;
	uint64_t stateChanges;
	uint64_t triangles;
	uint64_t draws;
};

struct GraphRenderer
{
	uint32_t graphRingBufferOffset{ 0 };
	vec4 lastGraphValue;
	vec4 lastColor;

	virtual ~GraphRenderer() = default;
	virtual void Render(void* c, vec4 color, float value, unsigned w, unsigned h) = 0;
	virtual void RecreateVB(unsigned w) = 0;
};

struct RenderProfilerRecord
{
	bool dirty{ true };
	std::string format;
	std::string text;
	size_t textChecksum{};
	uint32_t size{};
	bool isFloat{false};
	uint64_t intValue{};
	float floatValue{};
	bool renderGraph{false};
	vec4 color{ 0.6f, 0.6f, 0.6f, 1.0f };

	RenderProfilerRecord(const char* format_, bool isFloat_, bool renderGraph_)
	{
		format = format_;
		text = format;
		isFloat = isFloat_;
		renderGraph = renderGraph_;
	}
	virtual ~RenderProfilerRecord() = default;
	virtual void UpdateBuffer(void* data) = 0;
	virtual void CreateBuffer() = 0;
};

class GpuProfiler
{
protected:
	const char* fontDataPath = "..//font//1.fnt";
	const wchar_t* fontTexturePath = L"..//font//1_0.dds";
	const int rectSize = 100;
	const int rectPadding = 1;
	const int fontMarginInPixels = 5;
	const vec4 color;
	const float verticalOffset{100};

	unsigned w, h;
	float fntLineHeight = 20;
	float viewport[4];
	unsigned lastWidth{ 0 };
	unsigned lastHeight{ 0 };

	std::vector<GraphRenderer*> graphs;
	std::vector<RenderProfilerRecord*> records;
	std::vector<bool> renderRecords;

	bool updateViewport(unsigned w, unsigned h);
	void loadFont();
	void free();

	virtual void Begin() = 0;
	virtual void BeginGraph() = 0;
	virtual void UpdateViewportConstantBuffer() = 0;
	virtual void DrawRecords(int maxRecords) = 0;
	void RenderGraph(GraphRenderer* g, float value, const vec4& color);
	virtual void* getContext() = 0;

public:
	GpuProfiler(vec4 color_, float verticalOffset_) :
		color(color_), verticalOffset(verticalOffset_) {}
	virtual ~GpuProfiler() = default;

	virtual void AddRecord(const char* format, bool isFloat, bool renderGraph) = 0;
	void SetRecordColor(size_t index, vec4 color)
	{
		if (records[index])
			records[index]->color = color;
	}

	template<typename ... Args>
	void UpdateRecord(size_t num, Args ...args)
	{
		char buf[128];
		sprintf_s(buf, records[num]->format.c_str(), args...);

		if (records[num]->text == buf)
			return;

		records[num]->text = buf;
		records[num]->dirty = true;

		typedef typename std::common_type<Args...>::type common;
		const std::array<common, sizeof...(Args)> a = { { args... } };

		if (records[num]->isFloat)
			records[num]->floatValue = a[0];
		else
			records[num]->intValue = a[0];
	}

	void ProcessRecords();

	void Render();

	virtual void Init() = 0;
	virtual void Free() = 0;

};



