#include "cpp_hlsl_shared.h"

#if VERTEX==1
	struct VertexOutput
	{
		float4 Position : SV_Position;
	};

	struct VertexIn
	{
		float2 pos : POSITION;
	};

	VertexOutput main(VertexIn IN)
	{
		VertexOutput OUT;
		OUT.Position = float4(IN.pos, 0, 1);
		return OUT;
	}

#else
	Texture2D texture_ : register(t0);

	float4 main(float4 IN : SV_Position) : SV_Target
	{
		return texture_.Load(int3(IN.xy, 0));
	}

#endif
