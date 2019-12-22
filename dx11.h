#pragma once
#include <d3d11_3.h>
#include <wrl.h>
#include <memory>

using namespace Microsoft::WRL;

void initDX11(void* hwnd_, ID3D11Device*& device, ID3D11DeviceContext*& context, IDXGISwapChain*& swapChain);

void CreateVertexShader(ID3D11Device* device, std::shared_ptr<char[]> src, ComPtr<ID3D11VertexShader>& s);
void CreatePixelShader(ID3D11Device* device, std::shared_ptr<char[]> src, ComPtr<ID3D11PixelShader>& s);

void CreateVertexBuffer(ID3D11Device* device, void* vertex, void* index, UINT numVertex, UINT numIndex, ComPtr<ID3D11Buffer>& vb, ComPtr<ID3D11Buffer>& ib, ComPtr<ID3D11InputLayout> &inputLayout_, UINT &offset);

ComPtr<ID3D11Buffer> CreateConstanBuffer(ID3D11Device* device, UINT size);

ComPtr<ID3D11Query> CreateQuery(ID3D11Device* device, D3D11_QUERY type);
