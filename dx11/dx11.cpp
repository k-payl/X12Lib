#include "pch.h"
#include "dx11.h"
#include "common.h"
#include <cassert>
#include <d3dcompiler.h>
#include <string>

using namespace std;

#define HLSL_VER "5_0"

#ifndef NDEBUG
#define SHADER_COMPILE_FLAGS (D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG)
#else
#define SHADER_COMPILE_FLAGS (D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3DCOMPILE_OPTIMIZATION_LEVEL3)
#endif

ID3D11Device* g_device;
ID3D11DeviceContext* g_context;
IDXGISwapChain* g_swapchain;
ID3D11RenderTargetView* rtv;
ID3D11Texture2D* swapChainTexture;
ID3D11DepthStencilView* dsv;
ID3D11Texture2D* depthTex;

UINT width, height;

ID3D11Device* dx11::GetDx11Device() { return g_device; }
ID3D11DeviceContext* dx11::GetDx11Context() { return g_context; }
IDXGISwapChain* dx11::GetDx11Swapchain() { return g_swapchain; }
ID3D11RenderTargetView* dx11::GetDx11RTV() { return rtv; }
ID3D11DepthStencilView* dx11::GetDx11DSV() { return dsv; }
UINT dx11::GetWidth() { return width; }
UINT dx11::GetHeight() { return height; }

void dx11::InitDX11(void* hwnd_)
{
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;
	ComPtr<IDXGISwapChain> swapChain;
	
	HWND hwnd = *reinterpret_cast<HWND*>(hwnd_);

	HRESULT hr = S_OK;
	UINT createDeviceFlags = 0;

#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = 3;

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	UINT numFeatureLevels = 2;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		D3D_DRIVER_TYPE g_driverType = driverTypes[driverTypeIndex];
		D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;

		hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, device.GetAddressOf(), &g_featureLevel, context.GetAddressOf());

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1, D3D11_SDK_VERSION, device.GetAddressOf(), &g_featureLevel, context.GetAddressOf());
		}

		if (SUCCEEDED(hr))
			break;
	}
	ThrowIfFailed(hr);

	RECT r;
	GetClientRect(hwnd, &r);
	auto width = r.right - r.left;
	auto height = r.bottom - r.top;

	hr = E_FAIL;

	ComPtr<IDXGIFactory1> dxgiFactory;
	{
		ComPtr<IDXGIDevice> dxgiDevice;
		hr = device.As(&dxgiDevice);
		if (SUCCEEDED(hr))
		{
			ComPtr<IDXGIAdapter> adapter;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), &dxgiFactory);
		}
	}

	ThrowIfFailed(hr);

	ComPtr<IDXGIFactory2> dxgiFactory2;
	hr = dxgiFactory.As(&dxgiFactory2);

	if (dxgiFactory2)
	{
		ComPtr<IDXGISwapChain1> swapChain1;

		DXGI_SWAP_CHAIN_DESC1 sd{};
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		ThrowIfFailed(dxgiFactory2->CreateSwapChainForHwnd(device.Get(), hwnd, &sd, nullptr, nullptr, &swapChain1));

		ThrowIfFailed(swapChain1.As(&swapChain));
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd{};
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hwnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		ThrowIfFailed(dxgiFactory->CreateSwapChain(device.Get(), &sd, &swapChain));
	}

	// We block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	g_device = device.Detach();
	g_context = context.Detach();
	g_swapchain = swapChain.Detach();
}

void dx11::FreeDX11()
{
	g_device->Release();
	g_context->Release();
	g_swapchain->Release();
	rtv->Release();
	dsv->Release();
	depthTex->Release();
}

