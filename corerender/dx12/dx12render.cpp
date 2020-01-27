#include "pch.h"
#include "dx12render.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12uniformbuffer.h"
#include "dx12structuredbuffer.h"
#include "dx12context.h"
#include "dx12vertexbuffer.h"
#include "dx12texture.h"
#include "dx12descriptorheap.h"
#include "dx12uploadheap.h"
#include <d3dcompiler.h>
#include <algorithm>

Dx12CoreRenderer* _coreRender;

void Dx12CoreRenderer::ReleaseFrame(uint64_t fenceID)
{
	descriptorAllocator->ReclaimMemory(fenceID);
}

void Dx12CoreRenderer::sReleaseFrame(uint64_t fenceID)
{
	_coreRender->ReleaseFrame(fenceID);
}

Dx12CoreRenderer::Dx12CoreRenderer()
{
	assert(_coreRender == nullptr && "Should be created only one instance of Dx12CoreRenderer");
	_coreRender = this;
}
Dx12CoreRenderer::~Dx12CoreRenderer()
{
	_coreRender = nullptr;
}

FastFrameAllocator::PagePool* Dx12CoreRenderer::GetFastFrameAllocatorPool(UINT bufferSize)
{
	auto it = fastAllocatorPagePools.find(bufferSize);

	if (it != fastAllocatorPagePools.end())
		return it->second;

	FastFrameAllocator::PagePool* pool = new FastFrameAllocator::PagePool(bufferSize);

	fastAllocatorPagePools[bufferSize] = pool;

	return pool;
}

static bool CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	ComPtr<dxgifactory_t> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

void Dx12CoreRenderer::Init(HWND hwnd)
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif

	{
		ComPtr<dxgifactory_t> dxgiFactory;
		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

		ComPtr<IDXGIAdapter1> dxgiAdapter1;

		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			dxgiAdapter1->GetDesc1(&adapterDesc);

			// Choose adapter with the largest dedicated video memory
			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && 
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
				adapterDesc.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&adapter));
			}
		}
	}

	ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(GetDevice()->QueryInterface(__uuidof(ID3D12InfoQueue), &pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	descriptorSizeCBSRV = CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	surface = new Dx12WindowSurface();

	graphicCommandContext = new Dx12GraphicCommandContext(surface, sReleaseFrame);
	copyCommandContext = new Dx12CopyCommandContext();
	
	surface->Init(hwnd, graphicCommandContext->GetD3D12CmdQueue());

	tearingSupported = CheckTearingSupport();

	auto frameFn = std::bind(&Dx12GraphicCommandContext::CurentFrame, graphicCommandContext);
	descriptorAllocator = new DescriptorHeap::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, frameFn);
}

void Dx12CoreRenderer::Free()
{
	graphicCommandContext->Free();
	copyCommandContext->Free();

	for (auto& r : resources)
	{
		if (r->GetRefs() != 1)
			throw std::exception("Resource is not released properly");
	}
	resources.clear();

	for (Dx12UniformBuffer* b : uniformBufferVec)
		delete b;
	uniformBufferVec.clear();

	delete graphicCommandContext;
	graphicCommandContext = nullptr;

	delete copyCommandContext;
	copyCommandContext = nullptr;

	uniformBufferVec.clear();
	psoMap.clear();

	for (auto pools : fastAllocatorPagePools)
	{
		FastFrameAllocator::PagePool* pool = pools.second;
		delete pool;
	}
	fastAllocatorPagePools.clear();

	delete surface;
	surface = nullptr;

	delete descriptorAllocator;
	descriptorAllocator = nullptr;

	// WTF?
	adapter->Release();
	adapter->Release();

	Release(adapter);

#ifdef _DEBUG
	{
		ComPtr<IDXGIDebug1> pDebug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));
		pDebug->ReportLiveObjects(DXGI_DEBUG_DXGI, DXGI_DEBUG_RLO_ALL);
	}
#endif

	Release(device);
}

void Dx12CoreRenderer::RecreateBuffers(UINT w, UINT h)
{
	graphicCommandContext->WaitGPUAll();
	
	surface->ResizeBuffers(w, h);

	// after recreating swapchain's buffers frameIndex should be 0??
	graphicCommandContext->frameIndex = 0; // TODO: make frameIndex private
}

