#include "pch.h"
#include "dx12context.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12uniformbuffer.h"
#include "dx12vertexbuffer.h"
#include "dx12structuredbuffer.h"
#include "dx12texture.h"
#include "core.h"
#include <map>
#include <chrono>

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

uint64_t CalculateChecksum(const PipelineState& pso)
{
	uint64_t checksum = 0;
	checksum |= static_cast<Dx12CoreVertexBuffer*>(pso.vb)->ID();
	checksum |= static_cast<Dx12CoreShader*>(pso.shader)->ID() << 16;
	return checksum;
}

void Dx12GraphicCommandContext::SetPipelineState(const PipelineState& pso)
{
	uint64_t checksum = CalculateChecksum(pso);

	if (checksum == this->state.psoChecksum)
		return;

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader);
	Dx12CoreVertexBuffer* dx12vb = static_cast<Dx12CoreVertexBuffer*>(pso.vb);

	auto setState = [this, pso, dx12Shader](ID3D12PipelineState* state, uint64_t checksum)
	{
		resetState();

		cmdList->TrackResource(pso.vb);
		cmdList->TrackResource(pso.shader);

		this->state.shader = dx12Shader;
		this->state.psoChecksum = checksum;

		cmdList->d3dCmdList->SetPipelineState(state);
		++this->statistic.stateChanges;

		if (!dx12Shader->HasResources())
			cmdList->d3dCmdList->SetGraphicsRootSignature(GetCoreRender()->GetDefaultRootSignature());
		else
			cmdList->d3dCmdList->SetGraphicsRootSignature(this->state.shader->resourcesRootSignature.Get());
	};

	auto it = CR_GetGlobalPSOMap().find(checksum);
	if (it != CR_GetGlobalPSOMap().end())
	{
		ComPtr<ID3D12PipelineState> state = CR_GetGlobalPSOMap()[checksum];
		setState(state.Get(), checksum);
		return;
	}

	// Create PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout = {  &dx12vb->inputLayout[0], (UINT)dx12vb->inputLayout.size() };
	desc.pRootSignature = dx12Shader->HasResources() ? dx12Shader->resourcesRootSignature.Get() :
		GetCoreRender()->GetDefaultRootSignature();
	desc.VS = CD3DX12_SHADER_BYTECODE(dx12Shader->vs.Get());
	desc.PS = CD3DX12_SHADER_BYTECODE(dx12Shader->ps.Get());
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = UINT_MAX;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> d3dPipelineState;
	ThrowIfFailed(CR_GetD3DDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(d3dPipelineState.GetAddressOf())));

	CR_GetGlobalPSOMap()[checksum] = d3dPipelineState;

	setState(d3dPipelineState.Get(), checksum);
}

void Dx12GraphicCommandContext::SetVertexBuffer(Dx12CoreVertexBuffer* vb)// TODO add vb to tracked resources
{
	state.vb = vb;
	cmdList->d3dCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->d3dCmdList->IASetVertexBuffers(0, 1, &vb->vertexBufferView);
	cmdList->d3dCmdList->IASetIndexBuffer(&vb->indexBufferView);
}

void Dx12GraphicCommandContext::SetViewport(unsigned width, unsigned heigth)
{
	CD3DX12_VIEWPORT viewport(0.0f, 0.0f, (float)width, (float)heigth);
	cmdList->d3dCmdList->RSSetViewports(1, &viewport);
}

void Dx12GraphicCommandContext::SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth)
{
	CD3DX12_RECT rect(x, y, width, heigth);
	cmdList->d3dCmdList->RSSetScissorRects(1, &rect);
}

void Dx12GraphicCommandContext::GetBufferSize(unsigned& width, unsigned& heigth)
{
	assert(surface && "Dx12GraphicCommandContext::GetBufferSize() no surface for command buffer");
	width = surface->width;
	heigth = surface->height;
}

