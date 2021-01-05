#include "../cpp_hlsl_shared.h"
#include "rt_common.h"

RWStructuredBuffer<uint> gHitCounter : register(u0);
RWStructuredBuffer<uint> gRegroupedRayIndexes : register(u1);
StructuredBuffer<engine::Shaders::RayInfo> gRayInfo : register(t0);

cbuffer CameraBuffer : register(b0)
{
	uint maxSize_x;
};

groupshared uint global_offset;
groupshared uint local_offset_counter;

[numthreads(256, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID, uint3 localThreadId : SV_GroupThreadID)
{
	if (globalThreadId.x >= maxSize_x)
		return;

	bool hit = asuint(gRayInfo[globalThreadId.x].origin.w) & RAY_FLAG_HIT;

	if (globalThreadId.x == 0)
	{
		gHitCounter[0] = 0;
	}

	if (localThreadId.x == 0)
	{
		local_offset_counter = 0;
	}

	GroupMemoryBarrierWithGroupSync();

	uint offset;

	// Increment local offset
	if (hit)
	{
		InterlockedAdd(local_offset_counter, 1, offset);
	}

	GroupMemoryBarrierWithGroupSync();

	// Add number of hits to global hit counter
	if (localThreadId.x == 0)
	{
		InterlockedAdd(gHitCounter[0], local_offset_counter, global_offset);
	}

	GroupMemoryBarrierWithGroupSync();

	if (hit)
	{
		gRegroupedRayIndexes[global_offset + offset] = globalThreadId.x;
	}
}
