#include "rt_common.h"
#include "rt_resources.h"

[shader("raygeneration")] 
void RayGen() 
{
    uint pixelNum = DispatchRaysIndex().x;

    // Initialize the ray payload
    HitInfo payload;
    payload.colorAndDistance = float4(0, 0, 0, 0);

    RayDesc ray;

#if PRIMARY_RAY
    uint2 launchIndex = uint2(pixelNum % gCamera.width, pixelNum / gCamera.width);
    float2 dims = uint2(gCamera.width, gCamera.height);

    uint frame = gFrame.frame % 9;
    float2 jitter = float2(float(frame % 3), float(frame / 3));
    jitter /= 2.0f;
    jitter -= float2(0.5, 0.5);

    float2 ndc = float2(
        float(launchIndex.x + jitter.x) / dims.x * 2 - 1,
        float(dims.y - launchIndex.y - 1 + jitter.y) / dims.y * 2 - 1);
    
    ray.Origin = gCamera.origin.xyz;
    ray.Direction = GetWorldRay(ndc, gCamera.forward.xyz, gCamera.right.xyz, gCamera.up.xyz);
#else
    pixelNum = gRegroupedIndexes[pixelNum];

    ray.Origin = gRayInfo[pixelNum].originFlags.xyz;
    ray.Direction = gRayInfo[pixelNum].direction.xyz;
#endif

    ray.TMin = 0;
    ray.TMax = 1000;

    // Trace the ray
    TraceRay(
        // Parameter name: AccelerationStructure
        // Acceleration structure
        gSceneBVH,
    
        // Parameter name: RayFlags
        // Flags can be used to specify the behavior upon hitting a surface
        RAY_FLAG_NONE,
    
        // Parameter name: InstanceInclusionMask
        // Instance inclusion mask, which can be used to mask out some geometry to
        // this ray by and-ing the mask with a geometry mask. The 0xFF flag then
        // indicates no geometry will be masked
        0xFF,
    
        // Parameter name: RayContributionToHitGroupIndex
        // Depending on the type of ray, a given object can have several hit
        // groups attached (ie. what to do when hitting to compute regular
        // shading, and what to do when hitting to compute shadows). Those hit
        // groups are specified sequentially in the SBT, so the value below
        // indicates which offset (on 4 bits) to apply to the hit groups for this
        // ray. In this sample we only have one hit group per object, hence an
        // offset of 0.
        0, // camera ray
    
        // Parameter name: MultiplierForGeometryContributionToHitGroupIndex
        // The offsets in the SBT can be computed from the object ID, its instance
        // ID, but also simply by the order the objects have been pushed in the
        // acceleration structure. This allows the application to group shaders in
        // the SBT in the same order as they are added in the AS, in which case
        // the value below represents the stride (4 bits representing the number
        // of hit groups) between two consecutive objects.
        1,
    
        // Parameter name: MissShaderIndex
        // Index of the miss shader to use in case several consecutive miss
        // shaders are present in the SBT. This allows to change the behavior of
        // the program when no geometry have been hit, for example one to return a
        // sky color for regular rendering, and another returning a full
        // visibility value for shadow rays. This sample has only one miss shader,
        // hence an index 0
        0,
    
        // Parameter name: Ray
        // Ray information to trace
        ray,
    
        // Parameter name: Payload
        // Payload associated to the ray, which will be used to communicate
        // between the hit/miss shaders and the raygen
        payload);

#if PRIMARY_RAY
    gOutput[pixelNum] = float4(payload.colorAndDistance.rgb, 0);
#else
    gOutput[pixelNum] += float4(payload.colorAndDistance.rgb, 0); // TODO: ideally should "=" because hit can happen multiplies times in same point
#endif
}

