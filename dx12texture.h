#pragma once
#include "common.h"
#include "icorerender.h"
#include "dx12descriptorheap.h"

struct Dx12CoreTexture : public ICoreTexture
{
public:
	void InitFromExistingResource(ID3D12Resource* resource_);

	inline void AddRef() override { refs++; }
	inline int GetRefs() override { return refs; }
	void Release() override;

	D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return descriptorAllocation.descriptor; }

private:
	D3D12_RESOURCE_DESC desc;
	ComPtr<ID3D12Resource> resource;

	DescriptorHeap::Alloc descriptorAllocation;

	int refs{};
};

