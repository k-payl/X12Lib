#pragma once
#include  "dx12common.h"
#include "dx12descriptorheap.h"

namespace x12
{
	struct Dx12CoreTexture final : public ICoreTexture
	{
	public:
		void Init(LPCWSTR name, const uint8_t* data, size_t size,
			int32_t width, int32_t height, uint32_t mipCount, TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags);
		void InitFromExisting(ID3D12Resource* resource_);

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return SRVdescriptor.descriptor; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return RTVdescriptor.descriptor; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return DSVdescriptor.descriptor; }

		void TransiteToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList);

	private:
		TEXTURE_CREATE_FLAGS flags_;
		TEXTURE_FORMAT format_;

		void InitSRV();
		void InitRTV();
		void InitDSV();

		D3D12_RESOURCE_STATES state{};
		D3D12_RESOURCE_DESC desc{};
		ComPtr<ID3D12Resource> resource;

		x12::descriptorheap::Alloc SRVdescriptor;
		x12::descriptorheap::Alloc RTVdescriptor;
		x12::descriptorheap::Alloc DSVdescriptor;
	};
}
