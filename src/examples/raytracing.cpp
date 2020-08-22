#include "d3dx12.h"

#include "raytracing_utils.h"

#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "icorerender.h"
#include "scenemanager.h"
#include "resourcemanager.h"
#include "mesh.h"
#include "d3d12/dx12buffer.h"


using namespace x12;

#define VIDEO_API engine::INIT_FLAGS::DIRECTX12_RENDERER
#define RAYTRACING_SHADER_DIR L"../resources/shaders/raytracing/"
#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

typedef UINT16 Index;
struct Vertex { float v1, v2, v3; float n1, n2, n3; };

const wchar_t* hitGroupName = L"HitGroup";
const wchar_t* raygenname = L"RayGen";
const wchar_t* hitname = L"ClosestHit";
const wchar_t* missname = L"Miss";


struct CameraData
{
	float forward[4];
	float right[4];
	float up[4];	
	float origin[4];	
};

static struct Resources
{	
	ComPtr<ID3D12CommandQueue>          commandQueue;
	ComPtr<ID3D12GraphicsCommandList>   commandList;
	ComPtr<ID3D12CommandAllocator>      commandAllocators[engine::DeferredBuffers];
	ComPtr<ID3D12Fence>                 fence;

	// DXR
	ComPtr<ID3D12Device5> dxrDevice;
	ComPtr<ID3D12GraphicsCommandList4> dxrCommandList;

	// DXR resources
	ComPtr<ID3D12StateObject> dxrStateObject;

	ComPtr<ID3D12Resource> missShaderTable;
	ComPtr<ID3D12Resource> hitGroupShaderTable;
	ComPtr<ID3D12Resource> rayGenShaderTable;

	ComPtr<ID3D12RootSignature> raytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> raytracingLocalRootSignature;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	ComPtr<ID3D12Resource> raytracingOutput;	

	intrusive_ptr<x12::ICoreBuffer> indexBuffer;	

	intrusive_ptr<x12::ICoreBuffer> cameraBuffers[engine::DeferredBuffers];

	ComPtr<IDxcBlob> raygen;
	ComPtr<IDxcBlob> hit;
	ComPtr<IDxcBlob> miss;

	ComPtr<ID3D12Resource> accelerationStructure;
	ComPtr<ID3D12Resource> bottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource> topLevelAccelerationStructure;

	engine::StreamPtr<engine::Mesh> mesh;
} *res;

engine::Core* core_;

HWND hwnd;
UINT width, height;

engine::Camera* cam;
CameraData cameraData;

ID3D12Device* device;
ID3D12Device5* m_dxrDevice;
ID3D12GraphicsCommandList4* dxrCommandList;
ID3D12StateObject* dxrStateObject;
UINT descriptorSize;
UINT descriptorsAllocated;
UINT64 fenceValues[engine::DeferredBuffers];
UINT backBufferIndex;
HANDLE event;
D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputResourceUAVGpuDescriptor;
D3D12_GPU_DESCRIPTOR_HANDLE buffersGpuDescriptor;

namespace GlobalRootSignatureParams {
	enum Value {
		CameraConstantBuffer = 0,
		VertexIndexBuffers,
		OutputView,
		AccelerationStructure,
		Count
	};
}

void CreateDescriptorHeap();
void BuildGeometry();
void BuildAccelerationStructures();
void BuildShaderTables();
void CreateRaytracingOutputResource(UINT width, UINT height);
void WaitForGpu();
UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor);

