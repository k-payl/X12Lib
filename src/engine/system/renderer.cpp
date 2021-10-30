#include "renderer.h"
#include "core.h"
#include "camera.h"
#include "model.h"
#include "light.h"
#include "material.h"
#include "gameobject.h"
#include "scenemanager.h"
#include "resourcemanager.h"
#include "materialmanager.h"
#include "mesh.h"
#include "mainwindow.h"
#include "cpp_hlsl_shared.h"
#include "d3d12/dx12buffer.h"
#include "d3d12/dx12texture.h"

using namespace engine;

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

static const wchar_t* raygenname = L"RayGen";
static const wchar_t* hitGroupName = L"HitGroup";
static const wchar_t* hitname = L"ClosestHit";
static const wchar_t* missname = L"Miss";
static const wchar_t* shadowHitGroupName = L"ShadowHitGroup";
static const wchar_t* shadowhitname = L"ShadowClosestHit";
static const wchar_t* shadowmissname = L"ShadowMiss";

#pragma pack(push, 1)
struct HitArg
{
	uint32_t offset;
	float _padding[3];
	D3D12_GPU_VIRTUAL_ADDRESS vertexBuffer;
};
#pragma pack(pop)

static UINT hitRecordSize()
{
	return engine::Align(D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(HitArg), D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
}

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
		BlueNoiseTexture,
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

void engine::Renderer::BindRaytracingResources()
{
	// Bind root signature
	dxrCommandList->SetComputeRootSignature(raytracingGlobalRootSignature.Get());

	// Bind global resources
	dxrCommandList->SetComputeRoot32BitConstant(GlobalRootSignatureParams::FrameConstantBuffer, frame, 0);

	x12::Dx12CoreBuffer* dx12buffer = static_cast<x12::Dx12CoreBuffer*>(cameraBuffer.get());
	dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::CameraConstantBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(iterationRGBA32f.get());
	dxrCommandList->SetComputeRootUnorderedAccessView(GlobalRootSignatureParams::OutputView, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rayInfoBuffer.get());
	dxrCommandList->SetComputeRootUnorderedAccessView(GlobalRootSignatureParams::RayInfo, dx12buffer->GPUAddress());

	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructure, rtxScene->topLevelAccelerationStructure->GetGPUVirtualAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtxScene->sceneBuffer.get());
	dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstants, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(engine::GetSceneManager()->LightsBuffer());
	if (dx12buffer)
		dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::LightStructuredBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtxScene->perInstanceBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::TLASPerInstanceBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtxScene->materialsBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Materials, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(regroupedIndexesBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::RegroupedIndexes, dx12buffer->GPUAddress());

	dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::BlueNoiseTexture, blueNoiseHandle);

	if (texturesHandle.ptr)
		dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::Textures, texturesHandle);
}

void engine::Renderer::CreateBuffers(UINT w, UINT h)
{
	GetCoreRenderer()->CreateBuffer(rayInfoBuffer.getAdressOf(), L"Ray info",
		sizeof(Shaders::RayInfo), x12::BUFFER_FLAGS::UNORDERED_ACCESS_VIEW,
		x12::MEMORY_TYPE::GPU_READ,
		nullptr, w * h);

	GetCoreRenderer()->CreateBuffer(regroupedIndexesBuffer.getAdressOf(),
		L"Regrouped indexes for secondary rays",
		sizeof(uint32_t), x12::BUFFER_FLAGS::UNORDERED_ACCESS_VIEW,
		x12::MEMORY_TYPE::GPU_READ,
		nullptr, w * h);

	GetCoreRenderer()->CreateBuffer(iterationRGBA32f.getAdressOf(),
		L"iterationRGBA32f",
		sizeof(float[4]), x12::BUFFER_FLAGS::UNORDERED_ACCESS_VIEW,
		x12::MEMORY_TYPE::GPU_READ,
		nullptr, w * h);

	GetCoreRenderer()->CreateBuffer(hitCointerBuffer.getAdressOf(),
		L"Hits count",
		sizeof(uint32_t), x12::BUFFER_FLAGS::UNORDERED_ACCESS_VIEW,
		x12::MEMORY_TYPE::GPU_READ,
		nullptr, 1);

	GetCoreRenderer()->CreateBuffer(cameraBuffer.getAdressOf(),
		L"camera constant buffer",
		sizeof(Shaders::Camera), x12::BUFFER_FLAGS::CONSTANT_BUFFER_VIEW,
		x12::MEMORY_TYPE::CPU,
		nullptr);
}

