
#include "dx12commandlist.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12vertexbuffer.h"
#include "dx12buffer.h"
#include "dx12texture.h"
#include "dx12query.h"
#include "dx12resourceset.h"
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
	Dx12CoreQuery* resource_cast(ICoreQuery* vb) { return static_cast<Dx12CoreQuery*>(vb); }
	const Dx12CoreQuery* resource_cast(const ICoreQuery* vb) { return static_cast<const Dx12CoreQuery*>(vb); }
}

int Dx12GraphicCommandList::contextNum;
int Dx12CopyCommandList::contextNum;

void Dx12GraphicCommandList::SetGraphicPipelineState(const GraphicPipelineState& pso)
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

	if (!heapBinded)
	{
		d3dCmdList->SetDescriptorHeaps(1, renderer->DescriptorHeapPtr());
		heapBinded = true;
	}
}

void Dx12GraphicCommandList::SetComputePipelineState(const ComputePipelineState& pso)
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

	if (!heapBinded)
	{
		d3dCmdList->SetDescriptorHeaps(1, renderer->DescriptorHeapPtr());
		heapBinded = true;
	}
}

void Dx12GraphicCommandList::SetVertexBuffer(ICoreVertexBuffer* vb)// TODO add vb to tracked resources
{
	assert(state.pso.PsoChecksum > 0 && "PSO is not set");

	Dx12CoreVertexBuffer* dxBuffer = resource_cast(vb);

	state.pso.graphicDesc.vb = dxBuffer;

	UINT numBarriers;
	D3D12_RESOURCE_BARRIER barriers[2];

	if (dxBuffer->GetReadBarrier(&numBarriers, barriers))
		d3dCmdList->ResourceBarrier(numBarriers, barriers);

	d3dCmdList->IASetVertexBuffers(0, (UINT)dxBuffer->vertexBufferView.size(), &dxBuffer->vertexBufferView[0]);
	d3dCmdList->IASetIndexBuffer(dxBuffer->pIndexBufferVew());
}

void Dx12GraphicCommandList::SetViewport(unsigned width, unsigned heigth)
{
	D3D12_VIEWPORT v{0.0f, 0.0f, (float)width, (float)heigth, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH};
	if (memcmp(&state.viewport, &v, sizeof(D3D12_VIEWPORT)) != 0)
	{
		d3dCmdList->RSSetViewports(1, &v);
		memcpy(&state.viewport, &v, sizeof(D3D12_VIEWPORT));
	}
}

void Dx12GraphicCommandList::GetViewport(unsigned& width, unsigned& heigth)
{
	width = (unsigned)state.viewport.Width;
	heigth = (unsigned)state.viewport.Height;
}

void Dx12GraphicCommandList::SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth)
{
	D3D12_RECT r{LONG(x), LONG(y), LONG(width), LONG(heigth)};
	if (memcmp(&state.scissor, &r, sizeof(D3D12_RECT)) != 0)
	{
		d3dCmdList->RSSetScissorRects(1, &r);
		memcpy(&state.scissor, &r, sizeof(D3D12_RECT));
	}
}

void Dx12GraphicCommandList::setComputePipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso)
{
	d3dCmdList->SetPipelineState(d3dpso);

	++frameStatistic.stateChanges;

	state.pso.isCompute = true;
	state.pso.PsoChecksum = newChecksum;
	state.pso.d3dpso = d3dpso;
}

void Dx12GraphicCommandList::setGraphicPipeline(psomap_checksum_t newChecksum, ID3D12PipelineState* d3dpso)
{
	d3dCmdList->SetPipelineState(d3dpso);

	++frameStatistic.stateChanges;

	state.pso.isCompute = false;
	state.pso.PsoChecksum = newChecksum;
	state.pso.d3dpso = d3dpso;
}

void Dx12GraphicCommandList::Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount, uint32_t vertexOffset)
{
	const Dx12CoreVertexBuffer* dx12Vb = resource_cast(vb);

	if (state.pso.graphicDesc.vb.get() != const_cast<Dx12CoreVertexBuffer*>(dx12Vb))
		SetVertexBuffer(const_cast<ICoreVertexBuffer*>(vb));

	TrackResource(const_cast<Dx12CoreVertexBuffer*>(dx12Vb));

	if (vertexCount > 0)
	{
		assert(vertexCount <= dx12Vb->vertexCount);
	}

	if (dx12Vb->indexBuffer)
	{
		d3dCmdList->DrawIndexedInstanced(vertexCount > 0 ? vertexCount : dx12Vb->indexCount, 1, vertexOffset, 0, 0);
		frameStatistic.triangles += dx12Vb->indexCount / 3;
	}
	else
	{
		d3dCmdList->DrawInstanced(vertexCount > 0 ? vertexCount : dx12Vb->vertexCount, 1, vertexOffset, 0);
		frameStatistic.triangles += dx12Vb->vertexCount / 3;
	}

	++frameStatistic.drawCalls;
}

