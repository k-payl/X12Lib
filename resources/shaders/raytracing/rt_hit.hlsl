#include "../brdf.h"
#include "rt_common.h"
#include "rt_resources.h"

static const float fi = 1.324717957244;
static uint rng_state;
static const float png_01_convert = (1.0f / 4294967296.0f); // to convert into a 01 distributio

uint wang_hash(uint seed)
{
	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);
	return seed;
}

void ComputeRngSeed(uint index, uint iteration, uint depth)
{
	rng_state = uint(wang_hash((1 << 31) | (depth << 22) | iteration) ^ wang_hash(index));
}

uint rand_xorshift()
{
	rng_state ^= uint(rng_state << 13);
	rng_state ^= uint(rng_state >> 17);
	rng_state ^= uint(rng_state << 5);
	return rng_state;
}

float Uniform01()
{
	return float(rand_xorshift() * png_01_convert);
}

float goldenRatioU1()
{
	return frac(Uniform01() / fi);
}

float goldenRatioU2()
{
	return frac(Uniform01() / (fi * fi));
}

float rnd(inout uint seed)
{
	seed = (1664525u * seed + 1013904223u);
	return ((float)(seed & 0x00FFFFFF) / (float)0x01000000);
}


[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint pixelNum = DispatchRaysIndex().x;
	//ComputeRngSeed(pixelNum, uint(payload.colorAndDistance.w), 0);

	float3 barycentrics = float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

#if 0 // Indexing
	const uint indexSizeInBytes = 2;
	const uint indicesPerTriangle = 3;
	uint triangleIndexStride = indicesPerTriangle * indexSizeInBytes;
	uint baseIndex = PrimitiveIndex() * triangleIndexStride;

	const uint3 indices = Load3x16BitIndices(baseIndex);
#else
	const uint3 indices = uint3(PrimitiveIndex() * 3, PrimitiveIndex() * 3 + 1, PrimitiveIndex() * 3 + 2);
#endif

	float3 vertextPositions[3] = {
		lVertices[indices[0]].Position,
		lVertices[indices[1]].Position,
		lVertices[indices[2]].Position
	};
	
	float3 vertexNormals[3] = {
		lVertices[indices[0]].Normal,
		lVertices[indices[1]].Normal,
		lVertices[indices[2]].Normal
	};

	float2 vertexUV[3] = {
		lVertices[indices[0]].UV,
		lVertices[indices[1]].UV,
		lVertices[indices[2]].UV
	};
	
	float3 objectNormal = 
		vertexNormals[0] * barycentrics.x +
		vertexNormals[1] * barycentrics.y +
		vertexNormals[2] * barycentrics.z;

	float3 objectPosition = 
		vertextPositions[0] * barycentrics.x +
		vertextPositions[1] * barycentrics.y +
		vertextPositions[2] * barycentrics.z;

	float2 UV = 
		vertexUV[0] * barycentrics.x +
		vertexUV[1] * barycentrics.y +
		vertexUV[2] * barycentrics.z;

	objectPosition += objectNormal * 0.005f;

	float4 worldPosition = mul(float4(objectPosition, 1), gInstances[lInstanceData.offset].transform);
	float4 worldNormal =  mul(float4(objectNormal, 0), gInstances[lInstanceData.offset].normalTransform);
	worldNormal.xyz = normalize(worldNormal.xyz);

	float3 directLighting = 0;

	float4 bn = gBlueNoise.Load(uint4((pixelNum % gCamera.width) % 64, (pixelNum / gCamera.width) % 64, gFrame.frame % 64, 0));
	float u1 = bn.x;
	float u2 = bn.y;
	float tt = bn.x * 2 - 1;
	float bb = bn.y * 2 - 1;

#if PRIMARY_RAY
	const float3 V = normalize(gCamera.origin.xyz - worldPosition.xyz);
	const float3 throughput = 1;
#else
	const float3 V = -normalize(gRayInfo[gRegroupedIndexes[pixelNum]].direction.xyz);
	const float3 throughput = gRayInfo[gRegroupedIndexes[pixelNum]].throughput.xyz;
#endif

    RayDiff diff;
    float2 ddx_uv = float2(0, 0);
    float2 ddy_uv = float2(0, 0);
    {
        diff.dOdx = float3(0,0,0);
        diff.dOdy = float3(0,0,0);
        float3 r = GetWorldRay(payload.colorAndDistance.xy * 2 - 1, gCamera.forward.xyz, gCamera.right.xyz, gCamera.up.xyz);
        float3 rx = GetWorldRay((payload.colorAndDistance.xy + float2(1.0f/ gCamera.width, 0))* 2 - 1, gCamera.forward.xyz, gCamera.right.xyz, gCamera.up.xyz);
        float3 ry = GetWorldRay((payload.colorAndDistance.xy + float2(0, 1.0f/gCamera.height))* 2 - 1, gCamera.forward.xyz, gCamera.right.xyz, gCamera.up.xyz);
        diff.dDdx = rx - r;
        diff.dDdy = ry - r;
        
        float3 edge01 = mul((vertextPositions[1] - vertextPositions[0]).xyz, (float3x3)gInstances[lInstanceData.offset].transform);
        float3 edge02 = mul((vertextPositions[2] - vertextPositions[0]).xyz, (float3x3)gInstances[lInstanceData.offset].transform);
        float3 triNormalW = cross(edge01, edge02);

        propagate(diff, r, length(gCamera.origin.xyz - worldPosition.xyz), worldNormal.xyz);
        
        float2 dBarydx;
        float2 dBarydy;
        computeBarycentricDifferentials(diff, r, edge01, edge02, triNormalW, dBarydx, dBarydy);
        
        interpolateDifferentials(dBarydx, dBarydy, vertexUV, ddx_uv, ddy_uv);
    }

	const bool hitLight = gInstances[lInstanceData.offset].emission != 0;

	if (hitLight)
	{
		float NdotV = dot(V, worldNormal.xyz);
		float mis = 1;
		float4 prevOriginFlags = gRayInfo[gRegroupedIndexes[pixelNum]].originFlags;

#if !PRIMARY_RAY
		if (!(asuint(prevOriginFlags.w) & RAY_FLAG_SPECULAR_BOUNCE))
		{
			float brdfPdf = gRayInfo[gRegroupedIndexes[pixelNum]].throughput.w;
			float3 distToLight = length(worldPosition.xyz - prevOriginFlags.xyz);
			float lightPdf = distToLight * distToLight / NdotV;
			mis = powerHeuristic(brdfPdf, lightPdf);
		}
#endif

		// Add light hit emission
		payload.colorAndDistance.rgb = mis * gInstances[lInstanceData.offset].emission * throughput * float(NdotV > 0);
		gRayInfo[pixelNum].originFlags.w = asfloat(0);
		return;
	}
	else
	{
		payload.colorAndDistance.rgb = 0;
	}

	engine::Shaders::Material mat = gMaterials[gInstances[lInstanceData.offset].materialIndex];

	SurfaceHit surface;
	surface.roughness = mat.shading.x;
	surface.metalness = mat.shading.y;
	surface.albedo = mat.albedo.xyz;

	if (mat.albedoIndex != uint(-1))
	{
		float3 textureAlbedo = srgbInv(gTxMats[mat.albedoIndex].SampleGrad(gSampler, UV, ddx_uv, ddy_uv).rgb);
		surface.albedo *= textureAlbedo;
	}
	

	// Shadow rays (Next event estimation)

	//for (int i = 0; i < gScene.lightCount; ++i)
	const int i = 0;
	if (!hitLight)
	{
		float3 lightSample = gLights[i].center_world.xyz + gLights[i].T_world.xyz * tt + gLights[i].B_world.xyz * bb;
		float3 L = lightSample - worldPosition.xyz;
		float Ldist = length(L);
		L /= Ldist;

		float lightPdf = (Ldist * Ldist) / dot(L, gLights[i].normal.xyz);
		float brdfPdf = CookTorranceBRDFPdf(worldNormal.xyz, L, V, surface, RAY_TYPE_NONE);
		float3 brdf = CookTorranceBRDF(worldNormal.xyz, L, V, surface, RAY_TYPE_NONE);
		float3 e = powerHeuristic(lightPdf, brdfPdf) * gLights[i].color * brdf / lightPdf;
		float luma = length(e * throughput);

		const float maxLuma = 1;
		if (luma > maxLuma) // Fix fireflies
		{
			float ratio = luma / maxLuma;
			e = min(float3(maxLuma, maxLuma, maxLuma), luma / 3);
		}

		RayDesc ray;
		ray.Origin = worldPosition.xyz;
		ray.Direction = L;
		ray.TMin = 0;
		ray.TMax = max(Ldist - 0.001, 0);

		RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
			RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
			RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

		// Set up shadow ray. No work is done yet.
		q.TraceRayInline(
			gSceneBVH,
			0,
			0xFF,
			ray);

		// Trace ray
		q.Proceed();

		// Examine and act on the result of the traversal.
		// Was a hit committed?
		float isShadow = float(q.CommittedStatus() == COMMITTED_TRIANGLE_HIT);

		directLighting += e * (1 - isShadow) * (lightPdf > 0);
	}

	//{
	//	float pdf;
	//	float3 L = rayUniform(worldNormal.xyz, u1, u2, pdf);
	//
	//	float lightPdf = 1 / _2PI;
	//	float brdfPdf = CookTorranceBRDFPdf(worldNormal.xyz, L, V, surface, RAY_TYPE_NONE);
	//	float3 brdf = CookTorranceBRDF(worldNormal.xyz, L, V, surface, RAY_TYPE_NONE);
	//	float3 e = powerHeuristic(lightPdf, brdfPdf) * SkyColor * brdf / lightPdf;
	//
	//	RayDesc ray;
	//	ray.Origin = worldPosition.xyz;
	//	ray.Direction = L;
	//	ray.TMin = 0;
	//	ray.TMax = 1000;
	//
	//	RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
	//		RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
	//		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
	//
	//	// Set up shadow ray. No work is done yet.
	//	q.TraceRayInline(
	//		gSceneBVH,
	//		0,
	//		0xFF,
	//		ray);
	//
	//	// Trace ray
	//	q.Proceed();
	//
	//	// Examine and act on the result of the traversal.
	//	// Was a hit committed?
	//	float isShadow = float(q.CommittedStatus() == COMMITTED_TRIANGLE_HIT);
	//
	//	directLighting += e * (1 - isShadow);
	//}

	payload.colorAndDistance.rgb += directLighting * throughput;

	// Prepare new ray after first bounce
	// TODO: second bounce
#if PRIMARY_RAY
	uint raytype;
	float3 nextDirection = CookTorranceBRDFSample(worldNormal, V, surface, u1, u2, raytype);
	gRayInfo[pixelNum].direction.xyz = nextDirection;

	uint hitFlags = RAY_FLAG_HIT;
	if (raytype == RAY_TYPE_SPECULAR)
		hitFlags |= RAY_FLAG_SPECULAR_BOUNCE;
	gRayInfo[pixelNum].originFlags = float4(worldPosition.xyz, asfloat(hitFlags));

	float pdf = CookTorranceBRDFPdf(worldNormal, nextDirection, V, surface, raytype);

	float3 brdf = CookTorranceBRDF(worldNormal.xyz, nextDirection, V, surface, raytype);
	if (raytype != RAY_TYPE_SPECULAR)
		brdf *= max(dot(nextDirection, worldNormal.xyz), 0);
	pdf = max(pdf, float3(0.001, 0.001, 0.001)); // Fix black dots
	gRayInfo[pixelNum].throughput.xyz = float4(brdf / pdf > 10? 0 : brdf / pdf, pdf); // Fix fireflies
#endif

	//payload.colorAndDistance.xyz = float3(0.5, 0.5f, 0.5) * worldNormal.xyz + float3(0.5, 0.5f, 0.5);
	//payload.colorAndDistance.xyz = float3(gInstances[lInstanceData.offset].normalTransform[0][0], gInstances[lInstanceData.offset].normalTransform[0][1], gInstances[lInstanceData.offset].normalTransform[0][2]);
}
