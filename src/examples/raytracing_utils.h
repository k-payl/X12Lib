#pragma once
#include <dxcapi.h>
#include <d3d12.h>

IDxcBlob* CompileShader(LPCWSTR fileName);
void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc);
void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON, const wchar_t* resourceName = nullptr);
