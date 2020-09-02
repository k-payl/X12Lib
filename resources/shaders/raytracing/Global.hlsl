// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0);

struct CameraData
{
    float4 forward;
    float4 right;
    float4 up;
    float4 origin;
};

ConstantBuffer<CameraData> gCamera : register(b0);

struct SceneData
{
	uint numInstances;
};
ConstantBuffer<SceneData> gScene : register(b2);

//ByteAddressBuffer Indices : register(t1, space0);

//struct Vertex
//{
//    float3 position;
//    float3 normal;
//    float2 tex;
//};
//StructuredBuffer<Vertex> Vertices : register(t2, space0);