void engine::Renderer::CreateIndirectBuffer(UINT w, UINT h)
{
	D3D12_INDIRECT_ARGUMENT_DESC arg{};
	arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE::D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;

	D3D12_COMMAND_SIGNATURE_DESC desc{};
	desc.NumArgumentDescs = 1;
	desc.pArgumentDescs = &arg;
	desc.ByteStride = sizeof(D3D12_DISPATCH_RAYS_DESC);

	throwIfFailed(dxrDevice->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(rtxScene->dxrSecondaryCommandIndirect.GetAddressOf())));

	D3D12_DISPATCH_RAYS_DESC indirectData;
	indirectData.HitGroupTable.StartAddress = rtxScene->hitGroupShaderTable_secondary->GetGPUVirtualAddress();
	indirectData.HitGroupTable.SizeInBytes = rtxScene->hitGroupShaderTable_secondary->GetDesc().Width;
	indirectData.HitGroupTable.StrideInBytes = hitRecordSize();
	indirectData.MissShaderTable.StartAddress = rtxScene->missShaderTable_secondary->GetGPUVirtualAddress();
	indirectData.MissShaderTable.SizeInBytes = rtxScene->missShaderTable_secondary->GetDesc().Width;
	indirectData.MissShaderTable.StrideInBytes = 32;
	indirectData.RayGenerationShaderRecord.StartAddress = rtxScene->rayGenShaderTable_secondary->GetGPUVirtualAddress();
	indirectData.RayGenerationShaderRecord.SizeInBytes = rtxScene->rayGenShaderTable_secondary->GetDesc().Width;
	indirectData.Width = w * h;
	indirectData.Height = 1;
	indirectData.Depth = 1;

	GetCoreRenderer()->CreateBuffer(rtxScene->indirectArgBuffer.getAdressOf(),
		L"D3D12_DISPATCH_RAYS_DESC",
		sizeof(D3D12_DISPATCH_RAYS_DESC), x12::BUFFER_FLAGS::UNORDERED_ACCESS_VIEW,
		x12::MEMORY_TYPE::GPU_READ,
		&indirectData, 1);
}

void engine::Renderer::Resize(UINT w, UINT h)
{
	GetCoreRenderer()->WaitGPUAll();
	WaitForGpuAll();
	width = w; height = h;
	CreateRaytracingOutputResource(width, height);
	CreateBuffers(width, height);
	CreateIndirectBuffer(width, height);
	needClearBackBuffer = true;
	backBufferIndex = 0;
}

