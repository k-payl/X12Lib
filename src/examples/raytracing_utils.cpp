#include "common.h"

#include "d3dx12.h"

#include "core.h"
#include "model.h"
#include "mesh.h"
#include "cpp_hlsl_shared.h"
#include "scenemanager.h"

#include "d3d12/dx12buffer.h"

#include "raytracing_utils.h"

#include <string>
#include <sstream>
#include <fstream>

void WaitForGpu();

static ID3D12Resource* scratchResource;
static size_t scratchSize;
static IDxcCompiler* pCompiler = nullptr;
static IDxcLibrary* pLibrary = nullptr;
static IDxcIncludeHandler* dxcIncludeHandler;


const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS acelStructFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

void FreeUtils()
{
#define SAFE_RELEASE(ptr) if (ptr) { ptr->Release(); ptr = nullptr; }

	SAFE_RELEASE(scratchResource)
	SAFE_RELEASE(pCompiler)
	SAFE_RELEASE(pLibrary)
	SAFE_RELEASE(dxcIncludeHandler)
}

IDxcBlob* CompileShader(LPCWSTR fileName)
{

	HRESULT hr;

	// Initialize the DXC compiler and compiler helper
	if (!pCompiler)
	{
		throwIfFailed(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&pCompiler));
		throwIfFailed(DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&pLibrary));
		throwIfFailed(pLibrary->CreateIncludeHandler(&dxcIncludeHandler));
	}
	// Open and read the file
	std::ifstream shaderFile(fileName);
	if (shaderFile.good() == false)
	{
		throw std::logic_error("Cannot find shader file");
	}
	std::stringstream strStream;
	strStream << shaderFile.rdbuf();
	std::string sShader = strStream.str();

	// Create blob from the string
	IDxcBlobEncoding* pTextBlob;
	throwIfFailed(pLibrary->CreateBlobWithEncodingFromPinned(
		(LPBYTE)sShader.c_str(), (uint32_t)sShader.size(), 0, &pTextBlob));

	// Compile
	IDxcOperationResult* pResult;
	throwIfFailed(pCompiler->Compile(pTextBlob, fileName, L"", L"lib_6_5", nullptr, 0, nullptr, 0,
		dxcIncludeHandler, &pResult));

	// Verify the result
	HRESULT resultCode;
	throwIfFailed(pResult->GetStatus(&resultCode));
	if (FAILED(resultCode))
	{
		IDxcBlobEncoding* pError;
		hr = pResult->GetErrorBuffer(&pError);
		if (FAILED(hr))
		{
			throw std::logic_error("Failed to get shader compiler error");
		}

		// Convert error blob to a string
		std::vector<char> infoLog(pError->GetBufferSize() + 1);
		memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
		infoLog[pError->GetBufferSize()] = 0;

		std::string errorMsg = "Shader Compiler Error:\n";
		errorMsg.append(infoLog.data());

		MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
		throw std::logic_error("Failed compile shader");
	}

	IDxcBlob* pBlob;
	throwIfFailed(pResult->GetResult(&pBlob));
	return pBlob;
}

