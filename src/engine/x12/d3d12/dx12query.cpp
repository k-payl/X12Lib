
#include "dx12query.h"
#include "dx12render.h"
#include "core.h"

x12::Dx12CoreQuery::Dx12CoreQuery()
{
	auto device = d3d12::CR_GetD3DDevice();

	D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
	QueryHeapDesc.Count = 2;
	QueryHeapDesc.NodeMask = 1;
	QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

	throwIfFailed(device->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&queryHeap)));
	//set_ctx_object_name(queryHeap, L"query heap for %u timers", contextNum, maxNumQuerySlots);

	x12::memory::CreateCommittedBuffer(&queryReadBackBuffer, 16, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);

	//set_ctx_object_name(queryReadBackBuffer, L"query buffer for %u timers %u bytes", contextNum, maxNumQuerySlots, size);

	ICoreRenderer* renderer = engine::GetCoreRenderer();
	gpuTickDelta = static_cast<Dx12CoreRenderer*>(renderer)->GpuTickDelta();
}

float x12::Dx12CoreQuery::GetTime()
{
	D3D12_RANGE dataRange =
	{
		0,
		16,
	};

	UINT64* timingData;
	throwIfFailed(queryReadBackBuffer->Map(0, &dataRange, reinterpret_cast<void**>(&timingData)));

	UINT64 queryTiming[2];
	memcpy(&queryTiming[0], (uint8_t*)timingData, 16);

	auto range = CD3DX12_RANGE(0, 0);
	
	queryReadBackBuffer->Unmap(0, &range);

	UINT64 start = queryTiming[0];
	UINT64 end = queryTiming[1];

	if (end < start)
		return 0.f;

	double time = double(end - start) * gpuTickDelta;

	return static_cast<float>(time);
}