void dx11::CreatePixelShader(std::shared_ptr<char[]> src, ComPtr<ID3D11PixelShader>& s)
{
	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> errorBlob;

	LPCSTR pProfile = "ps_" HLSL_VER;
	const D3D_SHADER_MACRO defines1[] =
	{
		"PIXEL", "1",
		nullptr, nullptr
	};

	D3DCompile(src.get(), strlen(src.get()), "", defines1, NULL, "main", pProfile, SHADER_COMPILE_FLAGS, 0, shader.GetAddressOf(), &errorBlob);

	if (errorBlob)
	{
		char* c = (char*)errorBlob->GetBufferPointer();
		errorBlob->Release();
	}
	ThrowIfFailed(GetDx11Device()->CreatePixelShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, s.GetAddressOf()));
}
enum class SHADER_TYPE
{
	SHADER_VERTEX,
	SHADER_GEOMETRY,
	SHADER_FRAGMENT,
	SHADER_COMPUTE
};
const char* dgxgi_to_hlsl_type(DXGI_FORMAT f)
{
	switch (f)
	{
		case DXGI_FORMAT_R32_FLOAT:
			return "float";
		case DXGI_FORMAT_R32G32_FLOAT:
			return "float2";
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return "float3";
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return "float4";
		default:
			assert(false); // unknown type
			return nullptr;
			break;
	}
}

const char* get_shader_profile(SHADER_TYPE type)
{
	switch (type)
	{
		case SHADER_TYPE::SHADER_VERTEX: return "vs_" HLSL_VER;
		case SHADER_TYPE::SHADER_GEOMETRY: return "gs_" HLSL_VER;
		case SHADER_TYPE::SHADER_FRAGMENT: return "ps_" HLSL_VER;
		case SHADER_TYPE::SHADER_COMPUTE: return "cs_" HLSL_VER;
	}
	assert(false);
	return nullptr;
}

void dx11::CreateVertexBuffer(void* vertex, void* index, UINT numVertex, UINT numIndex, ComPtr<ID3D11Buffer>& vb_,
							  ComPtr<ID3D11Buffer>& ib_, ComPtr<ID3D11InputLayout>& inputLayout_, UINT &stride, bool cpu_usage, bool colors_)
{
	const int normals = 0;
	const int indexes = numIndex;
	const int texCoords = 0;
	const int colors = colors_;
	const int bytesWidth = 16 + 16 * normals + 8 * texCoords + 16 * colors;
	const int bytes = bytesWidth * numVertex;

	ComPtr<ID3DBlob> blob;

	unsigned int offset = 0;

	std::vector<D3D11_INPUT_ELEMENT_DESC> layout{ {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0} };
	offset += 16;

	//if (normals)
	//{
	//	layout.push_back({ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	//	offset += 16;
	//}

	//if (texCoords)
	//{
	//	layout.push_back({ "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
	//	offset += 8;
	//}

	if (colors)
	{
		layout.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offset, D3D11_INPUT_PER_VERTEX_DATA, 0 });
		offset += 16;
	}

	stride = offset;

	//
	// create dummy shader for CreateInputLayout() 
	std::string src;
	src = "struct VS_INPUT { ";

	for (int i = 0; i < layout.size(); i++)
	{
		const D3D11_INPUT_ELEMENT_DESC& el = layout[i];
		src += dgxgi_to_hlsl_type(el.Format) + string(" v") + std::to_string(i) + (i == 0 ? " : POSITION" : " : TEXCOORD") + std::to_string(el.SemanticIndex) + ";";
	}
	src += "}; struct VS_OUTPUT { float4 position : SV_POSITION; }; VS_OUTPUT mainVS(VS_INPUT input) { VS_OUTPUT o; o.position = float4(0,0,0,0); return o; } float4 PS( VS_OUTPUT input) : SV_Target { return float4(0,0,0,0); }";

	ComPtr<ID3DBlob> errorBuffer;
	ComPtr<ID3DBlob> shaderBuffer;

	ThrowIfFailed(D3DCompile(src.c_str(), src.size(), "", NULL, NULL, "mainVS", get_shader_profile(SHADER_TYPE::SHADER_VERTEX), SHADER_COMPILE_FLAGS, 0, &shaderBuffer, &errorBuffer));

	//
	// create input layout
	ThrowIfFailed((GetDx11Device()->CreateInputLayout(reinterpret_cast<const D3D11_INPUT_ELEMENT_DESC*>(&layout[0]), (UINT)layout.size(), shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), &inputLayout_)));

	D3D11_BUFFER_DESC bd{};
	bd.Usage = cpu_usage ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	bd.ByteWidth = bytes;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = cpu_usage ? D3D11_CPU_ACCESS_WRITE : 0;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertex;

	ThrowIfFailed(GetDx11Device()->CreateBuffer(&bd, vertex? &initData : nullptr, &vb_));

	// index buffer
	if (indexes)
	{
		int idxSize = 2; // 16bit
		const int idxBytes = idxSize * numIndex;

		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.Usage = cpu_usage ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = idxBytes;
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufferDesc.CPUAccessFlags = cpu_usage ? D3D11_CPU_ACCESS_WRITE : 0;
		bufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA ibData;
		ibData.pSysMem = index;
		ibData.SysMemPitch = 0;
		ibData.SysMemSlicePitch = 0;

		ThrowIfFailed(GetDx11Device()->CreateBuffer(&bufferDesc, &ibData, &ib_));
	}

}

