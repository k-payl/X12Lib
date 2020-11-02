#include "d3dx12.h"

#include "raytracing_utils.h"
#include "raytracing_d3dx12.h"

#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "icorerender.h"
#include "model.h"
#include "gameobject.h"
#include "scenemanager.h"
#include "resourcemanager.h"
#include "mesh.h"
#include "d3d12/dx12buffer.h"
#include "cpp_hlsl_shared.h"


using namespace x12;

#define VIDEO_API engine::INIT_FLAGS::DIRECTX12_RENDERER
#define RAYTRACING_SHADER_DIR L"../resources/shaders/raytracing/"
#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

typedef UINT16 Index;

const wchar_t* raygenname = L"RayGen";

const wchar_t* hitGroupName = L"HitGroup";
const wchar_t* hitname = L"ClosestHit";
const wchar_t* missname = L"Miss";

const wchar_t* shadowHitGroupName = L"ShadowHitGroup";
const wchar_t* shadowhitname = L"ShadowClosestHit";
const wchar_t* shadowmissname = L"ShadowMiss";


struct HitArg
{
	math::vec4 color = { 0.5, 0.5, 0.5, 1 };
	math::mat4 transform;
	D3D12_GPU_VIRTUAL_ADDRESS vertexBuffer;
};

UINT hitRecordSize()
{
	return x12::Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(HitArg), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
}

struct Fence
{
	ComPtr<ID3D12Fence> fence[engine::DeferredBuffers];
	UINT64 fenceValues[engine::DeferredBuffers];
};

static struct Resources
{
	ComPtr<ID3D12CommandQueue>          commandQueue;
	ComPtr<ID3D12GraphicsCommandList>   commandList;
	ComPtr<ID3D12CommandAllocator>      commandAllocators[engine::DeferredBuffers];

	Fence rtToSwapchainFence;
	Fence clearToRTFence;
	Fence frameEndFence;

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

	intrusive_ptr<x12::ICoreBuffer> indexBuffer;

	intrusive_ptr<x12::ICoreBuffer> cameraBuffers;

	ComPtr<IDxcBlob> raygen;
	ComPtr<IDxcBlob> hit;
	ComPtr<IDxcBlob> miss;

	ComPtr<ID3D12Resource> accelerationStructure;
	std::map<engine::Mesh*, ComPtr<ID3D12Resource>> bottomLevelAccelerationStructures;
	ComPtr<ID3D12Resource> topLevelAccelerationStructure;

	intrusive_ptr<x12::ICoreBuffer> sceneData;

	engine::StreamPtr<engine::Shader> copyShader;
	intrusive_ptr<IResourceSet> copyResources;
	intrusive_ptr<ICoreVertexBuffer> plane;
	ComPtr<ID3D12Resource> raytracingOutput;
	intrusive_ptr<ICoreTexture> raytracingOutputCore;
	//ComPtr<ID3D12Fence> rtToCopyFence;

	engine::StreamPtr<engine::Shader> clearShader;
	intrusive_ptr<IResourceSet> clearResources;
}
*res;

engine::Core* core_;

HWND hwnd;
UINT width, height;
bool needClearBackBuffer = true;

engine::Camera* cam;

math::mat4 cameraTransform;

ID3D12Device* device;
ID3D12Device5* m_dxrDevice;
ID3D12GraphicsCommandList4* dxrCommandList;
ID3D12StateObject* dxrStateObject;
UINT descriptorSize;
UINT descriptorsAllocated;
ID3D12CommandQueue *rtxQueue;

UINT64 rtToCopyFenceValue;
UINT backBufferIndex;
HANDLE event;
D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputResourceUAVGpuDescriptor;
size_t CameraBuffer;

namespace GlobalRootSignatureParams {
	enum Value {
		CameraConstantBuffer = 0,
		OutputView,
		AccelerationStructure,
		Scene,
		Lights,
		Count
	};
}

void CreateRaytracingOutputResource(UINT width, UINT height);
void WaitForGpu();
void Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, Fence& fence);
void SignalFence(ID3D12CommandQueue* ToQueue, Fence& fence);