void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc)
{
	std::wstringstream wstr;
	wstr << L"\n";
	wstr << L"--------------------------------------------------------------------\n";
	wstr << L"| D3D12 State Object 0x" << static_cast<const void*>(desc) << L": ";
	if (desc->Type == D3D12_STATE_OBJECT_TYPE_COLLECTION) wstr << L"Collection\n";
	if (desc->Type == D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE) wstr << L"Raytracing Pipeline\n";

	auto ExportTree = [](UINT depth, UINT numExports, const D3D12_EXPORT_DESC* exports)
	{
		std::wostringstream woss;
		for (UINT i = 0; i < numExports; i++)
		{
			woss << L"|";
			if (depth > 0)
			{
				for (UINT j = 0; j < 2 * depth - 1; j++) woss << L" ";
			}
			woss << L" [" << i << L"]: ";
			if (exports[i].ExportToRename) woss << exports[i].ExportToRename << L" --> ";
			woss << exports[i].Name << L"\n";
		}
		return woss.str();
	};

	for (UINT i = 0; i < desc->NumSubobjects; i++)
	{
		wstr << L"| [" << i << L"]: ";
		switch (desc->pSubobjects[i].Type)
		{
		case D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE:
			wstr << L"Global Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE:
			wstr << L"Local Root Signature 0x" << desc->pSubobjects[i].pDesc << L"\n";
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK:
			wstr << L"Node Mask: 0x"; //<< std::hex << std::setfill(L'0') << std::setw(8) << *static_cast<const UINT*>(desc->pSubobjects[i].pDesc) << std::setw(0) << std::dec << L"\n";
			break;
		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY:
		{
			wstr << L"DXIL Library 0x";
			auto lib = static_cast<const D3D12_DXIL_LIBRARY_DESC*>(desc->pSubobjects[i].pDesc);
			wstr << lib->DXILLibrary.pShaderBytecode << L", " << lib->DXILLibrary.BytecodeLength << L" bytes\n";
			wstr << ExportTree(1, lib->NumExports, lib->pExports);
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION:
		{
			wstr << L"Existing Library 0x";
			auto collection = static_cast<const D3D12_EXISTING_COLLECTION_DESC*>(desc->pSubobjects[i].pDesc);
			wstr << collection->pExistingCollection << L"\n";
			wstr << ExportTree(1, collection->NumExports, collection->pExports);
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
		{
			wstr << L"Subobject to Exports Association (Subobject [";
			auto association = static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
			UINT index = static_cast<UINT>(association->pSubobjectToAssociate - desc->pSubobjects);
			wstr << index << L"])\n";
			for (UINT j = 0; j < association->NumExports; j++)
			{
				wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
			}
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION:
		{
			wstr << L"DXIL Subobjects to Exports Association (";
			auto association = static_cast<const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(desc->pSubobjects[i].pDesc);
			wstr << association->SubobjectToAssociate << L")\n";
			for (UINT j = 0; j < association->NumExports; j++)
			{
				wstr << L"|  [" << j << L"]: " << association->pExports[j] << L"\n";
			}
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG:
		{
			wstr << L"Raytracing Shader Config\n";
			auto config = static_cast<const D3D12_RAYTRACING_SHADER_CONFIG*>(desc->pSubobjects[i].pDesc);
			wstr << L"|  [0]: Max Payload Size: " << config->MaxPayloadSizeInBytes << L" bytes\n";
			wstr << L"|  [1]: Max Attribute Size: " << config->MaxAttributeSizeInBytes << L" bytes\n";
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG:
		{
			wstr << L"Raytracing Pipeline Config\n";
			auto config = static_cast<const D3D12_RAYTRACING_PIPELINE_CONFIG*>(desc->pSubobjects[i].pDesc);
			wstr << L"|  [0]: Max Recursion Depth: " << config->MaxTraceRecursionDepth << L"\n";
			break;
		}
		case D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP:
		{
			wstr << L"Hit Group (";
			auto hitGroup = static_cast<const D3D12_HIT_GROUP_DESC*>(desc->pSubobjects[i].pDesc);
			wstr << (hitGroup->HitGroupExport ? hitGroup->HitGroupExport : L"[none]") << L")\n";
			wstr << L"|  [0]: Any Hit Import: " << (hitGroup->AnyHitShaderImport ? hitGroup->AnyHitShaderImport : L"[none]") << L"\n";
			wstr << L"|  [1]: Closest Hit Import: " << (hitGroup->ClosestHitShaderImport ? hitGroup->ClosestHitShaderImport : L"[none]") << L"\n";
			wstr << L"|  [2]: Intersection Import: " << (hitGroup->IntersectionShaderImport ? hitGroup->IntersectionShaderImport : L"[none]") << L"\n";
			break;
		}
		}
		wstr << L"|--------------------------------------------------------------------\n";
	}
	wstr << L"\n";
	OutputDebugStringW(wstr.str().c_str());
}

void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState, const wchar_t* resourceName)
{
	auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	throwIfFailed(pDevice->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		initialResourceState,
		nullptr,
		IID_PPV_ARGS(ppResource)));
	if (resourceName)
	{
		(*ppResource)->SetName(resourceName);
	}
}

void CreateRaygenLocalSignatureSubobject(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline, const wchar_t* raygenName, ID3D12RootSignature* raytracingLocalRootSignature)
{
	// Hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with them.
	// Local root signature to be used in a ray gen shader.
	auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSignature->SetRootSignature(raytracingLocalRootSignature);

	// Shader association
	auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
	rootSignatureAssociation->AddExport(raygenName);
}

UINT CreateDescriptorHeap(ID3D12Device* device, ID3D12DescriptorHeap** descriptorHeap)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.NumDescriptors = 200; // 1 raytracing output
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(descriptorHeap));
	descriptorHeap[0]->SetName(L"Descriptor heap for raytracing (gpu visible)");

	return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void TransformToDxr(FLOAT DXRTransform[3][4], const math::mat4& transform)
{
	DXRTransform[0][0] = DXRTransform[1][1] = DXRTransform[2][2] = 1;
	memcpy(DXRTransform[0], transform.el_2D[0], sizeof(math::vec4));
	memcpy(DXRTransform[1], transform.el_2D[1], sizeof(math::vec4));
	memcpy(DXRTransform[2], transform.el_2D[2], sizeof(math::vec4));
}

ShaderTable::ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName)
	: m_name(resourceName)
{
	m_shaderRecordSize = x12::Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
	m_shaderRecords.reserve(numShaderRecords);
	UINT bufferSize = numShaderRecords * m_shaderRecordSize;
	Allocate(device, bufferSize, resourceName);
	m_mappedShaderRecords = MapCpuWriteOnly();
}

void SerializeAndCreateRaytracingRootSignature(ID3D12Device* device, D3D12_ROOT_SIGNATURE_DESC& desc, Microsoft::WRL::ComPtr<ID3D12RootSignature>* rootSig)
{
	Microsoft::WRL::ComPtr<ID3DBlob> blob;
	Microsoft::WRL::ComPtr<ID3DBlob> error;

	throwIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
	throwIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

void ExpandScratchBuffer(ID3D12Device5* m_dxrDevice, size_t size)
{
	if (size > scratchSize)
	{
		scratchSize = size;
		AllocateUAVBuffer(m_dxrDevice, scratchSize, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	}
}

ComPtr<ID3D12Resource> BuildBLAS(engine::Model* model, ID3D12Device5* m_dxrDevice,
	ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList4* dxrCommandList, ID3D12CommandAllocator* commandAllocator)
{
	//std::vector<engine::Model*> models;
	//engine::GetSceneManager()->getObjectsOfType(models, engine::OBJECT_TYPE::MODEL);

	//const UINT topLevelInstances = models.size();

	//SceneData scene;
	//scene.instancesNum = topLevelInstances;

	//engine::GetCoreRenderer()->CreateConstantBuffer(res->sceneData.getAdressOf(), L"Scene data", sizeof(SceneData), false);
	//res->sceneData->SetData(&scene, sizeof(SceneData));

	//auto commandList = res->commandList.Get();
	//auto commandQueue = res->commandQueue.Get();
	//auto commandAllocator = res->commandAllocators[0].Get();

	//// Get required sizes for an acceleration structure.
	//D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	//topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	//topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	//topLevelInputs.NumDescs = topLevelInstances;
	//topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	//D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	//m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	//assert(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	////ComPtr<ID3D12Resource> scratchResource;
	//size_t scratchSize = topLevelPrebuildInfo.ScratchDataSizeInBytes;
	//AllocateUAVBuffer(device, scratchSize, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	//for (size_t i = 0; i < models.size(); ++i)
	{
		int id = model->GetId();

		engine::Mesh* m = model->GetMesh();

		//if (res->bottomLevelAccelerationStructures[m])
		//	continue;

		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = reinterpret_cast<x12::Dx12CoreBuffer*>(m->VertexBuffer())->GPUAddress();

		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = 0;// reinterpret_cast<x12::Dx12CoreBuffer*>(res->m_indexBuffer.get())->GPUAddress();
		geometryDesc.Triangles.IndexCount = 0;
		geometryDesc.Triangles.IndexFormat; // DXGI_FORMAT_R16_UINT;
		geometryDesc.Triangles.Transform3x4 = 0;
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDesc.Triangles.VertexCount = m->GetVertexCount();
		geometryDesc.Triangles.VertexBuffer.StartAddress = gpuAddress;
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(engine::Shaders::Vertex);

		// Mark the geometry as opaque. 
		// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
		// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
		geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
		bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInputs.Flags = acelStructFlags;
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.pGeometryDescs = &geometryDesc;
		bottomLevelInputs.NumDescs = 1;
		m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
		assert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		ExpandScratchBuffer(m_dxrDevice, bottomLevelPrebuildInfo.ScratchDataSizeInBytes);

		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		ComPtr<ID3D12Resource> bottomLevelAccelerationStructure;
		AllocateUAVBuffer(m_dxrDevice, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");

		// Bottom Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		bottomLevelBuildDesc.Inputs = bottomLevelInputs;
		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		bottomLevelBuildDesc.DestAccelerationStructureData = bottomLevelAccelerationStructure->GetGPUVirtualAddress();

		// Reset the command list for the acceleration structure construction.
		throwIfFailed(dxrCommandList->Reset(commandAllocator, nullptr));
		dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		dxrCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(bottomLevelAccelerationStructure.Get()));
		throwIfFailed(dxrCommandList->Close());

		ID3D12CommandList* commandLists[] = { dxrCommandList };
		commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

		WaitForGpu();

		return bottomLevelAccelerationStructure;
	}
}


ComPtr<ID3D12Resource> BuildTLAS(std::map<engine::Mesh*, ComPtr<ID3D12Resource>>& BLASes, ID3D12Device5* m_dxrDevice,
	ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList4* dxrCommandList, ID3D12CommandAllocator* commandAllocator)
{
	ComPtr<ID3D12Resource> TLAS;

	std::vector<engine::Model*> models;
	engine::GetSceneManager()->getObjectsOfType(models, engine::OBJECT_TYPE::MODEL);

	const UINT topLevelInstances = models.size();

	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = acelStructFlags;
	topLevelInputs.NumDescs = topLevelInstances;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	assert(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	ExpandScratchBuffer(m_dxrDevice, topLevelPrebuildInfo.ScratchDataSizeInBytes);

	// Create an instance desc for the bottom-level acceleration structure.
	intrusive_ptr<x12::ICoreBuffer> instanceDescsBuffer;
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDesc(topLevelInstances);

	for (int i = 0; i < topLevelInstances; ++i)
	{
		TransformToDxr(instanceDesc[i].Transform, models[i]->GetWorldTransform());
		instanceDesc[i].InstanceMask = 1;
		instanceDesc[i].InstanceID = 0;
		instanceDesc[i].InstanceContributionToHitGroupIndex = i;
		instanceDesc[i].AccelerationStructure = BLASes[models[i]->GetMesh()]->GetGPUVirtualAddress();
	}

	engine::GetCoreRenderer()->CreateStructuredBuffer(instanceDescsBuffer.getAdressOf(), L"instance desc for the top-level acceleration structure", sizeof(D3D12_RAYTRACING_INSTANCE_DESC), topLevelInstances, &instanceDesc[0], x12::BUFFER_FLAGS::NONE);

	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	AllocateUAVBuffer(m_dxrDevice, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &TLAS, initialResourceState, L"TopLevelAccelerationStructure");

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	topLevelInputs.InstanceDescs = reinterpret_cast<x12::Dx12CoreBuffer*>(instanceDescsBuffer.get())->GPUAddress();
	topLevelBuildDesc.Inputs = topLevelInputs;
	topLevelBuildDesc.DestAccelerationStructureData = TLAS->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();

	throwIfFailed(dxrCommandList->Reset(commandAllocator, nullptr));
	dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	throwIfFailed(dxrCommandList->Close());

	ID3D12CommandList* commandLists[] = { dxrCommandList };
	commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

	WaitForGpu();

	return TLAS;
}
