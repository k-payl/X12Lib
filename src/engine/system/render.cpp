#include "render.h"
#include "core.h"

using namespace engine;

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

void engine::Render::BindRaytracingResources()
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

	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::AccelerationStructure, rtx->topLevelAccelerationStructure->GetGPUVirtualAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtx->sceneBuffer.get());
	dxrCommandList->SetComputeRootConstantBufferView(GlobalRootSignatureParams::SceneConstants, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(engine::GetSceneManager()->LightsBuffer());
	if (dx12buffer)
		dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::LightStructuredBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtx->perInstanceBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::TLASPerInstanceBuffer, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtx->materialsBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::Materials, dx12buffer->GPUAddress());

	dx12buffer = static_cast<x12::Dx12CoreBuffer*>(regroupedIndexesBuffer.get());
	dxrCommandList->SetComputeRootShaderResourceView(GlobalRootSignatureParams::RegroupedIndexes, dx12buffer->GPUAddress());

	if (texturesHandle.ptr)
		dxrCommandList->SetComputeRootDescriptorTable(GlobalRootSignatureParams::Textures, texturesHandle);

}

void engine::Render::Update()
{
}
void engine::Render::RenderFrame(const ViewportData& viewport, const engine::CameraData& camera)
{
	if (totalInstances == 0)
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
		ICoreRenderer* renderer = engine::GetCoreRenderer();
		ID3D12CommandQueue* graphicQueue = reinterpret_cast<ID3D12CommandQueue*>(renderer->GetNativeGraphicQueue());

		renderer->WaitGPU();
		WaitForGpu();

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
				CameraBuffer = clearResources->FindInlineBufferIndex("CameraBuffer");
			}

			cmdList->BindResourceSet(clearResources.get());

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
				D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(outputRGBA32f.Get()) };
				d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
			}

			cmdList->CommandsEnd();
			renderer->ExecuteCommandList(cmdList);

			// Sync clear with RT
			Sync(graphicQueue, rtxQueue, clearToRTFence);

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
					dispatchDesc.HitGroupTable.StartAddress = rtx->hitGroupShaderTable->GetGPUVirtualAddress();
					dispatchDesc.HitGroupTable.SizeInBytes = rtx->hitGroupShaderTable->GetDesc().Width;
					dispatchDesc.HitGroupTable.StrideInBytes = hitRecordSize();
					dispatchDesc.MissShaderTable.StartAddress = rtx->missShaderTable->GetGPUVirtualAddress();
					dispatchDesc.MissShaderTable.SizeInBytes = rtx->missShaderTable->GetDesc().Width;
					dispatchDesc.MissShaderTable.StrideInBytes = 32;
					dispatchDesc.RayGenerationShaderRecord.StartAddress = rtx->rayGenShaderTable->GetGPUVirtualAddress();
					dispatchDesc.RayGenerationShaderRecord.SizeInBytes = rtx->rayGenShaderTable->GetDesc().Width;
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
						//rtx->regroupedIndexesBuffer->GetData(&d[0]);
						//std::sort(d.begin(), d.end());

						//uint32_t d1;
						//rtx->hitCointerBuffer->GetData(&d1);
						//int y = 0;
					}

					// Copy hits to indirect arguments buffer
					{
						dxrCommandList->SetPipelineState(copyHitCounterPSO.Get());

						dxrCommandList->SetComputeRootSignature(copyrHitCounterRootSignature.Get());

						auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(hitCointerBuffer.get());
						dxrCommandList->SetComputeRootShaderResourceView(0, dx12buffer->GPUAddress());

						dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtx->indirectBuffer.get());
						dxrCommandList->SetComputeRootUnorderedAccessView(1, dx12buffer->GPUAddress());

						dxrCommandList->Dispatch(1, 1, 1);

						// indirectBuffer Write->Read
						{
							auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtx->indirectBuffer.get());
							D3D12_RESOURCE_BARRIER preCopyBarriers[1] = { CD3DX12_RESOURCE_BARRIER::UAV(dx12buffer->GetResource()) };
							dxrCommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
						}
					}

					{
						// Bind pipeline
						dxrCommandList->SetPipelineState1(dxrSecondaryStateObject.Get());

						BindRaytracingResources();

						auto dx12buffer = static_cast<x12::Dx12CoreBuffer*>(rtx->indirectBuffer.get());
						dxrCommandList->ExecuteIndirect(rtx->dxrSecondaryCommandIndirect.Get(), 1, dx12buffer->GetResource(), 0, nullptr, 0);
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
		Sync(rtxQueue, graphicQueue, rtToSwapchainFence);

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
					CD3DX12_RESOURCE_BARRIER::Transition(outputRGBA32f.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
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
					CD3DX12_RESOURCE_BARRIER::Transition(outputRGBA32f.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
				};

				d3dCmdList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);
			}

			cmdList->CommandsEnd();
			renderer->ExecuteCommandList(cmdList);
		}

		backBufferIndex = (backBufferIndex + 1) % engine::DeferredBuffers;
		frame++;
	}
}