void Render()
{
	WaitForGpu();

	throwIfFailed(res->commandAllocators[backBufferIndex]->Reset());
	throwIfFailed(dxrCommandList->Reset(res->commandAllocators[backBufferIndex].Get(), nullptr));

	// raytracing
	{
		dxrCommandList->SetComputeRootSignature(res->raytracingGlobalRootSignature.Get());

		// Bind the heaps, acceleration structure and dispatch rays.    
		dxrCommandList->SetDescriptorHeaps(1, res->descriptorHeap.GetAddressOf());

		dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputView, raytracingOutputResourceUAVGpuDescriptor);
		dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::VertexIndexBuffers, buffersGpuDescriptor);
		dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructure, res->topLevelAccelerationStructure->GetGPUVirtualAddress());

		// camera
		{
			using namespace math;

			float verFullFovInRadians = cam->GetFullVertFOVInRadians();
			float aspect = float(width) / height;

			mat4 ViewInvMat_ = cam->GetWorldTransform();

			vec4 origin = cam->GetWorldPosition();
			vec3 forwardWS = -ViewInvMat_.Column3(2).Normalized();
			vec3 rightWS = ViewInvMat_.Column3(0).Normalized() * tan(verFullFovInRadians * 0.5f) * aspect;
			vec3 upWS = ViewInvMat_.Column3(1).Normalized() * tan(verFullFovInRadians * 0.5f);

			memcpy(cameraData.forward, &forwardWS, sizeof(vec3));
			memcpy(cameraData.right, &rightWS, sizeof(vec3));
			memcpy(cameraData.up, &upWS, sizeof(vec3));
			memcpy(cameraData.origin, &origin, sizeof(vec4));

			res->cameraBuffers[backBufferIndex]->SetData(&cameraData, sizeof(cameraData));
			x12::Dx12CoreBuffer* dx12buffer = reinterpret_cast<x12::Dx12CoreBuffer*>(res->cameraBuffers[backBufferIndex].get());

			dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::CameraConstantBuffer, dx12buffer->GPUAddress());
		}

		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		dispatchDesc.HitGroupTable.StartAddress = res->hitGroupShaderTable->GetGPUVirtualAddress();
		dispatchDesc.HitGroupTable.SizeInBytes = res->hitGroupShaderTable->GetDesc().Width;
		dispatchDesc.HitGroupTable.StrideInBytes = 64;
		dispatchDesc.MissShaderTable.StartAddress = res->missShaderTable->GetGPUVirtualAddress();
		dispatchDesc.MissShaderTable.SizeInBytes = res->missShaderTable->GetDesc().Width;
		dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
		dispatchDesc.RayGenerationShaderRecord.StartAddress = res->rayGenShaderTable->GetGPUVirtualAddress();
		dispatchDesc.RayGenerationShaderRecord.SizeInBytes = res->rayGenShaderTable->GetDesc().Width;
		dispatchDesc.Width = width;
		dispatchDesc.Height = height;
		dispatchDesc.Depth = 1;
		dxrCommandList->SetPipelineState1(res->dxrStateObject.Get());

		dxrCommandList->DispatchRays(&dispatchDesc);
	}
	throwIfFailed(dxrCommandList->Close());
	ID3D12CommandList* commandLists[] = { dxrCommandList };
	res->commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

	ICoreRenderer* renderer = engine::GetCoreRenderer();
	surface_ptr surface = renderer->GetWindowSurface(hwnd);
	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	cmdList->BindSurface(surface);

	{
		ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();;

		ID3D12Resource* backBuffer = (ID3D12Resource*)surface->GetNativeResource(backBufferIndex);

		D3D12_RESOURCE_BARRIER preCopyBarriers[2];
		preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
		preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(res->raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
		d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

		d3dCmdList->CopyResource(backBuffer, res->raytracingOutput.Get());

		D3D12_RESOURCE_BARRIER postCopyBarriers[2];
		postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
		postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(res->raytracingOutput.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		d3dCmdList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
	}

	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);

	backBufferIndex = (backBufferIndex + 1) % engine::DeferredBuffers;
}

void WaitForGpu()
{
	// Schedule a Signal command in the GPU queue.
	UINT64 fenceValue = fenceValues[backBufferIndex];
	if (SUCCEEDED(res->commandQueue->Signal(res->fence.Get(), fenceValue)))
	{
		// Wait until the Signal has been processed.
		if (SUCCEEDED(res->fence->SetEventOnCompletion(fenceValue, event)))
		{
			WaitForSingleObjectEx(event, INFINITE, FALSE);

			// Increment the fence value for the current frame.
			fenceValues[backBufferIndex]++;
		}
	}
}

