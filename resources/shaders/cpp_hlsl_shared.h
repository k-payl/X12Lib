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
#pragma pack(push, 1)
		struct VertexIn
		{
			float3 DECLARE_VERTEX_IN(Position, POSITION);
			float3 DECLARE_VERTEX_IN(Normal, TEXCOORD);
			float2 DECLARE_VERTEX_IN(UV, COLOR);
		};

		struct Vertex
		{
			float3 Position;
			float3 Normal;
			float2 UV;
		};

		struct InstanceData
		{
			float4 color;
			float4x4 transform;
			float4x4 normalTransform;
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
		};

		struct Camera
		{
			float4 forward;
			float4 right;
			float4 up;
			float4 origin;
		};
#pragma pack(pop)
	}
}
