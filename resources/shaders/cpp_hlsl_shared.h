#pragma once

#ifdef __cplusplus
	#define float2 math::vec2
	#define float3 math::vec3
	#define float4 math::vec4
	#define float4x4 math::mat4
	#define uint uint32_t
	#define DECLARE_VERTEX_IN(N, S) N
#else
	#define DECLARE_VERTEX_IN(N, S) N : S
#endif

namespace engine
{
	namespace Shaders
	{
		struct RayInfo
		{
			float4 origin;
			float4 direction;
		};

		struct Frame
		{
			uint frame;
		};

#pragma pack(push, 1)
		// For graphic
		struct VertexIn
		{
			float3 DECLARE_VERTEX_IN(Position, POSITION);
			float3 DECLARE_VERTEX_IN(Normal, TEXCOORD);
			float2 DECLARE_VERTEX_IN(UV, COLOR);
		};

		// For BLAS
		struct Vertex
		{
			float3 Position;
			float3 Normal;
			float2 UV;
		};

		struct InstancePointer
		{
			uint offset;
			float __padding[3];
		};

		struct InstanceData
		{
			float4x4 transform;
			float4x4 normalTransform;
			float emission; // For area light
			float __padding[3];
		};

		struct Light
		{
			float worldTransform[16];
			float4 center_world;
			float4 T_world;
			float4 B_world;
			float4 normal;
			float4 color; // multiplied by intensity
			float4 size;
		};

		struct Scene
		{
			uint instanceCount;
			uint lightCount;
			float __padding[2];
		};

		struct Camera
		{
			float4 forward;
			float4 right;
			float4 up;
			float4 origin;
			uint width; // move to separate buffer
			uint height;
			uint _padding[2];
		};
#pragma pack(pop)
	}
}