void engine::Render::InitFence(engine::Render::Fence& fence)
{
	for (int i = 0; i < engine::DeferredBuffers; i++)
	{
		throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence.fence[i])));
	}
	fence.fenceValues[0]++;
}

void engine::Render::SignalFence(ID3D12CommandQueue* ToQueue, engine::Render::Fence& fence)
{
	ToQueue->Signal(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	fence.fenceValues[backBufferIndex]++;
}

void engine::Render::Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, engine::Render::Fence& fence)
{
	ToQueue->Signal(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	ToQueue->Wait(fence.fence[backBufferIndex].Get(), fence.fenceValues[backBufferIndex]);
	fence.fenceValues[backBufferIndex]++;
}

void engine::Render::WaitForGpu()
{
	// Schedule a Signal command in the GPU queue.
	UINT64 fenceValue = frameEndFence.fenceValues[backBufferIndex];
	if (SUCCEEDED(commandQueue->Signal(frameEndFence.fence[backBufferIndex].Get(), fenceValue)))
	{
		// Wait until the Signal has been processed.
		if (SUCCEEDED(frameEndFence.fence[backBufferIndex]->SetEventOnCompletion(fenceValue, event)))
		{
			WaitForSingleObjectEx(event, INFINITE, FALSE);

			// Increment the fence value for the current frame.
			frameEndFence.fenceValues[backBufferIndex]++;
		}
	}
}

void engine::Render::WaitForGpuAll()
{
	for (int i = 0; i < 3; i++)
	{
		// Schedule a Signal command in the GPU queue.
		UINT64 fenceValue = frameEndFence.fenceValues[i];
		if (SUCCEEDED(commandQueue->Signal(frameEndFence.fence[i].Get(), fenceValue)))
		{
			// Wait until the Signal has been processed.
			if (SUCCEEDED(frameEndFence.fence[i]->SetEventOnCompletion(fenceValue, event)))
			{
				WaitForSingleObjectEx(event, INFINITE, FALSE);

				// Increment the fence value for the current frame.
				frameEndFence.fenceValues[i]++;
			}
		}
	}
}

// Build Shader Tables
void engine::Render::CreateShaderBindingTable(ComPtr<ID3D12StateObject> state, UINT totalInstances, ComPtr<ID3D12Resource>& r, ComPtr<ID3D12Resource>& h, ComPtr<ID3D12Resource>& m)
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
		for (size_t i = 0; i < rtx->blases.size(); ++i)
		{
			for (size_t j = 0; j < rtx->blases[i].instances; ++j)
			{
				HitArg args;
				args.offset = offset;
				args.vertexBuffer = rtx->blases[i].vbs[j];

				hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &args, sizeof(HitArg)));

				offset++;
			}
		}

		h = hitGroupShaderTable.GetResource();
	}
};

void engine::Render::Free()
{
	rtx = nullptr;
	WaitForGpu();
	FreeUtils();
	::CloseHandle(event);
}

