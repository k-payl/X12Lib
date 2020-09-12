
#include "gpuprofiler.h"
#include "dx12memory.h"
#include "core.h"
#include <fstream>
#include <string>
#include <cstdio>
#include <cinttypes>

using namespace math;
using namespace engine;

vec4 engine::graphData[4096];
std::vector<FontChar> engine::fontData;

bool GpuProfiler::updateViewport(unsigned w, unsigned h)
{
	if (lastWidth != w || lastHeight != h)
	{
		viewport[0] = float(w);
		viewport[1] = float(h);
		viewport[2] = 1.0f / w;
		viewport[3] = 1.0f / h;
		return true;
	}
	return false;
}

void GpuProfiler::ProcessRecords()
{
	for (int g = 0; g < records.size(); ++g)
	{
		RenderProfilerRecord* r = records[g];
		if (!r->dirty)
			continue;

		r->CreateBuffer();

		float offsetInPixels = float(fontMarginInPixels);

		static std::hash<std::string> hashFn;
		size_t newChecksum = hashFn(r->text);

		if (r->textChecksum != newChecksum)
		{
			r->textChecksum = newChecksum;

			r->size = (uint32_t)r->text.size() * 6;
			int i = 0;
			for (uint32_t j = 0; j < r->size; j += 6, ++i)
			{
				int ascii = r->text[i];
				FontChar& char_ = fontData[ascii];

				graphData[j + 0] = vec4(0.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 1] = vec4(0.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 2] = vec4(1.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 3] = vec4(0.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 4] = vec4(1.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 5] = vec4(1.0f, 0.0f, (float)ascii, offsetInPixels);
				offsetInPixels += char_.xadvance + rectPadding;
			}

			r->UpdateBuffer(graphData);
			r->dirty = false;
		}
	}
}

void GpuProfiler::Render(void* c, int width, int height)
{
	w = width;
	h = height;

	Begin(c);

	if (updateViewport(w, h))
		UpdateViewportConstantBuffer();

	ProcessRecords();

	DrawRecords(records.size());
	
	//if (w != lastWidth)
	//	for(int i = 0; i < graphs.size(); ++i)
	//	{
	//		if (graphs[i])
	//			graphs[i]->RecreateVB(w);
	//	}

	BeginGraph();

	for (int i = 0; i < renderRecords.size(); ++i)
	{
		if (!renderRecords[i])
			continue;

		if (w != lastWidth)
			graphs[i]->RecreateVB(w);

		RenderGraph(graphs[i], records[i]->floatValue * 30, records[i]->color);
	}

	
	//RenderGraph(graphs[1], ctx.gpu_ * 30, vec4(1, 0, 0.5f, 1));

	lastWidth = w;
	lastHeight = h;

	End();
}

void GpuProfiler::loadFont()
{
	std::fstream file(fontDataPath, std::ofstream::in);

	char buffer[4096];
	file.getline(buffer, 4096);
	file.getline(buffer, 4096);
	sscanf_s(buffer, "common lineHeight=%f", &fntLineHeight);
	file.getline(buffer, 4096);
	file.getline(buffer, 4096);

	int charsNum;
	sscanf_s(buffer, "chars count=%d", &charsNum);

	fontData.resize(256);

	for (int i = 0; i < charsNum; ++i)
	{
		file.getline(buffer, 4096);

		int id;
		sscanf_s(buffer, "char id=%d ", &id);

		if (id > 255)
			continue;

		FontChar& item = fontData[id];
		sscanf_s(buffer, "char id=%d x=%f y=%f width=%f height=%f xoffset=%f yoffset=%f xadvance=%f", &id, &item.x, &item.y, &item.w, &item.h, &item.xoffset, &item.yoffset, &item.xadvance);
	}

	file.close();
}

void GpuProfiler::free()
{
	for (RenderProfilerRecord* &r : records)
	{
		delete r;
		r = 0;
	}

	for (GraphRenderer* g : graphs)
		delete g;

	graphs.clear();
}

void GpuProfiler::RenderGraph(GraphRenderer* g, float value, const vec4& color)
{
	g->Render(getContext(), color, value, w, h);

	g->lastGraphValue = vec4((float)g->graphRingBufferOffset, h - value, 0, 0);
	g->graphRingBufferOffset++;

	if (g->graphRingBufferOffset >= w)
	{
		g->lastGraphValue.x = 0;
		g->graphRingBufferOffset = 0;
	}
}