void Dx12GraphicCommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	d3dCmdList->Dispatch(x, y, z);
	++frameStatistic.drawCalls;
}

void Dx12GraphicCommandList::Clear()
{
	FLOAT depth = 1.0f;
	const float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	if (state.surface)
	{
		Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());

		d3dCmdList->ClearDepthStencilView(resource_cast(dx12surface->depthBuffer.get())->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
		d3dCmdList->ClearRenderTargetView(resource_cast(dx12surface->colorBuffers[renderer->FrameIndex()].get())->GetRTV(), color, 0, nullptr);
	}
	else
	{
		if (state.depthStencil)
		{
			Dx12CoreTexture* dx12surface = static_cast<Dx12CoreTexture*>(state.depthStencil.get());
			d3dCmdList->ClearDepthStencilView(dx12surface->GetDSV(), D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
		}

		for (int i = 0; i < 8; i++)
		{
			if (state.renderTarget[i])
			{
				Dx12CoreTexture* dx12surface = static_cast<Dx12CoreTexture*>(state.renderTarget[i].get());
				d3dCmdList->ClearRenderTargetView(dx12surface->GetRTV(), color, 0, nullptr);
			}
		}
	}
}

void Dx12GraphicCommandList::CompileSet(IResourceSet* set_)
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

			auto [destCPU, destGPU] = renderer->AllocateSRVDescriptor(param.tableResourcesNum);

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
				else if (res.resources & RESOURCE_DEFINITION::RBF_TEXTURE_UAV)
					cpuHandleCPUVisible = res.texture->GetUAV();
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

void Dx12GraphicCommandList::BindResourceSet(IResourceSet* set_)
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

void Dx12GraphicCommandList::UpdateInlineConstantBuffer(size_t rootParameterIdx, const void* data, size_t size)
{
	DirectX::GraphicsResource alloc = renderer->FrameMemory()->Allocate(size);

	memcpy(alloc.Memory(), data, size);

	if (!state.pso.isCompute) [[likely]]
		d3dCmdList->SetGraphicsRootConstantBufferView((UINT)rootParameterIdx, alloc.GpuAddress());
	else
		d3dCmdList->SetComputeRootConstantBufferView((UINT)rootParameterIdx, alloc.GpuAddress());
}

void Dx12GraphicCommandList::EmitUAVBarrier(ICoreBuffer* buffer)
{
	auto* dx12res = resource_cast(buffer);
	d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(dx12res->GetResource()));
}

void x12::Dx12GraphicCommandList::StartQuery(ICoreQuery* query)
{
	Dx12CoreQuery* dx12query = resource_cast(query);
	d3dCmdList->EndQuery(dx12query->Heap(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
}

void x12::Dx12GraphicCommandList::StopQuery(ICoreQuery* query)
{
	Dx12CoreQuery* dx12query = resource_cast(query);

	d3dCmdList->EndQuery(dx12query->Heap(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
	d3dCmdList->ResolveQueryData(dx12query->Heap(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, dx12query->ReadbackBuffer(), 0);
}

void* x12::Dx12GraphicCommandList::GetNativeResource()
{
	return d3dCmdList;
}

void Dx12GraphicCommandList::TrackResource(IResourceUnknown* res)
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

void Dx12GraphicCommandList::ReleaseTrakedResources()
{
	for (IResourceUnknown* res : trakedResources)
		res->Release();
	trakedResources.clear();
}

void Dx12GraphicCommandList::NotifyFrameCompleted(uint64_t completed)
{
	if (submitedValue <= completed)
	{
		ReleaseTrakedResources();
	}

	ICoreCopyCommandList::NotifyFrameCompleted(completed);
}

Dx12GraphicCommandList::Dx12GraphicCommandList(Dx12CoreRenderer *renderer_, int32_t id_) : ICoreGraphicCommandList(id_),
	renderer(renderer_)
{
	device = CR_GetD3DDevice();

	throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3dCommandAllocator)));
	Dx12GraphicCommandList::set_ctx_object_name(d3dCommandAllocator, L"command allocator #%d", contextNum);

	throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCmdList)));
	Dx12GraphicCommandList::set_ctx_object_name(d3dCommandAllocator, L"command list #%d", contextNum);
	throwIfFailed(d3dCmdList->Close());

	descriptorSizeCBSRV = CR_CBSRV_DescriptorsSize();
	descriptorSizeDSV = CR_DSV_DescriptorsSize();
	descriptorSizeRTV = CR_RTV_DescriptorsSize();

	contextNum++;
}

Dx12GraphicCommandList::~Dx12GraphicCommandList()
{
	Free();
}

void Dx12GraphicCommandList::Free()
{
	ReleaseTrakedResources();

	statesStack = {};

	Release(d3dCommandAllocator);
	Release(d3dCmdList);
}

void Dx12GraphicCommandList::FrameEnd()
{
	resetStatistic();
}

void Dx12GraphicCommandList::PushState()
{
	statesStack.push(state);
}

