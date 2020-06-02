#include "pch.h"
#include "dx12context.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12vertexbuffer.h"
#include "dx12buffer.h"
#include "dx12texture.h"
#include <chrono>
#include "GraphicsMemory.h"

using namespace x12;
using namespace x12::d3d12;

namespace
{
	Dx12ResourceSet* resource_cast(IResourceSet* value) { return static_cast<Dx12ResourceSet*>(value); }
	const Dx12ResourceSet* resource_cast(const IResourceSet* value) { return static_cast<const Dx12ResourceSet*>(value); }
	Dx12CoreShader* resource_cast(ICoreShader* shader) { return static_cast<Dx12CoreShader*>(shader); }
	const Dx12CoreShader* resource_cast(const ICoreShader* shader) { return static_cast<const Dx12CoreShader*>(shader); }
	Dx12CoreVertexBuffer* resource_cast(ICoreVertexBuffer* vb) { return static_cast<Dx12CoreVertexBuffer*>(vb); }
	const Dx12CoreVertexBuffer* resource_cast(const ICoreVertexBuffer* vb) { return static_cast<const Dx12CoreVertexBuffer*>(vb); }
	Dx12CoreBuffer* resource_cast(ICoreBuffer* vb) { return static_cast<Dx12CoreBuffer*>(vb); }
	const Dx12CoreBuffer* resource_cast(const ICoreBuffer* vb) { return static_cast<const Dx12CoreBuffer*>(vb); }
	Dx12CoreTexture* resource_cast(ICoreTexture* vb) { return static_cast<Dx12CoreTexture*>(vb); }
	const Dx12CoreTexture* resource_cast(const ICoreTexture* vb) { return static_cast<const Dx12CoreTexture*>(vb); }
}

int Dx12GraphicCommandContext::contextNum;
int Dx12CopyCommandContext::contextNum;

uint64_t Signal(ID3D12CommandQueue* d3dCommandQueue, ID3D12Fence* d3dFence, uint64_t& fenceValue)
{
	uint64_t fenceValueForSignal = fenceValue;
	ThrowIfFailed(d3dCommandQueue->Signal(d3dFence, fenceValueForSignal));
	++fenceValue;
	return fenceValueForSignal;
}