void Dx12GraphicCommandContext::bindResources()
{
	assert(state.shader && "Shader must be set");

	using RootSignatureParameter = Dx12CoreShader::RootSignatureParameter;
	using PARAMETER_TYPE = Dx12CoreShader::PARAMETER_TYPE;
	
	std::vector<RootSignatureParameter>& params = state.shader->rootSignatureParameters;

	for (int rootIdx = 0; rootIdx < params.size(); ++rootIdx)
	{
		RootSignatureParameter& rootParam = params[rootIdx];

		State::ShaderResources& shaderResources = state.bind[(int)rootParam.shaderType];
		if (!shaderResources.dirty)
			continue;
		
		if (rootParam.type == PARAMETER_TYPE::INLINE_DESCRIPTOR)
		{
			int slot = rootParam.inlineResource.slot;

			switch (rootParam.inlineResource.resources)
			{
				case UNIFORM_BUFFER: 
				{
					Dx12UniformBuffer* buffer = shaderResources.resources[slot].CBV;

					if (buffer == nullptr)
						throw std::exception("Resource is not set");

					if (!shaderResources.resources[slot].dirtyFlags != RESOURCE_BIND_FLAGS::NONE && !buffer->dirty)
						continue;

					auto alloc = cmdList->fastAllocator->Allocate();
					memcpy(alloc.ptr, buffer->cache, buffer->dataSize);

					cmdList->d3dCmdList->SetGraphicsRootConstantBufferView(rootIdx, alloc.gpuPtr);

					shaderResources.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(shaderResources.resources[slot].dirtyFlags & ~RESOURCE_BIND_FLAGS::UNIFORM_BUFFER);
					buffer->dirty = false;
				}
					break;
				case TEXTURE_SRV:
					throw std::exception("Not impl");
					break;
				default:
					throw std::exception("Not impl");
					break;
			}
		}

		else if (rootParam.type == PARAMETER_TYPE::TABLE)
		{			
			bool changed = false;

			for (int i = 0; i < rootParam.tableResourcesNum && !changed; ++i)
			{
				int slot = rootParam.tableResources[i].slot;
				
				if (shaderResources.resources[slot].dirtyFlags & RESOURCE_BIND_FLAGS::UNIFORM_BUFFER)
				{
					Dx12UniformBuffer* buffer = shaderResources.resources[slot].CBV;
					changed = changed || buffer->dirty;
				}

				if (shaderResources.resources[slot].dirtyFlags & RESOURCE_BIND_FLAGS::TEXTURE_SRV)
					changed = true;
			}

			if (!changed)
				continue;
				
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHadle{ cmdList->gpuDescriptorHeapStartGPU.ptr + cmdList->gpuDescriptorsOffset * descriptorSizeCBSRV };

			for (int tableParamIdx = 0; tableParamIdx < rootParam.tableResourcesNum; ++tableParamIdx)
			{
				int slot = rootParam.tableResources[tableParamIdx].slot;

				switch (rootParam.tableResources[tableParamIdx].resources)
				{
					case UNIFORM_BUFFER:
					{
						D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleGPUVisible{ cmdList->gpuDescriptorHeapStart.ptr + (tableParamIdx + cmdList->gpuDescriptorsOffset) * descriptorSizeCBSRV };

						auto alloc = cmdList->fastAllocator->Allocate();
						D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandleCPUVisible = alloc.descriptor;

						Dx12UniformBuffer* buffer = shaderResources.resources[slot].CBV;

						if (buffer == nullptr)
							throw std::exception("Resource is not set");

						memcpy(alloc.ptr, buffer->cache, buffer->dataSize);
						buffer->dirty = false;

						CR_GetD3DDevice()->CopyDescriptorsSimple(1, cpuHandleGPUVisible, cpuHandleCPUVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

						shaderResources.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(shaderResources.resources[slot].dirtyFlags & ~RESOURCE_BIND_FLAGS::UNIFORM_BUFFER);
					}
					break;

					case TEXTURE_SRV:
					{
						D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleGPUVisible{ cmdList->gpuDescriptorHeapStart.ptr + (tableParamIdx + cmdList->gpuDescriptorsOffset) * descriptorSizeCBSRV };

						Dx12CoreTexture* texture = shaderResources.resources[slot].SRV.texture;

						if (texture == nullptr)
							throw std::exception("Resource is not set");

						auto cpuHandleCPUVisible = texture->GetHandle();

						CR_GetD3DDevice()->CopyDescriptorsSimple(1, cpuHandleGPUVisible, cpuHandleCPUVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

						shaderResources.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(shaderResources.resources[slot].dirtyFlags & ~RESOURCE_BIND_FLAGS::TEXTURE_SRV);

					}
					break;

					case STRUCTURED_BUFFER_SRV:
					{
						D3D12_CPU_DESCRIPTOR_HANDLE cpuHandleGPUVisible{ cmdList->gpuDescriptorHeapStart.ptr + (tableParamIdx + cmdList->gpuDescriptorsOffset) * descriptorSizeCBSRV };

						Dx12CoreStructuredBuffer* buffer = shaderResources.resources[slot].SRV.structuredBuffer;

						if (buffer == nullptr)
							throw std::exception("Resource is not set");

						auto cpuHandleCPUVisible = buffer->GetHandle();

						CR_GetD3DDevice()->CopyDescriptorsSimple(1, cpuHandleGPUVisible, cpuHandleCPUVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

						shaderResources.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(shaderResources.resources[slot].dirtyFlags & ~RESOURCE_BIND_FLAGS::STRUCTURED_BUFFER_SRV);

					}
					break;

					default:
						throw std::exception("Not impl");
						break;
				}
			}

			cmdList->gpuDescriptorsOffset += rootParam.tableResourcesNum;
			cmdList->d3dCmdList->SetGraphicsRootDescriptorTable((UINT)rootIdx, gpuHadle);
		}
	}
}

void Dx12GraphicCommandContext::Draw(Dx12CoreVertexBuffer* vb)
{
	if (state.d3dDescriptorHeap != cmdList->gpuDescriptorHeap)
	{
		state.d3dDescriptorHeap = cmdList->gpuDescriptorHeap;
		cmdList->d3dCmdList->SetDescriptorHeaps(1, &state.d3dDescriptorHeap);
	}

	bindResources();

	state.vb = vb;
	cmdList->TrackResource(const_cast<Dx12CoreVertexBuffer*>(vb));

	if (vb->indexBuffer)
	{
		cmdList->d3dCmdList->DrawIndexedInstanced(vb->indexCount, 1, 0, 0, 0);
		statistic.triangles += vb->indexCount / 3;
	}
	else
	{
		cmdList->d3dCmdList->DrawInstanced(vb->vertexCount, 1, 0, 0);
		statistic.triangles += vb->vertexCount / 3;
	}

	++statistic.drawCalls;
}

void Dx12GraphicCommandContext::BindUniformBuffer(int slot, Dx12UniformBuffer* buffer, SHADER_TYPE shaderType)
{
	State::ShaderResources& bindings = state.bind[(int)shaderType];
	bindings.resources[slot].CBV = buffer;
	bindings.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(bindings.resources[slot].dirtyFlags | RESOURCE_BIND_FLAGS::UNIFORM_BUFFER);
	bindings.dirty = true;
}

void Dx12GraphicCommandContext::BindTexture(int slot, Dx12CoreTexture* tex, SHADER_TYPE shaderType)
{
	State::ShaderResources& bindings = state.bind[(int)shaderType];
	bindings.resources[slot].SRV.texture = tex;
	bindings.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(bindings.resources[slot].dirtyFlags & ~RESOURCE_BIND_FLAGS::STRUCTURED_BUFFER_SRV);
	bindings.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(bindings.resources[slot].dirtyFlags | RESOURCE_BIND_FLAGS::TEXTURE_SRV);
	bindings.dirty = true;
}

void Dx12GraphicCommandContext::BindStructuredBuffer(int slot, Dx12CoreStructuredBuffer* buffer, SHADER_TYPE shaderType)
{
	State::ShaderResources& bindings = state.bind[(int)shaderType];
	bindings.resources[slot].SRV.structuredBuffer = buffer;
	bindings.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(bindings.resources[slot].dirtyFlags & ~RESOURCE_BIND_FLAGS::TEXTURE_SRV);
	bindings.resources[slot].dirtyFlags = static_cast<RESOURCE_BIND_FLAGS>(bindings.resources[slot].dirtyFlags | RESOURCE_BIND_FLAGS::STRUCTURED_BUFFER_SRV);
	bindings.dirty = true;
}

void Dx12GraphicCommandContext::UpdateUniformBuffer(Dx12UniformBuffer* buffer, const void* data, size_t offset, size_t size)
{
	++statistic.uniformBufferUpdates;
	memcpy(buffer->cache + offset, data, size);
	buffer->dirty = true;
}

void Dx12GraphicCommandContext::TimerBegin(uint32_t timerID)
{
	assert(timerID < maxNumTimers && "Timer ID out of range");
	cmdList->d3dCmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timerID * 2);
}

void Dx12GraphicCommandContext::TimerEnd(uint32_t timerID)
{
	assert(timerID < maxNumTimers && "Timer ID out of range");
	cmdList->d3dCmdList->EndQuery(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, timerID * 2 + 1);
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
	ret.ptr = gpuDescriptorHeapStart.ptr + gpuDescriptorsOffset * CR_CBSRV_DescriptorsSize();
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
	fastAllocator->FreeMemory();

	ReleaseTrakedResources();

	gpuDescriptorsOffset = 0;
	fenceOldValue = nextFenceID;
}
void Dx12GraphicCommandContext::CommandList::Begin()
{
}

Dx12GraphicCommandContext::Dx12GraphicCommandContext(Dx12WindowSurface* surface_, FinishFrameBroadcast finishFrameCallback_) :
	surface(surface_),
	finishFrameBroadcast(finishFrameCallback_)
{
	ThrowIfFailed(CR_GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3dCommandQueue)));

	for (int i = 0; i < DeferredBuffers; ++i)
		cmdLists[i].Init(this);

	cmdList = &cmdLists[0];

	// Query
	queryTiming.resize(maxNumQuerySlots);

	uint64_t gpuFrequency;
	d3dCommandQueue->GetTimestampFrequency(&gpuFrequency);
	gpuTickDelta = 1000.0 / static_cast<double>(gpuFrequency);

	D3D12_QUERY_HEAP_DESC QueryHeapDesc = {};
	QueryHeapDesc.Count = maxNumQuerySlots;
	QueryHeapDesc.NodeMask = 1;
	QueryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	ThrowIfFailed(CR_GetD3DDevice()->CreateQueryHeap(&QueryHeapDesc, IID_PPV_ARGS(&queryHeap)));
	queryHeap->SetName(L"Dx12commanbuffer query timers query heap");

	// We allocate MaxFrames + 1 instances as an instance is guaranteed to be written to if maxPresentFrameCount frames
	// have been submitted since. This is due to a fact that Present stalls when none of the m_maxframeCount frames are done/available.
	size_t FramesInstances = DeferredBuffers + 1;

	auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(FramesInstances * maxNumQuerySlots * sizeof(UINT64));
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&queryReadBackBuffer))
	);
	queryReadBackBuffer->SetName(L"Dx12commandbuffer query timers readback buffer");

	descriptorSizeCBSRV = CR_CBSRV_DescriptorsSize();
	descriptorSizeDSV = CR_DSV_DescriptorsSize();
	descriptorSizeRTV = CR_RTV_DescriptorsSize();
}

