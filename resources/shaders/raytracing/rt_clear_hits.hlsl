#include "../cpp_hlsl_shared.h"

RWStructuredBuffer<uint> gHitCounter : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 globalThreadId : SV_DispatchThreadID)
{
	gHitCounter[0] = 0;
}