void WaitForFenceValue(ID3D12Fence* d3dFence, uint64_t fenceValue, HANDLE fenceEvent,
					   std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	if (d3dFence->GetCompletedValue() < fenceValue)
	{
		ThrowIfFailed(d3dFence->SetEventOnCompletion(fenceValue, fenceEvent));
		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

void Dx12GraphicCommandContext::SetGraphicPipelineState(const GraphicPipelineState& pso)
{
	auto checksum = x12::CalculateChecksum(pso);

	if (checksum == state.pso.PsoChecksum && !state.pso.isCompute)
		return;

	Dx12CoreShader* dx12Shader = resource_cast(pso.shader.get());
	Dx12CoreVertexBuffer* dx12vb = resource_cast(pso.vb.get());

	resetOnlyPSOState();

	auto d3dstate = d3d12::D3D12GetCoreRender()->GetGraphicPSO(pso, checksum);
	setGraphicPipeline(checksum, d3dstate.Get());

	if (state.pso.graphicDesc.primitiveTopology != pso.primitiveTopology)
	{
		D3D12_PRIMITIVE_TOPOLOGY d3dtopology;
		switch (pso.primitiveTopology)
		{
			case PRIMITIVE_TOPOLOGY::LINE: d3dtopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
			case PRIMITIVE_TOPOLOGY::POINT: d3dtopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
				[[likely]]
			case PRIMITIVE_TOPOLOGY::TRIANGLE: d3dtopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
			default:
				notImplemented();
		}
		d3dCmdList->IASetPrimitiveTopology(d3dtopology);
	}

	if (!dx12Shader->HasResources())
		state.pso.d3drootSignature = d3d12::D3D12GetCoreRender()->GetDefaultRootSignature();
	else
		state.pso.d3drootSignature = dx12Shader->resourcesRootSignature;

	d3dCmdList->SetGraphicsRootSignature(state.pso.d3drootSignature.Get());

	state.pso.graphicDesc = pso;
}

void Dx12GraphicCommandContext::SetComputePipelineState(const ComputePipelineState& pso)
{
	auto checksum = CalculateChecksum(pso);

	if (checksum == state.pso.PsoChecksum && !state.pso.isCompute)
		return;

	resetOnlyPSOState();

	// Reset graphci states
	state.surface = nullptr;
	state.viewport = {};
	state.scissor = {};

	Dx12CoreShader* dx12Shader = resource_cast(pso.shader);

	auto d3dstate = d3d12::D3D12GetCoreRender()->GetComputePSO(pso, checksum);
	setComputePipeline(checksum, d3dstate.Get());

	if (!dx12Shader->HasResources())
		state.pso.d3drootSignature = d3d12::D3D12GetCoreRender()->GetDefaultRootSignature();
	else
		state.pso.d3drootSignature = dx12Shader->resourcesRootSignature;

	d3dCmdList->SetComputeRootSignature(state.pso.d3drootSignature.Get());

	state.pso.computeDesc = pso;
}

void Dx12GraphicCommandContext::SetVertexBuffer(ICoreVertexBuffer* vb)// TODO add vb to tracked resources
{
	assert(state.pso.PsoChecksum > 0 && "PSO is not set");

	Dx12CoreVertexBuffer* dxBuffer = resource_cast(vb);

	state.pso.graphicDesc.vb = dxBuffer;

	UINT numBarriers;
	D3D12_RESOURCE_BARRIER barriers[2];

	if (dxBuffer->GetReadBarrier(&numBarriers, barriers))
		d3dCmdList->ResourceBarrier(numBarriers, barriers);


	d3dCmdList->IASetVertexBuffers(0, 1, &dxBuffer->vertexBufferView);
	d3dCmdList->IASetIndexBuffer(dxBuffer->pIndexBufferVew());
}

void Dx12GraphicCommandContext::SetViewport(unsigned width, unsigned heigth)
{
	D3D12_VIEWPORT v{0.0f, 0.0f, (float)width, (float)heigth, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
	if (memcmp(&state.viewport, &v, sizeof(D3D12_VIEWPORT)) != 0)
	{
		d3dCmdList->RSSetViewports(1, &v);
		memcpy(&state.viewport, &v, sizeof(D3D12_VIEWPORT));
	}
}

void Dx12GraphicCommandContext::GetViewport(unsigned& width, unsigned& heigth)
{
	width = (unsigned)state.viewport.Width;
	heigth = (unsigned)state.viewport.Height;
}

void Dx12GraphicCommandContext::SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth)
{
	D3D12_RECT r{LONG(x), LONG(y), LONG(width), LONG(heigth)};
	if (memcmp(&state.scissor, &r, sizeof(D3D12_RECT)) != 0)
	{
		d3dCmdList->RSSetScissorRects(1, &r);
		memcpy(&state.scissor, &r, sizeof(D3D12_RECT));
	}
}

void Dx12GraphicCommandContext::setComputePipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso)
{
	d3dCmdList->SetPipelineState(d3dpso);

	++statistic.stateChanges;

	state.pso.isCompute = true;
	state.pso.PsoChecksum = newChecksum;
	state.pso.d3dpso = d3dpso;
}

void Dx12GraphicCommandContext::setGraphicPipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso)
{
	d3dCmdList->SetPipelineState(d3dpso);

	++statistic.stateChanges;

	state.pso.isCompute = false;
	state.pso.PsoChecksum = newChecksum;
	state.pso.d3dpso = d3dpso;
}

void Dx12GraphicCommandContext::Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount, uint32_t vertexOffset)
{
	const Dx12CoreVertexBuffer* dx12Vb = resource_cast(vb);

	if (state.pso.graphicDesc.vb.get() != const_cast<Dx12CoreVertexBuffer*>(dx12Vb))
		SetVertexBuffer(const_cast<ICoreVertexBuffer*>(vb));

	cmdList->TrackResource(const_cast<Dx12CoreVertexBuffer*>(dx12Vb));

	if (vertexCount > 0)
	{
		assert(vertexCount <= dx12Vb->vertexCount);
	}

	if (dx12Vb->indexBuffer)
	{
		d3dCmdList->DrawIndexedInstanced(vertexCount > 0 ? vertexCount : dx12Vb->indexCount, 1, vertexOffset, 0, 0);
		statistic.triangles += dx12Vb->indexCount / 3;
	}
	else
	{
		d3dCmdList->DrawInstanced(vertexCount > 0 ? vertexCount : dx12Vb->vertexCount, 1, vertexOffset, 0);
		statistic.triangles += dx12Vb->vertexCount / 3;
	}

	++statistic.drawCalls;
}