void Dx12GraphicCommandContext::CommandList::Init(Dx12GraphicCommandContext* parent_)
{
	parent = parent_;

	ThrowIfFailed(CR_GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dCommandAllocator)));
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCmdList)));
	ThrowIfFailed(d3dCmdList->Close());

	gpuDescriptorHeap = CreateDescriptorHeap(CR_GetD3DDevice(), MaxBindedResourcesPerFrame, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	gpuDescriptorHeapStart = gpuDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	gpuDescriptorHeapStartGPU = gpuDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	FastFrameAllocator::PagePool * pool = GetCoreRender()->GetFastFrameAllocatorPool(256); // TODO
	fastAllocator = new FastFrameAllocator::Allocator(pool);
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
	WaitGPUAll();

	for (int i = 0; i < DeferredBuffers; ++i)
		cmdLists[i].Free();

	Release(d3dCommandQueue);
	Release(d3dFence);
	Release(queryHeap);
	Release(queryReadBackBuffer);
}

void Dx12GraphicCommandContext::ClearBuiltinRenderTarget(vec4 color)
{
	assert(surface);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(surface->descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, CR_RTV_DescriptorsSize());
	cmdList->d3dCmdList->ClearRenderTargetView(rtv, &color.x, 0, nullptr);
}

void Dx12GraphicCommandContext::ClearBuiltinRenderDepthBuffer()
{
	assert(surface);
	FLOAT depth = 1.0f;
	auto dsv = surface->descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();
	cmdList->d3dCmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}
