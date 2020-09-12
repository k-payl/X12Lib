#pragma once

#ifdef __cplusplus
	#define float2 math::vec2
	#define float3 math::vec3
	#define float4 math::vec4
	#define NAME(N, S) N
#else
	#define NAME(N, S) N : S
#endif

namespace engine
{
	#pragma pack(push, 1)
	struct Vertex
	{
		float3 NAME(Position, POSITION);
		float3 NAME(Normal, TEXCOORD);
		float2 NAME(UV, COLOR);
	};
	#pragma pack(pop)
}
