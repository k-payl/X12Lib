#include "cpp_hlsl_shared.h"
#include "brdf.h"

struct VertexShaderOutput
{
	float4 Color    : COLOR;
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
		float4 color_out;
		float4x4 worldTransform;
		float4x4 NormalMat;
	};

	VertexShaderOutput main(engine::Shaders::VertexIn IN)
	{
		VertexShaderOutput OUT;
		
		OUT.Position = mul(worldTransform, float4(IN.Position.xyz, 1));
		OUT.worldPos = OUT.Position.xyz;
		OUT.Position = mul(MVP, OUT.Position);
		OUT.Color = color_out;
		OUT.UV = IN.UV;
		OUT.worldN = normalize(mul(NormalMat, float4(IN.Normal, 0)));
		OUT.cameraPos = cameraPos;

		return OUT;
	}
#else

    SamplerState textureSampler :register(s0);
	Texture2D texture_ : register(t0);

	cbuffer ShadingCB : register(b4)
	{
		float4 shading; // rough + metall
	};

	float4 main(VertexShaderOutput IN) : SV_Target
	{
		float4 tex = srgbInv(texture_.Sample(textureSampler, IN.UV));

		float3 Ln = normalize(float3(0.2, -0.7, 1));
		float3 V = normalize(IN.cameraPos - IN.worldPos);

		Material mat;
		mat.m_base_color = IN.Color.rgb * tex.rgb;
		mat.m_roughness = shading.r;
		mat.m_metalness = shading.g;

		const float intensity = 2;

		BRDF brdf = CookTorranceBRDF(normalize(IN.worldN), Ln, V, mat);
		float3 L = (brdf.m_diffuse + brdf.m_specular) * saturate(dot(Ln, normalize(IN.worldN))) * intensity;

		return float4(srgb(L), 1);
	}

#endif