void Dx12GraphicCommandContext::SetBuiltinRenderTarget()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(surface->descriptorHeapRTV->GetCPUDescriptorHandleForHeapStart(), frameIndex, CR_RTV_DescriptorsSize());
	auto dsv = surface->descriptorHeapDSV->GetCPUDescriptorHandleForHeapStart();
	cmdList->d3dCmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
}

void Dx12GraphicCommandContext::Begin()
{
	assert(surface);
	cmdList->Begin();
	cmdList->d3dCommandAllocator->Reset();
	cmdList->d3dCmdList->Reset(cmdList->d3dCommandAllocator, nullptr);

	ID3D12Resource *backBuffer = surface->colorBuffers[frameIndex].Get();

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->d3dCmdList->ResourceBarrier(1, &barrier);
}
void Dx12GraphicCommandContext::End()
{
	assert(surface);

	// Query

	// Write to buffer current time on GPU
	UINT64 resolveAddress = queryResolveToFrameID * maxNumQuerySlots * sizeof(UINT64);
	cmdList->d3dCmdList->ResolveQueryData(queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, maxNumQuerySlots, queryReadBackBuffer, resolveAddress);

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


	ID3D12Resource* backBuffer = surface->colorBuffers[frameIndex].Get();

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
																			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	cmdList->d3dCmdList->ResourceBarrier(1, &barrier);


	ThrowIfFailed(cmdList->d3dCmdList->Close());
}

