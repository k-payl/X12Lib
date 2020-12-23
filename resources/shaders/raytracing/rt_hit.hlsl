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

float3 rayUniform(float3 N, float u1, float u2, out float pdf)
{
	float3 UpVector = abs(N.z) < 0.9999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);

	float z = u1;
	float r = sqrt(max(0.f, 1.0 - z * z));
	float phi = _2PI * u2;
	float x = r * cos(phi);
	float y = r * sin(phi);

	pdf = 1 / (2 * PI);

	float3 H = normalize(TangentX * x + TangentY * y + N * z);

	return H;
}

float3 rayCosine(float3 N, float u1, float u2, out float pdf)
{
	float3 UpVector = abs(N.z) < 0.9999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);

	float3 dir;
	float r = sqrt(u1);
	float phi = 2.0 * PI * u2;
	dir.x = r * cos(phi);
	dir.y = r * sin(phi);
	dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));

	pdf = dir.z / PI;

	float3 H = normalize(TangentX * dir.x + TangentY * dir.y + N * dir.z);

	return H;
}

[shader("closesthit")] 
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
	uint pixelNum = DispatchRaysIndex().x;
	ComputeRngSeed(pixelNum, uint(payload.colorAndDistance.w), 0);

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
	float4 worldNormal =  mul(gInstances[lInstanceData.offset].normalTransform, float4(objectNormal, 0));
	worldNormal.xyz = normalize(worldNormal.xyz);

	float3 directLighting = 0;

	float u1 = goldenRatioU1();
	float u2 = goldenRatioU2();
	float tt = lerp(-1, 1, u1);
	float bb = lerp(-1, 1, u2);

	// Shadow rays (Next event estimation)
	for (int i = 0; i < gScene.lightCount; ++i)
	{
		float3 lightSample = gLights[i].center_world.xyz + gLights[i].T_world.xyz * tt + gLights[i].B_world.xyz * bb;
		float3 L = lightSample - worldPosition.xyz;
		float L_len = length(L);
		L /= L_len;
		float brdf = INVPI;
		float areaLightFactor = max(dot(gLights[i].normal, -L), 0) / (L_len * L_len);
		float3 directRadiance = areaLightFactor * brdf * max(dot(L, worldNormal.xyz), 0) * gLights[i].color;

		RayDesc ray;
		ray.Origin = worldPosition.xyz;
		ray.Direction = L;
		ray.TMin = 0;
		ray.TMax = max(L_len - 0.001, 0); // 0.001 for case when Area light is in TLAS

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

		directLighting += directRadiance * (1 - isShadow);
	}

	engine::Shaders::Material mat = gMaterials[gInstances[lInstanceData.offset].materialIndex];
	float3 albedo = mat.albedo;

	if (mat.albedoIndex != uint(-1))
	{
		float3 textureAlbedo = gTxMats[mat.albedoIndex].SampleLevel(gSampler, UV, 0).rgb;
		albedo *= textureAlbedo;
	}

	directLighting *= albedo;

	// Add area light emission for primary rays
	#if PRIMARY_RAY
		float3 V = normalize(worldPosition.xyz - gCamera.origin.xyz);
		if (gInstances[lInstanceData.offset].emission != 0)
			directLighting = float3(1, 1, 1) * gInstances[lInstanceData.offset].emission/* * saturate(dot(worldNormal.xyz, V))*/;
	#endif

	// BRDF multiplication for secondary hit
#if !PRIMARY_RAY
	float3 brdf = gRayInfo[gRegroupedIndexes[pixelNum]].hitbrdf.xyz;
	directLighting *= brdf;
#endif

	//payload.colorAndDistance.xyz = float3(0.5, 0.5f, 0.5) * worldNormal.xyz + float3(0.5, 0.5f, 0.5);
	payload.colorAndDistance.xyz = directLighting;

	// Prepare new ray after first bounce
	// TODO: second bounce
#if PRIMARY_RAY
	gRayInfo[pixelNum].origin.xyz = worldPosition.xyz;

	// Hit
	gRayInfo[pixelNum].origin.w = 1.0f;

	float pdf;
	float3 nextDirection = rayCosine(worldNormal, Uniform01(), Uniform01(), pdf);
	gRayInfo[pixelNum].direction.xyz = nextDirection;

	// Save BRDF
	gRayInfo[pixelNum].hitbrdf.xyz = albedo * INVPI /*brdf*/ * max(dot(nextDirection, worldNormal.xyz), 0) / pdf;
#endif
}