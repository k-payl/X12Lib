// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

ByteAddressBuffer Indices : register(t1, space0);

struct Vertex
{
    float3 position;
    float3 normal;
};
StructuredBuffer<Vertex> Vertices : register(t2, space0);