void engine::Renderer::RenderFrame(const ViewportData& viewport, const CameraData& camera)
{
	if (!rtxScene || rtxScene->totalInstances == 0)
	{
		x12::ICoreRenderer* renderer = GetCoreRenderer();
		x12::surface_ptr surface = renderer->GetWindowSurface(*viewport.hwnd);
		x12::ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();
		
		cmdList->CommandsBegin();
		cmdList->BindSurface(surface);
		cmdList->Clear();
		cmdList->CommandsEnd();
		renderer->ExecuteCommandList(cmdList);
	}
	else
	{
		ICoreRenderer* renderer = GetCoreRenderer();
		ID3D12CommandQueue* graphicQueue = reinterpret_cast<ID3D12CommandQueue*>(renderer->GetNativeGraphicQueue());

		renderer->WaitGPU();
		WaitForGpu(backBufferIndex);

		throwIfFailed(commandAllocators[backBufferIndex]->Reset());
		throwIfFailed(dxrCommandList->Reset(commandAllocators[backBufferIndex].Get(), nullptr));

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

			if (memcmp(&cameraTransform, &ViewInvMat_, sizeof(ViewInvMat_)) != 0 ||
				cameraData_width != width ||
				cameraData_height != height)
			{
				cameraTransform = ViewInvMat_;

				Shaders::Camera cameraData;
				memcpy(&cameraData.forward.x, &forwardWS, sizeof(vec3));
				memcpy(&cameraData.right.x, &rightWS, sizeof(vec3));
				memcpy(&cameraData.up.x, &upWS, sizeof(vec3));
				memcpy(&cameraData.origin.x, &origin, sizeof(vec4));
				cameraData.width = width;
				cameraData.height = height;
				cameraData_height = height;
				cameraData_width = width;

				cameraBuffer->SetData(&cameraData, sizeof(cameraData));

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
			cpso.shader = clearShader.get()->GetCoreShader();
			cmdList->SetComputePipelineState(cpso);

			if (!clearResources)
			{
				renderer->CreateResourceSet(clearResources.getAdressOf(), clearShader.get()->GetCoreShader());
				clearResources->BindTextueSRV("tex", outputRGBA32fcoreWeakPtr.get());
				cmdList->CompileSet(clearResources.get());
				cameraBufferIndex = clearResources->FindInlineBufferIndex("CameraBuffer");
			}

			cmdList->BindResourceSet(clearResources.get());

			uint32_t sizes[2] = { width, height };
			cmdList->UpdateInlineConstantBuffer(cameraBufferIndex, &sizes, 8);

			{
				constexpr int warpSize = 16;
				int numGroupsX = (width + (warpSize - 1)) / warpSize;
				int numGroupsY = (height + (warpSize - 1)) / warpSize;
				cmdList->Dispatch(numGroupsX, numGroupsY);
			}

			{
				ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();
				D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(outputTexture.Get()) };
				d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
			}

			cmdList->CommandsEnd();
			renderer->ExecuteCommandList(cmdList);

			// Sync clear with RT
			Sync(graphicQueue, rtxQueue.Get(), clearToRTFence);

			needClearBackBuffer = false;
			frame = 0;
		}

		// Raytracing
		{
			// Bind heap
			dxrCommandList->SetDescriptorHeaps(1, descriptorHeap.GetAddressOf());

			// Clear buffers before RT
			{
				dxrCommandList->SetPipelineState(clearRayInfoPSO.Get());

				dxrCommandList->SetComputeRootSignature(clearRayInfoRootSignature.Get());

				auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rayInfoBuffer.get());
				dxrCommandList->SetComputeRootUnorderedAccessView(ClearRayInfoRootSignatureParams::RayInfoBuffer, dx12buffer->GPUAddress());

				dx12buffer = static_cast<x12::Dx12CoreBuffer*>(iterationRGBA32f.get());
				dxrCommandList->SetComputeRootUnorderedAccessView(ClearRayInfoRootSignatureParams::IterationColorBuffer, dx12buffer->GPUAddress());

				dxrCommandList->SetComputeRoot32BitConstant(ClearRayInfoRootSignatureParams::Constant, width * height, 0);

				constexpr int warpSize = 256;
				int numGroupsX = (width * height + (warpSize - 1)) / warpSize;
				dxrCommandList->Dispatch(numGroupsX, 1, 1);

				D3D12_RESOURCE_BARRIER preCopyBarriers[] =
				{
					CD3DX12_RESOURCE_BARRIER::UAV(static_cast<x12::Dx12CoreBuffer*>(rayInfoBuffer.get())->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(static_cast<x12::Dx12CoreBuffer*>(iterationRGBA32f.get())->GetResource()),
				};
				dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
			}

			// Primary rays
			{
				// Bind pipeline
				dxrCommandList->SetPipelineState1(dxrPrimaryStateObject.Get());

				BindRaytracingResources();

				// Do raytracing
				{
					D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
					dispatchDesc.HitGroupTable.StartAddress = rtxScene->hitGroupShaderTable->GetGPUVirtualAddress();
					dispatchDesc.HitGroupTable.SizeInBytes = rtxScene->hitGroupShaderTable->GetDesc().Width;
					dispatchDesc.HitGroupTable.StrideInBytes = hitRecordSize();
					dispatchDesc.MissShaderTable.StartAddress = rtxScene->missShaderTable->GetGPUVirtualAddress();
					dispatchDesc.MissShaderTable.SizeInBytes = rtxScene->missShaderTable->GetDesc().Width;
					dispatchDesc.MissShaderTable.StrideInBytes = 32;
					dispatchDesc.RayGenerationShaderRecord.StartAddress = rtxScene->rayGenShaderTable->GetGPUVirtualAddress();
					dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rtxScene->rayGenShaderTable->GetDesc().Width;
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
						dxrCommandList->SetPipelineState(clearHitCounterPSO.Get());

						dxrCommandList->SetComputeRootSignature(clearHitCounterRootSignature.Get());

						auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(hitCointerBuffer.get());
						dxrCommandList->SetComputeRootUnorderedAccessView(0, dx12buffer->GPUAddress());

						dxrCommandList->Dispatch(1, 1, 1);

						// hitCointerBuffer Write->Read
						{
							auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(hitCointerBuffer.get());
							D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
							dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
						}
					}

					// Hit rays sorting
					{
						// RayInfo Write->Read
						{
							auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rayInfoBuffer.get());
							D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
							dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
						}

						dxrCommandList->SetPipelineState(regroupRayInfoPSO.Get());

						dxrCommandList->SetComputeRootSignature(regroupRayInfoRootSignature.Get());

						dxrCommandList->SetComputeRoot32BitConstant(RegroupRayInfoRootSignatureParams::ConstantIn, width * height, 0);

						auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rayInfoBuffer.get());
						dxrCommandList->SetComputeRootShaderResourceView(RegroupRayInfoRootSignatureParams::RayInfoIn, dx12buffer->GPUAddress());

						dx12buffer = static_cast<x12::Dx12CoreBuffer*>(hitCointerBuffer.get());
						dxrCommandList->SetComputeRootUnorderedAccessView(RegroupRayInfoRootSignatureParams::HitCounterOut, dx12buffer->GPUAddress());

						dx12buffer = static_cast<x12::Dx12CoreBuffer*>(regroupedIndexesBuffer.get());
						dxrCommandList->SetComputeRootUnorderedAccessView(RegroupRayInfoRootSignatureParams::RegroupedIndexesOut, dx12buffer->GPUAddress());

						constexpr int warpSize = 256;
						int numGroupsX = (width * height + (warpSize - 1)) / warpSize;
						dxrCommandList->Dispatch(numGroupsX, 1, 1);

						// RegroupedIndexes, Hit count Write->Read
						{
							auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(regroupedIndexesBuffer.get());
							auto dx12buffer1 = static_cast<x12::Dx12CoreBuffer*>(hitCointerBuffer.get());
							D3D12_RESOURCE_BARRIER preCopyBarriers[2] =
							{
								CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()),
								CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer1->GetResource()),
							};
							dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
						}

						//std::vector<uint32_t> d(width * height);
						//rtxScene->regroupedIndexesBuffer->GetData(&d[0]);
						//std::sort(d.begin(), d.end());

						//uint32_t d1;
						//rtxScene->hitCointerBuffer->GetData(&d1);
						//int y = 0;
					}

					// Copy hits to indirect arguments buffer
					{
						dxrCommandList->SetPipelineState(copyHitCounterPSO.Get());

						dxrCommandList->SetComputeRootSignature(copyrHitCounterRootSignature.Get());

						auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(hitCointerBuffer.get());
						dxrCommandList->SetComputeRootShaderResourceView(0, dx12buffer->GPUAddress());

						dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtxScene->indirectArgBuffer.get());
						dxrCommandList->SetComputeRootUnorderedAccessView(1, dx12buffer->GPUAddress());

						dxrCommandList->Dispatch(1, 1, 1);

						// indirectArgBuffer Write->Read
						{
							auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtxScene->indirectArgBuffer.get());
							D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
							dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
						}
					}

					{
						// Bind pipeline
						dxrCommandList->SetPipelineState1(dxrSecondaryStateObject.Get());

						BindRaytracingResources();

						auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtxScene->indirectArgBuffer.get());
						dxrCommandList->ExecuteIndirect(rtxScene->dxrSecondaryCommandIndirect.Get(), 1, dx12buffer->GetResource(), 0, nullptr, 0);
					}
				}

				{
					auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(iterationRGBA32f.get());
					D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
					dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
				}

				// Final accumulation
				{
					dxrCommandList->SetPipelineState(accumulationPSO.Get());

					dxrCommandList->SetComputeRootSignature(accumulationRootSignature.Get());

					uint32_t consts[2] = { width * height, width };
					dxrCommandList->SetComputeRoot32BitConstants(AccumulationRootSignatureParams::ConstantIn, 2, consts, 0);

					dxrCommandList->SetComputeRootDescriptorTable(AccumulationRootSignatureParams::ResultBufferInOut, raytracingOutputResourceUAVGpuDescriptor);

					auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(iterationRGBA32f.get());
					dxrCommandList->SetComputeRootShaderResourceView(AccumulationRootSignatureParams::LightingIn, dx12buffer->GPUAddress());

					constexpr int warpSize = 256;
					int numGroupsX = (width * height + (warpSize - 1)) / warpSize;
					dxrCommandList->Dispatch(numGroupsX, 1, 1);
				}

				throwIfFailed(dxrCommandList->Close());
				ID3D12CommandList* commandLists[] = { dxrCommandList.Get() };
				rtxQueue->ExecuteCommandLists(ARRAYSIZE(commandLists), commandLists);
		}

		// Sync RT with swapchain copy
		Sync(rtxQueue.Get(), graphicQueue, rtToSwapchainFence);

		// swapchain copy
		{
			surface_ptr surface = renderer->GetWindowSurface(hwnd);

			ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();
			cmdList->CommandsBegin();
			cmdList->BindSurface(surface);
			cmdList->SetViewport(width, height);
			cmdList->SetScissor(0, 0, width, height);

			GraphicPipelineState pso{};
			pso.shader = copyShader.get()->GetCoreShader();
			pso.vb = plane.get();
			pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
			cmdList->SetGraphicPipelineState(pso);

			cmdList->SetVertexBuffer(plane.get());

			{
				ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();

				D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
				};

				d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
			}

			if (!copyResourceSet)
			{
				renderer->CreateResourceSet(copyResourceSet.getAdressOf(), copyShader.get()->GetCoreShader());
				copyResourceSet->BindTextueSRV("texture_", outputRGBA32fcoreWeakPtr.get());
				cmdList->CompileSet(copyResourceSet.get());
			}

			cmdList->BindResourceSet(copyResourceSet.get());

			cmdList->Draw(plane.get());

			{
				ID3D12GraphicsCommandList* d3dCmdList = (ID3D12GraphicsCommandList*)cmdList->GetNativeResource();

				D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::Transition(outputTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				};

				d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
			}

			cmdList->CommandsEnd();
			renderer->ExecuteCommandList(cmdList);
		}

		backBufferIndex = (backBufferIndex + 1) % DeferredBuffers;
		frame++;
	}
}

