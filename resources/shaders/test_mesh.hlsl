#include "cpp_hlsl_shared.h"

struct VertexShaderOutput
{
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
	float3 WorldN   : TEXCOORD1;
	float3 WorldPos : TEXCOORD2;
	float3 CameraPos: TEXCOORD3;
};

#if VERTEX

	cbuffer CameraCB : register(b0)
	{
		float4x4 MVP;
		float4 CameraPos;
	};

	cbuffer TransformCB : register(b3)
	{
		float4x4 WorldTransform;
		float4x4 NormalMat;
	};

	VertexShaderOutput main(engine::Shaders::VertexIn IN)
	{
		VertexShaderOutput OUT;
		
#if 0
		OUT.WorldPos = mul(WorldTransform, float4(IN.Position.xyz, 1)).xyz;
		OUT.Position = mul(MVP, OUT.WorldPos);
#else
		OUT.WorldPos = IN.Position.xyz;
		OUT.Position = float4(IN.Position.xyz, 1);
#endif

		OUT.UV = IN.UV;
		OUT.WorldN = normalize(mul(NormalMat, float4(IN.Normal, 0)));
		OUT.CameraPos = CameraPos;

		return OUT;
	}
#else

    SamplerState textureSampler :register(s0);
	Texture2D texture_ : register(t0);

	float4 main(VertexShaderOutput IN) : SV_Target
	{
		float4 tex = texture_.Sample(textureSampler, IN.UV);
		return tex;
	}

#endif
