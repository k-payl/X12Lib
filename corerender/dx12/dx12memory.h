#pragma once
#include "common.h"
#include "icorerender.h"
#include "dx12descriptorheap.h"

namespace x12::memory
{
	void CreateCommittedBuffer(ID3D12Resource** out, UINT64 size, D3D12_RESOURCE_STATES state,
							   D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
							   D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE,
							   D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE);

	void CreateCommitted2DTexture(ID3D12Resource** out, UINT w, UINT h, UINT mipmaps,
								  DXGI_FORMAT format, D3D12_RESOURCE_FLAGS texFlags,
								  D3D12_RESOURCE_STATES state,
								  D3D12_CLEAR_VALUE* clear = nullptr,
								  D3D12_HEAP_TYPE heap = D3D12_HEAP_TYPE_DEFAULT,
								  D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE);


	namespace fast
	{
		constexpr UINT descriptorsInPage = 500;

		struct Alloc
		{
			void* ptr;								// The CPU-writeable address
			D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;		// The GPU-visible address
			D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
		};

		class Allocator
		{
			Page* currentPage{};
			std::vector<Page*> retiredPages;
			uint32_t offset{};

		public:
			Alloc Allocate();
			void Reset();
		};

		struct Page
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> d3d12resource;
			void* ptr{};
			D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;
			x12::descriptorheap::Alloc descriptors;

			Page() = default;
			~Page();
		};

		Page* GetPage(UINT size);
		void ReleasePage(Page* page);
		void Free();
	}
}
