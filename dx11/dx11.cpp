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

void initDX11(void* hwnd_, ID3D11Device*& device_, ID3D11DeviceContext*& context_, IDXGISwapChain*& swapChain_)
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

	device_ = device.Detach();
	context_ = context.Detach();
	swapChain_ = swapChain.Detach();

	//bool formatSupport = false;

	//D3D11_FEATURE_DATA_D3D11_OPTIONS2 FeatureData;
	//ZeroMemory(&FeatureData, sizeof(FeatureData));
	//hr = device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &FeatureData, sizeof(FeatureData));
	//if (SUCCEEDED(hr))
	//{
	//	// TypedUAVLoadAdditionalFormats contains a Boolean that tells you whether the feature is supported or not
	//	if (FeatureData.TypedUAVLoadAdditionalFormats)
	//	{
	//		// Can assume “all-or-nothing” subset is supported (e.g. R32G32B32A32_FLOAT)
	//		// Can not assume other formats are supported, so we check:
	//		D3D11_FEATURE_DATA_FORMAT_SUPPORT2 FormatSupport;
	//		ZeroMemory(&FormatSupport, sizeof(FormatSupport));
	//		FormatSupport.InFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	//		hr = device->CheckFeatureSupport(D3D11_FEATURE_FORMAT_SUPPORT2, &FormatSupport, sizeof(FormatSupport));
	//		if (SUCCEEDED(hr) && (FormatSupport.OutFormatSupport2 & D3D11_FORMAT_SUPPORT2_UAV_TYPED_LOAD) != 0)
	//			formatSupport = true;
	//	}
	//}

	//assert(formatSupport);
}

void CreatePixelShader(ID3D11Device* device, std::shared_ptr<char[]> src, ComPtr<ID3D11PixelShader>& s)
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
	ThrowIfFailed(device->CreatePixelShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, s.GetAddressOf()));
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

void CreateVertexBuffer(ID3D11Device* device, void* vertex, void* index, UINT numVertex, UINT numIndex, ComPtr<ID3D11Buffer>& vb_, ComPtr<ID3D11Buffer>& ib_, ComPtr<ID3D11InputLayout>& inputLayout_, UINT &stride)
{
	const int normals = 0;
	const int indexes = numIndex;
	const int texCoords = 0;
	const int colors = 1;
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
	ThrowIfFailed((device->CreateInputLayout(reinterpret_cast<const D3D11_INPUT_ELEMENT_DESC*>(&layout[0]), (UINT)layout.size(), shaderBuffer->GetBufferPointer(), shaderBuffer->GetBufferSize(), &inputLayout_)));

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = bytes;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertex;

	ThrowIfFailed(device->CreateBuffer(&bd, &initData, &vb_));

	// index buffer
	if (indexes)
	{
		int idxSize = 2; // 16bit
		const int idxBytes = idxSize * numIndex;

		D3D11_BUFFER_DESC bufferDesc;
		bufferDesc.Usage = D3D11_USAGE_DEFAULT;
		bufferDesc.ByteWidth = idxBytes;
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bufferDesc.CPUAccessFlags = 0;
		bufferDesc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA ibData;
		ibData.pSysMem = index;
		ibData.SysMemPitch = 0;
		ibData.SysMemSlicePitch = 0;

		ThrowIfFailed(device->CreateBuffer(&bufferDesc, &ibData, &ib_));
	}

}

ComPtr<ID3D11Buffer> CreateConstanBuffer(ID3D11Device* device, UINT size)
{
	ComPtr<ID3D11Buffer> ret;

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = size;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ThrowIfFailed(device->CreateBuffer(&desc, nullptr, ret.GetAddressOf()));

	return ret;
}

ComPtr<ID3D11Query> CreateQuery(ID3D11Device* device, D3D11_QUERY type)
{
	ComPtr<ID3D11Query> ret;

	D3D11_QUERY_DESC desc{};
	desc.Query = type;
	ThrowIfFailed(device->CreateQuery(&desc, &ret));

	return ret;
}

void CreateVertexShader(ID3D11Device* device, std::shared_ptr<char[]> src, ComPtr<ID3D11VertexShader>& s)
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
	ThrowIfFailed(device->CreateVertexShader(shader->GetBufferPointer(), shader->GetBufferSize(), nullptr, s.GetAddressOf()));
}
