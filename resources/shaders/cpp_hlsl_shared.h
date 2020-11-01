#pragma once

#ifdef __cplusplus
	#define float2 math::vec2
	#define float3 math::vec3
	#define float4 math::vec4
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
		struct Vertex
		{
			float3 DECLARE_VERTEX_IN(Position, POSITION);
			float3 DECLARE_VERTEX_IN(Normal, TEXCOORD);
			float2 DECLARE_VERTEX_IN(UV, COLOR);
		};

		struct Light
		{
			float worldTransform[16];
			float4 color;
			float4 size;
			float4 intensity;
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
