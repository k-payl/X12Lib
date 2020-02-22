#include "pch.h"
#include "dx12context.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12uniformbuffer.h"
#include "dx12vertexbuffer.h"
#include "dx12buffer.h"
#include "dx12texture.h"
#include <chrono>

#define ADD_DIRY_FLAGS(FLAGS, ADD) \
	FLAGS = static_cast<RESOURCE_BIND_FLAGS>(FLAGS | ADD);

#define REMOVE_DIRY_FLAGS(FLAGS, _REMOVE) \
	FLAGS = static_cast<RESOURCE_BIND_FLAGS>(FLAGS & ~_REMOVE);


uint64_t Signal(ID3D12CommandQueue *d3dCommandQueue, ID3D12Fence *d3dFence, uint64_t& fenceValue)
{
	uint64_t fenceValueForSignal = fenceValue;
	ThrowIfFailed(d3dCommandQueue->Signal(d3dFence, fenceValueForSignal));
	++fenceValue;
	return fenceValueForSignal;
}

void WaitForFenceValue(ID3D12Fence *d3dFence, uint64_t fenceValue, HANDLE fenceEvent,
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
	auto checksum = CalculateChecksum(pso);

	if (checksum == state.pso.graphicPsoChecksum && !state.pso.isCompute)
		return;

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader);
	Dx12CoreVertexBuffer* dx12vb = static_cast<Dx12CoreVertexBuffer*>(pso.vb);

	resetPSOState();

	state.pso.shader = dx12Shader;
	state.pso.vb = dx12vb;
	state.primitiveTopology = pso.primitiveTopology;

	auto d3dstate = GetCoreRender()->GetGraphicPSO(pso, checksum);
	setGraphicPipeline(checksum, d3dstate.Get());

	if (!dx12Shader->HasResources())
		state.pso.rootSignature = GetCoreRender()->GetDefaultRootSignature();
	else
		state.pso.rootSignature = dx12Shader->resourcesRootSignature;

	d3dCmdList->SetGraphicsRootSignature(state.pso.rootSignature.Get());
}

void Dx12GraphicCommandContext::SetComputePipelineState(const ComputePipelineState& pso)
{
	auto checksum = CalculateChecksum(pso);

	if (checksum == state.pso.computePsoChecksum && !state.pso.isCompute)
		return;

	resetPSOState();

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader);

	state.pso.shader = dx12Shader;

	auto d3dstate = GetCoreRender()->GetComputePSO(pso, checksum);
	setComputePipeline(checksum, d3dstate.Get());

	//auto d3dstate = GetCoreRender()->GetComputePSO(pso, checksum);
	//d3dCmdList->SetPipelineState(d3dstate.Get());
	//++statistic.stateChanges;
	//state.pso.isCompute = true;
	//state.pso.computePsoChecksum = checksum;
	//state.pso.d3dpso = std::move(d3dstate);

	if (!dx12Shader->HasResources())
		state.pso.rootSignature = GetCoreRender()->GetDefaultRootSignature();
	else
		state.pso.rootSignature = dx12Shader->resourcesRootSignature;

	d3dCmdList->SetComputeRootSignature(state.pso.rootSignature.Get());
}

void Dx12GraphicCommandContext::SetVertexBuffer(Dx12CoreVertexBuffer* vb)// TODO add vb to tracked resources
{
	assert(state.pso.graphicPsoChecksum > 0 && "PSO is not set");

	state.pso.vb = vb;

	D3D12_PRIMITIVE_TOPOLOGY d3dtopology;
	switch (state.primitiveTopology)
	{
		case PRIMITIVE_TOPOLOGY::LINE: d3dtopology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case PRIMITIVE_TOPOLOGY::POINT: d3dtopology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case PRIMITIVE_TOPOLOGY::TRIANGLE: d3dtopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		default: throw new std::exception("Not impl");
	}

	UINT numBarriers;
	D3D12_RESOURCE_BARRIER barriers[2];

	if (vb->GetReadBarrier(&numBarriers, barriers))
		d3dCmdList->ResourceBarrier(numBarriers, barriers);

	d3dCmdList->IASetPrimitiveTopology(d3dtopology);
	d3dCmdList->IASetVertexBuffers(0, 1, &vb->vertexBufferView);
	d3dCmdList->IASetIndexBuffer(vb->pIndexBufferVew());
}

