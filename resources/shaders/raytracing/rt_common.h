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
