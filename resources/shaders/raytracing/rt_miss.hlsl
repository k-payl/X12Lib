#include "rt_common.h"
#include "rt_resources.h"

[shader("miss")] 
void Miss(inout HitInfo payload : SV_RayPayload)
{
	payload.colorAndDistance.rgb = SkyColor;

#if !PRIMARY_RAY
	uint pixelNum = DispatchRaysIndex().x;
	float throughput = gRayInfo[gRegroupedIndexes[pixelNum]].direction.w;
	payload.colorAndDistance.rgb *= throughput;
#endif
}