void Dx12GraphicCommandContext::SetViewport(unsigned width, unsigned heigth)
{
	D3D12_VIEWPORT v{ 0.0f, 0.0f, (float)width, (float)heigth, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	if (memcmp(&state.viewport, &v, sizeof(D3D12_VIEWPORT)) != 0)
	{
		d3dCmdList->RSSetViewports(1, &v);
		memcpy(&state.viewport, &v, sizeof(D3D12_VIEWPORT));
	}
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

void Dx12GraphicCommandContext::bindResources()
{
	assert(state.pso.shader && "Shader must be set");
	static_assert(sizeof(state.shaderTypesUsedBits) * CHAR_BIT >= (size_t)SHADER_TYPE::NUM);

	if (!state.shaderTypesUsedBits)
		return;

	uint32_t shaderTypesUsedBits = state.shaderTypesUsedBits;

	using RootSignatureParameter = Dx12CoreShader::RootSignatureParameter;
	using PARAMETER_TYPE = Dx12CoreShader::PARAMETER_TYPE;
	
	std::vector<RootSignatureParameter>& params = state.pso.shader->rootSignatureParameters;

	for (int rootIdx = 0; rootIdx < params.size(); ++rootIdx)
	{
		RootSignatureParameter& rootParam = params[rootIdx];

		decltype(state.shaderTypesUsedBits) shaderNum = 1 << int(rootParam.shaderType);

		if (!(shaderTypesUsedBits & shaderNum)) // skip shaders without resources
			continue;

		auto& slotsData = state.binds[(int)rootParam.shaderType];
		
		if (rootParam.type == PARAMETER_TYPE::INLINE_DESCRIPTOR)
		{
			int slot = rootParam.inlineResource.slot;

			switch (rootParam.inlineResource.resources)
			{
				case RBF_UNIFORM_BUFFER:
				{
					Dx12UniformBuffer* buffer = slotsData[slot].CBV;

					if (!(slotsData[slot].bindDirtyFlags & RBF_UNIFORM_BUFFER) && !buffer->dirty)
						continue;

					auto alloc = cmdList->fastAllocator->Allocate();
					memcpy(alloc.ptr, buffer->cache, buffer->dataSize);

					if (!state.pso.isCompute)
						d3dCmdList->SetGraphicsRootConstantBufferView(rootIdx, alloc.gpuPtr);
					else
						d3dCmdList->SetComputeRootConstantBufferView(rootIdx, alloc.gpuPtr);

					slotsData[slot].bindDirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(slotsData[slot].bindDirtyFlags & ~RBF_UNIFORM_BUFFER);
					buffer->dirty = false;
				}
					break;
				default:
					throw std::exception("Not impl");
					break;
			}
		}

		else if (rootParam.type == PARAMETER_TYPE::TABLE)
		{			
			bool isDirty = false;

			for (int i = 0; i < rootParam.tableResourcesNum && !isDirty; ++i)
			{
				int slot = rootParam.tableResources[i].slot;
				
				isDirty = isDirty || (slotsData[slot].bindDirtyFlags != 0);
				if (isDirty)
					break;

				if (Dx12UniformBuffer* buffer = slotsData[slot].CBV)
					isDirty = isDirty || buffer->dirty;
			}

			if (!isDirty)
				continue;
				
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHadle{ cmdList->gpuDescriptorHeapStartGPU.ptr + cmdList->gpuDescriptorsOffset * descriptorSizeCBSRV };

			for (int tableParamIdx = 0; tableParamIdx < rootParam.tableResourcesNum; ++tableParamIdx)
			{
				int slot = rootParam.tableResources[tableParamIdx].slot;

				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleGPUVisible{ cmdList->gpuDescriptorHeapStart.ptr + (tableParamIdx + cmdList->gpuDescriptorsOffset) * descriptorSizeCBSRV };
				D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleCPUVisible{};

				auto type = rootParam.tableResources[tableParamIdx].resources;

				if (type & RBF_UNIFORM_BUFFER)
					{
						auto alloc = cmdList->fastAllocator->Allocate();
						cpuHandleCPUVisible = alloc.descriptor;

						Dx12UniformBuffer* buffer = slotsData[slot].CBV;

						memcpy(alloc.ptr, buffer->cache, buffer->dataSize);
						buffer->dirty = false;

						REMOVE_DIRY_FLAGS(slotsData[slot].bindDirtyFlags, RBF_UNIFORM_BUFFER);
					}
				if (type & RBF_TEXTURE_SRV)
					{
						Dx12CoreTexture* texture = slotsData[slot].SRV.texture;

						cpuHandleCPUVisible = texture->GetSRV();

						REMOVE_DIRY_FLAGS(slotsData[slot].bindDirtyFlags, RBF_TEXTURE_SRV);
					}
				if (type & RBF_BUFFER_SRV)
					{
						Dx12CoreBuffer* buffer = slotsData[slot].SRV.buffer;

						cpuHandleCPUVisible = buffer->GetSRV();

						REMOVE_DIRY_FLAGS(slotsData[slot].bindDirtyFlags, RBF_BUFFER_SRV);
					}
				if (type & RBF_BUFFER_UAV)
					{
						Dx12CoreBuffer* buffer = slotsData[slot].UAV.buffer;

						cpuHandleCPUVisible = buffer->GetUAV();

						REMOVE_DIRY_FLAGS(slotsData[slot].bindDirtyFlags, RBF_BUFFER_UAV);
					}

				device->CopyDescriptorsSimple(1, cpuHandleGPUVisible, cpuHandleCPUVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			cmdList->gpuDescriptorsOffset += rootParam.tableResourcesNum;

			if (!state.pso.isCompute)
				d3dCmdList->SetGraphicsRootDescriptorTable((UINT)rootIdx, gpuHadle);
			else
				d3dCmdList->SetComputeRootDescriptorTable((UINT)rootIdx, gpuHadle);
		}
	}
}

void Dx12GraphicCommandContext::setComputePipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso)
{
	d3dCmdList->SetPipelineState(d3dpso);

	++statistic.stateChanges;

	state.pso.isCompute = true;
	state.pso.computePsoChecksum = newChecksum;
	state.pso.graphicPsoChecksum = 0;
	state.pso.d3dpso = d3dpso;
}

void Dx12GraphicCommandContext::setGraphicPipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso)
{
	d3dCmdList->SetPipelineState(d3dpso);

	++statistic.stateChanges;

	state.pso.isCompute = false;
	state.pso.graphicPsoChecksum = newChecksum;
	state.pso.computePsoChecksum = 0;
	state.pso.d3dpso = d3dpso;
}

void Dx12GraphicCommandContext::Draw(Dx12CoreVertexBuffer* vb, uint32_t vertexCount, uint32_t vertexOffset)
{
	bindResources();

	state.pso.vb = vb;
	cmdList->TrackResource(const_cast<Dx12CoreVertexBuffer*>(vb));

	if (vertexCount > 0)
	{
		assert(vertexCount <= vb->vertexCount);
	}

	if (vb->indexBuffer)
	{
		d3dCmdList->DrawIndexedInstanced(vertexCount > 0 ? vertexCount : vb->indexCount, 1, vertexOffset, 0, 0);
		statistic.triangles += vb->indexCount / 3;
	}
	else
	{
		d3dCmdList->DrawInstanced(vertexCount > 0 ? vertexCount : vb->vertexCount, 1, vertexOffset, 0);
		statistic.triangles += vb->vertexCount / 3;
	}

	++statistic.drawCalls;
}

void Dx12GraphicCommandContext::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	bindResources();
	d3dCmdList->Dispatch(x, y, z);
	++statistic.drawCalls;
}

void Dx12GraphicCommandContext::Clear()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(surface->descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, CR_RTV_DescriptorsSize());
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = surface->descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();

	FLOAT depth = 1.0f;
	d3dCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);

	const vec4 color{ 0.0f, 0.0f, 0.0f, 0.0f };
	d3dCmdList->ClearRenderTargetView(rtv, &color.x, 0, nullptr);
}

