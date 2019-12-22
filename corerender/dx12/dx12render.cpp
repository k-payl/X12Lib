#include "dx12render.h"
#include "pch.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12uniformbuffer.h"
#include "dx12context.h"
#include "dx12vertexbuffer.h"
#include "dx12descriptorheap.h"
#include "dx12uploadheap.h"
#include "gpuprofiler.h"
#include <d3dcompiler.h>
#include <algorithm>


#define HLSL_VER "5_1"

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

	descriptorSizeCBSRV = CR_GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = CR_GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = CR_GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	surface = new Dx12WindowSurface();

	graphicCommandContext = new Dx12GraphicCommandContext(surface, sReleaseFrame);
	copyCommandContext = new Dx12CopyCommandContext();
	
	surface->Init(hwnd, graphicCommandContext->GetD3D12CmdQueue());

	tearingSupported = CheckTearingSupport();

	auto frameFn = std::bind(&Dx12GraphicCommandContext::CurentFrame, graphicCommandContext);
	descriptorAllocator = new DescriptorHeap::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, frameFn);

	gpuprofiler = new gpuProfiler();
	gpuprofiler->Init();
}

void Dx12CoreRenderer::Free()
{
	gpuprofiler->Free();
	delete gpuprofiler;

	graphicCommandContext->Free();
	copyCommandContext->Free();

	for (auto& r : resources)
	{
		if (r->GetRefs() != 1)
			throw new std::exception("Resource is not released properly");
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

#ifdef _DEBUG
	{
		ComPtr<IDXGIDebug1> pDebug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));
		pDebug->ReportLiveObjects(DXGI_DEBUG_DXGI, DXGI_DEBUG_RLO_ALL);
	}
#endif

	Release(adapter);
	Release(device);
}

void Dx12CoreRenderer::RecreateBuffers(UINT w, UINT h)
{
	graphicCommandContext->WaitGPUAll();
	
	surface->ResizeBuffers(w, h);

	// after recreating swapchain's buffers frameIndex should be 0??
	graphicCommandContext->frameIndex = 0; // TODO: make frameIndex private
}

const char* getShaderProfileName(SHADER_TYPE type)
{
	switch (type)
	{
		case SHADER_TYPE::SHADER_VERTEX: return "vs_" HLSL_VER;
		case SHADER_TYPE::SHADER_FRAGMENT: return "ps_" HLSL_VER;
	}
	assert(false);
	return nullptr;
}
D3D_SHADER_MACRO *getShaderMacro(SHADER_TYPE type)
{
	static D3D_SHADER_MACRO vertDefines[] =
	{
		"VERTEX", "1",
		nullptr, nullptr
	};
	static D3D_SHADER_MACRO fragmentDefins[] =
	{
		"FRAGMENT", "1",
		nullptr, nullptr
	};

	switch (type)
	{
		case SHADER_TYPE::SHADER_VERTEX: return vertDefines;
		case SHADER_TYPE::SHADER_FRAGMENT: return fragmentDefins;
	}
	return nullptr;
}

UINT formatInBytes(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return 16;
		default: assert(0);
	}
	return 0;
}
UINT formatInBytes(INDEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case INDEX_BUFFER_FORMAT::UNSIGNED_16: return 2;
		case INDEX_BUFFER_FORMAT::UNSIGNED_32: return 4;
		default: assert(0);
	}
	return 0;
}
DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: assert(0);
	}
	return DXGI_FORMAT_UNKNOWN;
}

ComPtr<ID3DBlob> compileShader(const char * src, SHADER_TYPE type)
{
	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> errorBlob;

	if (!src)
		return shader;

	constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR
	#if _DEBUG
		| D3DCOMPILE_DEBUG
		| D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
		| D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif

	if (FAILED(D3DCompile(src, strlen(src), "", getShaderMacro(type), NULL, "main", getShaderProfileName(type),
						flags, 0, shader.GetAddressOf(), errorBlob.GetAddressOf())))
	{
		if (errorBlob)
		{
			unsigned char* error = (unsigned char*)errorBlob->GetBufferPointer();
			printf("%s\n", error);
			assert(0);
		}
		
	}
	return shader;
}

Dx12CoreShader* Dx12CoreRenderer::CreateShader(const char* vertText, const char* fragText,
											const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	ComPtr<ID3DBlob> vertexBlob = compileShader(vertText, SHADER_TYPE::SHADER_VERTEX);
	ComPtr<ID3DBlob> fragmentBlob = compileShader(fragText, SHADER_TYPE::SHADER_FRAGMENT);

	auto* ptr = new Dx12CoreShader{ vertexBlob, fragmentBlob, variabledesc, varNum };
	ptr->AddRef();

	resources.push_back(ptr);

	return ptr;
}