void engine::Renderer::InitFence(Renderer::Fence& fence)
{
	for (int i = 0; i < DeferredBuffers; i++)
	{
		throwIfFailed(dxrDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.fence[i])));
	}
	fence.fenceValues[0]++;
}

void engine::Renderer::SignalFence(ID3D12CommandQueue* ToQueue, Renderer::Fence& fence)
{
	ToQueue->Signal(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	fence.fenceValues[backBufferIndex]++;
}

void engine::Renderer::Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, Renderer::Fence& fence)
{
	ToQueue->Signal(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	ToQueue->Wait(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	fence.fenceValues[backBufferIndex]++;
}

void engine::Renderer::WaitForGpuCurrent()
{
	WaitForGpu(backBufferIndex);
}

void engine::Renderer::WaitForGpu(UINT index)
{
	// Schedule a Signal command in the GPU queue.
	UINT64 fenceValue = frameEndFence.fenceValues[index];
	if (SUCCEEDED(rtxQueue->Signal(frameEndFence.fence[index].Get(), fenceValue)))
	{
		// Wait until the Signal has been processed.
		if (SUCCEEDED(frameEndFence.fence[index]->SetEventOnCompletion(fenceValue, event)))
		{
			WaitForSingleObjectEx(event, INFINITE, FALSE);

			// Increment the fence value for the current frame.
			frameEndFence.fenceValues[index]++;
		}
	}
}

void engine::Renderer::WaitForGpuAll()
{
	for (UINT i = 0; i < DeferredBuffers; i++)
	{
		WaitForGpu(i);
	}
}

// Build Shader Tables
void engine::Renderer::CreateShaderBindingTable(ComPtr<ID3D12StateObject> state, UINT totalInstances, ComPtr<ID3D12Resource>& r, ComPtr<ID3D12Resource>& h, ComPtr<ID3D12Resource>& m)
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

		ShaderTable rayGenShaderTable(dxrDevice.Get(), numShaderRecords, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, L"RayGenShaderTable");
		rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES));
		r = rayGenShaderTable.GetResource();
	}

	// Miss shader table
	{
		UINT numShaderRecords = 1;

		UINT shaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
		ShaderTable missShaderTable(dxrDevice.Get(), numShaderRecords, shaderRecordSize, L"MissShaderTable");
		missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderRecordSize));
		m = missShaderTable.GetResource();
	}

	// Hit group shader table
	{
		const UINT numShaderRecords = totalInstances;
		ShaderTable hitGroupShaderTable(dxrDevice.Get(), numShaderRecords, hitRecordSize(), L"HitGroupShaderTable");

		UINT offset = 0;
		for (size_t i = 0; i < rtxScene->blases.size(); ++i)
		{
			for (size_t j = 0; j < rtxScene->blases[i].instances; ++j)
			{
				HitArg args;
				args.offset = offset;
				args.vertexBuffer = rtxScene->blases[i].vbs[j];

				hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &args, sizeof(HitArg)));

				offset++;
			}
		}

		h = hitGroupShaderTable.GetResource();
	}
};