void Render()
{
	ICoreRenderer* renderer = engine::GetCoreRenderer();
	ID3D12CommandQueue* graphicQueue = reinterpret_cast<ID3D12CommandQueue*>(renderer->GetNativeGraphicQueue());

	renderer->WaitGPU();
	WaitForGpu();

	throwIfFailed(res->commandAllocators[backBufferIndex]->Reset());
	throwIfFailed(dxrCommandList->Reset(res->commandAllocators[backBufferIndex].Get(), nullptr));

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

		if (memcmp(&cameraTransform, &ViewInvMat_, sizeof(ViewInvMat_)) != 0)
		{
			cameraTransform = ViewInvMat_;

			engine::Shaders::Camera cameraData;
			memcpy(&cameraData.forward.x, &forwardWS, sizeof(vec3));
			memcpy(&cameraData.right.x, &rightWS, sizeof(vec3));
			memcpy(&cameraData.up.x, &upWS, sizeof(vec3));
			memcpy(&cameraData.origin.x, &origin, sizeof(vec4));
			res->cameraBuffers->SetData(&cameraData, sizeof(cameraData));

			needClearBackBuffer = true;
		}
	}

	if (needClearBackBuffer)
	{		
		surface_ptr surface = renderer->GetWindowSurface(hwnd);

		ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();
		cmdList->CommandsBegin();
		cmdList->BindSurface(surface);
		cmdList->SetViewport(width, height);
		cmdList->SetScissor(0, 0, width, height);

		ComputePipelineState cpso{};
		cpso.shader = res->clearShader.get()->GetCoreShader();
		cmdList->SetComputePipelineState(cpso);

		if (!res->clearResources)
		{
			renderer->CreateResourceSet(res->clearResources.getAdressOf(), res->clearShader.get()->GetCoreShader());
			res->clearResources->BindTextueSRV("tex", res->raytracingOutputCore.get());
			cmdList->CompileSet(res->clearResources.get());
			CameraBuffer = res->clearResources->FindInlineBufferIndex("CameraBuffer");
		}		

		cmdList->BindResourceSet(res->clearResources.get());

		uint32_t sizes[2] = { width, height };
		cmdList->UpdateInlineConstantBuffer(CameraBuffer, &sizes, 8);

		{
			constexpr int warpSize = 16;
			int numGroupsX = (width + (warpSize - 1)) / warpSize;
			int numGroupsY = (height + (warpSize - 1)) / warpSize;
			cmdList->Dispatch(numGroupsX, numGroupsY);
		}

		{
			ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();
			D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
			CD3DX12_RESOURCE_BARRIER::UAV(res->raytracingOutput.Get())};
			d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		cmdList->CommandsEnd();
		renderer->ExecuteCommandList(cmdList);

		// Sync clear with RT
		Sync(graphicQueue, rtxQueue, res->clearToRTFence);
	
		needClearBackBuffer = false;
	}

	// raytracing
	{
		dxrCommandList->SetComputeRootSignature(res->raytracingGlobalRootSignature.Get());

		// Bind the heaps, acceleration structure and dispatch rays.    
		dxrCommandList->SetDescriptorHeaps(1, res->descriptorHeap.GetAddressOf());

		dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::OutputView, raytracingOutputResourceUAVGpuDescriptor);
		dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructure, res->topLevelAccelerationStructure->GetGPUVirtualAddress());

		// camera
		{
			x12::Dx12CoreBuffer* dx12buffer = reinterpret_cast<x12::Dx12CoreBuffer*>(res->cameraBuffers.get());
			dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::CameraConstantBuffer, dx12buffer->GPUAddress());
		}

		// scene data
		{
			auto adress = static_cast<x12::Dx12CoreBuffer*>(res->sceneData.get())->GPUAddress();
			dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::Scene, adress);

			adress = static_cast<x12::Dx12CoreBuffer*>(engine::GetSceneManager()->LightsBuffer())->GPUAddress();
			dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Lights, adress);
		}

		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		dispatchDesc.HitGroupTable.StartAddress = res->hitGroupShaderTable->GetGPUVirtualAddress();
		dispatchDesc.HitGroupTable.SizeInBytes = res->hitGroupShaderTable->GetDesc().Width;
		dispatchDesc.HitGroupTable.StrideInBytes = hitRecordSize();
		dispatchDesc.MissShaderTable.StartAddress = res->missShaderTable->GetGPUVirtualAddress();
		dispatchDesc.MissShaderTable.SizeInBytes = res->missShaderTable->GetDesc().Width;
		dispatchDesc.MissShaderTable.StrideInBytes = 32;
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

	// Sync RT with swapchain copy
	Sync(rtxQueue, graphicQueue, res->rtToSwapchainFence);

	// swapchain copy
	{
		surface_ptr surface = renderer->GetWindowSurface(hwnd);

		ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();
		cmdList->CommandsBegin();
		cmdList->BindSurface(surface);
		cmdList->SetViewport(width, height);
		cmdList->SetScissor(0, 0, width, height);

		GraphicPipelineState pso{};
		pso.shader = res->copyShader.get()->GetCoreShader();
		pso.vb = res->plane.get();
		pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
		cmdList->SetGraphicPipelineState(pso);

		cmdList->SetVertexBuffer(res->plane.get());

		{
			ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();

			D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(res->raytracingOutput.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			};

			d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		if (!res->copyResources)
		{
			renderer->CreateResourceSet(res->copyResources.getAdressOf(), res->copyShader.get()->GetCoreShader());
			res->copyResources->BindTextueSRV("texture_", res->raytracingOutputCore.get());
			cmdList->CompileSet(res->copyResources.get());
		}

		cmdList->BindResourceSet(res->copyResources.get());

		cmdList->Draw(res->plane.get());

		{
			ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();

			D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(res->raytracingOutput.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};

			d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		cmdList->CommandsEnd();
		renderer->ExecuteCommandList(cmdList);
	}

	backBufferIndex = (backBufferIndex + 1) % engine::DeferredBuffers;
}

void InitFence(Fence& fence)
{
	for (int i = 0; i < engine::DeferredBuffers; i++)
	{
		throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.fence[i])));
	}
	fence.fenceValues[0]++;
}