void Dx12CoreRenderer::UpdateBufferResource(ID3D12Resource** dest, ID3D12Resource** intermediate, 
											UINT64 size, const void* data, D3D12_RESOURCE_FLAGS flags)
{
	auto d3dCmdList = copyCommandContext->GetD3D12CmdList();

	// Default heap
	// For a default heap, you need to use a fence because the GPU can be doing copying from an 
	// upload heap to a default heap for example at the same time it is drawing
	ThrowIfFailed(GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size, flags),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(dest)));

	if (data)
	{
		// Upload heap
		// No need fence to track copying RAM->VRAM
		ThrowIfFailed(GetDevice()->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(intermediate)));

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = size;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		UpdateSubresources(d3dCmdList, *dest, *intermediate, 0, 0, 1, &subresourceData);
	}
}

Dx12CoreVertexBuffer* Dx12CoreRenderer::CreateVertexBuffer(const void* vbData, const VeretxBufferDesc* vbDesc,
	const void* idxData, const IndexBufferDesc* idxDesc)
{
	UINT vertexStride = 0;
	for (int i = 0; i < vbDesc->attributesCount; ++i)
	{
		VertexAttributeDesc& attr = vbDesc->attributes[i];
		vertexStride += formatInBytes(attr.format);
	}

	UINT64 bufferSize = (UINT64)vbDesc->vertexCount * vertexStride;
	UINT64 idxBufferSize = idxBufferSize = (UINT64)formatInBytes(idxDesc->format) * idxDesc->vertexCount;

	ComPtr<ID3D12Resource> vertexBuffer;
	ComPtr<ID3D12Resource> indexBuffer;
	{
		ComPtr<ID3D12Resource> uploadVertexBuffer;
		ComPtr<ID3D12Resource> uploadIndexBuffer;

		copyCommandContext->Begin();
		
		UpdateBufferResource(&vertexBuffer, &uploadVertexBuffer, bufferSize, vbData);
		UpdateBufferResource(&indexBuffer, &uploadIndexBuffer, idxBufferSize, idxData);

		copyCommandContext->End();
		copyCommandContext->Submit();
		copyCommandContext->WaitGPUAll(); // wait GPU copying upload -> default heap
	}

	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = indexBuffer->GetGPUVirtualAddress();

	switch (idxDesc->format)
	{
		case INDEX_BUFFER_FORMAT::UNSIGNED_16: ibv.Format = DXGI_FORMAT_R16_UINT; break;
		case INDEX_BUFFER_FORMAT::UNSIGNED_32: ibv.Format = DXGI_FORMAT_R32_UINT; break;
		default: assert(0);
	}
	ibv.SizeInBytes = (UINT)idxBufferSize;

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	inputLayout.resize(vbDesc->attributesCount);

	for (int i = 0; i < vbDesc->attributesCount; ++i)
	{
		D3D12_INPUT_ELEMENT_DESC& l = inputLayout[i];
		l.SemanticName = vbDesc->attributes[i].semanticName;
		l.SemanticIndex = 0;
		l.Format = engineToDXGIFormat(vbDesc->attributes[i].format);
		l.InputSlot = i;
		l.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		l.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		l.InstanceDataStepRate = 0;
	}

	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vbv.SizeInBytes = (UINT)bufferSize;
	vbv.StrideInBytes = vertexStride;

	auto* ptr = new Dx12CoreVertexBuffer
	{
		vbDesc->vertexCount,
		vertexBuffer,
		vbv,
		idxData ? idxDesc->vertexCount : 0,
		indexBuffer,
		ibv,
		std::move(inputLayout)
	};

	ptr->AddRef();

	resources.push_back(ptr);

	return ptr;
}

auto Dx12CoreRenderer::CreateUniformBuffer(size_t size) -> Dx12UniformBuffer*
{
	auto s = uniformBufferVec.size();
	uniformBufferVec.emplace_back(new Dx12UniformBuffer(size));
	return uniformBufferVec[s];
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

	descriptorHeapRTV = CreateDescriptorHeap(CR_GetDevice(), DeferredBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorHeapDSV = CreateDescriptorHeap(CR_GetDevice(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	
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

		CR_GetDevice()->CreateRenderTargetView(color.Get(), nullptr, rtvHandle);

		colorBuffers[i] = color;

		rtvHandle.Offset(CR_RTV_DescriptorsSize());
	}

	// Create a depth buffer
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	ThrowIfFailed(CR_GetDevice()->CreateCommittedResource(
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

	CR_GetDevice()->CreateDepthStencilView(depthBuffer.Get(), &dsv, descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart());
}

void Dx12CoreRenderer::ReleaseResource(int& refs, IResourceUnknown *ptr)
{
	--refs;

	assert(refs > 0);

	if (refs == 1)
	{
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
}

void Dx12CoreRenderer::GPUProfileRender()
{
	gpuprofiler->Render();
}
