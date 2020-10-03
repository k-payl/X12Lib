#include "cpp_hlsl_shared.h"

struct VertexShaderOutput
{
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
	float3 worldN   : TEXCOORD1;
	float3 worldPos : TEXCOORD2;
	float3 cameraPos: TEXCOORD3;
};

#if VERTEX==1

	cbuffer CameraCB : register(b0)
	{
		float4x4 MVP;
		float4 cameraPos;
	};

	cbuffer TransformCB : register(b3)
	{
		float4x4 worldTransform;
		float4x4 NormalMat;
	};

	VertexShaderOutput main(engine::Shaders::Vertex IN)
	{
		VertexShaderOutput OUT;
		
		//OUT.Position = mul(worldTransform, float4(IN.Position.xyz, 1));
		OUT.Position = float4(IN.Position.xyz, 1);
		OUT.worldPos = OUT.Position.xyz;
		//OUT.Position = mul(MVP, OUT.Position);
		OUT.UV = IN.UV;
		OUT.worldN = normalize(mul(NormalMat, float4(IN.Normal, 0)));
		OUT.cameraPos = cameraPos;

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