void Dx12GraphicCommandList::PopState()
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

void Dx12GraphicCommandList::CommandsBegin()
{
	d3dCommandAllocator->Reset();
	d3dCmdList->Reset(d3dCommandAllocator, nullptr);

	assert(state_ == State::Free || state_ == State::Recordered);
	state_ = State::Opened;
}

void Dx12GraphicCommandList::CommandsEnd()
{
	heapBinded = false;

	assert(state_ == State::Opened);
	state_ = State::Recordered;

	if (state.surface)
	{
		Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());
		resource_cast(dx12surface->colorBuffers[renderer->FrameIndex()].get())->TransiteToState(D3D12_RESOURCE_STATE_PRESENT, d3dCmdList);
	}

	throwIfFailed(d3dCmdList->Close());

	resetFullState();
}

void Dx12GraphicCommandList::BindSurface(surface_ptr& surface_)
{
	if (surface_ == state.surface)
		return;

	state.surface = surface_;

	for (int i = 0; i < 8; i++)
		state.renderTarget[i] = nullptr;

	Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());

	resource_cast(dx12surface->colorBuffers[renderer->FrameIndex()].get())->TransiteToState(D3D12_RESOURCE_STATE_RENDER_TARGET, d3dCmdList);

	d3dCmdList->OMSetRenderTargets(1, &resource_cast(dx12surface->colorBuffers[renderer->FrameIndex()].get())->GetRTV(), FALSE, &resource_cast(dx12surface->depthBuffer.get())->GetDSV());
}

void x12::Dx12GraphicCommandList::SetRenderTargets(ICoreTexture** textures, uint32_t count, ICoreTexture* depthStencil)
{
	state.surface = nullptr;

	D3D12_CPU_DESCRIPTOR_HANDLE rtv[8];

	Dx12CoreTexture* dx12depthStenciltexture = static_cast<Dx12CoreTexture*>(depthStencil);
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	dsv.ptr = dx12depthStenciltexture ? dx12depthStenciltexture->GetDSV().ptr : 0;
	state.depthStencil = depthStencil;

	for (uint32_t i = 0; i < 8; i++)
	{
		if (i < count)
		{
			state.renderTarget[i] = textures[i];

			Dx12CoreTexture* dx12texture = static_cast<Dx12CoreTexture*>(textures[i]);
			dx12texture->TransiteToState(D3D12_RESOURCE_STATE_RENDER_TARGET, d3dCmdList);

			rtv[i] = dx12texture->GetRTV();
		}
		else
		{
			state.renderTarget[i] = nullptr;
		}
	}	

	d3dCmdList->OMSetRenderTargets(count, count? rtv : nullptr, FALSE, depthStencil ? &dsv : nullptr);
}

void Dx12GraphicCommandList::resetOnlyPSOState()
{
	state.pso = {};
	state.set_ = {};
}

void Dx12GraphicCommandList::resetFullState()
{
	state = {};
}

//void Dx12GraphicCommandList::transiteSurfaceToState(D3D12_RESOURCE_STATES newState)
//{
//	if (!state.surface)
//		return;
//
//	Dx12WindowSurface* dx12surface = static_cast<Dx12WindowSurface*>(state.surface.get());
//
//	if (dx12surface->state == newState)
//		return;
//
//	ID3D12Resource* backBuffer = resource_cast(dx12surface->colorBuffers[renderer->FrameIndex()].get())->re;
//
//	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
//																			dx12surface->state, newState);
//
//	d3dCmdList->ResourceBarrier(1, &barrier);
//
//	dx12surface->state = newState;
//}

void Dx12GraphicCommandList::resetStatistic()
{
	memset(&frameStatistic, 0, sizeof(Statistic));
}

// Dx12CopyCommandList
//
Dx12CopyCommandList::Dx12CopyCommandList() : ICoreCopyCommandList(-1)
{
	device = CR_GetD3DDevice();

	throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&d3dCommandAllocator)));
	set_ctx_object_name(d3dCommandAllocator, L"command allocator");

	throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, d3dCommandAllocator, nullptr, IID_PPV_ARGS(&d3dCommandList)));
	set_ctx_object_name(d3dCommandAllocator, L"command list");
	throwIfFailed(d3dCommandList->Close());

	contextNum++;
}

Dx12CopyCommandList::~Dx12CopyCommandList()
{
	assert(d3dCommandAllocator == nullptr && "Free() must be called before destructor");
	assert(d3dCommandList == nullptr && "Free() must be called before destructor");
}

void x12::Dx12CopyCommandList::FrameEnd()
{
}

void Dx12CopyCommandList::Free()
{
	Release(d3dCommandAllocator);
	Release(d3dCommandList);
}

void Dx12CopyCommandList::CommandsBegin()
{
	d3dCommandAllocator->Reset();
	d3dCommandList->Reset(d3dCommandAllocator, nullptr);
}

void Dx12CopyCommandList::CommandsEnd()
{
	throwIfFailed(d3dCommandList->Close());
}