void Dx12GraphicCommandContext::BindUniformBuffer(int slot, Dx12UniformBuffer* buffer, SHADER_TYPE shaderType)
{
	int shaderNum = (int)shaderType;
	state.shaderTypesUsedBits |= 1<<shaderNum;
	auto& shaderData = state.binds[shaderNum];

	shaderData[slot].CBV = buffer;

	ADD_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_UNIFORM_BUFFER);
	ADD_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_UNIFORM_BUFFER);
}

void Dx12GraphicCommandContext::BindTexture(int slot, Dx12CoreTexture* tex, SHADER_TYPE shaderType)
{
	int shaderNum = (int)shaderType;
	state.shaderTypesUsedBits |= 1 << shaderNum;
	auto& shaderData = state.binds[shaderNum];

	shaderData[slot].SRV.texture = tex;

	REMOVE_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_BUFFER_SRV);
	REMOVE_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_BUFFER_SRV);
	ADD_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_TEXTURE_SRV);
	ADD_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_TEXTURE_SRV);
}

void Dx12GraphicCommandContext::BindSRVStructuredBuffer(int slot, Dx12CoreBuffer* buffer, SHADER_TYPE shaderType)
{
	int shaderNum = (int)shaderType;
	state.shaderTypesUsedBits |= 1 << shaderNum;
	auto& shaderData = state.binds[shaderNum];

	shaderData[slot].SRV.buffer = buffer;

	REMOVE_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_TEXTURE_SRV);
	REMOVE_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_TEXTURE_SRV);
	ADD_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_BUFFER_SRV);
	ADD_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_BUFFER_SRV);
}

