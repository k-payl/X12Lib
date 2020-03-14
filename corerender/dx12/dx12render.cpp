#include "dx12render.h"
#include "pch.h"
#include "dx12render.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12uniformbuffer.h"
#include "dx12buffer.h"
#include "dx12context.h"
#include "dx12vertexbuffer.h"
#include "dx12texture.h"
#include "dx12descriptorheap.h"
#include "dx12memory.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <inttypes.h>

Dx12CoreRenderer* _coreRender;

void Dx12CoreRenderer::ReleaseFrame(uint64_t fenceID)
{
	descriptorAllocator->ReclaimMemory(fenceID);
}

void Dx12CoreRenderer::sReleaseFrameCallback(uint64_t fenceID)
{
	_coreRender->ReleaseFrame(fenceID);
}

Dx12CoreRenderer::Dx12CoreRenderer()
{
	assert(_coreRender == nullptr && "Should be created only one instance of Dx12CoreRenderer");
	_coreRender = this;
}
Dx12CoreRenderer::~Dx12CoreRenderer()
{
	_coreRender = nullptr;
}

void Dx12CoreRenderer::Init()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif

	{
		ComPtr<dxgifactory_t> dxgiFactory;
		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

		ComPtr<IDXGIAdapter1> dxgiAdapter1;

		SIZE_T maxDedicatedVideoMemory = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			dxgiAdapter1->GetDesc1(&adapterDesc);

			// Choose adapter with the largest dedicated video memory
			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && 
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
				adapterDesc.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				maxDedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
				ThrowIfFailed(dxgiAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&adapter));
			}
		}
	}

	ThrowIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

	// Enable debug messages in debug mode.
#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	if (SUCCEEDED(GetDevice()->QueryInterface(__uuidof(ID3D12InfoQueue), &pInfoQueue)))
	{
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		// Suppress messages based on their severity level
		D3D12_MESSAGE_SEVERITY Severities[] =
		{
			D3D12_MESSAGE_SEVERITY_INFO
		};

		// Suppress individual messages by their ID
		D3D12_MESSAGE_ID DenyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
		};

		D3D12_INFO_QUEUE_FILTER NewFilter = {};
		//NewFilter.DenyList.NumCategories = _countof(Categories);
		//NewFilter.DenyList.pCategoryList = Categories;
		NewFilter.DenyList.NumSeverities = _countof(Severities);
		NewFilter.DenyList.pSeverityList = Severities;
		NewFilter.DenyList.NumIDs = _countof(DenyIds);
		NewFilter.DenyList.pIDList = DenyIds;

		ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	descriptorSizeCBSRV = CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	graphicCommandContext = new Dx12GraphicCommandContext(sReleaseFrameCallback);
	copyCommandContext = new Dx12CopyCommandContext();

	tearingSupported = CheckTearingSupport();

	auto frameFn = std::bind(&Dx12GraphicCommandContext::CurentFrame, graphicCommandContext);
	descriptorAllocator = new x12::descriptorheap::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, frameFn);
}

void Dx12CoreRenderer::Free()
{
	x12::memory::dynamic::Free();

	{
		PresentSurfaces();
		surfaces.clear();
	}

	graphicCommandContext->Free();
	copyCommandContext->Free();

	IResourceUnknown::CheckResources();

	{
		std::scoped_lock guard(uniformBufferMutex);
		uniformBufferVec.clear();
	}

	delete graphicCommandContext;
	graphicCommandContext = nullptr;

	delete copyCommandContext;
	copyCommandContext = nullptr;

	psoMap.clear();

	delete descriptorAllocator;
	descriptorAllocator = nullptr;

	Release(adapter);

#ifdef _DEBUG
	{
		ComPtr<IDXGIDebug1> pDebug;
		DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug));
		pDebug->ReportLiveObjects(DXGI_DEBUG_DXGI, DXGI_DEBUG_RLO_ALL);
	}
#endif

	Release(device);
}

auto Dx12CoreRenderer::_FetchSurface(HWND hwnd) -> surface_ptr
{
	if (auto it = surfaces.find(hwnd); it == surfaces.end())
	{
		surface_ptr surface = std::make_shared<Dx12WindowSurface>();
		surface->Init(hwnd, graphicCommandContext->GetD3D12CmdQueue());

		surfaces[hwnd] = surface;

		return std::move(surface);
	}
	else
		return it->second;
}

void Dx12CoreRenderer::RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight)
{
	graphicCommandContext->WaitGPUAll();

	surface_ptr surf = _FetchSurface(hwnd);
	surf->ResizeBuffers(newWidth, newHeight);

	// after recreating swapchain's buffers frameIndex should be 0??
	graphicCommandContext->frameIndex = 0; // TODO: make frameIndex private
}

