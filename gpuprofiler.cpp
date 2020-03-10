#include "pch.h"
#include "gpuprofiler.h"
#include "dx12memory.h"
#include "core.h"
#include <fstream>
#include <string>
#include <cstdio>
#include <cinttypes>

vec4 graphData[4096];
std::vector<FontChar> fontData;

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

void GpuProfiler::Render(const RenderPerfomanceData& ctx)
{
	Begin();

	if (updateViewport(w, h))
		UpdateViewportConstantBuffer();

	records[0]->text = "====Core Render Profiler====";

	char float_buff[100];

	sprintf_s(float_buff, "CPU: %0.2f ms.", ctx.cpu_);
	records[1]->text = float_buff;

	sprintf_s(float_buff, "GPU: %0.2f ms.", ctx.gpu_);
	records[2]->text = float_buff;

	if (ctx.extended)
	{
		sprintf_s(float_buff, "Uniform buffer updates: %" PRId64, ctx.uniformUpdates);
		records[3]->text = float_buff;

		sprintf_s(float_buff, "State changes: %" PRId64, ctx.stateChanges);
		records[4]->text = float_buff;

		sprintf_s(float_buff, "Triangles: %" PRId64, ctx.triangles);
		records[5]->text = float_buff;

		sprintf_s(float_buff, "Draw calls: %" PRId64, ctx.draws);
		records[6]->text = float_buff;
	}

	int maxRecords = ctx.extended ? recordsNum : 3;

	for (int g = 0; g < maxRecords; ++g)
	{
		RenderProfilerRecord* r = records[g];

		r->CreateBuffer();

		float offsetInPixels = float(fontMarginInPixels);

		static std::hash<std::string> hashFn;
		size_t newChecksum = hashFn(r->text);

		if (r->textChecksum != newChecksum)
		{
			r->textChecksum = newChecksum;

			r->size = (uint32_t)r->text.size() * 6;
			int i = 0;
			for (uint32_t j = 0; j < r->size; j+=6, ++i)
			{
				int ascii = r->text[i];
				FontChar& char_ = fontData[ascii];

				graphData[j + 0] = vec4(0.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 1] = vec4(1.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 2] = vec4(0.0f, 1.0f, (float)ascii, offsetInPixels);
				graphData[j + 3] = vec4(0.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 4] = vec4(1.0f, 0.0f, (float)ascii, offsetInPixels);
				graphData[j + 5] = vec4(1.0f, 1.0f, (float)ascii, offsetInPixels);
				offsetInPixels += char_.xadvance + rectPadding;
			}

			r->UpdateBuffer(graphData);
		}
	}

	DrawFont(maxRecords);

	// Graphs.

	if (w != lastWidth)
		for(int i = 0; i < GraphsCount; ++i)
			graphs[i]->RecreateVB(w);

	BeginGraph();

	RenderGraph(graphs[0], ctx.cpu_ * 30, vec4(0, 0.5f, 0.5f, 1));
	RenderGraph(graphs[1], ctx.gpu_ * 30, vec4(1, 0, 0.5f, 1));

	lastWidth = w;
	lastHeight = h;
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
	for (RenderProfilerRecord* r : records)
		delete r;

	for (Graph* g : graphs)
		delete g;

	graphs.clear();
}

void GpuProfiler::RenderGraph(Graph* g, float value, const vec4& color)
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