void engine::Renderer::Free()
{
	rtxScene = nullptr;
	WaitForGpu(backBufferIndex);
	FreeUtils();
	::CloseHandle(event);
}

void engine::Renderer::OnSceneChanged()
{
	rtxScene = std::make_unique<Renderer::RTXscene>();

	cam = GetSceneManager()->CreateCamera();

	needClearBackBuffer = true;

	std::vector<Model*> models;
	GetSceneManager()->GetObjectsOfType(models, OBJECT_TYPE::MODEL);

	std::vector<Light*> areaLights;
	GetSceneManager()->GetAreaLights(areaLights);

	// BLASes
	{
		struct BLASmesh
		{
			Mesh* mesh;
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			math::mat4 transform;
			Material* material;
		};

		std::vector<BLASmesh> ms;

		for (size_t i = 0; i < models.size(); ++i)
		{
			Mesh* m = models[i]->GetMesh();
			ms.push_back({ m, nullptr, models[i]->GetWorldTransform(), models[i]->GetMaterial() });
		}

		std::sort(ms.begin(), ms.end(), [](const BLASmesh& l, const BLASmesh& r) -> bool { return l.transform < r.transform; });

		// Parallel arrays
		std::vector<Mesh*> blas_meshes;
		std::vector<Material*> blas_materials;
		std::vector<D3D12_GPU_VIRTUAL_ADDRESS> blas_meshesh_vb;
		std::vector<math::vec3> emisssions;

		blas_meshes.push_back(ms[0].mesh);
		blas_meshesh_vb.push_back(reinterpret_cast<x12::Dx12CoreBuffer*>(ms[0].mesh->VertexBuffer())->GPUAddress());
		blas_materials.push_back(ms[0].material ? ms[0].material : GetMaterialManager()->GetDefaultMaterial());
		emisssions.push_back(vec3());

		assert(ms.size() > 0);

		math::mat4 t = ms[0].transform;

		for (int i = 0; i < ms.size(); i++)
		{
			if (i == ms.size() - 1 || ms[i + 1].transform < t || t < ms[i + 1].transform)
			{
				// Create BLAS
				auto resource = BuildBLAS(blas_meshes, dxrDevice.Get(), rtxQueue.Get(), dxrCommandList.Get(), commandAllocators[backBufferIndex].Get());

				rtxScene->blases.push_back({ ms[i].transform, (UINT)blas_meshes.size(), resource, blas_meshesh_vb, blas_materials, emisssions });
				rtxScene->totalInstances += (UINT)blas_meshes.size();

				blas_meshes.clear();
				blas_meshesh_vb.clear();
				blas_materials.clear();
				emisssions.clear();
			}

			if (i < ms.size() - 1)
			{
				Mesh* mesh = ms[i + 1].mesh;

				blas_meshes.push_back(mesh);
				blas_meshesh_vb.push_back(reinterpret_cast<x12::Dx12CoreBuffer*>(mesh->VertexBuffer())->GPUAddress());
				blas_materials.push_back(ms[i + 1].material ? ms[i + 1].material : GetMaterialManager()->GetDefaultMaterial());
				emisssions.push_back(vec3());
			}
		}
	}

	for (int i = 0; i < areaLights.size(); i++)
	{
		engine::Mesh* m = planeMesh.get();
		std::vector<Mesh*> blas_meshes = {m};
		std::vector<Material*> blas_materials = { GetMaterialManager()->GetDefaultMaterial() };
		std::vector<D3D12_GPU_VIRTUAL_ADDRESS> blas_meshesh_vb = { reinterpret_cast<x12::Dx12CoreBuffer*>(m->VertexBuffer())->GPUAddress() };
		std::vector<math::vec3> emisssions{vec3(areaLights[i]->GetIntensity())};

		// Create BLAS
		auto resource = BuildBLAS(blas_meshes, dxrDevice.Get(), rtxQueue.Get(), dxrCommandList.Get(), commandAllocators[backBufferIndex].Get());

		rtxScene->blases.push_back({ areaLights[i]->GetWorldTransform(), (UINT)blas_meshes.size(), resource, blas_meshesh_vb, blas_materials, emisssions });
		rtxScene->totalInstances += 1;
	}

	// TLAS
	rtxScene->topLevelAccelerationStructure = BuildTLAS(rtxScene->blases, dxrDevice.Get(),
		rtxQueue.Get(), dxrCommandList.Get(), commandAllocators[backBufferIndex].Get());

	// Materials
	{
		std::vector<Texture*> sceneTextures;
		std::unordered_map<Texture*, UINT> textureToIndex;

		std::vector<Material*> sceneMaterial;
		std::vector<Shaders::Material> sceneMaterialGPU;
		std::unordered_map<Material*, UINT> materialToIndex;

		for (size_t i = 0; i < rtxScene->blases.size(); ++i)
		{
			for (size_t j = 0; j < rtxScene->blases[i].instances; ++j)
			{
				Material* mat = rtxScene->blases[i].materials[j];

				auto it = materialToIndex.find(mat);
				if (it == materialToIndex.end())
				{
					materialToIndex[mat] = sceneMaterial.size();
					sceneMaterial.push_back(mat);

					Shaders::Material gpuMaterial{};
					gpuMaterial.albedo = mat->GetValue(Material::Params::Albedo);
					gpuMaterial.shading.x = mat->GetValue(Material::Params::Roughness).x;
					gpuMaterial.shading.y = mat->GetValue(Material::Params::Metalness).x;

					Texture* texture = mat->GetTexture(Material::Params::Albedo);

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

		GetCoreRenderer()->CreateBuffer(rtxScene->materialsBuffer.getAdressOf(),
			L"Materials",
			sizeof(Shaders::Material), x12::BUFFER_FLAGS::SHADER_RESOURCE_VIEW,
			x12::MEMORY_TYPE::GPU_READ,
			&sceneMaterialGPU[0], sceneMaterialGPU.size());

		// Textures
		{
			// blue noise 2d array (8 layers)
			blueNoiseHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, descriptorSize);
			auto dx12tex = static_cast<x12::Dx12CoreTexture*>(blueNoise.get()->GetCoreTexture());
			D3D12_CPU_DESCRIPTOR_HANDLE destCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, descriptorSize);
			dxrDevice->CopyDescriptorsSimple(1, destCPU, dx12tex->GetSRV(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

			texturesHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1 + 1, descriptorSize);

			for (int i = 0; i < sceneTextures.size(); i++)
			{
				auto coreTexture = sceneTextures[i]->GetCoreTexture();
				Dx12CoreTexture* dx12CoreTexure = static_cast<x12::Dx12CoreTexture*>(coreTexture);
				D3D12_CPU_DESCRIPTOR_HANDLE cputextureHandle = dx12CoreTexure->GetSRV();
				D3D12_CPU_DESCRIPTOR_HANDLE destCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1 + 1 + i, descriptorSize);

				dxrDevice->CopyDescriptorsSimple(1, destCPU, cputextureHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}
		}

		// Instances Data
		std::vector<Shaders::InstanceData> instancesData(rtxScene->totalInstances);

		UINT offset = 0;
		for (size_t i = 0; i < rtxScene->blases.size(); ++i)
		{
			for (size_t j = 0; j < rtxScene->blases[i].instances; ++j)
			{
				math::mat4 transform = rtxScene->blases[i].transform;
				vec3 pos, scale;
				quat rot;
				math::decompositeTransform(transform, pos, rot, scale);
				//transform = transform * mat4(1 / scale.x, 1 / scale.y, 1 / scale.z);
				instancesData[offset].transform = transform;
				instancesData[offset].normalTransform = transform.Inverse().Transpose();
				math::compositeTransform(transform, vec3(0, 0, 0), rot, vec3(1, 1, 1));

				instancesData[offset].emission = rtxScene->blases[i].emissions[j].x;
				instancesData[offset].materialIndex = materialToIndex[rtxScene->blases[i].materials[j]];
				offset++;
			}
		}

		GetCoreRenderer()->CreateBuffer(rtxScene->perInstanceBuffer.getAdressOf(),
			L"TLAS per-instance data",
			sizeof(Shaders::InstanceData), 
			x12::BUFFER_FLAGS::SHADER_RESOURCE_VIEW, x12::MEMORY_TYPE::GPU_READ,
			&instancesData[0], instancesData.size());
	}

	// Scene buffer
	{
		Shaders::Scene scene;
		scene.instanceCount = rtxScene->totalInstances;
		scene.lightCount = (uint32_t)areaLights.size();

		GetCoreRenderer()->CreateBuffer(rtxScene->sceneBuffer.getAdressOf(),
			L"Scene constant buffer",
			sizeof(Shaders::Scene),
			x12::BUFFER_FLAGS::CONSTANT_BUFFER_VIEW, x12::MEMORY_TYPE::GPU_READ,
			&scene, 1);
	}

	CreateShaderBindingTable(dxrPrimaryStateObject, rtxScene->totalInstances, rtxScene->rayGenShaderTable, rtxScene->hitGroupShaderTable, rtxScene->missShaderTable);
	CreateShaderBindingTable(dxrSecondaryStateObject, rtxScene->totalInstances, rtxScene->rayGenShaderTable_secondary, rtxScene->hitGroupShaderTable_secondary, rtxScene->missShaderTable_secondary);

	CreateIndirectBuffer(width, height);
}

void engine::Renderer::OnSceneLoaded()
{
	GetRenderer()->OnSceneChanged();
}

ComPtr<ID3D12StateObject> engine::Renderer::CreatePSO(ComPtr<IDxcBlob> r, ComPtr<IDxcBlob> h, ComPtr<IDxcBlob> m)
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
	CreateRaygenLocalSignatureSubobject(&raytracingPipeline, hitGroupName, raytracingLocalRootSignature.Get());
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.

	// Global root signature
	// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	globalRootSignature->SetRootSignature(raytracingGlobalRootSignature.Get());

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
	throwIfFailed(dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&RTXPSO)));

	return RTXPSO;
};

