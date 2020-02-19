#pragma once
#include "common.h"
#include "dx12descriptorheap.h"

namespace x12::fastdescriptorallocator
{
	struct Page;

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
}

