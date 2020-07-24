
#include "dx12common.h"
#include "dx12render.h"
#include "dx12commandlist.h"

using namespace engine;

static std::mutex resourcesMutex;
std::vector<x12::IResourceUnknown*> x12::IResourceUnknown::resources;

bool x12::d3d12::CheckTearingSupport()
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

x12::IResourceUnknown::IResourceUnknown(uint16_t id_) : id(id_)
{
	AddRef();
	std::scoped_lock guard(resourcesMutex);
	resources.push_back(this);
}

void x12::IResourceUnknown::ReleaseResource(int& refs, x12::IResourceUnknown* ptr)
{
	assert(refs == 1);

	//std::scoped_lock guard(resourcesMutex);

	auto it = std::find_if(resources.begin(), resources.end(), [ptr](const IResourceUnknown* r) -> bool
	{
		return r == ptr;
	});

	assert(it != resources.end());

	resources.erase(it);
	delete ptr;
}

DXGI_FORMAT x12::d3d12::engineToDXGIFormat(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: assert(0);
	}
	return DXGI_FORMAT_UNKNOWN;
}

void x12::IResourceUnknown::Release()
{
	--refs;
	assert(refs > 0);

	if (refs == 1)
		ReleaseResource(refs, this);
}

void x12::IResourceUnknown::CheckResources()
{
	for (auto& r : resources)
	{
		if (r->GetRefs() != 1)
			throw std::exception("Resource is not released properly");
	}
}

void x12::Dx12WindowSurface::Init(HWND hwnd, ICoreRenderer* render)
{
	ID3D12CommandQueue* queue = static_cast<Dx12CoreRenderer*>(render)->CommandQueue();

	RECT r;
	GetClientRect(hwnd, &r);

	width = r.right - r.left;
	height = r.bottom - r.top;

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = {1, 0};
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = DeferredBuffers;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = x12::d3d12::CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; // It is recommended to always allow tearing if tearing support is available.

	ComPtr<dxgifactory_t> dxgiFactory4;

#if defined(_DEBUG)
	const UINT createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#else 
	const UINT createFactoryFlags = 0;
#endif

	throwIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

	ComPtr<IDXGISwapChain1> swapChain1;
	throwIfFailed(dxgiFactory4->CreateSwapChainForHwnd(queue, hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));

	throwIfFailed(swapChain1.As(&swapChain));

	descriptorHeapRTV = d3d12::CreateDescriptorHeap(d3d12::CR_GetD3DDevice(), DeferredBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	x12::d3d12::set_name(descriptorHeapRTV.Get(), L"Descriptor heap for backbuffers buffer %u RTV descriptors", DeferredBuffers);

	descriptorHeapDSV = d3d12::CreateDescriptorHeap(d3d12::CR_GetD3DDevice(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	x12::d3d12::set_name(descriptorHeapDSV.Get(), L"Descriptor heap for backbuffers buffer %u DSV descriptors", 1);

	ResizeBuffers(width, height);
}

void x12::Dx12WindowSurface::ResizeBuffers(unsigned width_, unsigned height_)
{
	width = math::max(width_, 1u);
	height = math::max(height_, 1u);

	depthBuffer.Reset();

	for (int i = 0; i < DeferredBuffers; ++i)
		colorBuffers[i].Reset();

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	throwIfFailed(swapChain->GetDesc(&swapChainDesc));
	throwIfFailed(swapChain->ResizeBuffers(DeferredBuffers, width, height, swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

	// Create handles for color render target
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < DeferredBuffers; ++i)
	{
		ComPtr<ID3D12Resource> color;
		throwIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&color)));

		d3d12::CR_GetD3DDevice()->CreateRenderTargetView(color.Get(), nullptr, rtvHandle);

		x12::d3d12::set_name(color.Get(), L"Swapchain back buffer #%d", i);

		colorBuffers[i] = color;

		rtvHandle.Offset(d3d12::CR_RTV_DescriptorsSize());
	}

	// Create a depth buffer
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = {1.0f, 0};

	x12::memory::CreateCommitted2DTexture(&depthBuffer, width, height, 1, DXGI_FORMAT_D32_FLOAT,
										  D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue);

	x12::d3d12::set_name(depthBuffer.Get(), L"Swapchain back depth buffer");

	// Create handles for depth stencil
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	d3d12::CR_GetD3DDevice()->CreateDepthStencilView(depthBuffer.Get(), &dsv, descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart());
}

void x12::Dx12WindowSurface::Present()
{
	UINT syncInterval = d3d12::CR_IsVSync() ? 1 : 0;
	UINT presentFlags = d3d12::CR_IsTearingSupport() && !d3d12::CR_IsVSync() ? DXGI_PRESENT_ALLOW_TEARING : 0;

	throwIfFailed(swapChain->Present(syncInterval, presentFlags));
}