void Dx12GraphicCommandContext::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	d3dCmdList->Dispatch(x, y, z);
	++statistic.drawCalls;
}

void Dx12GraphicCommandContext::Clear()
{
	Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(dx12surface->descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, CR_RTV_DescriptorsSize());
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = dx12surface->descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();

	FLOAT depth = 1.0f;
	d3dCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);

	const float color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	d3dCmdList->ClearRenderTargetView(rtv, color, 0, nullptr);
}

void Dx12GraphicCommandContext::BuildResourceSet(IResourceSet* set_)
{
	Dx12ResourceSet* dx12set = resource_cast(set_);

	if (!dx12set->dirty)
		return;

	for (int i = 0; i < dx12set->rootParametersNum; ++i)
	{
		const RootSignatureParameter<Dx12ResourceSet::BindedResource>& param = dx12set->resources[i];

		if (param.type == ROOT_PARAMETER_TYPE::INLINE_DESCRIPTOR)
			continue;

		else if (param.type == ROOT_PARAMETER_TYPE::TABLE)
		{
			if (!dx12set->resourcesDirty[i])
				continue;

			auto [destCPU, destGPU] = newGPUHandle(param.tableResourcesNum);

			for (int j = 0; j < param.tableResourcesNum; ++j)
			{
				const Dx12ResourceSet::BindedResource& res = param.tableResources[j];
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleCPUVisible;

				if (res.resources & RESOURCE_DEFINITION::RBF_UNIFORM_BUFFER)
					cpuHandleCPUVisible = res.buffer->GetCBV();
				else if (res.resources & RESOURCE_DEFINITION::RBF_BUFFER_SRV)
					cpuHandleCPUVisible = res.buffer->GetSRV();
				else if (res.resources & RESOURCE_DEFINITION::RBF_TEXTURE_SRV)
					cpuHandleCPUVisible = res.texture->GetSRV();
				else if (res.resources & RESOURCE_DEFINITION::RBF_BUFFER_UAV)
					cpuHandleCPUVisible = res.buffer->GetUAV();
				else
					notImplemented();

				device->CopyDescriptorsSimple(1, destCPU, cpuHandleCPUVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				destCPU.ptr += descriptorSizeCBSRV;
			}

			dx12set->gpuDescriptors[i] = destGPU;
			dx12set->resourcesDirty[i] = false;
		}
		else
			unreacheble();
	}

	dx12set->dirty = false;
}

void Dx12GraphicCommandContext::BindResourceSet(IResourceSet* set_)
{
	Dx12ResourceSet* dx12set = resource_cast(set_);
	state.set_ = const_cast<Dx12ResourceSet*>(dx12set);

	assert(state.set_.get());

	for (int i = 0; i < dx12set->rootParametersNum; ++i)
	{
		const RootSignatureParameter<Dx12ResourceSet::BindedResource>& param = dx12set->resources[i];

		if (param.type == ROOT_PARAMETER_TYPE::INLINE_DESCRIPTOR)
			continue;

		else if (param.type == ROOT_PARAMETER_TYPE::TABLE)
		{
			if (!state.pso.isCompute) [[likely]]
				d3dCmdList->SetGraphicsRootDescriptorTable(i, dx12set->gpuDescriptors[i]);
			else
				d3dCmdList->SetComputeRootDescriptorTable(i, dx12set->gpuDescriptors[i]);
		}
		else
			unreacheble();
	}
}

void Dx12GraphicCommandContext::UpdateInlineConstantBuffer(size_t rootParameterIdx, const void* data, size_t size)
{
	DirectX::GraphicsResource alloc = frameMemory->Allocate(size);

	memcpy(alloc.Memory(), data, size);

	if (!state.pso.isCompute) [[likely]]
		d3dCmdList->SetGraphicsRootConstantBufferView((UINT)rootParameterIdx, alloc.GpuAddress());
	else
		d3dCmdList->SetComputeRootConstantBufferView((UINT)rootParameterIdx, alloc.GpuAddress());
}

void Dx12GraphicCommandContext::EmitUAVBarrier(ICoreBuffer* buffer)
{
	auto* dxRs = resource_cast(buffer);
	d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(dxRs->GetResource()));
}

