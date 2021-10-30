#pragma once
#include "../consts.h"

#define RAY_FLAG_HIT (1 << 0)
#define RAY_FLAG_SPECULAR_BOUNCE (1 << 1)

// Hit information, aka ray payload
// This sample only carries a shading color and hit distance.
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct HitInfo {
  float4 colorAndDistance;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates
struct Attributes {
  float2 bary;
};

float3 GetWorldRay(float2 ndc, float3 forwardWS, float3 rightWS, float3 upWS)
{
    return normalize(forwardWS + rightWS * ndc.x + upWS * ndc.y);
}

static float3 SkyColor = float3(0.0f, 0.2f, 0.7f);

float3 srgbInv(float3 v)
{
	return pow(v, 2.2);
}

float powerHeuristic(float a, float b)
{
	float t = a * a;
	return t / (b * b + t);
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

	pdf = 1 / _2PI;

	float3 H = normalize(TangentX * x + TangentY * y + N * z);

	return H;
}



 struct RayDiff
{
    float3 dOdx;
    float3 dOdy;
    float3 dDdx;
    float3 dDdy;
 };
    
// Implements Equation 6.1 of this chapter.
void propagate(inout RayDiff rayDiff, float3 D, float t, float3 N)
{
    float3 dodx = rayDiff.dOdx + t * rayDiff.dDdx; // Igehy Equation 10
    float3 dody = rayDiff.dOdy + t * rayDiff.dDdy;
    float rcpDN = 1.0f / dot(D, N); // Igehy Eqns 10 & 12
    float dtdx = -dot(dodx, N) * rcpDN;
    float dtdy = -dot(dody, N) * rcpDN;
    rayDiff.dOdx += D * dtdx;
    rayDiff.dOdy += D * dtdy;
}

 // Implements Equation 6.10 of this chapter.
 void interpolateDifferentials(float2 dBarydx , float2 dBarydy ,
    float2 vertexValues[3], out float2 dx, out float2 dy)
 {
     float2 delta1 = vertexValues[1] - vertexValues[0];
     float2 delta2 = vertexValues[2] - vertexValues[0];
     dx = dBarydx.x * delta1 + dBarydx.y * delta2;
     dy = dBarydy.x * delta1 + dBarydy.y * delta2;
 }

 // Implements Equation 6.9 of this chapter.
 void computeBarycentricDifferentials(RayDiff rayDiff,
    float3 rayDir, float3 edge01, float3 edge02,
    float3 triNormalW , out float2 dBarydx , out float2 dBarydy)
 {
     float3 Nu = cross(edge02, triNormalW);
     float3 Nv = cross(edge01, triNormalW);
     float3 Lu = Nu / (dot(Nu, edge01));
     float3 Lv = Nv / (dot(Nv, edge02));

     dBarydx.x = dot(Lu, rayDiff.dOdx); // du / dx
     dBarydx.y = dot(Lv, rayDiff.dOdx); // dv / dx
     dBarydy.x = dot(Lu, rayDiff.dOdy); // du / dy
     dBarydy.y = dot(Lv, rayDiff.dOdy); // dv / dy
 }

// Load three 16 bit indices from a byte addressed buffer.
//uint3 Load3x16BitIndices(uint offsetBytes)
//{
//    uint3 indices;
//
//    // ByteAdressBuffer loads must be aligned at a 4 byte boundary.
//    // Since we need to read three 16 bit indices: { 0, 1, 2 } 
//    // aligned at a 4 byte boundary as: { 0 1 } { 2 0 } { 1 2 } { 0 1 } ...
//    // we will load 8 bytes (~ 4 indices { a b | c d }) to handle two possible index triplet layouts,
//    // based on first index's offsetBytes being aligned at the 4 byte boundary or not:
//    //  Aligned:     { 0 1 | 2 - }
//    //  Not aligned: { - 0 | 1 2 }
//    const uint dwordAlignedOffset = offsetBytes & ~3;
//    const uint2 four16BitIndices = Indices.Load2(dwordAlignedOffset);
//
//    // Aligned: { 0 1 | 2 - } => retrieve first three 16bit indices
//    if (dwordAlignedOffset == offsetBytes)
//    {
//        indices.x = four16BitIndices.x & 0xffff;
//        indices.y = (four16BitIndices.x >> 16) & 0xffff;
//        indices.z = four16BitIndices.y & 0xffff;
//    }
//    else // Not aligned: { - 0 | 1 2 } => retrieve last three 16bit indices
//    {
//        indices.x = (four16BitIndices.x >> 16) & 0xffff;
//        indices.y = four16BitIndices.y & 0xffff;
//        indices.z = (four16BitIndices.y >> 16) & 0xffff;
//    }
//
//    return indices;
//}