void SignalFence(ID3D12CommandQueue* ToQueue, Fence& fence)
{
	ToQueue->Signal(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	fence.fenceValues[backBufferIndex]++;
}

void Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, Fence& fence)
{
	ToQueue->Signal(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);	
	ToQueue->Wait(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	fence.fenceValues[backBufferIndex]++;
}

void WaitForGpu()
{
	// Schedule a Signal command in the GPU queue.
	UINT64 fenceValue = res->frameEndFence.fenceValues[backBufferIndex];
	if (SUCCEEDED(res->commandQueue->Signal(res->frameEndFence.fence[backBufferIndex].Get(), fenceValue)))
	{
		// Wait until the Signal has been processed.
		if (SUCCEEDED(res->frameEndFence.fence[backBufferIndex]->SetEventOnCompletion(fenceValue, event)))
		{
			WaitForSingleObjectEx(event, INFINITE, FALSE);

			// Increment the fence value for the current frame.
			res->frameEndFence.fenceValues[backBufferIndex]++;
		}
	}
}

void Init()
{
	engine::GetSceneManager()->LoadScene("scene.yaml");

	device = (ID3D12Device*)engine::GetCoreRenderer()->GetNativeDevice();

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
	throwIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5)));

	bool tier11 = opt5.RaytracingTier >= D3D12_RAYTRACING_TIER::D3D12_RAYTRACING_TIER_1_1;

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&res->commandQueue)));
	rtxQueue = res->commandQueue.Get();

	// Create a command allocator for each back buffer that will be rendered to.
	for (UINT n = 0; n < engine::DeferredBuffers; n++)
	{
		throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&res->commandAllocators[n])));
	}

	// Create a command list for recording graphics commands.
	throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, res->commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&res->commandList)));
	throwIfFailed(res->commandList->Close());

	InitFence(res->rtToSwapchainFence);
	InitFence(res->clearToRTFence);
	InitFence(res->frameEndFence);

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

		rootParameters[GlobalRootSignatureParams::Scene].InitAsConstantBufferView(2); // scene constants
		rootParameters[GlobalRootSignatureParams::Lights].InitAsShaderResourceView(1); // lights

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr);

		SerializeAndCreateRaytracingRootSignature(device, desc, &res->raytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[2];
		rootParameters[0].InitAsConstants(4 + 16, 1, 0); // camera constants
		rootParameters[1].InitAsShaderResourceView(2, 0); // vertex buffer

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		SerializeAndCreateRaytracingRootSignature(device, desc, &res->raytracingLocalRootSignature);
	}

	{
		// Create subobjects that combine into a RTPSO
		CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		res->raygen = CompileShader(RAYTRACING_SHADER_DIR "raygen.hlsl");
		res->hit = CompileShader(RAYTRACING_SHADER_DIR "hit.hlsl");
		res->miss = CompileShader(RAYTRACING_SHADER_DIR "miss.hlsl");

		res->copyShader = engine::GetResourceManager()->CreateGraphicShader("../resources/shaders/copy.hlsl", nullptr, 0);
		res->copyShader.get();

		const x12::ConstantBuffersDesc buffersdesc[1] =
		{
			"CameraBuffer",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		res->clearShader = engine::GetResourceManager()->CreateComputeShader("../resources/shaders/clear.hlsl", &buffersdesc[0], 1);
		res->clearShader.get();

		// DXIL library
		// This contains the shaders and their entrypoints for the state object.
		// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.

		auto addLibrary = [&raytracingPipeline](ComPtr<IDxcBlob> s, const std::vector<const wchar_t*>& export_)
		{
			auto raygen_lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
			D3D12_SHADER_BYTECODE shaderBytecode;
			shaderBytecode.BytecodeLength = s->GetBufferSize();
			shaderBytecode.pShaderBytecode = s->GetBufferPointer();
			raygen_lib->SetDXILLibrary(&shaderBytecode);

			for (auto e : export_)
				raygen_lib->DefineExport(e);
		};

		addLibrary(res->raygen, { raygenname });
		addLibrary(res->hit, { hitname, shadowhitname });
		addLibrary(res->miss, { missname, shadowmissname });

		// Triangle hit group
		{
			auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			hitGroup->SetClosestHitShaderImport(hitname);
			hitGroup->SetHitGroupExport(hitGroupName);
			hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
		}

		// Shadow hit group
		{
			auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			hitGroup->SetClosestHitShaderImport(shadowhitname);
			hitGroup->SetHitGroupExport(shadowHitGroupName);
			hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);
		}

		// Shader config
		// Defines the maximum sizes in bytes for the ray payload and attribute structure.
		auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
		UINT payloadSize = 4 * sizeof(float);   // float4 color
		UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
		shaderConfig->Config(payloadSize, attributeSize);

		// Local root signature and shader association
		CreateRaygenLocalSignatureSubobject(&raytracingPipeline, hitGroupName, res->raytracingLocalRootSignature.Get());
		CreateRaygenLocalSignatureSubobject(&raytracingPipeline, shadowHitGroupName, res->raytracingLocalRootSignature.Get());
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
		UINT maxRecursionDepth = 2; // ~ primary rays only. 
		pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
		PrintStateObjectDesc(raytracingPipeline);
#endif

		// Create the state object.
		throwIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&res->dxrStateObject)));
		dxrStateObject = res->dxrStateObject.Get();
	}

	descriptorSize = CreateDescriptorHeap(device, res->descriptorHeap.GetAddressOf());

	std::vector<engine::Model*> models;
	engine::GetSceneManager()->getObjectsOfType(models, engine::OBJECT_TYPE::MODEL);

	// BLASes
	{
		for (size_t i = 0; i < models.size(); ++i)
		{
			int id = models[i]->GetId();

			engine::Mesh* m = models[i]->GetMesh();
			if (res->bottomLevelAccelerationStructures[m])
				continue;

			res->bottomLevelAccelerationStructures[m] = BuildBLAS(models[i], m_dxrDevice, res->commandQueue.Get(), dxrCommandList, res->commandAllocators[backBufferIndex].Get());
		}
	}

	// TLAS
	res->topLevelAccelerationStructure = BuildTLAS(res->bottomLevelAccelerationStructures, m_dxrDevice, res->commandQueue.Get(), dxrCommandList, res->commandAllocators[backBufferIndex].Get());

	// Scene buffer
	{
		engine::Shaders::Scene scene;
		scene.instanceCount = models.size();

		std::vector<engine::Model*> lights;
		engine::GetSceneManager()->getObjectsOfType(lights, engine::OBJECT_TYPE::LIGHT);

		scene.lightCount = lights.size();

		engine::GetCoreRenderer()->CreateConstantBuffer(res->sceneData.getAdressOf(), L"Scene data", sizeof(engine::Shaders::Scene), false);
		res->sceneData->SetData(&scene, sizeof(engine::Shaders::Scene));
	}

	// Build Shader Tables
	{
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier;
		void* shadowMissShaderIdentifier;
		void* shadowHitGroupShaderIdentifier;

		// Get shader identifiers
		{
			ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
			throwIfFailed(res->dxrStateObject.As(&stateObjectProperties));

			rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(raygenname);
			missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(missname);
			shadowMissShaderIdentifier = stateObjectProperties->GetShaderIdentifier(shadowmissname);
			hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(hitGroupName);
			shadowHitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(shadowHitGroupName);
		}

		// Ray gen shader table
		{
			UINT numShaderRecords = 1;

			ShaderTable rayGenShaderTable(device, numShaderRecords, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, L"RayGenShaderTable");
			rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES/*, &rootArguments, sizeof(rootArguments)*/));
			res->rayGenShaderTable = rayGenShaderTable.GetResource();
		}

		// Miss shader table
		{
			UINT numShaderRecords = 2;

			UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
			missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderRecordSize));
			missShaderTable.push_back(ShaderRecord(shadowMissShaderIdentifier, shaderRecordSize));
			res->missShaderTable = missShaderTable.GetResource();
		}

		// Hit group shader table
		{
			std::vector<engine::Model*> models;
			engine::GetSceneManager()->getObjectsOfType(models, engine::OBJECT_TYPE::MODEL);

			const UINT numShaderRecords = models.size() * 2;
			ShaderTable hitGroupShaderTable(device, numShaderRecords, hitRecordSize(), L"HitGroupShaderTable");

			for (size_t i = 0; i < models.size(); ++i)
			{
				engine::Mesh* m = models[i]->GetMesh();
				x12::ICoreBuffer* vb = m->VertexBuffer();

				HitArg args;
				args.transform = models[i]->GetWorldTransform();
				args.vertexBuffer = reinterpret_cast<x12::Dx12CoreBuffer*>(m->VertexBuffer())->GPUAddress();;

				hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &args, sizeof(HitArg)));
			}

			// shadows
			for (size_t i = 0; i < models.size(); ++i)
			{
				hitGroupShaderTable.push_back(ShaderRecord(shadowHitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, nullptr, sizeof(HitArg)));
			}

			res->hitGroupShaderTable = hitGroupShaderTable.GetResource();
		}
	}

	engine::MainWindow* window = core_->GetWindow();

	int w, h;
	window->GetClientSize(w, h);
	width = w;
	height = h;
	CreateRaytracingOutputResource(w, h);

	engine::GetCoreRenderer()->CreateConstantBuffer(res->cameraBuffers.getAdressOf(), L"camera constant buffer", sizeof(engine::Shaders::Camera), false);

	{
		float planeVert[6 * 2] =
		{
			-1, -1,
			1, 1,
			-1, 1,
			-1, -1,
			1, -1,
			1, 1
		};

		VertexAttributeDesc attr;
		attr.format = VERTEX_BUFFER_FORMAT::FLOAT2;
		attr.offset = 0;
		attr.semanticName = "POSITION";

		VeretxBufferDesc desc{};
		desc.vertexCount = 6;
		desc.attributesCount = 1;
		desc.attributes = &attr;

		engine::GetCoreRenderer()->CreateVertexBuffer(res->plane.getAdressOf(), L"plane", planeVert, &desc, nullptr, nullptr, MEMORY_TYPE::GPU_READ);
	}
}

void CreateRaytracingOutputResource(UINT width, UINT height)
{
	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	throwIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&res->raytracingOutput)));
	res->raytracingOutput->SetName(L"raytracingOutput");

	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex = 0;// AllocateDescriptor(&uavDescriptorHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);	

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(res->raytracingOutput.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	engine::GetCoreRenderer()->CreateTextureFrom(res->raytracingOutputCore.getAdressOf(), L"Raytracing output", res->raytracingOutput.Get());
}

void messageCallback(HWND hwnd, engine::WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	if (type == engine::WINDOW_MESSAGE::SIZE && (width != param1 && height != param2))
	{
		engine::GetCoreRenderer()->WaitGPUAll();
		WaitForGpu();
		width = param1; height = param2;
		CreateRaytracingOutputResource(width, height);
		needClearBackBuffer = true;
		backBufferIndex = 0;
	}
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
	window->AddMessageCallback(messageCallback);
	hwnd = *window->handle();

	core_->Start();

	FreeUtils();
	::CloseHandle(event);
	delete res;

	core_->Free();
	engine::DestroyCore(core_);

	return 0;
}
