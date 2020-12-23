#include "pch.h"
#include "vkrender.h"
#include "vkcommon.h"
#include "vkcontext.h"
#include "vkvertexbuffer.h"
#include "vkshader.h"

x12::VkCoreRenderer::VkCoreRenderer()
{
	assert(_coreRender == nullptr);
	_coreRender = this;
}

x12::VkCoreRenderer::~VkCoreRenderer()
{
	_coreRender = nullptr;
}

auto x12::VkCoreRenderer::Init() -> void
{
	instance = x12::vk::createInstance();

	// Select GPU
	VkPhysicalDevice physicalDevices[16];
	uint32_t physicalDeviceCount = _countof(physicalDevices);
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices));
	physicalDevice = x12::vk::pickPhysicalDevice(physicalDevices, physicalDeviceCount);
	assert(physicalDevice);

	// Check extensions
	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, nullptr));

	std::vector<VkExtensionProperties> extensions(extensionCount);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, 0, &extensionCount, extensions.data()));

	// Create instance of physical device
	device = x12::vk::createDevice(instance, physicalDevice);

	// Get GPU family indexes
	familyIndex = vk::getGraphicsFamilyIndex(physicalDevice);
	assert(familyIndex != VK_QUEUE_FAMILY_IGNORED);

	vkGetDeviceQueue(device, familyIndex, 0, &graphicsQueue);
	//vkGetDeviceQueue(device, presentFamilyIndex, 0, &presentQueue);

	VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = familyIndex;
	VK_CHECK(vkCreateCommandPool(device, &poolInfo, 0, &commandPool));

	graphicCommandContext = new VkGraphicCommandList();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	allocatorInfo.instance = instance;

	vmaCreateAllocator(&allocatorInfo, &allocator);
}

auto x12::VkCoreRenderer::Free() -> void
{
	vmaDestroyAllocator(allocator);
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDevice(device, 0);
	vkDestroyInstance(instance, 0);
}

auto x12::VkCoreRenderer::GetGraphicCommandList() -> ICoreGraphicCommandList*
{
	return graphicCommandContext;
}

auto x12::VkCoreRenderer::GetGraphicCommandList(int32_t id) -> ICoreGraphicCommandList*
{
	return nullptr;
}

auto x12::VkCoreRenderer::GetCopyCommandContext() -> ICoreCopyCommandList*
{
	return nullptr;
}

auto x12::VkCoreRenderer::_FetchSurface(HWND hwnd) -> surface_ptr
{
	if (auto it = surfaces.find(hwnd); it == surfaces.end())
	{
		surface_ptr surface = std::make_shared<x12::VkWindowSurface>();
		surface->Init(hwnd, this);

		surfaces[hwnd] = surface;

		return std::move(surface);
	}
	else
		return it->second;
}

auto x12::VkCoreRenderer::RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void
{
	//graphicCommandContext->WaitGPUAll();

	surface_ptr surf = _FetchSurface(hwnd);
	surf->ResizeBuffers(newWidth, newHeight);

	// after recreating swapchain's buffers frameIndex should be 0??
	//graphicCommandContext->frameIndex = 0; // TODO: make frameIndex private

}

auto x12::VkCoreRenderer::GetWindowSurface(HWND hwnd) -> surface_ptr
{
	surface_ptr surf = _FetchSurface(hwnd);
	surfacesForPresenting.push_back(surf);

	return surf;
}

auto x12::VkCoreRenderer::PresentSurfaces() -> void
{
	return void();
}

auto x12::VkCoreRenderer::ExecuteCommandList(ICoreCopyCommandList* cmdList) -> void
{
}

auto x12::VkCoreRenderer::WaitGPU() -> void
{
}

auto x12::VkCoreRenderer::WaitGPUAll() -> void
{
}

bool x12::VkCoreRenderer::CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText, const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	return false;
}

bool x12::VkCoreRenderer::CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	auto* ptr = new VkCoreShader{};
	ptr->InitCompute(name, text, variabledesc, varNum);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool x12::VkCoreRenderer::CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE mem)
{
	auto* ptr = new VkCoreVertexBuffer{};
	ptr->Init(name, vbData, vbDesc, idxData, idxDesc, mem);
	ptr->AddRef();
	*out = ptr;

	return false;
}

bool x12::VkCoreRenderer::CreateBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, BUFFER_FLAGS flags, MEMORY_TYPE mem, const void* data, size_t num)
{
	return false;
}

bool x12::VkCoreRenderer::CreateTexture(ICoreTexture** out, LPCWSTR name, const uint8_t* data, size_t size, int32_t width, int32_t height, uint32_t mipCount, TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags)
{
	return false;
}

bool x12::VkCoreRenderer::CreateTextureFrom(ICoreTexture** out, LPCWSTR name, ID3D12Resource* d3dexistingtexture)
{
	return false;
}

bool x12::VkCoreRenderer::CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture)
{
	return false;
}

bool x12::VkCoreRenderer::CreateResourceSet(IResourceSet** out, const ICoreShader* shader)
{
	return false;
}

bool x12::VkCoreRenderer::CreateQuery(ICoreQuery** out)
{
	return false;
}