void Dx12GraphicCommandContext::TimerBegin(uint32_t timerID)
{
	assert(timerID < maxNumTimers && "Timer ID out of range");
	d3dCmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timerID * 2);
}

void Dx12GraphicCommandContext::TimerEnd(uint32_t timerID)
{
	assert(timerID < maxNumTimers && "Timer ID out of range");
	d3dCmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timerID * 2 + 1);
}

auto Dx12GraphicCommandContext::TimerGetTimeInMs(uint32_t timerID) -> float
{
	assert(timerID < maxNumTimers && "Timer ID out of range");

	UINT64 start = queryTiming[timerID * 2];
	UINT64 end = queryTiming[timerID * 2 + 1];

	if (end < start)
		return 0.f;

	double time = double(end - start) * gpuTickDelta;

	return static_cast<float>(time);
}

void Dx12GraphicCommandContext::CommandList::TrackResource(IResourceUnknown* res)
{
	if (trakedResources.empty())
	{
		trakedResources.push_back(res);
		res->AddRef();
	}
	else
	{
		if (trakedResources.back() != res)
		{
			trakedResources.push_back(res);
			res->AddRef();
		}
	}
}

void Dx12GraphicCommandContext::CommandList::ReleaseTrakedResources()
{
	for (IResourceUnknown* res : trakedResources)
		res->Release();
	trakedResources.clear();
}

void Dx12GraphicCommandContext::CommandList::CompleteGPUFrame(uint64_t nextFenceID)
{
	ReleaseTrakedResources();

	fenceOldValue = nextFenceID;
}

Dx12GraphicCommandContext::Dx12GraphicCommandContext(FinishFrameBroadcast finishFrameCallback_) :
	finishFrameBroadcast(finishFrameCallback_)
{
	device = CR_GetD3DDevice();

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));
	set_ctx_object_name(d3dFence, L"fence");

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3dCommandQueue)));
	set_ctx_object_name(d3dCommandQueue, L"queue");

	for (int i = 0; i < DeferredBuffers; ++i)
		cmdLists[i].Init(this, i);

	cmdList = &cmdLists[0];
	d3dCmdList = cmdList->d3dCmdList;

	// Query
	queryTiming.resize(maxNumQuerySlots);

	uint64_t gpuFrequency;
	d3dCommandQueue->GetTimestampFrequency(&gpuFrequency);
	gpuTickDelta = 1000.0 / static_cast<double>(gpuFrequency);

	D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
	QueryHeapDesc.Count = maxNumQuerySlots;
	QueryHeapDesc.NodeMask = 1;
	QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

	ThrowIfFailed(device->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&queryHeap)));
	set_ctx_object_name(queryHeap, L"query heap for %u timers", contextNum, maxNumQuerySlots);

	// We allocate MaxFrames + 1 instances as an instance is guaranteed to be written to if maxPresentFrameCount frames
	// have been submitted since. This is due to a fact that Present stalls when none of the m_maxframeCount frames are done/available.
	size_t FramesInstances = DeferredBuffers + 1;

	UINT64 size = FramesInstances * maxNumQuerySlots * sizeof(UINT64);

	x12::memory::CreateCommittedBuffer(&queryReadBackBuffer, size, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);

	set_ctx_object_name(queryReadBackBuffer, L"query buffer for %u timers %u bytes", contextNum, maxNumQuerySlots, size);

	descriptorSizeCBSRV = CR_CBSRV_DescriptorsSize();
	descriptorSizeDSV = CR_DSV_DescriptorsSize();
	descriptorSizeRTV = CR_RTV_DescriptorsSize();

	frameMemory = std::make_unique<DirectX::GraphicsMemory>(CR_GetD3DDevice());

	gpuDescriptorHeap = CreateDescriptorHeap(device, MaxBindedResourcesPerFrame, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	set_ctx_object_name(gpuDescriptorHeap.Get(), L"descriptor heap for %lu descriptors", MaxBindedResourcesPerFrame);
	gpuDescriptorHeapStart = gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuDescriptorHeapStartGPU = gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	contextNum++;
}

