#include "../cpp_hlsl_shared.h"

RWStructuredBuffer<engine::Shaders::RayInfo> gRayInfo : register(u0);
RWStructuredBuffer<float4> gIterationBuffer : register(u1);

cbuffer CameraBuffer : register(b0)
{
	uint maxSize_x;
};

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= maxSize_x)
		return;

	gRayInfo[dispatchThreadId.x].origin = 0;
	gRayInfo[dispatchThreadId.x].direction = 0;
	gIterationBuffer[dispatchThreadId.x] = 0;
}
