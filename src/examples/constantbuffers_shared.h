#include <memory>
#include <fstream>
#include <filesystem>

using namespace math;

namespace fs = std::filesystem;

#define UPD_INTERVAL 0.5f

#define USE_PROFILER_REALTIME
//#define USE_PROFILE_TO_CSV

const auto numCubesX = 1;
const auto numCubesY = 10;

static vec4 colors[] = { vec4(1,1,1,1),vec4(0,1,0,1),vec4(0,0,1,1),vec4(1,0,0,1) };

inline mat4 cubePosition(int i, int j, float offset)
{
	mat4 ret;

	vec3 pos = vec3(6.0f * (j - numCubesY / 2), offset, -6);
	ret.SetColumn3(3, pos);

	return ret;
}

inline vec4 cubeColor(int i, int j)
{
	return colors[(i * numCubesX + j) % _countof(colors)];
}

struct DynamicCB
{
	vec4 color_out;
	mat4 transform;
	mat4 NM;
};

struct VertexPosColor
{
	vec4 Position;
	vec4 Color;
};

#if 1
const uint32_t veretxCount = 8;
VertexPosColor vertexData[veretxCount] = {
	{ vec4(-1.0f,-1.0f, -1.0f, 1.0f) * 0.5f, vec4(0.0f, 0.0f, 0.0f, 1.0f) }, // 0
	{ vec4(-1.0f, 1.0f, -1.0f, 1.0f) * 0.5f, vec4(0.0f, 1.0f, 0.0f, 1.0f) }, // 1
	{ vec4(1.0f,  1.0f, -1.0f, 1.0f) * 0.5f, vec4(1.0f, 1.0f, 0.0f, 1.0f) }, // 2
	{ vec4(1.0f, -1.0f, -1.0f, 1.0f) * 0.5f, vec4(1.0f, 0.0f, 0.0f, 1.0f) }, // 3
	{ vec4(-1.0f,-1.0f,  1.0f, 1.0f) * 0.5f, vec4(0.0f, 0.0f, 1.0f, 1.0f) }, // 4
	{ vec4(-1.0f, 1.0f,  1.0f, 1.0f) * 0.5f, vec4(0.0f, 1.0f, 1.0f, 1.0f) }, // 5
	{ vec4(1.0f,  1.0f,  1.0f, 1.0f) * 0.5f, vec4(1.0f, 1.0f, 1.0f, 1.0f) }, // 6
	{ vec4(1.0f, -1.0f,  1.0f, 1.0f) * 0.5f, vec4(1.0f, 0.0f, 1.0f, 1.0f) }  // 7
};
const uint32_t idxCount = 36;
static WORD indexData[idxCount] =
{
	0, 1, 2, 0, 2, 3,
	4, 6, 5, 4, 7, 6,
	4, 5, 1, 4, 1, 0,
	3, 2, 6, 3, 6, 7,
	1, 5, 6, 1, 6, 2,
	4, 0, 3, 4, 3, 7
};
#else
const uint32_t veretxCount = 4;
VertexPosColor vertexData[veretxCount] = {
	{ vec4(-1.0f,-1.0f, 0.0f, 1.0f) * 0.5, vec4(0.0f, 0.0f, 0.0f, 1.0f) }, // 0
	{ vec4(-1.0f, 1.0f, 0.0f, 1.0f) * 0.5, vec4(0.0f, 1.0f, 0.0f, 1.0f) }, // 1
	{ vec4(1.0f,  1.0f, 0.0f, 1.0f) * 0.5, vec4(1.0f, 1.0f, 0.0f, 1.0f) }, // 2
	{ vec4(1.0f, -1.0f, 0.0f, 1.0f) * 0.5, vec4(1.0f, 1.0f, 0.0f, 1.0f) }, // 2
};
const uint32_t idxCount = 6;
static WORD indexData[idxCount] =
{
	0, 1, 2,
	0, 2, 3
};

#endif

