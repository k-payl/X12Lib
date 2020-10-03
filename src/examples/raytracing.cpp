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


struct CameraData
{
	float forward[4];
	float right[4];
	float up[4];
	float origin[4];
};

struct SceneData
{
	uint32_t instancesNum;
};

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
	ComPtr<ID3D12Fence> rtToCopyFence;

	engine::StreamPtr<engine::Shader> clearShader;
	intrusive_ptr<IResourceSet> clearResources;
}
*res;

engine::Core* core_;

HWND hwnd;
UINT width, height;
bool needClearBackBuffer = true;

engine::Camera* cam;
CameraData cameraData;
math::mat4 cameraTransform;

ID3D12Device* device;
ID3D12Device5* m_dxrDevice;
ID3D12GraphicsCommandList4* dxrCommandList;
ID3D12StateObject* dxrStateObject;
UINT descriptorSize;
UINT descriptorsAllocated;
UINT64 fenceValues[engine::DeferredBuffers];
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

void CreateDescriptorHeap();
void BuildAccelerationStructures();
void BuildShaderTables();
void CreateRaytracingOutputResource(UINT width, UINT height);
void WaitForGpu();

void Render()
{
	ICoreRenderer* renderer = engine::GetCoreRenderer();

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

			memcpy(cameraData.forward, &forwardWS, sizeof(vec3));
			memcpy(cameraData.right, &rightWS, sizeof(vec3));
			memcpy(cameraData.up, &upWS, sizeof(vec3));
			memcpy(cameraData.origin, &origin, sizeof(vec4));
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

		// sync clear with raytracing
		{
			ID3D12CommandQueue* queue = reinterpret_cast<ID3D12CommandQueue*>(renderer->GetNativeGraphicQueue());
			queue->Signal(res->rtToCopyFence.Get(), rtToCopyFenceValue);			
			res->commandQueue->Wait(res->rtToCopyFence.Get(), rtToCopyFenceValue);
			rtToCopyFenceValue++;
		}
	
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

	// sync rt with swapchain copy
	{
		res->commandQueue->Signal(res->rtToCopyFence.Get(), rtToCopyFenceValue);
		ID3D12CommandQueue* queue = reinterpret_cast<ID3D12CommandQueue*>(renderer->GetNativeGraphicQueue());
		queue->Wait(res->rtToCopyFence.Get(), rtToCopyFenceValue);
		rtToCopyFenceValue++;
	}

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
	throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&res->rtToCopyFence)));
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

		rootParameters[GlobalRootSignatureParams::Scene].InitAsConstantBufferView(2); // scene constants
		rootParameters[GlobalRootSignatureParams::Lights].InitAsShaderResourceView(1); // lights

		//CD3DX12_DESCRIPTOR_RANGE geomRanges[1];
		//geomRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.
		//rootParameters[GlobalRootSignatureParams::VertexIndexBuffers].InitAsDescriptorTable(1, &geomRanges[0]);

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr);

		SerializeAndCreateRaytracingRootSignature(desc, &res->raytracingGlobalRootSignature);
	}

	// Local Root Signature
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	{
		CD3DX12_ROOT_PARAMETER rootParameters[2];
		rootParameters[0].InitAsConstants(4 + 16, 1, 0); // camera constants
		rootParameters[1].InitAsShaderResourceView(2, 0); // vertex buffer

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		SerializeAndCreateRaytracingRootSignature(desc, &res->raytracingLocalRootSignature);
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
		CreateRaygenLocalSignatureSubobject(&raytracingPipeline, hitGroupName);
		CreateRaygenLocalSignatureSubobject(&raytracingPipeline, shadowHitGroupName);
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

	CreateDescriptorHeap();

	BuildAccelerationStructures();

	BuildShaderTables();

	engine::MainWindow* window = core_->GetWindow();

	int w, h;
	window->GetClientSize(w, h);
	width = w;
	height = h;
	CreateRaytracingOutputResource(w, h);

	engine::GetCoreRenderer()->CreateConstantBuffer(res->cameraBuffers.getAdressOf(), L"camera constant buffer", sizeof(cameraData), false);

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

void CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.NumDescriptors = 1; // 1 raytracing output
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptorHeapDesc.NodeMask = 0;
	device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&res->descriptorHeap));
	res->descriptorHeap->SetName(L"Descriptor heap for raytracing (gpu visible)");

	descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void TransformToDxr(FLOAT DXRTransform[3][4], const math::mat4& transform)
{
	DXRTransform[0][0] = DXRTransform[1][1] = DXRTransform[2][2] = 1;
	memcpy(DXRTransform[0], transform.el_2D[0], sizeof(math::vec4));
	memcpy(DXRTransform[1], transform.el_2D[1], sizeof(math::vec4));
	memcpy(DXRTransform[2], transform.el_2D[2], sizeof(math::vec4));
}