void Dx12GraphicCommandContext::Submit()
{
	ID3D12CommandList* const commandLists[1] = { cmdList->d3dCmdList };
	d3dCommandQueue->ExecuteCommandLists(1, commandLists);
}

void Dx12GraphicCommandContext::Present()
{
	assert(surface);
	
	UINT syncInterval = CR_IsVSync() ? 1 : 0;
	UINT presentFlags = CR_IsTearingSupport() && !CR_IsVSync() ? DXGI_PRESENT_ALLOW_TEARING : 0;

	ThrowIfFailed(surface->swapChain->Present(syncInterval, presentFlags));
}

void Dx12GraphicCommandContext::resetState()
{
	state.vb = nullptr;
	state.shader = nullptr;
	state.d3dDescriptorHeap = nullptr;

	// TODO: GPU reset
	memset(&state, 0, sizeof(state.bind));
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

	// Fence ID that we want to wait
	uint64_t fenceIDToWait = cmdList->fenceOldValue;

	// Wait GPU for prevois fenceID
	WaitForFenceValue(d3dFence, fenceIDToWait, fenceEvent);

	// Here frame fenceIDToWait is completed on GPU

	cmdList->CompleteGPUFrame(fenceValue);
	
	resetState();
	resetStatistic();

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

	resetState();
	resetStatistic();

	if (finishFrameBroadcast)
		finishFrameBroadcast(fenceIDEmited);
}


// Dx12CopyCommandContext
//
Dx12CopyCommandContext::Dx12CopyCommandContext()
{
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&d3dCommandAllocator)));
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCommandList)));
	ThrowIfFailed(d3dCommandList->Close());

	ThrowIfFailed(CR_GetD3DDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3dFence)));

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	// Command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	ThrowIfFailed(CR_GetD3DDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3dCommandQueue)));
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

void Dx12CopyCommandContext::Begin()
{
	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);
}

void Dx12CopyCommandContext::End()
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