void engine::Render::OnLoadScene()
{
	rtx = std::make_unique<engine::Render::RTXscene>();

	cam = engine::GetSceneManager()->CreateCamera();

	std::vector<engine::Model*> models;
	engine::GetSceneManager()->GetObjectsOfType(models, engine::OBJECT_TYPE::MODEL);

	std::vector<engine::Light*> areaLights;
	engine::GetSceneManager()->GetAreaLights(areaLights);

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
			ms.push_back({ m, nullptr, models[i]->GetWorldTransform(), models[i]->GetMaterial() });
		}

		std::sort(ms.begin(), ms.end(), [](const BLASmesh& l, const BLASmesh& r) -> bool { return l.transform < r.transform; });

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
				auto resource = BuildBLAS(blas_meshes, m_dxrDevice, commandQueue.Get(), dxrCommandList.Get(), commandAllocators[backBufferIndex].Get());

				rtx->blases.push_back({ ms[i].transform, (UINT)blas_meshes.size(), resource, blas_meshesh_vb, blas_materials });
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
	rtx->topLevelAccelerationStructure = BuildTLAS(rtx->blases, m_dxrDevice,
		commandQueue.Get(), dxrCommandList.Get(), commandAllocators[backBufferIndex].Get());

	// Materials
	{
		std::vector<engine::Texture*> sceneTextures;
		std::unordered_map<engine::Texture*, UINT> textureToIndex;

		std::vector<engine::Material*> sceneMaterial;
		std::vector<engine::Shaders::Material> sceneMaterialGPU;
		std::unordered_map<engine::Material*, UINT> materialToIndex;

		for (size_t i = 0; i < rtx->blases.size(); ++i)
		{
			for (size_t j = 0; j < rtx->blases[i].instances; ++j)
			{
				engine::Material* mat = rtx->blases[i].materials[j];

				auto it = materialToIndex.find(mat);
				if (it == materialToIndex.end())
				{
					materialToIndex[mat] = sceneMaterial.size();
					sceneMaterial.push_back(mat);

					engine::Shaders::Material gpuMaterial{};
					gpuMaterial.albedo = mat->GetValue(engine::Material::Params::Albedo);
					gpuMaterial.shading.x = mat->GetValue(engine::Material::Params::Roughness).x;

					engine::Texture* texture = mat->GetTexture(engine::Material::Params::Albedo);

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

		engine::GetCoreRenderer()->CreateStructuredBuffer(rtx->materialsBuffer.getAdressOf(), L"Materials",
			sizeof(engine::Shaders::Material), sceneMaterialGPU.size(), &sceneMaterialGPU[0], x12::BUFFER_FLAGS::SHADER_RESOURCE);

		// Textures
		{
			for (int i = 0; i < sceneTextures.size(); i++)
			{
				auto coreTexture = sceneTextures[i]->GetCoreTexture();
				Dx12CoreTexture* dx12CoreTexure = static_cast<x12::Dx12CoreTexture*>(coreTexture);
				D3D12_CPU_DESCRIPTOR_HANDLE cputextureHandle = dx12CoreTexure->GetSRV();
				D3D12_CPU_DESCRIPTOR_HANDLE destCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1 + i, descriptorSize);

				device->CopyDescriptorsSimple(1, destCPU, cputextureHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				if (!texturesHandle.ptr)
					texturesHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, descriptorSize);

			}
		}

		// Instances Data
		std::vector<engine::Shaders::InstanceData> instancesData(totalInstances);

		UINT offset = 0;
		for (size_t i = 0; i < rtx->blases.size(); ++i)
		{
			for (size_t j = 0; j < rtx->blases[i].instances; ++j)
			{
				math::mat4 transform = rtx->blases[i].transform;
				instancesData[offset].transform = transform;
				instancesData[offset].normalTransform = transform.Inverse().Transpose();
				instancesData[offset].emission = 0;
				instancesData[offset].materialIndex = materialToIndex[rtx->blases[i].materials[j]];
				offset++;
			}
		}

		engine::GetCoreRenderer()->CreateStructuredBuffer(rtx->perInstanceBuffer.getAdressOf(), L"TLAS per-instance data",
			sizeof(engine::Shaders::InstanceData), instancesData.size(), &instancesData[0], x12::BUFFER_FLAGS::SHADER_RESOURCE);
	}

	// Scene buffer
	{
		engine::Shaders::Scene scene;
		scene.instanceCount = totalInstances;
		scene.lightCount = (uint32_t)areaLights.size();

		engine::GetCoreRenderer()->CreateConstantBuffer(rtx->sceneBuffer.getAdressOf(), L"Scene data", sizeof(engine::Shaders::Scene), false);
		rtx->sceneBuffer->SetData(&scene, sizeof(engine::Shaders::Scene));
	}

	CreateShaderBindingTable(dxrPrimaryStateObject, totalInstances, rtx->rayGenShaderTable, rtx->hitGroupShaderTable, rtx->missShaderTable);
	CreateShaderBindingTable(dxrSecondaryStateObject, totalInstances, rtx->rayGenShaderTable_secondary, rtx->hitGroupShaderTable_secondary, rtx->missShaderTable_secondary);


	{
		D3D12_INDIRECT_ARGUMENT_DESC arg{};
		arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE::D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;

		D3D12_COMMAND_SIGNATURE_DESC desc{};
		desc.NumArgumentDescs = 1;
		desc.pArgumentDescs = &arg;
		desc.ByteStride = sizeof(D3D12_DISPATCH_RAYS_DESC);

		throwIfFailed(device->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(rtx->dxrSecondaryCommandIndirect.GetAddressOf())));

		D3D12_DISPATCH_RAYS_DESC indirectData;
		indirectData.HitGroupTable.StartAddress = rtx->hitGroupShaderTable_secondary->GetGPUVirtualAddress();
		indirectData.HitGroupTable.SizeInBytes = rtx->hitGroupShaderTable_secondary->GetDesc().Width;
		indirectData.HitGroupTable.StrideInBytes = hitRecordSize();
		indirectData.MissShaderTable.StartAddress = rtx->missShaderTable_secondary->GetGPUVirtualAddress();
		indirectData.MissShaderTable.SizeInBytes = rtx->missShaderTable_secondary->GetDesc().Width;
		indirectData.MissShaderTable.StrideInBytes = 32;
		indirectData.RayGenerationShaderRecord.StartAddress = rtx->rayGenShaderTable_secondary->GetGPUVirtualAddress();
		indirectData.RayGenerationShaderRecord.SizeInBytes = rtx->rayGenShaderTable_secondary->GetDesc().Width;
		indirectData.Width = width * height;
		indirectData.Height = 1;
		indirectData.Depth = 1;

		engine::GetCoreRenderer()->CreateStructuredBuffer(rtx->indirectBuffer.getAdressOf(), L"D3D12_DISPATCH_RAYS_DESC",
			sizeof(D3D12_DISPATCH_RAYS_DESC), 1, &indirectData, x12::BUFFER_FLAGS::UNORDERED_ACCESS);
	}
}