std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> Dx12GraphicCommandContext::newGPUHandle(UINT num)
{
	assert(gpuDescriptorsOffset < MaxBindedResourcesPerFrame);

	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> ret;
	ret.first.ptr = gpuDescriptorHeapStart.ptr + size_t(gpuDescriptorsOffset) * descriptorSizeCBSRV;
	ret.second.ptr = gpuDescriptorHeapStartGPU.ptr + size_t(gpuDescriptorsOffset) * descriptorSizeCBSRV;

	gpuDescriptorsOffset += num;

	return ret;
}
void Dx12GraphicCommandContext::CommandList::Init(Dx12GraphicCommandContext* parent_, int num)
{
	parent = parent_;
	device = CR_GetD3DDevice();

	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dCommandAllocator)));
	Dx12GraphicCommandContext::set_ctx_object_name(d3dCommandAllocator, L"command allocator for #%d deferred frame", num);

	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCmdList)));
	Dx12GraphicCommandContext::set_ctx_object_name(d3dCommandAllocator, L"command list for #%d deferred frame", num);
	ThrowIfFailed(d3dCmdList->Close());
}

void Dx12GraphicCommandContext::CommandList::Free()
{
	Release(d3dCommandAllocator);
	Release(d3dCmdList);
}

Dx12GraphicCommandContext::~Dx12GraphicCommandContext()
{
	assert(d3dCommandQueue == nullptr && "Free() must be called before destructor");
	assert(d3dFence == nullptr && "Free() must be called before destructor");
	::CloseHandle(fenceEvent);
}

void Dx12GraphicCommandContext::Free()
{
	statesStack = {};

	WaitGPUAll();

	for (int i = 0; i < DeferredBuffers; ++i)
		cmdLists[i].Free();

	Release(d3dCommandQueue);
	Release(d3dFence);
	Release(queryHeap);
	Release(queryReadBackBuffer);
}

void Dx12GraphicCommandContext::FrameEnd()
{
	resetStatistic();
}

void Dx12GraphicCommandContext::PushState()
{
	statesStack.push(state);
}

void Dx12GraphicCommandContext::PopState()
{
	StateCache& state_ = statesStack.top();

	if (state_.pso.isCompute)
		SetComputePipelineState(state_.pso.computeDesc);
	else
		SetGraphicPipelineState(state_.pso.graphicDesc);

	BindResourceSet(state_.set_.get());

	if (!state_.pso.isCompute)
	{
		BindSurface(state_.surface);
		SetVertexBuffer(state_.pso.graphicDesc.vb.get());
		SetScissor(state_.scissor.left, state_.scissor.top, state_.scissor.right, state_.scissor.bottom);
		SetViewport((unsigned)state_.viewport.Width, (unsigned)state_.viewport.Height);
	}

	state = state_;

	statesStack.pop();
}

void Dx12GraphicCommandContext::CommandsBegin()
{
	cmdList->d3dCommandAllocator->Reset();
	d3dCmdList->Reset(cmdList->d3dCommandAllocator, nullptr);
	d3dCmdList->SetDescriptorHeaps(1, gpuDescriptorHeap.GetAddressOf());
}
void Dx12GraphicCommandContext::BindSurface(surface_ptr& surface_)
{
	if (surface_ == state.surface)
		return;

	state.surface = surface_;

	Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());

	ID3D12Resource* backBuffer = dx12surface->colorBuffers[frameIndex].Get();

	transiteSurfaceToState(D3D12_RESOURCE_STATE_RENDER_TARGET);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(dx12surface->descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, CR_RTV_DescriptorsSize());
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = dx12surface->descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();

	d3dCmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
}
void Dx12GraphicCommandContext::CommandsEnd()
{
	// Query
	// Write to buffer current time on GPU
	UINT64 resolveAddress = queryResolveToFrameID * maxNumQuerySlots * sizeof(UINT64);
	d3dCmdList->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, maxNumQuerySlots, queryReadBackBuffer, resolveAddress);

	// Grab read-back data for the queries from a finished frame m_maxframeCount ago.
	UINT readBackFrameID = (queryResolveToFrameID + 1) % (DeferredBuffers + 1);
	SIZE_T readBackBaseOffset = readBackFrameID * maxNumQuerySlots * sizeof(UINT64);
	D3D12_RANGE dataRange =
	{
		readBackBaseOffset,
		readBackBaseOffset + maxNumQuerySlots * sizeof(UINT64),
	};

	UINT64* timingData;
	ThrowIfFailed(queryReadBackBuffer->Map(0, &dataRange, reinterpret_cast<void**>(&timingData)));

	memcpy(&queryTiming[0], (uint8_t*)timingData + readBackBaseOffset, sizeof(UINT64) * maxNumQuerySlots);

	queryReadBackBuffer->Unmap(0, &CD3DX12_RANGE(0, 0));

	queryResolveToFrameID = readBackFrameID;

	// ---- Query

	transiteSurfaceToState(D3D12_RESOURCE_STATE_PRESENT);

	ThrowIfFailed(d3dCmdList->Close());

	resetFullState();
}

