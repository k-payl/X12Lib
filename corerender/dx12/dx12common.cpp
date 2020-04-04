#include "pch.h"
#include "dx12common.h"
#include "dx12render.h"

static std::mutex resourcesMutex;
std::vector<IResourceUnknown*> IResourceUnknown::resources;

bool x12::impl::CheckTearingSupport()
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

IResourceUnknown::IResourceUnknown()
{
	AddRef();
	std::scoped_lock guard(resourcesMutex);
	resources.push_back(this);
}

void IResourceUnknown::ReleaseResource(int& refs, IResourceUnknown* ptr)
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

DXGI_FORMAT x12::impl::engineToDXGIFormat(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: assert(0);
	}
	return DXGI_FORMAT_UNKNOWN;
}

void IResourceUnknown::Release()
{
	--refs;
	assert(refs > 0);

	if (refs == 1)
		ReleaseResource(refs, this);
}

void IResourceUnknown::CheckResources()
{
	for (auto& r : resources)
	{
		if (r->GetRefs() != 1)
			throw std::exception("Resource is not released properly");
	}
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
	swapChainDesc.Flags = x12::impl::CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0; // It is recommended to always allow tearing if tearing support is available.

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
	x12::impl::set_name(descriptorHeapRTV.Get(), L"Descriptor heap for backbuffers buffer %u RTV descriptors", DeferredBuffers);

	descriptorHeapDSV = CreateDescriptorHeap(CR_GetD3DDevice(), 1, D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	x12::impl::set_name(descriptorHeapDSV.Get(), L"Descriptor heap for backbuffers buffer %u DSV descriptors", 1);

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

		x12::impl::set_name(color.Get(), L"Swapchain back buffer #%d", i);

		colorBuffers[i] = color;

		rtvHandle.Offset(CR_RTV_DescriptorsSize());
	}

	// Create a depth buffer
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	x12::memory::CreateCommitted2DTexture(&depthBuffer, width, height, 1, DXGI_FORMAT_D32_FLOAT,
										  D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue);

	x12::impl::set_name(depthBuffer.Get(), L"Swapchain back depth buffer");

	// Create handles for depth stencil
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	CR_GetD3DDevice()->CreateDepthStencilView(depthBuffer.Get(), &dsv, descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart());
}

void Dx12WindowSurface::Present()
{
	UINT syncInterval = CR_IsVSync() ? 1 : 0;
	UINT presentFlags = CR_IsTearingSupport() && !CR_IsVSync() ? DXGI_PRESENT_ALLOW_TEARING : 0;

	ThrowIfFailed(swapChain->Present(syncInterval, presentFlags));
}
