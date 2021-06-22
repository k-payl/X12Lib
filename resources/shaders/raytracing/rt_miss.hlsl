#include "rt_common.h"
#include "rt_resources.h"

[shader("miss")] 
void Miss(inout HitInfo payload : SV_RayPayload)
{
	payload.colorAndDistance.rgb = SkyColor;

#if !PRIMARY_RAY
	float mis = 1;
	uint pixelNum = DispatchRaysIndex().x;
	const float3 throughput = gRayInfo[gRegroupedIndexes[pixelNum]].throughput.xyz;
	//float brdfPdf = gRayInfo[gRegroupedIndexes[pixelNum]].throughput.w;

	//if (!(asuint(gRayInfo[gRegroupedIndexes[pixelNum]].originFlags.w) & RAY_FLAG_SPECULAR_BOUNCE))
	//{
	//	float lightPdf = 1 / _2PI;
	//	mis = powerHeuristic(brdfPdf, lightPdf);
	//}

	payload.colorAndDistance.rgb *= throughput * mis;
#endif
}