void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig)
{
	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;

	throwIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
	throwIfFailed(device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(*rootSig))));
}

void CreateRaygenLocalSignatureSubobject(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline, const wchar_t* raygenName)
{
	// Hit group and miss shaders in this sample are not using a local root signature and thus one is not associated with them.
	// Local root signature to be used in a ray gen shader.
	auto localRootSignature = raytracingPipeline->CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	localRootSignature->SetRootSignature(res->raytracingLocalRootSignature.Get());

	// Shader association
	auto rootSignatureAssociation = raytracingPipeline->CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
	rootSignatureAssociation->AddExport(raygenName);
}

void Init()
{
	device = (ID3D12Device*)engine::GetCoreRenderer()->GetNativeDevice();

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&res->commandQueue)));

	// Create a command allocator for each back buffer that will be rendered to.
	for (UINT n = 0; n < engine::DeferredBuffers; n++)
	{
		throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&res->commandAllocators[n])));
	}

	// Create a command list for recording graphics commands.
	throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, res->commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&res->commandList)));
	throwIfFailed(res->commandList->Close());

	// Create a fence for tracking GPU execution progress.
	throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&res->fence)));
	fenceValues[0]++;

	event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// DXR interfaces
	throwIfFailed(device->QueryInterface(IID_PPV_ARGS(&res->dxrDevice)));
	m_dxrDevice = res->dxrDevice.Get();

	throwIfFailed(res->commandList->QueryInterface(IID_PPV_ARGS(&res->dxrCommandList)));
	dxrCommandList = res->dxrCommandList.Get();

	// Global Root Signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];

		CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
		UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		rootParameters[GlobalRootSignatureParams::OutputView].InitAsDescriptorTable(1, &UAVDescriptor);

		rootParameters[GlobalRootSignatureParams::CameraConstantBuffer].InitAsConstantBufferView(0); // camera constants

		rootParameters[GlobalRootSignatureParams::AccelerationStructure].InitAsShaderResourceView(0); // acceleration structure		

		CD3DX12_DESCRIPTOR_RANGE geomRanges[1];
		geomRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.
		rootParameters[GlobalRootSignatureParams::VertexIndexBuffers].InitAsDescriptorTable(1, &geomRanges[0]);

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr);

		SerializeAndCreateRaytracingRootSignature(desc, &res->raytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[1];

		//CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
		//UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
		//rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptor);

		//rootParameters[1].InitAsShaderResourceView(0); // acceleration structure

		////rootParameters[2].InitAsConstants(SizeOfInUint32(m_rayGenCB), 0, 0);
		rootParameters[0].InitAsConstants(4, 1, 0); // camera constants

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
		
		//CD3DX12_ROOT_SIGNATURE_DESC desc(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		SerializeAndCreateRaytracingRootSignature(desc, &res->raytracingLocalRootSignature);
	}

	{
		// Create 7 subobjects that combine into a RTPSO
		CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		res->raygen = CompileShader(RAYTRACING_SHADER_DIR "RayGen.hlsl");
		res->hit = CompileShader(RAYTRACING_SHADER_DIR "Hit.hlsl");
		res->miss = CompileShader(RAYTRACING_SHADER_DIR "Miss.hlsl");

		// DXIL library
		// This contains the shaders and their entrypoints for the state object.
		// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
		
		auto raygen_lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE raygen_shaderBytecode;
		raygen_shaderBytecode.BytecodeLength = res->raygen->GetBufferSize();
		raygen_shaderBytecode.pShaderBytecode = res->raygen->GetBufferPointer();
		raygen_lib->SetDXILLibrary(&raygen_shaderBytecode);
		raygen_lib->DefineExport(raygenname);
		
		auto hit_lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE hit_shaderBytecode;
		hit_shaderBytecode.BytecodeLength = res->hit->GetBufferSize();
		hit_shaderBytecode.pShaderBytecode = res->hit->GetBufferPointer();
		hit_lib->SetDXILLibrary(&hit_shaderBytecode);
		hit_lib->DefineExport(hitname);		
		
		auto miss_lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
		D3D12_SHADER_BYTECODE miss_shaderBytecode;
		miss_shaderBytecode.BytecodeLength = res->miss->GetBufferSize();
		miss_shaderBytecode.pShaderBytecode = res->miss->GetBufferPointer();
		miss_lib->SetDXILLibrary(&miss_shaderBytecode);
		miss_lib->DefineExport(missname);

		// Triangle hit group
		// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
		// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
		auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
		hitGroup->SetClosestHitShaderImport(hitname);
		hitGroup->SetHitGroupExport(hitGroupName);
		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

		// Shader config
		// Defines the maximum sizes in bytes for the ray payload and attribute structure.
		auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		UINT payloadSize = 4 * sizeof(float);   // float4 color
		UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
		shaderConfig->Config(payloadSize, attributeSize);

		// Local root signature and shader association
		CreateRaygenLocalSignatureSubobject(&raytracingPipeline, hitGroupName);
		// This is a root signature that enables a shader to have unique arguments that come from shader tables.

		// Global root signature
		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
		auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
		globalRootSignature->SetRootSignature(res->raytracingGlobalRootSignature.Get());

		// Pipeline config
		// Defines the maximum TraceRay() recursion depth.
		auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
		// PERFOMANCE TIP: Set max recursion depth as low as needed 
		// as drivers may apply optimization strategies for low recursion depths. 
		UINT maxRecursionDepth = 1; // ~ primary rays only. 
		pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
		PrintStateObjectDesc(raytracingPipeline);
#endif

		// Create the state object.
		throwIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&res->dxrStateObject)));
		dxrStateObject = res->dxrStateObject.Get();
	}

	CreateDescriptorHeap();

	BuildGeometry();

	BuildAccelerationStructures();

	BuildShaderTables();

	engine::MainWindow* window = core_->GetWindow();

	int w, h;
	window->GetClientSize(w, h);
	width = w;
	height = h;
	CreateRaytracingOutputResource(w, h);

	for (int i = 0; i < engine::DeferredBuffers; i++)
		engine::GetCoreRenderer()->CreateConstantBuffer(res->cameraBuffers[i].getAdressOf(), L"camera constant buffer", sizeof(cameraData), false);
}

void CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};	
	descriptorHeapDesc.NumDescriptors = 3; // 3 = 1 raytracing output texture UAV + 1 vertex + 1 index buffer
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&res->descriptorHeap));
	res->descriptorHeap->SetName(L"Descriptor heap for UAV");

	descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void BuildGeometry()
{
	// TODO: support with and without index buffer
	Index indices[] =
	{
		0, 1, 2,
		0, 3, 1
	};
	engine::GetCoreRenderer()->CreateStructuredBuffer(res->indexBuffer.getAdressOf(), L"index buffer", sizeof(Index), 6, indices, BUFFER_FLAGS::SHADER_RESOURCE);

#if 0
	float offset = 1;
	Vertex vertices[] =
	{
		{ -offset, offset, 0, 0, 0, 1 },
		{  offset,-offset, 0, 0, 1, 0 },
		{ -offset,-offset, 0, 0.5, 0.5, 0 },
		{  offset, offset, 0, 1, 0, 0 },
	};
	engine::GetCoreRenderer()->CreateStructuredBuffer(res->m_vertexBuffer.getAdressOf(), L"vertex buffer", sizeof(Vertex), 4, vertices, BUFFER_FLAGS::SHADER_RESOURCE);
#else
	res->mesh = engine::GetResourceManager()->CreateStreamMesh("meshes\\Teapot.002.mesh");
	engine::Mesh *m = res->mesh.get();
	x12::ICoreBuffer* vb = m->VertexBuffer();
#endif

	// setup descriptors

	buffersGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, descriptorSize);

	// dummy allocation for index buffer
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle;
	AllocateDescriptor(&cpuDescriptorHandle);
	device->CopyDescriptorsSimple(1, cpuDescriptorHandle, reinterpret_cast<x12::Dx12CoreBuffer*>(res->indexBuffer.get())->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// vertex buffer
	AllocateDescriptor(&cpuDescriptorHandle);
	device->CopyDescriptorsSimple(1, cpuDescriptorHandle, reinterpret_cast<x12::Dx12CoreBuffer*>(vb)->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void TransformToDxr(FLOAT DXRTransform[3][4], const math::mat4& transform)
{
	DXRTransform[0][0] = DXRTransform[1][1] = DXRTransform[2][2] = 1;
	memcpy(DXRTransform[0], transform.el_2D[0], sizeof(math::vec4));
	memcpy(DXRTransform[1], transform.el_2D[1], sizeof(math::vec4));
	memcpy(DXRTransform[2], transform.el_2D[2], sizeof(math::vec4));
}

void BuildAccelerationStructures()
{
	const UINT topLevelInstances = 2;

	auto commandList = res->commandList.Get();
	auto commandQueue = res->commandQueue.Get();
	auto commandAllocator = res->commandAllocators[0].Get();

	// Reset the command list for the acceleration structure construction.
	commandList->Reset(commandAllocator, nullptr);

	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	geometryDesc.Triangles.IndexBuffer = 0;// reinterpret_cast<x12::Dx12CoreBuffer*>(res->m_indexBuffer.get())->GPUAddress();
	geometryDesc.Triangles.IndexCount = 0;
	geometryDesc.Triangles.IndexFormat;// DXGI_FORMAT_R16_UINT;
	geometryDesc.Triangles.Transform3x4 = 0;
	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geometryDesc.Triangles.VertexCount = res->mesh.get()->GetvertexCount();
	geometryDesc.Triangles.VertexBuffer.StartAddress = reinterpret_cast<x12::Dx12CoreBuffer*>(res->mesh.get()->VertexBuffer())->GPUAddress();
	geometryDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);

	// Mark the geometry as opaque. 
	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = topLevelInstances;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	assert(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.pGeometryDescs = &geometryDesc;
	bottomLevelInputs.NumDescs = 1;
	m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	assert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	ComPtr<ID3D12Resource> scratchResource;
	AllocateUAVBuffer(device, std::max(topLevelPrebuildInfo.ScratchDataSizeInBytes, bottomLevelPrebuildInfo.ScratchDataSizeInBytes), &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	// Allocate resources for acceleration structures.
	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	{
		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &res->bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");
		AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &res->topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
	}

	math::mat4 transforms[topLevelInstances];
	math::compositeTransform(transforms[0], math::vec3(0, 0, 0), math::quat(), math::vec3(1, 1, 1));
	math::compositeTransform(transforms[1], math::vec3(6, 0, 0), math::quat(), math::vec3(1, 1, 1));

	// Create an instance desc for the bottom-level acceleration structure.
	intrusive_ptr<x12::ICoreBuffer> instanceDescsBuffer;
	D3D12_RAYTRACING_INSTANCE_DESC instanceDesc[topLevelInstances] = {};

	for (int i = 0; i < topLevelInstances; ++i)
	{
		TransformToDxr(instanceDesc[i].Transform, transforms[i]);
		instanceDesc[i].InstanceMask = 1;
		instanceDesc[i].InstanceID = 0;
		instanceDesc[i].InstanceContributionToHitGroupIndex = i;
		instanceDesc[i].AccelerationStructure = res->bottomLevelAccelerationStructure->GetGPUVirtualAddress();
	}
	engine::GetCoreRenderer()->CreateStructuredBuffer(instanceDescsBuffer.getAdressOf(), L"instance desc for the bottom-level acceleration structure", sizeof(instanceDesc) * topLevelInstances, topLevelInstances, &instanceDesc, BUFFER_FLAGS::NONE);

	// Bottom Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	bottomLevelBuildDesc.Inputs = bottomLevelInputs;
	bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	bottomLevelBuildDesc.DestAccelerationStructureData = res->bottomLevelAccelerationStructure->GetGPUVirtualAddress();

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	topLevelInputs.InstanceDescs = reinterpret_cast<x12::Dx12CoreBuffer*>(instanceDescsBuffer.get())->GPUAddress();
	topLevelBuildDesc.Inputs = topLevelInputs;
	topLevelBuildDesc.DestAccelerationStructureData = res->topLevelAccelerationStructure->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();

	dxrCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
	dxrCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(res->bottomLevelAccelerationStructure.Get()));
	dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);

	// Kick off acceleration structure construction.
	throwIfFailed(dxrCommandList->Close());
	ID3D12CommandList* commandLists[] = { dxrCommandList };
	commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

	// Wait for GPU to finish as the locally created temporary GPU resources will get released once we go out of scope.
	WaitForGpu();
}

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

class GpuUploadBuffer
{
public:
	ComPtr<ID3D12Resource> GetResource() { return m_resource; }

protected:
	ComPtr<ID3D12Resource> m_resource;

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
		throwIfFailed(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(resourceName);
	}

	uint8_t* MapCpuWriteOnly()
	{
		uint8_t* mappedData;
		// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		throwIfFailed(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
		return mappedData;
	}
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
	ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr)
		: m_name(resourceName)
	{
		m_shaderRecordSize = x12::Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		m_shaderRecords.reserve(numShaderRecords);
		UINT bufferSize = numShaderRecords * m_shaderRecordSize;
		Allocate(device, bufferSize, resourceName);
		m_mappedShaderRecords = MapCpuWriteOnly();
	}

	void push_back(const ShaderRecord& shaderRecord)
	{
		assert(m_shaderRecords.size() < m_shaderRecords.capacity());
		m_shaderRecords.push_back(shaderRecord);
		shaderRecord.CopyTo(m_mappedShaderRecords);
		m_mappedShaderRecords += m_shaderRecordSize;
	}

	UINT GetShaderRecordSize() { return m_shaderRecordSize; }
};

