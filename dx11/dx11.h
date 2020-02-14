#pragma once
#include <d3d11_3.h>
#include <wrl.h>
#include <memory>

namespace dx11
{
	using namespace Microsoft::WRL;

	ID3D11Device* GetDx11Device();
	ID3D11DeviceContext* GetDx11Context();
	IDXGISwapChain* GetDx11Swapchain();
	ID3D11RenderTargetView* GetDx11RTV();
	ID3D11DepthStencilView* GetDx11DSV();
	UINT GetWidth();
	UINT GetHeight();

	void InitDX11(void* hwnd);
	void FreeDX11();

	void _Resize(UINT w, UINT h);

	void CreateVertexShader(std::shared_ptr<char[]> src, ComPtr<ID3D11VertexShader>& s);
	void CreatePixelShader(std::shared_ptr<char[]> src, ComPtr<ID3D11PixelShader>& s);
	void CreateVertexBuffer(void* vertex, void* index, UINT numVertex, UINT numIndex, ComPtr<ID3D11Buffer>& vb,
							ComPtr<ID3D11Buffer>& ib, ComPtr<ID3D11InputLayout> &inputLayout_, UINT &offset,
							bool cpu_usage = false, bool colors_ = false);
	auto CreateConstanBuffer(UINT size) -> ComPtr<ID3D11Buffer>;
	auto CreateStructuredBuffer(ComPtr<ID3D11ShaderResourceView>& SRVOut, ComPtr<ID3D11Buffer>& bufferOut, size_t structureSize, size_t num, const void* data) -> void;
	auto CreateQuery(D3D11_QUERY type) -> ComPtr<ID3D11Query>;
}