bool Dx12CoreRenderer::CreateShader(Dx12CoreShader** out, const char* vertText, const char* fragText,
											const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	auto* ptr = new Dx12CoreShader{};
	ptr->Init(vertText, fragText, variabledesc, varNum);
	ptr->AddRef();

	resources.push_back(ptr);

	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateVertexBuffer(Dx12CoreVertexBuffer** out, const void* vbData, const VeretxBufferDesc* vbDesc,
	const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_USAGE usage)
{
	auto* ptr = new Dx12CoreVertexBuffer{};
	ptr->Init(vbData, vbDesc, idxData, idxDesc, usage);
	ptr->AddRef();

	resources.push_back(ptr);

	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateUniformBuffer(Dx12UniformBuffer **out, size_t size)
{
	auto ptr = new Dx12UniformBuffer((UINT)size);
	uniformBufferVec.emplace_back(ptr);

	*out = ptr;
	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateStructuredBuffer(Dx12CoreStructuredBuffer** out, size_t structureSize, size_t num, const void* data)
{
	auto* ptr = new Dx12CoreStructuredBuffer;
	ptr->Init(structureSize, num, data);
	ptr->AddRef();

	resources.push_back(ptr);
	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateTexture(Dx12CoreTexture** out, std::unique_ptr<uint8_t[]> ddsData, std::vector<D3D12_SUBRESOURCE_DATA> subresources,
									 ID3D12Resource* d3dtexture)
{
	assert(subresources.size() == 1); // Not impl

	// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
	// the command list that references it has finished executing on the GPU.
	// We will flush the GPU at the end of this method to ensure the resource is not
	// prematurely destroyed.
	ComPtr<ID3D12Resource> d3dTextureUploadHeap;

	D3D12_RESOURCE_DESC desc = {};
	desc = d3dtexture->GetDesc();

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(d3dtexture, 0, (UINT)subresources.size());

	// Create the GPU upload buffer.
	ThrowIfFailed(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&d3dTextureUploadHeap)));

	copyCommandContext->Begin();
		UpdateSubresources(copyCommandContext->GetD3D12CmdList(), d3dtexture, d3dTextureUploadHeap.Get(), 0, 0, 1, &subresources[0]);
	copyCommandContext->End();
	copyCommandContext->Submit();
	copyCommandContext->WaitGPUAll(); // wait GPU copying upload -> default heap

	auto* ptr = new Dx12CoreTexture();
	ptr->InitFromExistingResource(d3dtexture);
	ptr->AddRef();

	resources.push_back(ptr);

	*out = ptr;

	return ptr != nullptr;
}

ID3D12RootSignature* Dx12CoreRenderer::GetDefaultRootSignature()
{
	if (defaultRootSignature)
		return defaultRootSignature.Get();

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ID3DBlob* blob;
	ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));

	ThrowIfFailed(GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&defaultRootSignature)));

	return defaultRootSignature.Get();
}

void Dx12WindowSurface::Init(HWND hwnd, ID3D12CommandQueue* queue)
{
	RECT r;
	GetClientRect(hwnd, &r);

	width = r.right - r.left;
	height = r.bottom - r.top;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = DeferredBuffers;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; // It is recommended to always allow tearing if tearing support is available.

	ComPtr<dxgifactory_t> dxgiFactory4;

#if defined(_DEBUG)
	const UINT createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#else 
	const UINT createFactoryFlags = 0;
#endif

	ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	ComPtr<IDXGISwapChain1> swapChain1;
	ThrowIfFailed(dxgiFactory4->CreateSwapChainForHwnd(queue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	ThrowIfFailed(swapChain1.As(&swapChain));

	descriptorHeapRTV = CreateDescriptorHeap(CR_GetD3DDevice(), DeferredBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorHeapDSV = CreateDescriptorHeap(CR_GetD3DDevice(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	
	ResizeBuffers(width, height);
}

void Dx12WindowSurface::ResizeBuffers(unsigned width_, unsigned height_)
{
	width = max(width_, 1u);
	height = max(height_, 1u);

	depthBuffer.Reset();

	for (int i = 0; i < DeferredBuffers; ++i)
		colorBuffers[i].Reset();

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	ThrowIfFailed(swapChain->GetDesc(&swapChainDesc));
	ThrowIfFailed(swapChain->ResizeBuffers(DeferredBuffers, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));
	
	// Create handles for color render target
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < DeferredBuffers; ++i)
	{
		ComPtr<ID3D12Resource> color;
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&color)));

		CR_GetD3DDevice()->CreateRenderTargetView(color.Get(), nullptr, rtvHandle);

		colorBuffers[i] = color;

		rtvHandle.Offset(CR_RTV_DescriptorsSize());
	}

	// Create a depth buffer
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(&depthBuffer)
	));

	// Create handles for depth stencil
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	CR_GetD3DDevice()->CreateDepthStencilView(depthBuffer.Get(), &dsv, descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart());
}

void Dx12CoreRenderer::ReleaseResource(int& refs, IResourceUnknown *ptr)
{
	assert(refs == 1);

	auto it = std::find_if(resources.begin(), resources.end(), [ptr](const intrusive_ptr<IResourceUnknown>& r) -> bool
	{
		return r.get() == ptr;
	});

	assert(it != resources.end());

	refs = 10; // hack to avoid recursion because ~intrusive_ptr calls Release()
	resources.erase(it);
	refs = 0;
	delete ptr;
}

uint64_t Dx12CoreRenderer::UniformBufferUpdates()
{
	return graphicCommandContext->getStat().uniformBufferUpdates;
}

uint64_t Dx12CoreRenderer::StateChanges()
{
	return graphicCommandContext->getStat().stateChanges;
}

uint64_t Dx12CoreRenderer::Triangles()
{
	return graphicCommandContext->getStat().triangles;
}

uint64_t Dx12CoreRenderer::DrawCalls()
{
	return graphicCommandContext->getStat().drawCalls;
}