void engine::Renderer::Init()
{
	GetSceneManager()->AddCallbackSceneLoaded(OnSceneLoaded);

	MainWindow* window = core__->GetWindow();
	hwnd = *window->handle();

	planeMesh = GetResourceManager()->CreateStreamMesh("std#plane");

	{
		auto device = (ID3D12Device*)GetCoreRenderer()->GetNativeDevice();
		throwIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxrDevice)));
	}
	
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 opt5{};
	throwIfFailed(dxrDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &opt5, sizeof(opt5)));

	bool tier11 = opt5.RaytracingTier >= D3D12_RAYTRACING_TIER::D3D12_RAYTRACING_TIER_1_1;
	if (opt5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
		abort();

	// Create the command queue.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ComPtr<ID3D12CommandQueue> commandQueue;
	throwIfFailed(dxrDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
	rtxQueue = commandQueue.Get();

	// Create a command allocator for each back buffer that will be rendered to.
	for (UINT n = 0; n < DeferredBuffers; n++)
	{
		throwIfFailed(dxrDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
	}

	// Create a command list for recording graphics commands.
	ComPtr<ID3D12GraphicsCommandList> commandList;
	throwIfFailed(dxrDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList)));
	throwIfFailed(commandList->Close());

	InitFence(rtToSwapchainFence);
	InitFence(clearToRTFence);
	InitFence(frameEndFence);

	event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	throwIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxrCommandList)));

	{
		copyShader = GetResourceManager()->CreateGraphicShader("../resources/shaders/copy.hlsl", nullptr, 0);
		copyShader.get();

		const x12::ConstantBuffersDesc buffersdesc[1] =
		{
			"CameraBuffer",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		clearShader = GetResourceManager()->CreateComputeShader("../resources/shaders/clear.hlsl", &buffersdesc[0], 1);
		clearShader.get();
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
		ranges.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6);  // 2d array
		rootParameters[GlobalRootSignatureParams::BlueNoiseTexture].InitAsDescriptorTable(1, &ranges);

		CD3DX12_DESCRIPTOR_RANGE ranges1;
		ranges1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 100, 7);  // array of textures
		rootParameters[GlobalRootSignatureParams::Textures].InitAsDescriptorTable(1, &ranges1);

		D3D12_STATIC_SAMPLER_DESC sampler{};
		sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		sampler.ShaderRegister = 0;
		sampler.RegisterSpace = 0;
		sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 1, &sampler);

		SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &raytracingGlobalRootSignature);
	}

	// Local Root Signature
	{
		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
		rootParameters[LocalRootSignatureParams::InstanceConstants].InitAsConstants(sizeof(Shaders::InstancePointer) / 4, 2, 0);
		rootParameters[LocalRootSignatureParams::VertexBuffer].InitAsShaderResourceView(2, 0);

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &raytracingLocalRootSignature);
	}

	descriptorSize = CreateDescriptorHeap(dxrDevice.Get(), descriptorHeap.GetAddressOf());

	int w, h;
	window->GetClientSize(w, h);
	width = w;
	height = h;
	CreateRaytracingOutputResource(w, h);
	CreateBuffers(w, h);

	CompileRTShaders();

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

		GetCoreRenderer()->CreateVertexBuffer(plane.getAdressOf(), L"plane", planeVert, &desc, nullptr, nullptr, MEMORY_TYPE::GPU_READ);
	}

	blueNoise = engine::GetResourceManager()->CreateStreamTexture("textures/LDR_RGBA_0.dds", x12::TEXTURE_CREATE_FLAGS::USAGE_SHADER_RESOURCE);
	blueNoise.get();

}