void Dx12GraphicCommandContext::BindUAVStructuredBuffer(int slot, Dx12CoreBuffer* buffer, SHADER_TYPE shaderType)
{
	int shaderNum = (int)shaderType;
	state.shaderTypesUsedBits |= 1 << shaderNum;
	auto& shaderData = state.binds[shaderNum];

	shaderData[slot].UAV.buffer = buffer;

	REMOVE_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_TEXTURE_UAV);
	REMOVE_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_TEXTURE_UAV);
	ADD_DIRY_FLAGS(shaderData[slot].bindFlags, RBF_BUFFER_UAV);
	ADD_DIRY_FLAGS(shaderData[slot].bindDirtyFlags, RBF_BUFFER_UAV);

	if (buffer->resourceState() != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) // TODO: batch
	{
		d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(buffer->GetResource(), buffer->resourceState(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		buffer->resourceState() = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
}

void Dx12GraphicCommandContext::UpdateUniformBuffer(Dx12UniformBuffer* buffer, const void* data, size_t offset, size_t size)
{
	++statistic.uniformBufferUpdates;
	memcpy(buffer->cache + offset, data, size);
	buffer->dirty = true;
}

void Dx12GraphicCommandContext::EmitUAVBarrier(Dx12CoreBuffer* buffer)
{
	d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(buffer->GetResource()));
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

	double time = double(end - start)* gpuTickDelta;

	return static_cast<float>(time);
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12GraphicCommandContext::CommandList::newGPUHandle()
{
	assert(gpuDescriptorsOffset < MaxBindedResourcesPerFrame);

	D3D12_CPU_DESCRIPTOR_HANDLE ret;
	ret.ptr = gpuDescriptorHeapStart.ptr + size_t(gpuDescriptorsOffset) * CR_CBSRV_DescriptorsSize();
	++gpuDescriptorsOffset;
	return ret;
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
	fastAllocator->Reset();

	ReleaseTrakedResources();

	gpuDescriptorsOffset = 0;
	fenceOldValue = nextFenceID;
}

void Dx12GraphicCommandContext::CommandList::CommandsBegin()
{
	d3dCmdList->SetDescriptorHeaps(1, &gpuDescriptorHeap);
}

Dx12GraphicCommandContext::Dx12GraphicCommandContext(FinishFrameBroadcast finishFrameCallback_) :
	finishFrameBroadcast(finishFrameCallback_)
{
	device = CR_GetD3DDevice();

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3dCommandQueue)));

	for (int i = 0; i < DeferredBuffers; ++i)
		cmdLists[i].Init(this);

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
	queryHeap->SetName(L"Dx12commanbuffer query timers query heap");

	// We allocate MaxFrames + 1 instances as an instance is guaranteed to be written to if maxPresentFrameCount frames
	// have been submitted since. This is due to a fact that Present stalls when none of the m_maxframeCount frames are done/available.
	size_t FramesInstances = DeferredBuffers + 1;

	UINT64 size = FramesInstances * maxNumQuerySlots * sizeof(UINT64);
	x12::memory::CreateCommittedBuffer(&queryReadBackBuffer, size, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);

	queryReadBackBuffer->SetName(L"Dx12commandbuffer query timers readback buffer");

	descriptorSizeCBSRV = CR_CBSRV_DescriptorsSize();
	descriptorSizeDSV = CR_DSV_DescriptorsSize();
	descriptorSizeRTV = CR_RTV_DescriptorsSize();
}