template<typename T>
void addObjectsRecursive(std::vector<T*>& ret, engine::GameObject* root, engine::OBJECT_TYPE type)
{
	size_t childs = root->GetNumChilds();

	for (size_t i = 0; i < childs; i++)
	{
		engine::GameObject* g = root->GetChild(i);
		addObjectsRecursive<T>(ret, g, type);
	}

	if (root->GetType() == type && root->IsEnabled())
		ret.push_back(static_cast<T*>(root));
}

template<typename T>
void getObjects(std::vector<T*>& vec, engine::OBJECT_TYPE type)
{
	size_t objects = engine::GetSceneManager()->GetNumObjects();

	for (size_t i = 0; i < objects; i++)
	{
		T* g = static_cast<T*>(engine::GetSceneManager()->GetObject_(i));
		addObjectsRecursive<T>(vec, g, type);
	}
}

void BuildAccelerationStructures()
{
	std::vector<engine::Model*> models;
	getObjects(models, engine::OBJECT_TYPE::MODEL);

	const UINT topLevelInstances = models.size();

	SceneData scene;
	scene.instancesNum = topLevelInstances;

	engine::GetCoreRenderer()->CreateConstantBuffer(res->sceneData.getAdressOf(), L"Scene data", sizeof(SceneData), false);
	res->sceneData->SetData(&scene, sizeof(SceneData));

	auto commandList = res->commandList.Get();
	auto commandQueue = res->commandQueue.Get();
	auto commandAllocator = res->commandAllocators[0].Get();

	// Get required sizes for an acceleration structure.
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = topLevelInstances;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	assert(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	ComPtr<ID3D12Resource> scratchResource;
	size_t scratchSize = topLevelPrebuildInfo.ScratchDataSizeInBytes;
	AllocateUAVBuffer(device, scratchSize, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	for (size_t i = 0; i < models.size(); ++i)
	{
		int id = models[i]->GetId();

		engine::Mesh* m = models[i]->GetMesh();

		if (res->bottomLevelAccelerationStructures[m])
			continue;

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
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = topLevelInputs;
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.pGeometryDescs = &geometryDesc;
		bottomLevelInputs.NumDescs = 1;
		m_dxrDevice->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
		assert(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		if (bottomLevelPrebuildInfo.ScratchDataSizeInBytes > scratchSize)
		{
			scratchSize = bottomLevelPrebuildInfo.ScratchDataSizeInBytes;
			AllocateUAVBuffer(device, scratchSize, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
		}

		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

		ComPtr<ID3D12Resource> bottomLevelAccelerationStructure;
		AllocateUAVBuffer(device, bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &bottomLevelAccelerationStructure, initialResourceState, L"BottomLevelAccelerationStructure");

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

		res->bottomLevelAccelerationStructures[m] = bottomLevelAccelerationStructure;
	}

	// Create an instance desc for the bottom-level acceleration structure.
	intrusive_ptr<x12::ICoreBuffer> instanceDescsBuffer;
	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDesc(topLevelInstances);

	for (int i = 0; i < topLevelInstances; ++i)
	{
		TransformToDxr(instanceDesc[i].Transform, models[i]->GetWorldTransform());
		instanceDesc[i].InstanceMask = 1;
		instanceDesc[i].InstanceID = 0;
		instanceDesc[i].InstanceContributionToHitGroupIndex = i;
		instanceDesc[i].AccelerationStructure = res->bottomLevelAccelerationStructures[models[i]->GetMesh()]->GetGPUVirtualAddress();
	}
	engine::GetCoreRenderer()->CreateStructuredBuffer(instanceDescsBuffer.getAdressOf(), L"instance desc for the top-level acceleration structure", sizeof(instanceDesc) * topLevelInstances, topLevelInstances, &instanceDesc[0], BUFFER_FLAGS::NONE);

	D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	AllocateUAVBuffer(device, topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &res->topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");

	// Top Level Acceleration Structure desc
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	topLevelInputs.InstanceDescs = reinterpret_cast<x12::Dx12CoreBuffer*>(instanceDescsBuffer.get())->GPUAddress();
	topLevelBuildDesc.Inputs = topLevelInputs;
	topLevelBuildDesc.DestAccelerationStructureData = res->topLevelAccelerationStructure->GetGPUVirtualAddress();
	topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();

	throwIfFailed(dxrCommandList->Reset(commandAllocator, nullptr));
	dxrCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	throwIfFailed(dxrCommandList->Close());

	ID3D12CommandList* commandLists[] = { dxrCommandList };
	commandQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);

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
		getObjects(models, engine::OBJECT_TYPE::MODEL);

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

	::CloseHandle(event);
	delete res;

	core_->Free();
	engine::DestroyCore(core_);

	return 0;
}