void BuildShaderTables()
{
	void* rayGenShaderIdentifier;
	void* missShaderIdentifier;
	void* hitGroupShaderIdentifier;
	UINT shaderIdentifierSize;

	// Get shader identifiers.
	{
		ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
		throwIfFailed(res->dxrStateObject.As(&stateObjectProperties));

		rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(raygenname);
		missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(missname);
		hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(hitGroupName);

		shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	}

	// Ray gen shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;// +sizeof(rootArguments);
		ShaderTable rayGenShaderTable(device, numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize/*, &rootArguments, sizeof(rootArguments)*/));
		res->rayGenShaderTable = rayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;
		UINT shaderRecordSize = shaderIdentifierSize;
		ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
		res->missShaderTable = missShaderTable.GetResource();
	}

	// Hit group shader table
	{
		UINT numShaderRecords = 2;
		UINT shaderRecordSize = shaderIdentifierSize + 16;
		ShaderTable hitGroupShaderTable(device, numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");

		static math::vec4 colors[2] = { math::vec4(1,0,0,1), math::vec4(0,1,0,1) };

		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &colors[0], 16));
		hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize, &colors[1], 16));
		res->hitGroupShaderTable = hitGroupShaderTable.GetResource();
	}
}

UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor)
{
	auto descriptorHeapCpuBase = res->descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	UINT descriptorIndexToUse = descriptorsAllocated++;
	*cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeapCpuBase, descriptorIndexToUse, descriptorSize);
	return descriptorIndexToUse;
}

void CreateRaytracingOutputResource(UINT width, UINT height)
{
	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	throwIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&res->raytracingOutput)));

	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle;
	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex = AllocateDescriptor(&uavDescriptorHandle);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(res->raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);
}

// ----------------------------
// Main
// ----------------------------

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	core_ = engine::CreateCore();

	core_->AddRenderProcedure(Render);
	core_->AddInitProcedure(Init);

	res = new Resources();
	core_->Init(VIDEO_API);
	cam = engine::GetSceneManager()->CreateCamera();

	engine::MainWindow* window = core_->GetWindow();
	hwnd = *window->handle();

	core_->Start();

	::CloseHandle(event);
	delete res;

	core_->Free();
	engine::DestroyCore(core_);

	return 0;
}
