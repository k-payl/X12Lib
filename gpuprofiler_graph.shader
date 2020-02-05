
#if VERTEX==1

	cbuffer ViewportCB : register(b0)
	{
		float4 viewport; // w, h, 1/w, 1/h
	};

	cbuffer GraphTransform : register(b1)
	{
		float4 transform; // offset, 0, 0, 0
	};

	float2 pixelToNDC(float2 pos)
	{
		return pos * float2(viewport.z, -viewport.w) * 2 + float2(-1, 1);
	}

	struct VertexShaderOutput
	{
		float4 Position : SV_Position;
	};

	struct VertexPosColor
	{
		float4 Position : POSITION;
	};

	VertexShaderOutput main(VertexPosColor IN)
	{
		VertexShaderOutput OUT;
		OUT.Position.xy = pixelToNDC(float2(-IN.Position.x + transform.x, IN.Position.y));
		OUT.Position.z = 0;
		OUT.Position.w = 1;
		return OUT;
	}

#else

	cbuffer GraphColor : register(b0)
	{
		float4 color;
	};

	float4 main() : SV_Target
	{
		return color;
	}

#endif
