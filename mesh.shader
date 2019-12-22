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

	struct VertexPosColor
	{
		float4 Position : POSITION;
		float4 Color    : TEXCOORD;
	};

	struct VertexShaderOutput
	{
		float4 Color    : COLOR;
		float4 Position : SV_Position;
	};

	VertexShaderOutput main(VertexPosColor IN)
	{
		VertexShaderOutput OUT;
		
		OUT.Position = float4(IN.Position.xy * transform.xy + transform.zw, IN.Position.z, 1);
		OUT.Position = mul(MVP, OUT.Position);
		OUT.Color = color_out;

		return OUT;
	}

#else

	struct PixelShaderInput
	{
		float4 Color : COLOR;
	};

	float4 main(PixelShaderInput IN) : SV_Target
	{
		return IN.Color;
	}

#endif