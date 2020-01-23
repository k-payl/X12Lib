struct VertexShaderOutput
{
	float4 Position : SV_Position;
	float2 Rect : COLOR;
};

#if VERTEX==1

	cbuffer ViewportCB : register(b0)
	{
		float4 viewport; // w, h, 1/w, 1/h
	};
	cbuffer TransformCB : register(b1)
	{
		float4 transform; // y offset (in pixels), 0, 0, 0
	};

	float2 pixelToNDC(float2 pos)
	{
		return pos * float2(viewport.z, -viewport.w) * 2 + float2(-1, 1);
	}

	struct VertexPosColor
	{
		float4 Position : POSITION; // x, y, id, offset x
	};

	struct FontChar
	{
		float x, y;
		float w, h;
		float xoffset, yoffset;
		float xadvance;
		int _align;
	};
	StructuredBuffer<FontChar> character_buffer : register(t0);

	VertexShaderOutput main(VertexPosColor IN)
	{
		FontChar item = character_buffer[int(IN.Position.z)];

		float2 pos = IN.Position.xy * float2(item.w, item.h);
		pos += float2(IN.Position.w, transform.x + item.yoffset);

		VertexShaderOutput OUT;
		OUT.Position = float4(pixelToNDC(pos), 0, 1);

		OUT.Rect.x = item.x + IN.Position.x * item.w;
		OUT.Rect.y = item.y + IN.Position.y * item.h;

		return OUT;
	}

#else

	Texture2D texture_font : register(t0);

	float4 main(VertexShaderOutput IN) : SV_Target
	{
		float4 tex = texture_font.Load(int3(IN.Rect, 0));
		return float4(float3(1,1,1)*0.6, tex.r);
	}

#endif
