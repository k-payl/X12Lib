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
	rng_state = uint(wang_hash((1 << 31) /*| (depth << 22)*/ | iteration) ^ wang_hash(index));
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

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint pixelNum = DispatchRaysIndex().y * DispatchRaysDimensions().x + DispatchRaysIndex().x;
	ComputeRngSeed(pixelNum, uint(payload.colorAndDistance.w), 0);

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

	objectPosition += objectNormal * 0.002f;

	float4 worldPosition = mul(float4(objectPosition, 1), gInstances[lInstanceData.offset].transform);
	float4 worldNormal =  mul(gInstances[lInstanceData.offset].normalTransform, float4(objectNormal, 0));
	float3 V = worldPosition.xyz - gCamera.origin.xyz;

	float3 directLighting = 0;

	float tt = lerp(-1, 1, goldenRatioU1());
	float bb = lerp(-1, 1, goldenRatioU2());

	for (int i = 0; i < gScene.lightCount; ++i)
	{
		float3 lightSample = gLights[i].center_world.xyz + gLights[i].T_world.xyz * tt + gLights[i].B_world.xyz * bb;
		float3 L = lightSample - worldPosition.xyz;
		float L_len = length(L);
		L /= L_len;
		float brdf = INVPI;
		float areaLightFactor = max(dot(gLights[i].normal, -L), 0) / (L_len * L_len);
		float3 directRadiance = /*WTF*/20 * areaLightFactor * brdf * max(dot(L, worldNormal.xyz), 0) * gLights[i].color;

		RayDesc ray;
		ray.Origin = worldPosition.xyz;
		ray.Direction = L;
		ray.TMin = 0;
		ray.TMax = max(L_len - 0.001, 0);

		RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
			RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
			RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

		// Set up shadow ray. No work is done yet.
		q.TraceRayInline(
			gSceneBVH,
			0,
			0xFF,
			ray);

		// trace ray
		q.Proceed();

		// Examine and act on the result of the traversal.
		// Was a hit committed?
		float isShadow = float(q.CommittedStatus() == COMMITTED_TRIANGLE_HIT);

		directLighting += directRadiance * (1 - isShadow);
	}

	//float3 textureAlbedo = gTxMats[0].SampleLevel(gSampler, UV, 0).rgb; // TODO texture index
	//directLighting *= textureAlbedo;

	//const float3 light1 = float3(5, -4, 10);
	//const float3 Ln = normalize(light1 - worldPosition.xyz);
	//directLighting = max(0, dot(objectNormal, Ln)) * float3(1,1,1) + float3(0.3, 0.3, 0.3);

	// Add area light emission for primary rays
	directLighting += float3(1, 1, 1) * gInstances[lInstanceData.offset].emission * sign(dot(worldNormal.xyz, V)); // TODO: handle back face. need only for primary visibility

	payload.colorAndDistance = float4(/*lInstanceData.color.rgb **/ directLighting, RayTCurrent());
}

[shader("closesthit")] 
void ShadowClosestHit(inout HitInfo payload, Attributes attrib)
{
	payload.colorAndDistance = float4(0, 0, 0, 1);
}
