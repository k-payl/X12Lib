#pragma once
#include "dx12common.h"
#include "dx12descriptorheap.h"

namespace x12
{
	struct Dx12CoreBuffer final : public ICoreBuffer
	{
	private:

		BUFFER_FLAGS flags;
		MEMORY_TYPE memoryType;
		ComPtr<ID3D12Resource> resource;
		D3D12_RESOURCE_STATES state;

		void* ptr{};

		x12::descriptorheap::Alloc CBVdescriptor;
		x12::descriptorheap::Alloc SRVdescriptor;
		x12::descriptorheap::Alloc UAVdescriptor;

		size_t size;

		ComPtr<ID3D12Resource> stagingResource;
		D3D12_RESOURCE_STATES stagingState;

		std::wstring name;

	public:
		Dx12CoreBuffer(size_t size, const void* data, MEMORY_TYPE memory_type, BUFFER_FLAGS flags, LPCWSTR name);
		~Dx12CoreBuffer();

		D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return SRVdescriptor.descriptor; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const { return UAVdescriptor.descriptor; }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCBV() const { return CBVdescriptor.descriptor; }
		ID3D12Resource* GetResource() const { return resource.Get(); }
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress() { return resource->GetGPUVirtualAddress(); }

		void _GPUCopyToStaging(ICoreGraphicCommandList* cmdList);
		void _GetStagingData(void* data);

		void Map();
		void Unmap();

		D3D12_RESOURCE_STATES& resourceState() { return state; }
		D3D12_RESOURCE_STATES& stagingResourceState() { return stagingState; }

		void GetData(void* data) override;
		void SetData(const void* data, size_t size) override;

		void initSRV(UINT num, UINT structireSize, bool raw);
		void initUAV(UINT num, UINT structireSize, bool raw);
		void initCBV(UINT size);
	};

}