void Dx12GraphicCommandContext::Submit()
{
	ID3D12CommandList* const commandLists[] = {d3dCmdList};
	d3dCommandQueue->ExecuteCommandLists(1, commandLists);

	frameMemory->Commit(d3dCommandQueue);
}

void Dx12GraphicCommandContext::resetOnlyPSOState()
{
	state.pso = {};
	state.set_ = {};
}

void Dx12GraphicCommandContext::resetFullState()
{
	state = {};
}

void Dx12GraphicCommandContext::transiteSurfaceToState(D3D12_RESOURCE_STATES newState)
{
	if (!state.surface)
		return;

	Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());

	if (dx12surface->state == newState)
		return;

	ID3D12Resource* backBuffer = dx12surface->colorBuffers[frameIndex].Get();

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
																			dx12surface->state, newState);

	d3dCmdList->ResourceBarrier(1, &barrier);

	dx12surface->state = newState;
}

void Dx12GraphicCommandContext::resetStatistic()
{
	memset(&statistic, 0, sizeof(Statistic));
}

void Dx12GraphicCommandContext::WaitGPUFrame()
{
	Signal(d3dCommandQueue, d3dFence, fenceValue);

	// Shift to next frame
	frameIndex = (frameIndex + 1u) % DeferredBuffers;

	cmdList = &cmdLists[frameIndex];
	d3dCmdList = cmdList->d3dCmdList;

	// Fence ID that we want to wait
	uint64_t fenceIDToWait = cmdList->fenceOldValue;

	// Wait GPU for prevois fenceID
	WaitForFenceValue(d3dFence, fenceIDToWait, fenceEvent);

	// Here frame fenceIDToWait is completed on GPU

	cmdList->CompleteGPUFrame(fenceValue);

	if (finishFrameBroadcast)
		finishFrameBroadcast(fenceIDToWait);
}

void Dx12GraphicCommandContext::WaitGPUAll()
{
	uint64_t fenceIDEmited = Signal(d3dCommandQueue, d3dFence, fenceValue);

	WaitForFenceValue(d3dFence, fenceIDEmited, fenceEvent);

	// Here all frames are completed

	for (int i = 0; i < DeferredBuffers; ++i)
	{
		cmdLists[i].CompleteGPUFrame(fenceValue);
	}

	if (finishFrameBroadcast)
		finishFrameBroadcast(fenceIDEmited);
}

// Dx12CopyCommandContext
//
Dx12CopyCommandContext::Dx12CopyCommandContext()
{
	device = CR_GetD3DDevice();

	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&d3dCommandAllocator)));
	set_ctx_object_name(d3dCommandAllocator, L"command allocator");

	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCommandList)));
	set_ctx_object_name(d3dCommandAllocator, L"command list");
	ThrowIfFailed(d3dCommandList->Close());

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));
	set_ctx_object_name(d3dCommandAllocator, L"fence");

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3dCommandQueue)));
	set_ctx_object_name(d3dCommandQueue, L"command queue");

	contextNum++;
}

