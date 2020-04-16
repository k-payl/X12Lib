#include "pch.h"
#include "dx12descriptorheap.h"
#include "dx12render.h"

using namespace x12::descriptorheap;

Allocator::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE type, std::function<uint64_t()> frameFn_)
	: heapType(type)
	, frameFn(frameFn_)
{}

std::shared_ptr<Page> Allocator::CreateAllocatorPage()
{
	auto newPage = std::make_shared<Page>(heapType, numDescriptorsPerPage, *this);

	allPages.emplace_back(newPage);
	availablePages.insert(allPages.size() - 1);

	return newPage;
}

Alloc Allocator::Allocate(uint32_t numDescriptors)
{
	Alloc allocation;

	for (auto iter = availablePages.begin(); iter != availablePages.end(); )
	{
		auto allocatorPage = allPages[*iter];

		allocation = allocatorPage->Allocate(numDescriptors);

		if (allocatorPage->NumFreeHandles() == 0)
		{
			availablePages.erase(iter++);
		}
		else
			++iter;

		// A valid allocation has been found.
		if (!allocation.IsNull())
		{
			break;
		}
	}

	// No available heap could satisfy the requested number of descriptors.
	if (allocation.IsNull())
	{
		numDescriptorsPerPage = std::max(numDescriptorsPerPage, numDescriptors);
		auto newPage = CreateAllocatorPage();

		allocation = newPage->Allocate(numDescriptors);
	}

	return allocation;
}

void Allocator::ReclaimMemory(uint64_t fenceID)
{
	for (size_t i = 0; i < allPages.size(); ++i)
	{
		auto& page = allPages[i];

		page->ReleaseRetiredDescriptors(fenceID);

		if (page->NumFreeHandles() > 0)
		{
			availablePages.insert(i);
		}
	}
}

Alloc::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE descriptor, uint32_t numDescriptors, uint32_t descriptorSize, std::shared_ptr<Page> page)
	: descriptor(descriptor)
	, numDescriptors(numDescriptors)
	, descriptorIncrementSize(descriptorSize)
	, page(page)
{}


Alloc::~Alloc()
{
	Free();
}

Alloc::Alloc(Alloc&& allocation)
	: descriptor(allocation.descriptor)
	, numDescriptors(allocation.numDescriptors)
	, descriptorIncrementSize(allocation.descriptorIncrementSize)
	, page(std::move(allocation.page))
{
	allocation.descriptor.ptr = 0;
	allocation.numDescriptors = 0;
	allocation.descriptorIncrementSize = 0;
}

Alloc& Alloc::operator=(Alloc&& other)
{
	Free();

	descriptor = other.descriptor;
	numDescriptors = other.numDescriptors;
	descriptorIncrementSize = other.descriptorIncrementSize;
	page = std::move(other.page);

	other.descriptor.ptr = 0;
	other.numDescriptors = 0;
	other.descriptorIncrementSize = 0;

	return *this;
}

D3D12_CPU_DESCRIPTOR_HANDLE Alloc::GetDescriptorHandle(uint32_t offset) const
{
	assert(offset < numDescriptors);
	return { descriptor.ptr + (descriptorIncrementSize * offset) };
}

void Alloc::Free()
{
	if (!IsNull() && page)
	{
		page->Free(*this);

		descriptor.ptr = 0;
		numDescriptors = 0;
		descriptorIncrementSize = 0;
		page.reset();
	}
}

Page::Page(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, Allocator& parent_)
	: heapType(type)
	, numDescriptorsInPage(numDescriptors)
	, parent(parent_)
{
#ifndef NTESTS

	d3d12DescriptorHeap = CreateDescriptorHeap(CR_GetD3DDevice(), numDescriptorsInPage, heapType);

	x12::impl::set_name(d3d12DescriptorHeap.Get(), L"Descriptor heap page for static resources %u descriptors", numDescriptorsInPage);

	baseDescriptor = d3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
#else
	descriptorIncrementSize = 1;
#endif
	
	numFreeHandles = numDescriptorsInPage;

	AddNewBlock(0, numFreeHandles);
}

D3D12_DESCRIPTOR_HEAP_TYPE Page::GetHeapType() const
{
	return heapType;
}

uint32_t Page::NumFreeHandles() const
{
	return numFreeHandles;
}

bool Page::HasSpace(uint32_t numDescriptors) const
{
	return freeListBySize.lower_bound(numDescriptors) != freeListBySize.end();
}

void Page::AddNewBlock(uint32_t offset, uint32_t numDescriptors)
{
	auto offsetIt = freeListByOffset.emplace(offset, numDescriptors);
	auto sizeIt = freeListBySize.emplace(numDescriptors, offsetIt.first);
	offsetIt.first->second.FreeListBySizeIt = sizeIt;
}

