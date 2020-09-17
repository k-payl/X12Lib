#pragma once
#include "dx12common.h"

namespace x12
{
	struct Dx12CoreQuery : public ICoreQuery
	{
		ComPtr<ID3D12QueryHeap> queryHeap;
		ComPtr<ID3D12Resource> queryReadBackBuffer;
		double gpuTickDelta{};

	public:
		Dx12CoreQuery();

		ID3D12QueryHeap* Heap() { return queryHeap.Get(); }
		ID3D12Resource* ReadbackBuffer() { return queryReadBackBuffer.Get(); }

		float GetTime() override;
	};
}