void Dx12GraphicCommandContext::CommandList::Init(Dx12GraphicCommandContext* parent_)
{
	parent = parent_;
	device = CR_GetD3DDevice();

	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dCommandAllocator)));
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCmdList)));
	ThrowIfFailed(d3dCmdList->Close());

	gpuDescriptorHeap = CreateDescriptorHeap(device, MaxBindedResourcesPerFrame, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	gpuDescriptorHeapStart = gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuDescriptorHeapStartGPU = gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	fastAllocator = new x12::fastdescriptorallocator::Allocator;
}

void Dx12GraphicCommandContext::CommandList::Free()
{
	delete fastAllocator;
	Release(d3dCommandAllocator);
	Release(d3dCmdList);
	Release(gpuDescriptorHeap);
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
	{
		if (state.pso.computePsoChecksum != state_.pso.computePsoChecksum)
		{
			setComputePipeline(state_.pso.computePsoChecksum, state_.pso.d3dpso.Get());

			state.pso.shader = state_.pso.shader; // TODO:remove  copy paste
			state.pso.vb = state_.pso.vb;
			state.primitiveTopology = state_.primitiveTopology;
		}
	}
	else
	{
		if (state.pso.graphicPsoChecksum != state_.pso.graphicPsoChecksum)
		{
			setGraphicPipeline(state_.pso.graphicPsoChecksum, state_.pso.d3dpso.Get());

			state.pso.shader = state_.pso.shader;
			state.pso.vb = state_.pso.vb;
			state.primitiveTopology = state_.primitiveTopology;
		}
	}

	d3dCmdList->SetGraphicsRootSignature(state_.pso.rootSignature.Get());

	if (state_.pso.vb)
		SetVertexBuffer(state_.pso.vb.get());

	SetScissor(state_.scissor.left, state_.scissor.top, state_.scissor.right, state_.scissor.bottom);

	SetViewport((unsigned)state_.viewport.Width, (unsigned)state_.viewport.Height);

	// ...

	state = state_;

	for (auto& s : state.binds)
		for (auto &a : s)
			a.bindDirtyFlags = a.bindFlags; // need set all resources before draw

	statesStack.pop();
}

void Dx12GraphicCommandContext::CommandsBegin(surface_ptr surface_)
{
	if (surface_)
		surface = surface_;

	cmdList->d3dCommandAllocator->Reset();
	d3dCmdList->Reset(cmdList->d3dCommandAllocator, nullptr);
	cmdList->CommandsBegin();

	if (needReBindSurface)
		bindSurface();
}
void Dx12GraphicCommandContext::bindSurface()
{
	ID3D12Resource* backBuffer = surface->colorBuffers[frameIndex].Get();

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
																			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	d3dCmdList->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(surface->descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, CR_RTV_DescriptorsSize());
	D3D12_CPU_DESCRIPTOR_HANDLE dsv = surface->descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();

	d3dCmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

	needReBindSurface = false;
}
void Dx12GraphicCommandContext::CommandsEnd()
{
	needReBindSurface = true;

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

	if (surface)
	{
		ID3D12Resource* backBuffer = surface->colorBuffers[frameIndex].Get();

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		d3dCmdList->ResourceBarrier(1, &barrier);
	}

	ThrowIfFailed(d3dCmdList->Close());

	resetFullState();
}

void Dx12GraphicCommandContext::Submit()
{
	ID3D12CommandList* const commandLists[] = { d3dCmdList };
	d3dCommandQueue->ExecuteCommandLists(1, commandLists);
}

void Dx12GraphicCommandContext::resetPSOState()
{
	//state.viewport = {};
	//state.scissor = {};
	state.pso = {};
	//state.primitiveTopology = {};
	state.shaderTypesUsedBits = 0;
	memset(&state.binds, 0, sizeof(state.binds));
}

void Dx12GraphicCommandContext::resetFullState()
{
	state = {};
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
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCommandList)));
	ThrowIfFailed(d3dCommandList->Close());

	ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3dCommandQueue)));
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
	ID3D12CommandList* const commandLists[1] = { d3dCommandList };
	d3dCommandQueue->ExecuteCommandLists(1, commandLists);

	Signal(d3dCommandQueue, d3dFence, fenceValue);
}

void Dx12CopyCommandContext::WaitGPUAll()
{
	uint64_t fenceIDEmited = Signal(d3dCommandQueue, d3dFence, fenceValue);
	WaitForFenceValue(d3dFence, fenceIDEmited, fenceEvent);
}
