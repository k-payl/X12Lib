
struct VertexShaderOutput
{
	float4 Color    : COLOR;
	float4 Position : SV_Position;
	float2 UV       : TEXCOORD0;
};

#if VERTEX==1

	cbuffer CameraCB : register(b0)
	{
		float4x4 MVP;
	};
	cbuffer TransformCB : register(b3)
	{
		float4 transform;
		float4 color_out;
	};

	struct Vertex // from mesh.h
	{
		float3 Position : POSITION;
		float3 Normal   : TEXCOORD;
		float2 UV		: COLOR;
	};

	VertexShaderOutput main(Vertex IN)
	{
		VertexShaderOutput OUT;
		
		OUT.Position = float4(IN.Position.xy + transform.zw, IN.Position.z, 1);
		OUT.Position = mul(MVP, OUT.Position);
		OUT.Color = color_out;
		OUT.UV = IN.UV;

		return OUT;
	}

#else
    SamplerState textureSampler :register(s0);
	Texture2D texture_ : register(t0);

	float4 main(VertexShaderOutput IN) : SV_Target
	{
		float4 tex = texture_.Sample(textureSampler, IN.UV);
		return IN.Color * tex;
	}

#endif