Dx12CopyCommandContext::~Dx12CopyCommandContext()
{
	assert(d3dCommandAllocator == nullptr && "Free() must be called before destructor");
	assert(d3dCommandList == nullptr && "Free() must be called before destructor");
	assert(d3dCommandQueue == nullptr && "Free() must be called before destructor");
	assert(d3dFence == nullptr && "Free() must be called before destructor");

	::CloseHandle(fenceEvent);
}

void Dx12CopyCommandContext::Free()
{
	WaitGPUAll();

	Release(d3dCommandAllocator);
	Release(d3dCommandList);
	Release(d3dCommandQueue);
	Release(d3dFence);
}

void Dx12CopyCommandContext::CommandsBegin()
{
	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);
}

void Dx12CopyCommandContext::CommandsEnd()
{
	ThrowIfFailed(d3dCommandList->Close());
}

void Dx12CopyCommandContext::Submit()
{
	ID3D12CommandList* const commandLists[1] = {d3dCommandList};
	d3dCommandQueue->ExecuteCommandLists(1, commandLists);

	Signal(d3dCommandQueue, d3dFence, fenceValue);
}

void Dx12CopyCommandContext::WaitGPUAll()
{
	uint64_t fenceIDEmited = Signal(d3dCommandQueue, d3dFence, fenceValue);
	WaitForFenceValue(d3dFence, fenceIDEmited, fenceEvent);
}

Dx12ResourceSet::Dx12ResourceSet(const Dx12CoreShader* shader)
{
	resourcesMap = shader->resourcesMap;

	rootParametersNum = shader->rootSignatureParameters.size();
	resources.resize(rootParametersNum);
	resourcesDirty.resize(rootParametersNum);
	gpuDescriptors.resize(rootParametersNum);

	for (size_t i = 0; i < shader->rootSignatureParameters.size(); ++i)
	{
		const RootSignatureParameter<ResourceDefinition>& in = shader->rootSignatureParameters[i];
		RootSignatureParameter<BindedResource>& out = resources[i];

		out.type = in.type;
		out.shaderType = in.shaderType;
		out.tableResourcesNum = in.tableResourcesNum;

		if (in.type == ROOT_PARAMETER_TYPE::INLINE_DESCRIPTOR)
		{
			out.inlineResource = in.inlineResource;
		}
		else if (in.type == ROOT_PARAMETER_TYPE::TABLE)
		{
			out.tableResources.resize(in.tableResources.size());

			for (int j = 0; j < out.tableResources.size(); ++j)
				out.tableResources[j] = in.tableResources[j];
		}
		else
			unreacheble();
	}
}

void Dx12ResourceSet::BindConstantBuffer(const char* name, ICoreBuffer* buffer)
{
	Bind<ICoreBuffer, Dx12CoreBuffer>(name, buffer, RESOURCE_DEFINITION::RBF_UNIFORM_BUFFER);
}

void Dx12ResourceSet::BindStructuredBufferSRV(const char* name, ICoreBuffer* buffer)
{
	Bind<ICoreBuffer, Dx12CoreBuffer>(name, buffer, RESOURCE_DEFINITION::RBF_BUFFER_SRV);
}

void Dx12ResourceSet::BindStructuredBufferUAV(const char* name, ICoreBuffer* buffer)
{
	Bind<ICoreBuffer, Dx12CoreBuffer>(name, buffer, RESOURCE_DEFINITION::RBF_BUFFER_UAV);
}

void Dx12ResourceSet::BindTextueSRV(const char* name, ICoreTexture* texture)
{
	Bind<ICoreTexture, Dx12CoreTexture>(name, texture, RESOURCE_DEFINITION::RBF_TEXTURE_SRV);
}

void x12::Dx12ResourceSet::checkResourceIsTable(const resource_index& index)
{
	assert(index.second != -1 && "Resource is not in table");
}

void x12::Dx12ResourceSet::checkResourceIsInlineDescriptor(const resource_index& index)
{
	assert(index.second == -1 && "Resource in some table. Inline resource can not be in table");
}

Dx12ResourceSet::resource_index& Dx12ResourceSet::findResourceIndex(const char* name)
{
	auto it = resourcesMap.find(name);
	assert(it != resourcesMap.end());
	return it->second;
}

size_t Dx12ResourceSet::FindInlineBufferIndex(const char* name)
{
	auto& index = findResourceIndex(name);
	checkResourceIsInlineDescriptor(index);
	return index.first;
}
