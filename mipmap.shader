
cbuffer Frame : register(b0)
{
	uint chunk;
};

RWStructuredBuffer<float4> tex_out : register(u1);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (chunk == 0)
		tex_out[chunk] = 1;
	else
		tex_out[chunk] = tex_out[chunk - 1] + tex_out[chunk - 1];
}

