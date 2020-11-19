#include "d3dx12.h"

#include "raytracing_utils.h"
#include "raytracing_d3dx12.h"

#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "icorerender.h"
#include "model.h"
#include "light.h"
#include "material.h"
#include "gameobject.h"
#include "scenemanager.h"
#include "resourcemanager.h"
#include "materialmanager.h"
#include "mesh.h"
#include "d3d12/dx12buffer.h"
#include "d3d12/dx12texture.h"
#include "cpp_hlsl_shared.h"
using namespace x12;

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

const wchar_t* raygenname = L"RayGen";

const wchar_t* hitGroupName = L"HitGroup";
const wchar_t* hitname = L"ClosestHit";
const wchar_t* missname = L"Miss";

const wchar_t* shadowHitGroupName = L"ShadowHitGroup";
const wchar_t* shadowhitname = L"ShadowClosestHit";
const wchar_t* shadowmissname = L"ShadowMiss";

#pragma pack(push, 1)
struct HitArg
{
	uint32_t offset;
	float _padding[3];
	D3D12_GPU_VIRTUAL_ADDRESS vertexBuffer;
};
#pragma pack(pop)

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

	// DXR
	ComPtr<ID3D12Device5> dxrDevice;
	ComPtr<ID3D12GraphicsCommandList4> dxrCommandList;

	// DXR resources
	ComPtr<ID3D12StateObject> dxrPrimaryStateObject;
	ComPtr<ID3D12StateObject> dxrSecondaryStateObject;

	ComPtr<ID3D12CommandSignature> dxrSecondaryCommandIndirect;

	ComPtr<ID3D12Resource> missShaderTable;
	ComPtr<ID3D12Resource> hitGroupShaderTable;
	ComPtr<ID3D12Resource> rayGenShaderTable;

	ComPtr<ID3D12Resource> missShaderTable_secondary;
	ComPtr<ID3D12Resource> hitGroupShaderTable_secondary;
	ComPtr<ID3D12Resource> rayGenShaderTable_secondary;

	ComPtr<IDxcBlob> raygen;
	ComPtr<IDxcBlob> hit;
	ComPtr<IDxcBlob> miss;

	ComPtr<IDxcBlob> raygen_secondary;
	ComPtr<IDxcBlob> hit_secondary;
	ComPtr<IDxcBlob> miss_secondary;

	ComPtr<ID3D12RootSignature> raytracingGlobalRootSignature;
	ComPtr<ID3D12RootSignature> raytracingLocalRootSignature;
	ComPtr<ID3D12RootSignature> clearRayInfoRootSignature;
	ComPtr<ID3D12RootSignature> regroupRayInfoRootSignature;
	ComPtr<ID3D12RootSignature> clearHitCounterRootSignature;
	ComPtr<ID3D12RootSignature> accumulationRootSignature;
	ComPtr<ID3D12RootSignature> copyrHitCounterRootSignature;

	ComPtr<ID3D12PipelineState> clearRayInfoPSO;
	ComPtr<ID3D12PipelineState> regroupRayInfoPSO;
	ComPtr<ID3D12PipelineState> clearHitCounterPSO;
	ComPtr<ID3D12PipelineState> accumulationPSO;
	ComPtr<ID3D12PipelineState> copyHitCounterPSO;

	ComPtr<ID3D12Resource> topLevelAccelerationStructure;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	intrusive_ptr<x12::ICoreBuffer> sceneBuffer;
	intrusive_ptr<x12::ICoreBuffer> cameraBuffer;
	intrusive_ptr<x12::ICoreBuffer> perInstanceBuffer;
	intrusive_ptr<x12::ICoreBuffer> materialsBuffer;
	intrusive_ptr<x12::ICoreBuffer> rayInfoBuffer; // w * h * sizeof(RayInfo)
	intrusive_ptr<x12::ICoreBuffer> regroupedIndexesBuffer; // w * h * sizeof(uint32_t)
	intrusive_ptr<x12::ICoreBuffer> iterationRGBA32f; // w * h * sizeof(float4). primary + secondary + ... + N-th bounce
	intrusive_ptr<x12::ICoreBuffer> hitCointerBuffer; // 4 byte
	intrusive_ptr<x12::ICoreBuffer> indirectBuffer; // sizeof(D3D12_DISPATCH_RAYS_DESC)
	engine::StreamPtr<engine::Mesh> planeMesh;
	ComPtr<ID3D12Resource> outputRGBA32f; // final image accumulation (Texture)
	intrusive_ptr<ICoreTexture> outputRGBA32fcoreWeakPtr;

	engine::StreamPtr<engine::Shader> copyShader;
	intrusive_ptr<IResourceSet> copyResourceSet;
	intrusive_ptr<ICoreVertexBuffer> plane;

	engine::StreamPtr<engine::Shader> clearShader;
	intrusive_ptr<IResourceSet> clearResources;

	engine::StreamPtr<engine::Shader> clearRayInfoBuffer;

	std::vector<BLAS> blases;

	Fence rtToSwapchainFence;
	Fence clearToRTFence;
	Fence frameEndFence;
}
*res;

