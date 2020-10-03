#include "cpp_hlsl_shared.h"

RWTexture2D<float4> tex : register(u0);

cbuffer CameraBuffer : register(b0)
{
	uint maxSize_x;
	uint maxSize_y;
};

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x < maxSize_x && dispatchThreadId.y < maxSize_y)
		tex[dispatchThreadId.xy] = float4(0, 0, 0, 0);
}