auto Dx12CoreRenderer::GetWindowSurface(HWND hwnd) -> surface_ptr
{
	surface_ptr surf = _FetchSurface(hwnd);
	surfacesForPresenting.push_back(surf);

	return surf;
}

auto Dx12CoreRenderer::PresentSurfaces() -> void
{
	for (const auto& s : surfacesForPresenting)
		s->Present();
	surfacesForPresenting.clear();

	graphicCommandContext->FrameEnd();

	++frame;
}

bool Dx12CoreRenderer::CreateShader(Dx12CoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
											const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	auto* ptr = new Dx12CoreShader{};
	ptr->InitGraphic(name, vertText, fragText, variabledesc, varNum);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateComputeShader(Dx12CoreShader** out, LPCWSTR name, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	auto* ptr = new Dx12CoreShader{};
	ptr->InitCompute(name, text, variabledesc, varNum);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateVertexBuffer(Dx12CoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
	const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_FLAGS usage)
{
	auto* ptr = new Dx12CoreVertexBuffer{};
	ptr->Init(name, vbData, vbDesc, idxData, idxDesc, usage);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateUniformBuffer(Dx12UniformBuffer **out, size_t size)
{
	std::scoped_lock guard(uniformBufferMutex);

	auto idx = uniformBufferVec.size();
	auto ptr = new Dx12UniformBuffer((UINT)size, idx);
	*out = ptr;

	uniformBufferVec.emplace_back(ptr);

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateStructuredBuffer(Dx12CoreBuffer** out, LPCWSTR name, size_t structureSize,
											  size_t num, const void* data, BUFFER_FLAGS flags)
{
	auto* ptr = new Dx12CoreBuffer;
	ptr->InitStructuredBuffer(structureSize, num, data, flags, name);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateRawBuffer(Dx12CoreBuffer** out, size_t size)
{
	auto* ptr = new Dx12CoreBuffer;
	ptr->InitRawBuffer(size);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool Dx12CoreRenderer::CreateTextureFrom(Dx12CoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> ddsData,
									 std::vector<D3D12_SUBRESOURCE_DATA> subresources,
									 ID3D12Resource* d3dexistingtexture)
{
	assert(subresources.size() == 1); // Not impl

	// Note: ComPtr's are CPU objects but this resource needs to stay in scope until
	// the command list that references it has finished executing on the GPU.
	// We will flush the GPU at the end of this method to ensure the resource is not
	// prematurely destroyed.
	ComPtr<ID3D12Resource> d3dTextureUploadHeap;

	D3D12_RESOURCE_DESC desc = {};
	desc = d3dexistingtexture->GetDesc();

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(d3dexistingtexture, 0, (UINT)subresources.size());

	// Create the GPU upload buffer.
	x12::memory::CreateCommittedBuffer(&d3dTextureUploadHeap, uploadBufferSize, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

	set_name(d3dTextureUploadHeap.Get(), L"Upload buffer for cpu->gpu copying %u bytes for '%s' texture", uploadBufferSize, name);

	copyCommandContext->CommandsBegin();
		UpdateSubresources(copyCommandContext->GetD3D12CmdList(), d3dexistingtexture, d3dTextureUploadHeap.Get(), 0, 0, 1, &subresources[0]);
	copyCommandContext->CommandsEnd();
	copyCommandContext->Submit();
	copyCommandContext->WaitGPUAll(); // wait GPU copying upload -> default heap

	auto* ptr = new Dx12CoreTexture();
	ptr->InitFromExisting(d3dexistingtexture);
	ptr->AddRef();

	*out = ptr;

	return ptr != nullptr;
}

ComPtr<ID3D12RootSignature> Dx12CoreRenderer::GetDefaultRootSignature()
{
	if (defaultRootSignature)
		return defaultRootSignature;

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ID3DBlob* blob;
	ThrowIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));

	ThrowIfFailed(GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&defaultRootSignature)));

	return defaultRootSignature;
}

psomap_checksum_t CalculateChecksum(const GraphicPipelineState& pso)
{
	// 0: 15 (16)  vb ID
	// 16:31 (16)  shader ID
	// 32:34 (3)   PRIMITIVE_TOPOLOGY
	// 35:38 (4)   src blend
	// 39:42 (4)   dst blend

	static_assert(11 == static_cast<int>(BLEND_FACTOR::NUM));

	auto* dx12buffer = static_cast<Dx12CoreVertexBuffer*>(pso.vb);
	auto* dx12shader = static_cast<Dx12CoreShader*>(pso.shader);

	assert(dx12buffer != nullptr);
	assert(dx12shader != nullptr);

	uint64_t checksum = uint64_t(dx12buffer->ID() << 0);
	checksum |= uint64_t(dx12shader->ID() << 16);
	checksum |= uint64_t(pso.primitiveTopology) << 32;
	checksum |= uint64_t(pso.src) << 35;
	checksum |= uint64_t(pso.dst) << 39;

	return checksum;
}
psomap_checksum_t CalculateChecksum(const ComputePipelineState& pso)
{
	auto* dx12shader = static_cast<Dx12CoreShader*>(pso.shader);
	assert(dx12shader != nullptr);

	uint64_t checksum = uint64_t(dx12shader->ID() << 0);

	return checksum;
}

ComPtr<ID3D12PipelineState> Dx12CoreRenderer::GetGraphicPSO(const GraphicPipelineState& pso, psomap_checksum_t checksum)
{
	std::unique_lock lock(psoMutex);

	if (auto it = psoMap.find(checksum); it != psoMap.end())
		return it->second;

	lock.unlock();

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader);
	Dx12CoreVertexBuffer* dx12vb = static_cast<Dx12CoreVertexBuffer*>(pso.vb);

	// Create PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout = { &dx12vb->inputLayout[0], (UINT)dx12vb->inputLayout.size() };
	desc.pRootSignature = dx12Shader->HasResources() ? dx12Shader->resourcesRootSignature.Get() :
		GetCoreRender()->GetDefaultRootSignature().Get();
	desc.VS = CD3DX12_SHADER_BYTECODE(dx12Shader->vs.Get());
	desc.PS = CD3DX12_SHADER_BYTECODE(dx12Shader->ps.Get());
	desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	auto setRTBlending = [&pso](D3D12_RENDER_TARGET_BLEND_DESC& out) -> void
	{
		out.BlendEnable = pso.src != BLEND_FACTOR::NONE || pso.dst != BLEND_FACTOR::NONE;
		out.LogicOpEnable = FALSE;
		out.SrcBlend = static_cast<D3D12_BLEND>(pso.src);
		out.DestBlend = static_cast<D3D12_BLEND>(pso.dst);
		out.BlendOp = D3D12_BLEND_OP_ADD;
		out.SrcBlendAlpha = D3D12_BLEND_ZERO;
		out.DestBlendAlpha = D3D12_BLEND_ONE;
		out.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		out.LogicOp = D3D12_LOGIC_OP_NOOP;
		out.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	};

	desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	setRTBlending(desc.BlendState.RenderTarget[0]);

	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(pso.primitiveTopology);
	assert(desc.PrimitiveTopologyType <= D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
	assert(desc.PrimitiveTopologyType > D3D12_PRIMITIVE_TOPOLOGY_TYPE::D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED);
	desc.SampleMask = UINT_MAX;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> d3dPipelineState;
	ThrowIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(d3dPipelineState.GetAddressOf())));

	set_name(d3dPipelineState.Get(), L"PSO (graphic) #" PRIu64, psoNum);

	lock.lock();

	if (auto it = psoMap.find(checksum); it == psoMap.end())
	{
		psoMap[checksum] = d3dPipelineState;
		psoNum++;
		return d3dPipelineState;
	}
	else
		return it->second;
}

auto Dx12CoreRenderer::GetComputePSO(const ComputePipelineState& pso, psomap_checksum_t checksum) -> ComPtr<ID3D12PipelineState>
{
	std::unique_lock lock(psoMutex);

	if (auto it = psoMap.find(checksum); it != psoMap.end())
		return it->second;

	lock.unlock();

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader);

	// Create PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = dx12Shader->HasResources() ? dx12Shader->resourcesRootSignature.Get() :
		GetCoreRender()->GetDefaultRootSignature().Get();
	desc.CS = CD3DX12_SHADER_BYTECODE(dx12Shader->cs.Get());

	ComPtr<ID3D12PipelineState> d3dPipelineState;
	ThrowIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(d3dPipelineState.GetAddressOf())));

	set_name(d3dPipelineState.Get(), L"PSO (compute) #" PRIu64, psoNum);

	lock.lock();

	if (auto it = psoMap.find(checksum); it == psoMap.end())
	{
		psoMap[checksum] = d3dPipelineState;
		psoNum++;
		return d3dPipelineState;
	}
	else
		return it->second;

}

x12::descriptorheap::Alloc Dx12CoreRenderer::AllocateDescriptor(UINT num)
{
	return descriptorAllocator->Allocate(num);
}

uint64_t Dx12CoreRenderer::UniformBufferUpdates()
{
	return graphicCommandContext->uniformBufferUpdates();
}

uint64_t Dx12CoreRenderer::StateChanges()
{
	return graphicCommandContext->stateChanges();
}

uint64_t Dx12CoreRenderer::Triangles()
{
	return graphicCommandContext->triangles();
}

uint64_t Dx12CoreRenderer::DrawCalls()
{
	return graphicCommandContext->drawCalls();
}
