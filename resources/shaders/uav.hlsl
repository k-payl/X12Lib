
cbuffer ChunkNumber : register(b0)
{
	uint chunk;
};

RWStructuredBuffer<float4> powerOfTwo : register(u1);

[numthreads(1, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (chunk == 0)
		powerOfTwo[chunk] = 1.0f;
	else
		powerOfTwo[chunk] = powerOfTwo[chunk - 1] + powerOfTwo[chunk - 1];
}

