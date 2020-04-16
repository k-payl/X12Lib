#pragma once
#include  "dx12common.h"
#include "dx12descriptorheap.h"

namespace x12
{
	struct Dx12CoreTexture final : public ICoreTexture
	{
	public:
		void Init();
		void InitFromExisting(ID3D12Resource* resource_);

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return SRVdescriptor.descriptor; }

	private:
		D3D12_RESOURCE_DESC desc;
		ComPtr<ID3D12Resource> resource;

		x12::descriptorheap::Alloc SRVdescriptor;
		x12::descriptorheap::Alloc RTVdescriptor;
	};

}