void engine::Render::OnObjAdded()
{
	GetRender()->OnLoadScene();
}

ComPtr<ID3D12StateObject> engine::Render::CreatePSO(ComPtr<IDxcBlob> r, ComPtr<IDxcBlob> h, ComPtr<IDxcBlob> m)
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
	throwIfFailed(m_dxrDevice->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&RTXPSO)));

	return RTXPSO;
};

void engine::Render::Init()
{
	GetSceneManager()->AddCallbackSceneLoaded(OnObjAdded);

	engine::MainWindow* window = core__->GetWindow();
	hwnd = *window->handle();

	rtx = std::make_unique<engine::Render::RTXscene>();

	planeMesh = engine::GetResourceManager()->CreateStreamMesh("std#plane");

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
	throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));
	rtxQueue = commandQueue.Get();

	// Create a command allocator for each back buffer that will be rendered to.
	for (UINT n = 0; n < engine::DeferredBuffers; n++)
	{
		throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocators[n])));
	}

	// Create a command list for recording graphics commands.
	throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&commandList)));
	throwIfFailed(commandList->Close());

	InitFence(rtToSwapchainFence);
	InitFence(clearToRTFence);
	InitFence(frameEndFence);

	event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	// DXR interfaces
	throwIfFailed(device->QueryInterface(IID_PPV_ARGS(&dxrDevice)));
	m_dxrDevice = dxrDevice.Get();

	throwIfFailed(commandList->QueryInterface(IID_PPV_ARGS(&dxrCommandList)));

	{
		copyShader = engine::GetResourceManager()->CreateGraphicShader("../resources/shaders/copy.hlsl", nullptr, 0);
		copyShader.get();

		const x12::ConstantBuffersDesc buffersdesc[1] =
		{
			"CameraBuffer",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		clearShader = engine::GetResourceManager()->CreateComputeShader("../resources/shaders/clear.hlsl", &buffersdesc[0], 1);
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

		SerializeAndCreateRaytracingRootSignature(device, desc, &raytracingGlobalRootSignature);
	}

	// Local Root Signature
	{
		CD3DX12_ROOT_PARAMETER rootParameters[LocalRootSignatureParams::Count];
		rootParameters[LocalRootSignatureParams::InstanceConstants].InitAsConstants(sizeof(engine::Shaders::InstancePointer) / 4, 2, 0);
		rootParameters[LocalRootSignatureParams::VertexBuffer].InitAsShaderResourceView(2, 0);

		CD3DX12_ROOT_SIGNATURE_DESC desc(ARRAYSIZE(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);

		SerializeAndCreateRaytracingRootSignature(device, desc, &raytracingLocalRootSignature);
	}

	{
		std::vector<std::pair<std::wstring, std::wstring>> defines_primary = { { L"PRIMARY_RAY", L"1" } };

		raygen = CompileShader(L"rt_raygen.hlsl", false, defines_primary);
		hit = CompileShader(L"rt_hit.hlsl", false, defines_primary);
		miss = CompileShader(L"rt_miss.hlsl", false, defines_primary);

		raygen_secondary = CompileShader(L"rt_raygen.hlsl");
		hit_secondary = CompileShader(L"rt_hit.hlsl");
		miss_secondary = CompileShader(L"rt_miss.hlsl");
	}

	dxrPrimaryStateObject = CreatePSO(raygen, hit, miss);
	dxrSecondaryStateObject = CreatePSO(raygen_secondary, hit_secondary, miss_secondary);

	descriptorSize = CreateDescriptorHeap(device, descriptorHeap.GetAddressOf());

	int w, h;
	window->GetClientSize(w, h);
	width = w;
	height = h;
	CreateRaytracingOutputResource(w, h);

	engine::GetCoreRenderer()->CreateStructuredBuffer(rayInfoBuffer.getAdressOf(), L"Ray info",
		sizeof(engine::Shaders::RayInfo), w* h, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateStructuredBuffer(regroupedIndexesBuffer.getAdressOf(), L"Regrouped indexes for secondary rays",
		sizeof(uint32_t), w* h, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateStructuredBuffer(iterationRGBA32f.getAdressOf(), L"iterationRGBA32f",
		sizeof(float[4]), w* h, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateStructuredBuffer(hitCointerBuffer.getAdressOf(), L"Hits count",
		sizeof(uint32_t), 1, nullptr, x12::BUFFER_FLAGS::UNORDERED_ACCESS);

	engine::GetCoreRenderer()->CreateConstantBuffer(cameraBuffer.getAdressOf(), L"camera constant buffer",
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
			SerializeAndCreateRaytracingRootSignature(device, desc, &clearRayInfoRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = clearRayInfoRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(clearRayInfoPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(device, desc, &regroupRayInfoRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = regroupRayInfoRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(regroupRayInfoPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(device, desc, &clearHitCounterRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = clearHitCounterRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(clearHitCounterPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(device, desc, &accumulationRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = accumulationRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(accumulationPSO.GetAddressOf())));
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
			SerializeAndCreateRaytracingRootSignature(device, desc, &copyrHitCounterRootSignature);
		}

		// Create PSO
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = copyrHitCounterRootSignature.Get();
		desc.CS = CD3DX12_SHADER_BYTECODE(v->GetBufferPointer(), v->GetBufferSize());

		throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(copyHitCounterPSO.GetAddressOf())));
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

		engine::GetCoreRenderer()->CreateVertexBuffer(plane.getAdressOf(), L"plane", planeVert, &desc, nullptr, nullptr, MEMORY_TYPE::GPU_READ);
	}
}

void engine::Render::CreateRaytracingOutputResource(UINT width, UINT height)
{
	// Create the output resource. The dimensions and format should match the swap-chain.
	auto uavDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	throwIfFailed(device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &uavDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&outputRGBA32f)));
	outputRGBA32f->SetName(L"raytracingOutput");

	UINT m_raytracingOutputResourceUAVDescriptorHeapIndex = 0;// AllocateDescriptor(&uavDescriptorHandle);
	D3D12_CPU_DESCRIPTOR_HANDLE uavDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	device->CreateUnorderedAccessView(outputRGBA32f.Get(), nullptr, &UAVDesc, uavDescriptorHandle);
	raytracingOutputResourceUAVGpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), m_raytracingOutputResourceUAVDescriptorHeapIndex, descriptorSize);

	engine::GetCoreRenderer()->CreateTextureFrom(outputRGBA32fcoreWeakPtr.getAdressOf(), L"Raytracing output", outputRGBA32f.Get());
}
