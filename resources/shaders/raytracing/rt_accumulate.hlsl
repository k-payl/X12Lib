#include "../cpp_hlsl_shared.h"

StructuredBuffer<float4> gIn : register(t0);
RWTexture2D<float4> gInOut : register(u0);

cbuffer CameraBuffer : register(b0)
{
	uint maxSize_x;
	uint width;
};

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= maxSize_x)
		return;

	uint2 textureIndex = uint2(dispatchThreadId.x % width, dispatchThreadId.x / width);
	float4 oldcolor = gInOut.Load(int3(textureIndex, 0));
	float4 newcolor = gIn[dispatchThreadId.x];

	float a = oldcolor.a / (oldcolor.a + 1.0f);

	gInOut[textureIndex] = float4(newcolor.rgb * (1 - a) + oldcolor.rgb * a, oldcolor.a + 1);
}