void engine::Renderer::CompileRTShaders()
{
	WaitForGpuAll();

	{
		std::vector<std::pair<std::wstring, std::wstring>> defines_primary = { { L"PRIMARY_RAY", L"1" } };

		raygen = CompileShader(L"rt_raygen.hlsl", false, defines_primary);
		hit = CompileShader(L"rt_hit.hlsl", false, defines_primary);
		miss = CompileShader(L"rt_miss.hlsl", false, defines_primary);

		raygenSecondary = CompileShader(L"rt_raygen.hlsl");
		hitSecondary = CompileShader(L"rt_hit.hlsl");
		missSecondary = CompileShader(L"rt_miss.hlsl");
	}

	dxrPrimaryStateObject = CreatePSO(raygen, hit, miss);
	dxrSecondaryStateObject = CreatePSO(raygenSecondary, hitSecondary, missSecondary);

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
			SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &clearRayInfoRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = clearRayInfoRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(dxrDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(clearRayInfoPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &regroupRayInfoRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = regroupRayInfoRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(dxrDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(regroupRayInfoPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &clearHitCounterRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = clearHitCounterRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(dxrDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(clearHitCounterPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &accumulationRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = accumulationRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(dxrDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(accumulationPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(dxrDevice.Get(), desc, &copyrHitCounterRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = copyrHitCounterRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(dxrDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(copyHitCounterPSO.GetAddressOf())));
	}

}

void engine::Renderer::CreateRaytracingOutputResource(UINT width, UINT height)
{
	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	throwIfFailed(dxrDevice.Get()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&outputTexture)));
	outputTexture->SetName(L"raytracingOutput");

	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex = 0;// AllocateDescriptor(&uavDescriptorHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	dxrDevice.Get()->CreateUnorderedAccessView(outputTexture.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	GetCoreRenderer()->CreateTextureFrom(outputRGBA32fcoreWeakPtr.getAdressOf(), L"Raytracing output", outputTexture.Get());

	copyResourceSet = 0;
	clearResources = 0;
}
