#include "common.hlsl"

[shader("miss")] 
void Miss(inout HitInfo payload : SV_RayPayload)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);
	
	float ramp = launchIndex.y / dims.y;
	payload.colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
}

[shader("miss")] 
void ShadowMiss(inout HitInfo payload : SV_RayPayload)
{
	payload.colorAndDistance = float4(0, 0, 0, 0);
}
