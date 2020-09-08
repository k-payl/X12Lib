#include "core.h"
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

x12::TEXTURE_FORMAT x12::D3DToEng(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8_UNORM:				return x12::TEXTURE_FORMAT::R8;
	case DXGI_FORMAT_R8G8_UNORM:			return x12::TEXTURE_FORMAT::RG8;
	case DXGI_FORMAT_R8G8B8A8_UNORM:		return x12::TEXTURE_FORMAT::RGBA8;
	case DXGI_FORMAT_B8G8R8A8_UNORM:		return x12::TEXTURE_FORMAT::BGRA8;
	case DXGI_FORMAT_R16_FLOAT:				return x12::TEXTURE_FORMAT::R16F;
	case DXGI_FORMAT_R16G16_FLOAT:			return x12::TEXTURE_FORMAT::RG16F;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:	return x12::TEXTURE_FORMAT::RGBA16F;
	case DXGI_FORMAT_R32_FLOAT:				return x12::TEXTURE_FORMAT::R32F;
	case DXGI_FORMAT_R32G32_FLOAT:			return x12::TEXTURE_FORMAT::RG32F;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:	return x12::TEXTURE_FORMAT::RGBA32F;
	case DXGI_FORMAT_R32_UINT:				return x12::TEXTURE_FORMAT::R32UI;
	case DXGI_FORMAT_BC1_UNORM:				return x12::TEXTURE_FORMAT::DXT1;
	case DXGI_FORMAT_BC2_UNORM:				return x12::TEXTURE_FORMAT::DXT3;
	case DXGI_FORMAT_BC3_UNORM:				return x12::TEXTURE_FORMAT::DXT5;
	case DXGI_FORMAT_R24G8_TYPELESS:		return x12::TEXTURE_FORMAT::D24S8;
	}

	engine::LogCritical("DXGIFormatToEng(): unknown format\n");
	return x12::TEXTURE_FORMAT::UNKNOWN;
}

DXGI_FORMAT x12::EngToD3D(x12::TEXTURE_FORMAT format)
{
	switch (format)
	{
	case x12::TEXTURE_FORMAT::R8:		return DXGI_FORMAT_R8_UNORM;
	case x12::TEXTURE_FORMAT::RG8:		return DXGI_FORMAT_R8G8_UNORM;
	case x12::TEXTURE_FORMAT::RGBA8:		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case x12::TEXTURE_FORMAT::BGRA8:		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case x12::TEXTURE_FORMAT::R16F:		return DXGI_FORMAT_R16_FLOAT;
	case x12::TEXTURE_FORMAT::RG16F:		return DXGI_FORMAT_R16G16_FLOAT;
	case x12::TEXTURE_FORMAT::RGBA16F:	return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case x12::TEXTURE_FORMAT::R32F:		return DXGI_FORMAT_R32_FLOAT;
	case x12::TEXTURE_FORMAT::RG32F:		return DXGI_FORMAT_R32G32_FLOAT;
	case x12::TEXTURE_FORMAT::RGBA32F:	return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case x12::TEXTURE_FORMAT::R32UI:		return DXGI_FORMAT_R32_UINT;
	case x12::TEXTURE_FORMAT::DXT1:		return DXGI_FORMAT_BC1_UNORM;
	case x12::TEXTURE_FORMAT::DXT3:		return DXGI_FORMAT_BC2_UNORM;
	case x12::TEXTURE_FORMAT::DXT5:		return DXGI_FORMAT_BC3_UNORM;
	case x12::TEXTURE_FORMAT::D24S8:		return DXGI_FORMAT_R24G8_TYPELESS;
	}

	engine::LogCritical("EngToDX11Format(): unknown format\n");
	return DXGI_FORMAT_UNKNOWN;
}

size_t x12::BitsPerPixel(DXGI_FORMAT fmt)
{
	switch (fmt)
	{
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		return 128;

	case DXGI_FORMAT_R32G32B32_TYPELESS:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32B32_SINT:
		return 96;

	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_Y416:
	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		return 64;

	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_AYUV:
	case DXGI_FORMAT_Y410:
	case DXGI_FORMAT_YUY2:
		return 32;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		return 24;

	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
	case DXGI_FORMAT_A8P8:
	case DXGI_FORMAT_B4G4R4A4_UNORM:
		return 16;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_NV11:
		return 12;

	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_AI44:
	case DXGI_FORMAT_IA44:
	case DXGI_FORMAT_P8:
		return 8;

	case DXGI_FORMAT_R1_UNORM:
		return 1;

	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		return 4;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		return 8;

	default:
		abort();
	}
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
		case VERTEX_BUFFER_FORMAT::FLOAT3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case VERTEX_BUFFER_FORMAT::FLOAT2: return DXGI_FORMAT_R32G32_FLOAT;
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

void* x12::Dx12WindowSurface::GetNativeResource(int i)
{
	return colorBuffers[i].Get();
}
