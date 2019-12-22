#if VERTEX==1

	cbuffer ViewportCB : register(b0)
	{
		float4 viewport; // w, h, 1/w, 1/h
	};
	cbuffer TransformCB : register(b1)
	{
		float4 transform; // x offset, y offset, width, height (in pixels)
	};

	float2 pixelToNDC(float2 pos)
	{
		return pos * float2(viewport.z, -viewport.w) * 2 + float2(-1, 1);
	}


	struct VertexPosColor
	{
		float4 Position : POSITION;
	};

	struct VertexShaderOutput
	{
		float4 Position : SV_Position;
	};

	VertexShaderOutput main(VertexPosColor IN)
	{
		VertexShaderOutput OUT;
		float2 pos = IN.Position.xy * transform.zw + transform.xy;
		OUT.Position = float4(pixelToNDC(pos), 0, 1);
		return OUT;
	}

#else

	float4 main() : SV_Target
	{
		return float4(0, 1, 0, 1);
	}

#endif