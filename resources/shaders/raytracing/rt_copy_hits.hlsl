#include "../cpp_hlsl_shared.h"

StructuredBuffer<uint> gHitCounter : register(t0);

struct DispatchRaysIndirectBuffer
{
	uint64_t StartAddress;
	uint64_t SizeInBytes;

	uint64_t StartAddress_miss;
	uint64_t SizeInBytes_miss;
	uint64_t StrideInBytes_miss;

	uint64_t StartAddress_hit;
	uint64_t SizeInBytes_hit;
	uint64_t StrideInBytes_hit;

	uint64_t StartAddress_callable;
	uint64_t SizeInBytes_callable;
	uint64_t StrideInBytes_callable;

	uint width;
	uint height;
	uint depth;
};
RWStructuredBuffer<DispatchRaysIndirectBuffer> gIndirectArgumentBuffer : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID)
{
	gIndirectArgumentBuffer[0].width = gHitCounter[0];
}