engine::Core* core_;

HWND hwnd;
UINT width, height;
bool needClearBackBuffer = true;
uint32_t frame;

engine::Camera* cam;
size_t CameraBuffer;
math::mat4 cameraTransform;

ID3D12Device* device;
ID3D12Device5* m_dxrDevice;
ID3D12GraphicsCommandList4* dxrCommandList;
ID3D12StateObject* dxrPrimaryStateObject;
UINT descriptorSize;
UINT descriptorsAllocated;
ID3D12CommandQueue *rtxQueue;

UINT backBufferIndex;
HANDLE event;
D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputResourceUAVGpuDescriptor;
D3D12_GPU_DESCRIPTOR_HANDLE texturesHandle;


namespace GlobalRootSignatureParams {
	enum {
		FrameConstantBuffer = 0,
		CameraConstantBuffer,
		OutputView,
		RayInfo,
		AccelerationStructure,
		SceneConstants,
		LightStructuredBuffer,
		TLASPerInstanceBuffer,
		Materials,
		RegroupedIndexes,
		Textures,
		Count
	};
}

namespace LocalRootSignatureParams {
	enum {
		InstanceConstants = 0,
		VertexBuffer,
		Count
	};
}

namespace ClearRayInfoRootSignatureParams {
	enum {
		Constant = 0,
		RayInfoBuffer,
		IterationColorBuffer,
		Count
	};
}

namespace RegroupRayInfoRootSignatureParams {
	enum {
		ConstantIn = 0,
		RayInfoIn,
		HitCounterOut,
		RegroupedIndexesOut,
		Count
	};
}

namespace AccumulationRootSignatureParams {
	enum {
		ConstantIn = 0,
		LightingIn,
		ResultBufferInOut,
		Count
	};
}

void CreateRaytracingOutputResource(UINT width, UINT height);
void WaitForGpu();
void Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, Fence& fence);
void SignalFence(ID3D12CommandQueue* ToQueue, Fence& fence);

