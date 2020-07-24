#pragma once
#include "dx12common.h"
#include "dx12descriptorheap.h"

namespace x12
{
	struct Dx12CoreBuffer final : public ICoreBuffer
	{
	private:
		void initSRV(UINT num, UINT structireSize, D3D12_BUFFER_SRV_FLAGS flag = D3D12_BUFFER_SRV_FLAG_NONE, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		void initUAV(UINT num, UINT structireSize, D3D12_BUFFER_UAV_FLAGS flag = D3D12_BUFFER_UAV_FLAG_NONE, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);
		void initCBV(UINT size);

		BUFFER_FLAGS flags;
		ComPtr<ID3D12Resource> resource;
		D3D12_RESOURCE_STATES state;

		void* ptr;

		x12::descriptorheap::Alloc CBVdescriptor;
		x12::descriptorheap::Alloc SRVdescriptor;
		x12::descriptorheap::Alloc UAVdescriptor;

		size_t size;

		ComPtr<ID3D12Resource> stagingResource;
		D3D12_RESOURCE_STATES stagingState;

		std::wstring name;

	public:
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return SRVdescriptor.descriptor; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return UAVdescriptor.descriptor; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCBV() const { return CBVdescriptor.descriptor; }

		void InitBuffer(size_t structureSize, size_t num, const void* data, BUFFER_FLAGS flags, LPCWSTR name);

		ID3D12Resource* GetResource() const { return resource.Get(); }

		void _GPUCopyToStaging(ICoreGraphicCommandList* cmdList);
		void _GetStagingData(void* data);

		D3D12_RESOURCE_STATES& resourceState() { return state; }
		D3D12_RESOURCE_STATES& stagingResourceState() { return stagingState; }

		void GetData(void* data) override;
		void SetData(const void* data, size_t size) override;

		~Dx12CoreBuffer();
	};

}