Alloc Page::Allocate(uint32_t numDescriptors)
{
	// There are less than the requested number of descriptors left in the heap.
	// Return a NULL descriptor and try another heap.
	if (numDescriptors > numFreeHandles)
	{
		return Alloc();
	}

	// Get the first block that is large enough to satisfy the request.
	auto smallestBlockIt = freeListBySize.lower_bound(numDescriptors);
	if (smallestBlockIt == freeListBySize.end())
	{
		// There was no free block that could satisfy the request.
		return Alloc();
	}

	// The size of the smallest block that satisfies the request.
	uint32_t blockSize = smallestBlockIt->first;

	// The pointer to the same entry in the FreeListByOffset map.
	auto offsetIt = smallestBlockIt->second;

	// The offset in the descriptor heap.
	uint32_t blockOffset = offsetIt->first;

	// Remove the existing free block from the free list.
	freeListBySize.erase(smallestBlockIt);
	freeListByOffset.erase(offsetIt);

	// Compute the new free block that results from splitting this block.
	uint32_t newBlockOffset = blockOffset + numDescriptors;
	uint32_t newBlockSize = blockSize - numDescriptors;

	if (newBlockSize > 0)
		AddNewBlock(newBlockOffset, newBlockSize);

	// Decrement free handles.
	numFreeHandles -= numDescriptors;

	return Alloc(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(baseDescriptor, blockOffset, CR_CBSRV_DescriptorsSize()),
		numDescriptors, CR_CBSRV_DescriptorsSize(), shared_from_this());
}

uint32_t Page::ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
	return static_cast<uint32_t>(handle.ptr - baseDescriptor.ptr) / CR_CBSRV_DescriptorsSize();
}

void Page::Free(const Alloc& alloc)
{
	// Compute the offset of the descriptor within the descriptor heap.
	auto offset = ComputeOffset(alloc.descriptor);

	retiredDescriptorsQueue.emplace(offset, alloc.numDescriptors, parent.frameFn());
}

void Page::FreeBlock(uint32_t offset, uint32_t numDescriptors)
{
	// Find the first element whose offset is greater than the specified offset.
	// This is the block that should appear after the block that is being freed.
	auto nextBlockIt = freeListByOffset.upper_bound(offset);

	// Find the block that appears before the block being freed.
	auto prevBlockIt = nextBlockIt;
	// If it's not the first block in the list.
	if (prevBlockIt != freeListByOffset.begin())
	{
		// Go to the previous block in the list.
		--prevBlockIt;
	}
	else
	{
		// Otherwise, just set it to the end of the list to indicate that no
		// block comes before the one being freed.
		prevBlockIt = freeListByOffset.end();
	}

	// Add the number of free handles back to the heap.
	// This needs to be done before merging any blocks since merging
	// blocks modifies the numDescriptors variable.
	numFreeHandles += numDescriptors;

	if (prevBlockIt != freeListByOffset.end() &&
		offset == prevBlockIt->first + prevBlockIt->second.Size)
	{
		// The previous block is exactly behind the block that is to be freed.
		//
		// PrevBlock.Offset           Offset
		// |                          |
		// |<-----PrevBlock.Size----->|<------Size-------->|
		//

		// Increase the block size by the size of merging with the previous block.
		offset = prevBlockIt->first;
		numDescriptors += prevBlockIt->second.Size;

		// Remove the previous block from the free list.
		freeListBySize.erase(prevBlockIt->second.FreeListBySizeIt);
		freeListByOffset.erase(prevBlockIt);
	}

	if (nextBlockIt != freeListByOffset.end() &&
		offset + numDescriptors == nextBlockIt->first)
	{
		// The next block is exactly in front of the block that is to be freed.
		//
		// Offset               NextBlock.Offset 
		// |                    |
		// |<------Size-------->|<-----NextBlock.Size----->|

		// Increase the block size by the size of merging with the next block.
		numDescriptors += nextBlockIt->second.Size;

		// Remove the next block from the free list.
		freeListBySize.erase(nextBlockIt->second.FreeListBySizeIt);
		freeListByOffset.erase(nextBlockIt);
	}

	// Add the freed block to the free list.
	AddNewBlock(offset, numDescriptors);
}

void Page::ReleaseRetiredDescriptors(uint64_t frameNumber)
{
	while (!retiredDescriptorsQueue.empty() && retiredDescriptorsQueue.front().FrameNumber <= frameNumber)
	{
		auto& staleDescriptor = retiredDescriptorsQueue.front();

		// The offset of the descriptor in the heap.
		auto offset = staleDescriptor.Offset;
		// The number of descriptors that were allocated.
		auto numDescriptors = staleDescriptor.Size;

		FreeBlock(offset, numDescriptors);

		retiredDescriptorsQueue.pop();
	}
}
