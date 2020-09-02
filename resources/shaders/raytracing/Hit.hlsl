#include "Common.hlsl"
#include "Global.hlsl"


struct InstanceData
{
    float4 color;
	float4x4 transform;
};
ConstantBuffer<InstanceData> instanceData : register(b1);

struct Vertex
{
    float3 position;
    float3 normal;
    float2 tex;
};
StructuredBuffer<Vertex> Vertices : register(t2, space0);


[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

#if 0 // Indexing

    // Get the base index of the triangle's first 16 bit index.
    const uint indexSizeInBytes = 2;
    const uint indicesPerTriangle = 3;
    uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
    uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	const uint3 indices = Load3x16BitIndices(baseIndex);
    

#else
	
	const uint3 indices = uint3(PrimitiveIndex() * 3, PrimitiveIndex() * 3 + 1, PrimitiveIndex() * 3 + 2);

#endif

	float3 vertextPositions[3] = {
        Vertices[indices[0]].position,
        Vertices[indices[1]].position,
        Vertices[indices[2]].position
    };
	
    float3 vertexNormals[3] = {
        Vertices[indices[0]].normal,
        Vertices[indices[1]].normal,
        Vertices[indices[2]].normal
        };
	
    float3 N = vertexNormals[0] * barycentrics.x +
        vertexNormals[1] * barycentrics.y +
        vertexNormals[2] * barycentrics.z;

    float3 pos = vertextPositions[0] * barycentrics.x +
        vertextPositions[1] * barycentrics.y +
        vertextPositions[2] * barycentrics.z;
		
	pos += N * 0.001f;
	float4 worldPos = mul(float4(pos, 1), instanceData.transform);
	
	const float3 light = float3(5, -4, 10);
	const float3 Ln = normalize(light - worldPos.xyz);
	float s = 0;
	
	RayDesc ray;
	ray.Origin = worldPos.xyz;
	ray.Direction = Ln;
	ray.TMin = 0;
	ray.TMax = 100000;

	#if 0 // inline ray trace
		RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

		// Set up a trace. No work is done yet.
		q.TraceRayInline(
			SceneBVH,
			0,
			0xFF,
			ray);

		// trace ray
		q.Proceed();

		// Examine and act on the result of the traversal.
		// Was a hit committed?
		if(q.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
		{
			s = 1;
		}
	#else
		HitInfo shadowPayload;
				
		TraceRay(
			SceneBVH,
			RAY_FLAG_NONE,
			0xFF,
		
			// Parameter name: RayContributionToHitGroupIndex
			gScene.numInstances,
		
			// Parameter name: MultiplierForGeometryContributionToHitGroupIndex
			0,
		
			// Parameter name: MissShaderIndex
			1,
			ray,
			shadowPayload);
			
		s = shadowPayload.colorAndDistance.a;
		
	#endif

	payload.colorAndDistance = float4(instanceData.color.rgb * max(0, dot(N, Ln)) * (1 - s), RayTCurrent());
	//payload.colorAndDistance = float4(worldPos.xyz, RayTCurrent());
}

[shader("closesthit")] 
void ShadowClosestHit(inout HitInfo payload, Attributes attrib)
{
	payload.colorAndDistance = float4(0, 0, 0, 1);
}
