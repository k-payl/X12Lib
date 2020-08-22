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