void BindRaytracingResources()
{
	// Bind root signature
	dxrCommandList->SetComputeRootSignature(res->raytracingGlobalRootSignature.Get());

	// Bind global resources
	dxrCommandList->SetComputeRoot32BitConstant(GlobalRootSignatureParams::FrameConstantBuffer, frame, 0);

	x12::Dx12CoreBuffer* dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->cameraBuffer.get());
	dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::CameraConstantBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->iterationRGBA32f.get());
	dxrCommandList->SetComputeRootUnorderedAccessView(GlobalRootSignatureParams::OutputView, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->rayInfoBuffer.get());
	dxrCommandList->SetComputeRootUnorderedAccessView(GlobalRootSignatureParams::RayInfo, dx12buffer->GPUAddress());

	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructure, res->topLevelAccelerationStructure->GetGPUVirtualAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->sceneBuffer.get());
	dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstants, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(engine::GetSceneManager()->LightsBuffer());
	if (dx12buffer)
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::LightStructuredBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->perInstanceBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::TLASPerInstanceBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->materialsBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Materials, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->regroupedIndexesBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::RegroupedIndexes, dx12buffer->GPUAddress());

	if (texturesHandle.ptr)
		dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::Textures, texturesHandle);

}

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
			cameraData.width = width;
			cameraData.height = height;

			res->cameraBuffer->SetData(&cameraData, sizeof(cameraData));

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
			res->clearResources->BindTextueSRV("tex", res->outputRGBA32fcoreWeakPtr.get());
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
			D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(res->outputRGBA32f.Get())};
			d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		cmdList->CommandsEnd();
		renderer->ExecuteCommandList(cmdList);

		// Sync clear with RT
		Sync(graphicQueue, rtxQueue, res->clearToRTFence);
	
		needClearBackBuffer = false;
		frame = 0;
	}

	// Raytracing
	{
		// Bind heap
		dxrCommandList->SetDescriptorHeaps(1, res->descriptorHeap.GetAddressOf());

		// Clear buffers before RT
		{
			dxrCommandList->SetPipelineState(res->clearRayInfoPSO.Get());

			dxrCommandList->SetComputeRootSignature(res->clearRayInfoRootSignature.Get());

			auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->rayInfoBuffer.get());
			dxrCommandList->SetComputeRootUnorderedAccessView(ClearRayInfoRootSignatureParams::RayInfoBuffer, dx12buffer->GPUAddress());

			dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->iterationRGBA32f.get());
			dxrCommandList->SetComputeRootUnorderedAccessView(ClearRayInfoRootSignatureParams::IterationColorBuffer, dx12buffer->GPUAddress());

			dxrCommandList->SetComputeRoot32BitConstant(ClearRayInfoRootSignatureParams::Constant, width * height, 0);

			constexpr int warpSize = 256;
			int numGroupsX = (width * height + (warpSize - 1)) / warpSize;
			dxrCommandList->Dispatch(numGroupsX, 1, 1);

			D3D12_RESOURCE_BARRIER preCopyBarriers[] =
			{
				CD3DX12_RESOURCE_BARRIER::UAV(static_cast<x12::Dx12CoreBuffer*>(res->rayInfoBuffer.get())->GetResource()),
				CD3DX12_RESOURCE_BARRIER::UAV(static_cast<x12::Dx12CoreBuffer*>(res->iterationRGBA32f.get())->GetResource()),
			};
			dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		// Primary rays
		{
			// Bind pipeline
			dxrCommandList->SetPipelineState1(res->dxrPrimaryStateObject.Get());

			BindRaytracingResources();

			// Do raytracing
			{
				D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
				dispatchDesc.HitGroupTable.StartAddress = res->hitGroupShaderTable->GetGPUVirtualAddress();
				dispatchDesc.HitGroupTable.SizeInBytes = res->hitGroupShaderTable->GetDesc().Width;
				dispatchDesc.HitGroupTable.StrideInBytes = hitRecordSize();
				dispatchDesc.MissShaderTable.StartAddress = res->missShaderTable->GetGPUVirtualAddress();
				dispatchDesc.MissShaderTable.SizeInBytes = res->missShaderTable->GetDesc().Width;
				dispatchDesc.MissShaderTable.StrideInBytes = 32;
				dispatchDesc.RayGenerationShaderRecord.StartAddress = res->rayGenShaderTable->GetGPUVirtualAddress();
				dispatchDesc.RayGenerationShaderRecord.SizeInBytes = res->rayGenShaderTable->GetDesc().Width;
				dispatchDesc.Width = width * height;
				dispatchDesc.Height = 1;
				dispatchDesc.Depth = 1;
				dxrCommandList->DispatchRays(&dispatchDesc);
			}
		}

		// Indirect lighting
		if (1)
		for (int i = 0; i < 1; ++i)
		{
			// Clear hits
			{
				dxrCommandList->SetPipelineState(res->clearHitCounterPSO.Get());

				dxrCommandList->SetComputeRootSignature(res->clearHitCounterRootSignature.Get());

				auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->hitCointerBuffer.get());
				dxrCommandList->SetComputeRootUnorderedAccessView(0, dx12buffer->GPUAddress());

				dxrCommandList->Dispatch(1, 1, 1);

				// hitCointerBuffer Write->Read
				{
					auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->hitCointerBuffer.get());
					D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
					dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
				}
			}

			// Hit rays sorting
			{
				// RayInfo Write->Read
				{
					auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->rayInfoBuffer.get());
					D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
					dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
				}

				dxrCommandList->SetPipelineState(res->regroupRayInfoPSO.Get());

				dxrCommandList->SetComputeRootSignature(res->regroupRayInfoRootSignature.Get());

				dxrCommandList->SetComputeRoot32BitConstant(RegroupRayInfoRootSignatureParams::ConstantIn, width * height, 0);

				auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->rayInfoBuffer.get());
				dxrCommandList->SetComputeRootShaderResourceView(RegroupRayInfoRootSignatureParams::RayInfoIn, dx12buffer->GPUAddress());

				dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->hitCointerBuffer.get());
				dxrCommandList->SetComputeRootUnorderedAccessView(RegroupRayInfoRootSignatureParams::HitCounterOut, dx12buffer->GPUAddress());

				dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->regroupedIndexesBuffer.get());
				dxrCommandList->SetComputeRootUnorderedAccessView(RegroupRayInfoRootSignatureParams::RegroupedIndexesOut, dx12buffer->GPUAddress());

				constexpr int warpSize = 256;
				int numGroupsX = (width * height + (warpSize - 1)) / warpSize;
				dxrCommandList->Dispatch(numGroupsX, 1, 1);

				// RegroupedIndexes, Hit count Write->Read
				{
					auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->regroupedIndexesBuffer.get());
					auto dx12buffer1 = static_cast<x12::Dx12CoreBuffer*>(res->hitCointerBuffer.get());
					D3D12_RESOURCE_BARRIER preCopyBarriers[2] =
					{
						CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()),
						CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer1->GetResource()),
					};
					dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
				}

				//std::vector<uint32_t> d(width * height);
				//res->regroupedIndexesBuffer->GetData(&d[0]);
				//std::sort(d.begin(), d.end());

				//uint32_t d1;
				//res->hitCointerBuffer->GetData(&d1);
				//int y = 0;
			}

			// Copy hits to indirect arguments buffer
			{
				dxrCommandList->SetPipelineState(res->copyHitCounterPSO.Get());

				dxrCommandList->SetComputeRootSignature(res->copyrHitCounterRootSignature.Get());

				auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->hitCointerBuffer.get());
				dxrCommandList->SetComputeRootShaderResourceView(0, dx12buffer->GPUAddress());

				dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->indirectBuffer.get());
				dxrCommandList->SetComputeRootUnorderedAccessView(1, dx12buffer->GPUAddress());

				dxrCommandList->Dispatch(1, 1, 1);

				// indirectBuffer Write->Read
				{
					auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->indirectBuffer.get());
					D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
					dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
				}
			}

			{
				// Bind pipeline
				dxrCommandList->SetPipelineState1(res->dxrSecondaryStateObject.Get());

				BindRaytracingResources();

				auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->indirectBuffer.get());
				dxrCommandList->ExecuteIndirect(res->dxrSecondaryCommandIndirect.Get(), 1, dx12buffer->GetResource(), 0, nullptr, 0);
			}
		}

		{
			auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->iterationRGBA32f.get());
			D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
			dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		// Final accumulation
		{
			dxrCommandList->SetPipelineState(res->accumulationPSO.Get());

			dxrCommandList->SetComputeRootSignature(res->accumulationRootSignature.Get());

			uint32_t consts[2] = { width * height, width };
			dxrCommandList->SetComputeRoot32BitConstants(AccumulationRootSignatureParams::ConstantIn, 2, consts, 0);

			dxrCommandList->SetComputeRootDescriptorTable(AccumulationRootSignatureParams::ResultBufferInOut, raytracingOutputResourceUAVGpuDescriptor);

			auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(res->iterationRGBA32f.get());
			dxrCommandList->SetComputeRootShaderResourceView(AccumulationRootSignatureParams::LightingIn, dx12buffer->GPUAddress());

			constexpr int warpSize = 256;
			int numGroupsX = (width * height + (warpSize - 1)) / warpSize;
			dxrCommandList->Dispatch(numGroupsX, 1, 1);
		}

		throwIfFailed(dxrCommandList->Close());
		ID3D12CommandList* commandLists[] = { dxrCommandList };
		rtxQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);
	}

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
				CD3DX12_RESOURCE_BARRIER::Transition(res->outputRGBA32f.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			};

			d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		if (!res->copyResourceSet)
		{
			renderer->CreateResourceSet(res->copyResourceSet.getAdressOf(), res->copyShader.get()->GetCoreShader());
			res->copyResourceSet->BindTextueSRV("texture_", res->outputRGBA32fcoreWeakPtr.get());
			cmdList->CompileSet(res->copyResourceSet.get());
		}

		cmdList->BindResourceSet(res->copyResourceSet.get());

		cmdList->Draw(res->plane.get());

		{
			ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();

			D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
				CD3DX12_RESOURCE_BARRIER::Transition(res->outputRGBA32f.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
			};

			d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
		}

		cmdList->CommandsEnd();
		renderer->ExecuteCommandList(cmdList);
	}

	backBufferIndex = (backBufferIndex + 1) % engine::DeferredBuffers;
	frame++;
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
#if 1
	engine::GetSceneManager()->LoadScene("sponza/sponza.yaml");
