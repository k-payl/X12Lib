#include "rt_common.h"
#include "rt_resources.h"

[shader("miss")] 
void Miss(inout HitInfo payload : SV_RayPayload)
{
//	payload.colorAndDistance.rgb = SkyColor;
//
//	// BRDF multiplication for secondary hit
//#if !PRIMARY_RAY
//	uint pixelNum = DispatchRaysIndex().x;
//	float3 brdf = gRayInfo[gRegroupedIndexes[pixelNum]].hitbrdf.xyz;
//	payload.colorAndDistance.rgb *= brdf;
//#endif
}