ComPtr<ID3D11Buffer> dx11::CreateConstanBuffer(UINT size)
{
	ComPtr<ID3D11Buffer> ret;

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = size;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ThrowIfFailed(dx11::GetDx11Device()->CreateBuffer(&desc, nullptr, ret.GetAddressOf()));

	return ret;
}

auto dx11::CreateStructuredBuffer(ComPtr<ID3D11ShaderResourceView>& SRVOut, ComPtr<ID3D11Buffer>& bufferOut, size_t structureSize, size_t num, const void* data) -> void
{
	assert(structureSize % 16 == 0);

	size_t bytes = structureSize * num;

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//desc.CPUAccessFlags = 0;
	desc.ByteWidth = UINT(bytes);
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = (UINT)structureSize;

	ThrowIfFailed(GetDx11Device()->CreateBuffer(&desc, nullptr, &bufferOut));

	// Create SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.BufferEx.NumElements = desc.ByteWidth / desc.StructureByteStride;

	ThrowIfFailed(GetDx11Device()->CreateShaderResourceView(bufferOut.Get(), &srvDesc, &SRVOut));

	D3D11_BOX box{};
	box.right = (UINT)bytes;
	box.bottom = 1;
	box.back = 1;
	GetDx11Context()->UpdateSubresource(bufferOut.Get(), 0, &box, data, (UINT)bytes, 0);
}

ComPtr<ID3D11Query> dx11::CreateQuery(D3D11_QUERY type)
{
	ComPtr<ID3D11Query> ret;

	D3D11_QUERY_DESC desc{};
	desc.Query = type;
	ThrowIfFailed(dx11::GetDx11Device()->CreateQuery(&desc, &ret));

	return ret;
}

auto dx11::UpdateUniformBuffer(ID3D11Buffer* b, const void* data, UINT size) -> void
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	GetDx11Context()->Map(b, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, data, size);
	GetDx11Context()->Unmap(b, 0);
}

void dx11::CreateVertexShader(std::shared_ptr<char[]> src, ComPtr<ID3D11VertexShader>& s)
{
	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> errorBlob;

	LPCSTR pProfile = "vs_" HLSL_VER;
	const D3D_SHADER_MACRO defines1[] =
	{
		"VERTEX", "1",
		nullptr, nullptr
	};

	D3DCompile(src.get(), strlen(src.get()), "", defines1, NULL, "main", pProfile, SHADER_COMPILE_FLAGS, 0, shader.GetAddressOf(), &errorBlob);

	if (errorBlob)
	{
		char* c = (char*)errorBlob->GetBufferPointer();
		errorBlob->Release();
	}
	ThrowIfFailed(dx11::GetDx11Device()->CreateVertexShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, s.GetAddressOf()));
}

void dx11::_Resize(UINT w, UINT h)
{
	width = w;
	height = h;

	if (rtv)
		rtv->Release();

	ThrowIfFailed(g_swapchain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));

	ID3D11Texture2D* pBuffer;
	ThrowIfFailed(g_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)& pBuffer));

	ThrowIfFailed(g_device->CreateRenderTargetView(pBuffer, NULL, &rtv));
	pBuffer->Release();

	if (depthTex)
		depthTex->Release();

	// Depth resource
	D3D11_TEXTURE2D_DESC descDepth{};
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_R32_TYPELESS;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	ThrowIfFailed(g_device->CreateTexture2D(&descDepth, nullptr, &depthTex));

	if (dsv)
		dsv->Release();

	// Depth DSV
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV{};
	descDSV.Format = DXGI_FORMAT_D32_FLOAT;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	ThrowIfFailed(g_device->CreateDepthStencilView(depthTex, &descDSV, &dsv));
}