#else
	engine::GetSceneManager()->LoadScene("scene.yaml");
#endif

	res->planeMesh = engine::GetResourceManager()->CreateStreamMesh("std#plane");

	device = (ID3D12Device*)engine::GetCoreRenderer()->GetNativeDevice();

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
	throwIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5)));

	bool tier11 = opt5.RaytracingTier >= D3D12_RAYTRACING_TIER::D3D12_RAYTRACING_TIER_1_1;
	if (opt5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		abort();

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

	{
		res->copyShader = engine::GetResourceManager()->CreateGraphicShader("../resources/shaders/copy.hlsl", nullptr, 0);
		res->copyShader.get();

		const x12::ConstantBuffersDesc buffersdesc[1] =
		{
			"CameraBuffer",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		res->clearShader = engine::GetResourceManager()->CreateComputeShader("../resources/shaders/clear.hlsl", &buffersdesc[0], 1);
		res->clearShader.get();
	}

	// Global Root Signature
	{
		CD3DX12_ROOT_PARAMETER rootParameters[GlobalRootSignatureParams::Count];

		rootParameters[GlobalRootSignatureParams::FrameConstantBuffer].InitAsConstants(1, 3);
		rootParameters[GlobalRootSignatureParams::OutputView].InitAsUnorderedAccessView(0);
		rootParameters[GlobalRootSignatureParams::RayInfo].InitAsUnorderedAccessView(1);
		rootParameters[GlobalRootSignatureParams::CameraConstantBuffer].InitAsConstantBufferView(0);
		rootParameters[GlobalRootSignatureParams::AccelerationStructure].InitAsShaderResourceView(0);
		rootParameters[GlobalRootSignatureParams::SceneConstants].InitAsConstantBufferView(1);
		rootParameters[GlobalRootSignatureParams::LightStructuredBuffer].InitAsShaderResourceView(1);
		rootParameters[GlobalRootSignatureParams::TLASPerInstanceBuffer].InitAsShaderResourceView(3);
		rootParameters[GlobalRootSignatureParams::Materials].InitAsShaderResourceView(4);
		rootParameters[GlobalRootSignatureParams::RegroupedIndexes].InitAsShaderResourceView(5);

		CD3DX12_DESCRIPTOR_RANGE ranges;
		ranges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 100, 6);  // array of textures
		rootParameters[GlobalRootSignatureParams::Textures].InitAsDescriptorTable(1, &ranges);

		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 1, &sampler);

		SerializeAndCreateRaytracingRootSignature(device, desc, &res->raytracingGlobalRootSignature);
	}

	// Local Root Signature
	{
		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
		rootParameters[LocalRootSignatureParams::InstanceConstants].InitAsConstants(sizeof(engine::Shaders::InstancePointer) / 4, 2, 0);
		rootParameters[LocalRootSignatureParams::VertexBuffer].InitAsShaderResourceView(2, 0);

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		SerializeAndCreateRaytracingRootSignature(device, desc, &res->raytracingLocalRootSignature);
	}

	{
		std::vector<std::pair<std::wstring, std::wstring>> defines_primary;
		defines_primary.emplace_back(L"PRIMARY_RAY", L"1");

		res->raygen = CompileShader(L"rt_raygen.hlsl", false, defines_primary);
		res->hit = CompileShader(L"rt_hit.hlsl", false, defines_primary);
		res->miss = CompileShader(L"rt_miss.hlsl", false, defines_primary);

		res->raygen_secondary = CompileShader(L"rt_raygen.hlsl");
		res->hit_secondary = CompileShader(L"rt_hit.hlsl");
		res->miss_secondary = CompileShader(L"rt_miss.hlsl");
	}

	auto CreatePSO = [](ComPtr<IDxcBlob> r, ComPtr<IDxcBlob> h, ComPtr<IDxcBlob> m) -> ComPtr<ID3D12StateObject>
	{
		ComPtr<ID3D12StateObject> RTXPSO;

		// Create subobjects that combine into a RTPSO
		CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

		// DXIL library
		// This contains the shaders and their entrypoints for the state object.
		// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.

		auto addLibrary = [](CD3DX12_STATE_OBJECT_DESC& raytracingPipeline, ComPtr<IDxcBlob> s, const std::vector<const wchar_t*>& export_)
		{
			auto raygen_lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
			D3D12_SHADER_BYTECODE shaderBytecode;
			shaderBytecode.BytecodeLength = s->GetBufferSize();
			shaderBytecode.pShaderBytecode = s->GetBufferPointer();
			raygen_lib->SetDXILLibrary(&shaderBytecode);

			for (auto e : export_)
				raygen_lib->DefineExport(e);
		};

		addLibrary(raytracingPipeline, r, { raygenname });
		addLibrary(raytracingPipeline, h, { hitname });
		addLibrary(raytracingPipeline, m, { missname });

		// Triangle hit group
		{
			auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			hitGroup->SetClosestHitShaderImport(hitname);
			hitGroup->SetHitGroupExport(hitGroupName);
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
		UINT maxRecursionDepth = 2; // ~ primary rays only + TraceRayInline(). 
		pipelineConfig->Config(maxRecursionDepth);

#if _DEBUG
		PrintStateObjectDesc(raytracingPipeline);
#endif
		// Create the state object.
		throwIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&RTXPSO)));

		return RTXPSO;
	};

	res->dxrPrimaryStateObject = CreatePSO(res->raygen, res->hit, res->miss);
	res->dxrSecondaryStateObject = CreatePSO(res->raygen_secondary, res->hit_secondary, res->miss_secondary);

	descriptorSize = CreateDescriptorHeap(device, res->descriptorHeap.GetAddressOf());

	std::vector<engine::Model*> models;
	engine::GetSceneManager()->getObjectsOfType(models, engine::OBJECT_TYPE::MODEL);

	std::vector<engine::Light*> areaLights;
	engine::GetSceneManager()->getObjectsOfType(areaLights, engine::OBJECT_TYPE::LIGHT);
	std::remove_if(areaLights.begin(), areaLights.end(), [](const engine::Light* l)-> bool { return l->GetLightType() != engine::LIGHT_TYPE::AREA; });

	UINT totalInstances = 0;

	// BLASes
	{
		struct BLASmesh
		{
			engine::Mesh* mesh;
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			math::mat4 transform;
			engine::Material* material;
		};

		std::vector<BLASmesh> ms;
		std::vector<engine::Material*> sceneMaterials;

		for (size_t i = 0; i < models.size(); ++i)
		{
			engine::Mesh* m = models[i]->GetMesh();
			ms.push_back({m, nullptr, models[i]->GetWorldTransform(), models[i]->GetMaterial() });
		}

		std::sort(ms.begin(), ms.end(), [](const BLASmesh& l , const BLASmesh& r) -> bool { return l.transform < r.transform; });

		// Parallel arrays
		std::vector<engine::Mesh*> blas_meshes;
		std::vector<engine::Material*> blas_materials;
		std::vector<D3D12_GPU_VIRTUAL_ADDRESS> blas_meshesh_vb;

		blas_meshes.push_back(ms[0].mesh);
		blas_meshesh_vb.push_back(reinterpret_cast<x12::Dx12CoreBuffer*>(ms[0].mesh->VertexBuffer())->GPUAddress());
		blas_materials.push_back(ms[0].material ? ms[0].material : engine::GetMaterialManager()->GetDefaultMaterial());

		assert(ms.size() > 0);

		math::mat4 t = ms[0].transform;

		for (int i = 0; i < ms.size(); i++)
		{
			if (i == ms.size() - 1 || ms[i + 1].transform < t || t < ms[i + 1].transform)
			{
				// Create BLAS
				auto resource = BuildBLAS(blas_meshes, m_dxrDevice, res->commandQueue.Get(), dxrCommandList, res->commandAllocators[backBufferIndex].Get());

				res->blases.push_back({ ms[i].transform, (UINT)blas_meshes.size(), resource, blas_meshesh_vb, blas_materials });
				totalInstances += (UINT)blas_meshes.size();

				blas_meshes.clear();
				blas_meshesh_vb.clear();
				blas_materials.clear();
			}

			if (i < ms.size() - 1)
			{
				engine::Mesh* mesh = ms[i + 1].mesh;

				blas_meshes.push_back(mesh);
				blas_meshesh_vb.push_back(reinterpret_cast<x12::Dx12CoreBuffer*>(mesh->VertexBuffer())->GPUAddress());
				blas_materials.push_back(ms[i + 1].material ? ms[i + 1].material : engine::GetMaterialManager()->GetDefaultMaterial());
			}
		}
	}

	// TLAS
	res->topLevelAccelerationStructure = BuildTLAS(res->blases, m_dxrDevice,
		res->commandQueue.Get(), dxrCommandList, res->commandAllocators[backBufferIndex].Get());

	// Materials
	{
		std::vector<engine::Texture*> sceneTextures;
		std::unordered_map<engine::Texture*, UINT> textureToIndex;

		std::vector<engine::Material*> sceneMaterial;
		std::vector<engine::Shaders::Material> sceneMaterialGPU;
		std::unordered_map<engine::Material*, UINT> materialToIndex;

		for (size_t i = 0; i < res->blases.size(); ++i)
		{
			for (size_t j = 0; j < res->blases[i].instances; ++j)
			{
				engine::Material* mat = res->blases[i].materials[j];

				auto it = materialToIndex.find(mat);
				if (it == materialToIndex.end())
				{
					materialToIndex[mat] = sceneMaterial.size();
					sceneMaterial.push_back(mat);

					engine::Shaders::Material gpuMaterial{};
					gpuMaterial.albedo = mat->GetValue(engine::Material::Params::Albedo);
					gpuMaterial.shading.x = mat->GetValue(engine::Material::Params::Roughness).x;

					engine::Texture * texture = mat->GetTexture(engine::Material::Params::Albedo);

					if (texture)
					{
						auto itt = textureToIndex.find(texture);
						if (itt == textureToIndex.end())
						{
							textureToIndex[texture] = sceneTextures.size();
							sceneTextures.push_back(texture);
						}
						gpuMaterial.albedoIndex = textureToIndex[texture];
					}
					else
					{
						gpuMaterial.albedoIndex = UINT(-1);
					}
					
					sceneMaterialGPU.push_back(gpuMaterial);
				}
			}
		}

		engine::GetCoreRenderer()->CreateStructuredBuffer(res->materialsBuffer.getAdressOf(), L"Materials",
			sizeof(engine::Shaders::Material), sceneMaterialGPU.size(), &sceneMaterialGPU[0], x12::BUFFER_FLAGS::SHADER_RESOURCE);

		// Textures
		{
			for (int i = 0; i < sceneTextures.size(); i++)
			{
				auto coreTexture = sceneTextures[i]->GetCoreTexture();
				Dx12CoreTexture* dx12CoreTexure = static_cast<x12::Dx12CoreTexture*>(coreTexture);
				D3D12_CPU_DESCRIPTOR_HANDLE cputextureHandle = dx12CoreTexure->GetSRV();
				D3D12_CPU_DESCRIPTOR_HANDLE destCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1 + i, descriptorSize);

				device->CopyDescriptorsSimple(1, destCPU, cputextureHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				if (!texturesHandle.ptr)
					texturesHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, descriptorSize);

			}
		}

		// Instances Data
		std::vector<engine::Shaders::InstanceData> instancesData(totalInstances);

		UINT offset = 0;
		for (size_t i = 0; i < res->blases.size(); ++i)
		{
			for (size_t j = 0; j < res->blases[i].instances; ++j)
			{
				math::mat4 transform = res->blases[i].transform;
				instancesData[offset].transform = transform;
				instancesData[offset].normalTransform = transform.Inverse().Transpose();
				instancesData[offset].emission = 0;
				instancesData[offset].materialIndex = materialToIndex[res->blases[i].materials[j]];
				offset++;
			}
		}

		engine::GetCoreRenderer()->CreateStructuredBuffer(res->perInstanceBuffer.getAdressOf(), L"TLAS per-instance data",
			sizeof(engine::Shaders::InstanceData), instancesData.size(), &instancesData[0], x12::BUFFER_FLAGS::SHADER_RESOURCE);
	}

	// Scene buffer
	{
		engine::Shaders::Scene scene;
		scene.instanceCount = totalInstances;
		scene.lightCount = (uint32_t)areaLights.size();

		engine::GetCoreRenderer()->CreateConstantBuffer(res->sceneBuffer.getAdressOf(), L"Scene data", sizeof(engine::Shaders::Scene), false);
		res->sceneBuffer->SetData(&scene, sizeof(engine::Shaders::Scene));
	}

	// Build Shader Tables
	auto CreateShaderBindingTable = [&](ComPtr<ID3D12StateObject> state, ComPtr<ID3D12Resource>& r, ComPtr<ID3D12Resource>& h, ComPtr<ID3D12Resource>& m)
	{
		void* rayGenShaderIdentifier;
		void* missShaderIdentifier;
		void* hitGroupShaderIdentifier;

		// Get shader identifiers
		{
			ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
			throwIfFailed(state.As(&stateObjectProperties));

			rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(raygenname);
			missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(missname);
			hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(hitGroupName);
		}

		// Ray gen shader table
		{
			UINT numShaderRecords = 1;

			ShaderTable rayGenShaderTable(device, numShaderRecords, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, L"RayGenShaderTable");
			rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
			r = rayGenShaderTable.GetResource();
		}

		// Miss shader table
		{
			UINT numShaderRecords = 1;

			UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			ShaderTable missShaderTable(device, numShaderRecords, shaderRecordSize, L"MissShaderTable");
			missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderRecordSize));
			m = missShaderTable.GetResource();
		}

		// Hit group shader table
		{
			const UINT numShaderRecords = totalInstances;
			ShaderTable hitGroupShaderTable(device, numShaderRecords, hitRecordSize(), L"HitGroupShaderTable");

			UINT offset = 0;
			for (size_t i = 0; i < res->blases.size(); ++i)
			{
				for (size_t j = 0; j < res->blases[i].instances; ++j)
				{
					HitArg args;
					args.offset = offset;
					args.vertexBuffer = res->blases[i].vbs[j];

					hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &args, sizeof(HitArg)));

					offset++;
				}
			}

			h = hitGroupShaderTable.GetResource();
		}
	};

	CreateShaderBindingTable(res->dxrPrimaryStateObject, res->rayGenShaderTable, res->hitGroupShaderTable, res->missShaderTable);
	CreateShaderBindingTable(res->dxrSecondaryStateObject, res->rayGenShaderTable_secondary, res->hitGroupShaderTable_secondary, res->missShaderTable_secondary);

	engine::MainWindow* window = core_->GetWindow();

	int w, h;
	window->GetClientSize(w, h);
	width = w;
	height = h;
	CreateRaytracingOutputResource(w, h);

	engine::GetCoreRenderer()->CreateStructuredBuffer(res->rayInfoBuffer.getAdressOf(), L"Ray info",
		sizeof(engine::Shaders::RayInfo), w * h, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateStructuredBuffer(res->regroupedIndexesBuffer.getAdressOf(), L"Regrouped indexes for secondary rays",
		sizeof(uint32_t), w * h, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateStructuredBuffer(res->iterationRGBA32f.getAdressOf(), L"iterationRGBA32f",
		sizeof(float[4]), w * h, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateStructuredBuffer(res->hitCointerBuffer.getAdressOf(), L"Hits count",
		sizeof(uint32_t), 1, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateConstantBuffer(res->cameraBuffer.getAdressOf(), L"camera constant buffer",
		sizeof(engine::Shaders::Camera), false);

	// Clear RayInfo
	{
		// Shader
		auto v = CompileShader(L"rt_clear_rtinfo.hlsl", true);

		// Root signature
		{
			CD3DX12_ROOT_PARAMETER rootParameters[ClearRayInfoRootSignatureParams::Count];
			rootParameters[ClearRayInfoRootSignatureParams::RayInfoBuffer].InitAsUnorderedAccessView(0);
			rootParameters[ClearRayInfoRootSignatureParams::IterationColorBuffer].InitAsUnorderedAccessView(1);
			rootParameters[ClearRayInfoRootSignatureParams::Constant].InitAsConstants(1, 0);
		
			CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
			SerializeAndCreateRaytracingRootSignature(device, desc, &res->clearRayInfoRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = res->clearRayInfoRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(res->clearRayInfoPSO.GetAddressOf())));
	}

	// Regroup RayInfo
	{
		// Shader
		auto v = CompileShader(L"rt_regroup_rays.hlsl", true);

		// Root signature
		{
			CD3DX12_ROOT_PARAMETER rootParameters[RegroupRayInfoRootSignatureParams::Count];
			rootParameters[RegroupRayInfoRootSignatureParams::RayInfoIn].InitAsShaderResourceView(0);
			rootParameters[RegroupRayInfoRootSignatureParams::ConstantIn].InitAsConstants(1, 0);
			rootParameters[RegroupRayInfoRootSignatureParams::HitCounterOut].InitAsUnorderedAccessView(0);
			rootParameters[RegroupRayInfoRootSignatureParams::RegroupedIndexesOut].InitAsUnorderedAccessView(1);

			CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
			SerializeAndCreateRaytracingRootSignature(device, desc, &res->regroupRayInfoRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = res->regroupRayInfoRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(res->regroupRayInfoPSO.GetAddressOf())));
	}

	// Clear hit counter
	{
		// Shader
		auto v = CompileShader(L"rt_clear_hits.hlsl", true);

		// Root signature
		{
			CD3DX12_ROOT_PARAMETER rootParameters[1];
			rootParameters[0].InitAsUnorderedAccessView(0);

			CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
			SerializeAndCreateRaytracingRootSignature(device, desc, &res->clearHitCounterRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = res->clearHitCounterRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(res->clearHitCounterPSO.GetAddressOf())));
	}

	// Accumulation
	{
		// Shader
		auto v = CompileShader(L"rt_accumulate.hlsl", true);

		// Root signature
		{
			CD3DX12_ROOT_PARAMETER rootParameters[AccumulationRootSignatureParams::Count];
			rootParameters[AccumulationRootSignatureParams::ConstantIn].InitAsConstants(2, 0);
			rootParameters[AccumulationRootSignatureParams::LightingIn].InitAsShaderResourceView(0);

			CD3DX12_DESCRIPTOR_RANGE UAVDescriptor;
			UAVDescriptor.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
			rootParameters[AccumulationRootSignatureParams::ResultBufferInOut].InitAsDescriptorTable(1, &UAVDescriptor);

			CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
			SerializeAndCreateRaytracingRootSignature(device, desc, &res->accumulationRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = res->accumulationRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(res->accumulationPSO.GetAddressOf())));
	}

	// Hits copy
	{
		// Shader
		auto v = CompileShader(L"rt_copy_hits.hlsl", true);

		// Root signature
		{
			CD3DX12_ROOT_PARAMETER rootParameters[2];
			rootParameters[0].InitAsShaderResourceView(0);
			rootParameters[1].InitAsUnorderedAccessView(0);

			CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters);
			SerializeAndCreateRaytracingRootSignature(device, desc, &res->copyrHitCounterRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = res->copyrHitCounterRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(res->copyHitCounterPSO.GetAddressOf())));
	}


	{
		D3D12_INDIRECT_ARGUMENT_DESC arg{};
		arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE::D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;

		D3D12_COMMAND_SIGNATURE_DESC desc{};
		desc.NumArgumentDescs = 1;
		desc.pArgumentDescs = &arg;
		desc.ByteStride = sizeof(D3D12_DISPATCH_RAYS_DESC);

		throwIfFailed(device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(res->dxrSecondaryCommandIndirect.GetAddressOf())));

		D3D12_DISPATCH_RAYS_DESC indirectData;
		indirectData.HitGroupTable.StartAddress = res->hitGroupShaderTable_secondary->GetGPUVirtualAddress();
		indirectData.HitGroupTable.SizeInBytes = res->hitGroupShaderTable_secondary->GetDesc().Width;
		indirectData.HitGroupTable.StrideInBytes = hitRecordSize();
		indirectData.MissShaderTable.StartAddress = res->missShaderTable_secondary->GetGPUVirtualAddress();
		indirectData.MissShaderTable.SizeInBytes = res->missShaderTable_secondary->GetDesc().Width;
		indirectData.MissShaderTable.StrideInBytes = 32;
		indirectData.RayGenerationShaderRecord.StartAddress = res->rayGenShaderTable_secondary->GetGPUVirtualAddress();
		indirectData.RayGenerationShaderRecord.SizeInBytes = res->rayGenShaderTable_secondary->GetDesc().Width;
		indirectData.Width = width * height;
		indirectData.Height = 1;
		indirectData.Depth = 1;

		engine::GetCoreRenderer()->CreateStructuredBuffer(res->indirectBuffer.getAdressOf(), L"D3D12_DISPATCH_RAYS_DESC",
			sizeof(D3D12_DISPATCH_RAYS_DESC), 1, &indirectData, x12::BUFFER_FLAGS::UNORDERED_ACCESS);
	}

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
	throwIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&res->outputRGBA32f)));
	res->outputRGBA32f->SetName(L"raytracingOutput");

	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex = 0;// AllocateDescriptor(&uavDescriptorHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(res->outputRGBA32f.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(res->descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	engine::GetCoreRenderer()->CreateTextureFrom(res->outputRGBA32fcoreWeakPtr.getAdressOf(), L"Raytracing output", res->outputRGBA32f.Get());
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
	core_->Init("", engine::INIT_FLAGS::DIRECTX12_RENDERER);
	cam = engine::GetSceneManager()->CreateCamera();

	engine::MainWindow* window = core_->GetWindow();
	window->AddMessageCallback(messageCallback);
	hwnd = *window->handle();

	core_->Start();

	WaitForGpu();
	FreeUtils();
	::CloseHandle(event);
	delete res;

	core_->Free();
	engine::DestroyCore(core_);

	return 0;
}
