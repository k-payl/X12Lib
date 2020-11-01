#pragma once
#include <dxcapi.h>
#include <d3d12.h>
#include "raytracing_d3dx12.h"
#include <map>

namespace math
{
	struct mat4;
	struct vec4;
}

namespace engine
{
	class Model;
	class Mesh;
}

void FreeUtils();

Microsoft::WRL::ComPtr<ID3D12Resource> BuildBLAS(engine::Model* model, ID3D12Device5* m_dxrDevice,
	ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList4* dxrCommandList, ID3D12CommandAllocator* commandAllocator);

Microsoft::WRL::ComPtr<ID3D12Resource> BuildTLAS(std::map<engine::Mesh*, Microsoft::WRL::ComPtr<ID3D12Resource>>& BLASes, ID3D12Device5* m_dxrDevice,
	ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList4* dxrCommandList, ID3D12CommandAllocator* commandAllocator);

IDxcBlob* CompileShader(LPCWSTR fileName);
void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc);
void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource,
	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON, const wchar_t* resourceName = nullptr);
void CreateRaygenLocalSignatureSubobject(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline,
	const wchar_t* raygenName, ID3D12RootSignature* raytracingLocalRootSignature);
UINT CreateDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap** descriptorHeap);
void TransformToDxr(FLOAT DXRTransform[3][4], const math::mat4& transform);
void SerializeAndCreateRaytracingRootSignature(ID3D12Device* device, D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig);

class GpuUploadBuffer
{
public:
	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() { return m_resource; }

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;

	GpuUploadBuffer() {}
	~GpuUploadBuffer()
	{
		if (m_resource.Get())
		{
			m_resource->Unmap(0, nullptr);
		}
	}

	void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
	{
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_resource));
		m_resource->SetName(resourceName);
	}

	uint8_t* MapCpuWriteOnly()
	{
		uint8_t* mappedData;
		// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
		return mappedData;
	}
};


// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
	ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
		shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
	{
	}

	ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
		shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
		localRootArguments(pLocalRootArguments, localRootArgumentsSize)
	{
	}

	void CopyTo(void* dest) const
	{
		uint8_t* byteDest = static_cast<uint8_t*>(dest);
		memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
		if (localRootArguments.ptr)
		{
			memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
		}
	}

	struct PointerWithSize {
		void* ptr;
		UINT size;

		PointerWithSize() : ptr(nullptr), size(0) {}
		PointerWithSize(void* _ptr, UINT _size) : ptr(_ptr), size(_size) {};
	};
	PointerWithSize shaderIdentifier;
	PointerWithSize localRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable : public GpuUploadBuffer
{
	uint8_t* m_mappedShaderRecords;
	UINT m_shaderRecordSize;

	// Debug support
	std::wstring m_name;
	std::vector<ShaderRecord> m_shaderRecords;

public:
	ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr);

	void push_back(const ShaderRecord& shaderRecord)
	{
		//assert(m_shaderRecords.size() < m_shaderRecords.capacity());
		m_shaderRecords.push_back(shaderRecord);
		shaderRecord.CopyTo(m_mappedShaderRecords);
		m_mappedShaderRecords += m_shaderRecordSize;
	}

	UINT GetShaderRecordSize() { return m_shaderRecordSize; }
};
