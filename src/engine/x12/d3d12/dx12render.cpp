#include "dx12render.h"
#include "core.h"
#include "dx12shader.h"
#include "dx12buffer.h"
#include "dx12commandlist.h"
#include "dx12vertexbuffer.h"
#include "dx12texture.h"
#include "dx12descriptorheap.h"
#include "dx12query.h"
#include "dx12memory.h"
#include "dx12resourceset.h"

#include "GraphicsMemory.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <inttypes.h>

using namespace x12;

void WaitForFenceValue(ID3D12Fence* d3dFence, uint64_t nextFenceValue, HANDLE fenceEvent,
					   std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
	if (d3dFence->GetCompletedValue() < nextFenceValue)
	{
		throwIfFailed(d3dFence->SetEventOnCompletion(nextFenceValue, fenceEvent));
		::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
	}
}

x12::Dx12CoreRenderer::Dx12CoreRenderer()
{
	for (int i  = 0; i < QUEUE_NUM; i++)
	{
		assert(queues[i].d3dCommandQueue == nullptr && "Free() must be called before destructor");
		assert(queues[i].d3dFence == nullptr && "Free() must be called before destructor");
		::CloseHandle(queues[i].fenceEvent);
	}

	assert(_coreRender == nullptr && "Should be created only one instance of Dx12CoreRenderer");
	_coreRender = this;
}
x12::Dx12CoreRenderer::~Dx12CoreRenderer()
{
	_coreRender = nullptr;
}

void x12::Dx12CoreRenderer::Init()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	throwIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif

	{
		ComPtr<dxgifactory_t> dxgiFactory;
		UINT createFactoryFlags = 0;
#if defined(_DEBUG)
		createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		throwIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

		ComPtr<IDXGIAdapter1> dxgiAdapter1;

		SIZE_T maxDedicatedVideoMemory = 0;
		UINT adapaterNum = 0;
		for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			dxgiAdapter1->GetDesc1(&adapterDesc);

			if (core__->IsVerboseRenderer())
			{
				wprintf_s(L"GPU%d: '%s', dedicated memory %Iu, shared memory: %Iu, software %d \n", adapaterNum,
					adapterDesc.Description, adapterDesc.DedicatedVideoMemory, adapterDesc.SharedSystemMemory, adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ? 1 : 0);
			}

			// Choose adapter with the largest dedicated video memory
			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 && 
				SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
				adapterDesc.DedicatedVideoMemory > maxDedicatedVideoMemory)
			{
				if (adapter)
					adapter->Release();

				maxDedicatedVideoMemory = adapterDesc.DedicatedVideoMemory;
				throwIfFailed(dxgiAdapter1->QueryInterface(__uuidof(IDXGIAdapter4), (void**)&adapter));
			}

			adapaterNum++;
		}

		if (adapter && core__->IsVerboseRenderer())
		{
			DXGI_ADAPTER_DESC1 adapterDesc;
			adapter->GetDesc1(&adapterDesc);

			wprintf_s(L"Selected GPU (DirectX12): %s\n", adapterDesc.Description);
		}
	}

	if (!adapter)
	{
		wprintf_s(L"No descrete adapter");
	}

	throwIfFailed(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));

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

		throwIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
	}
#endif

	descriptorSizeCBSRV = d3d12::CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	descriptorSizeRTV = d3d12::CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV = d3d12::CR_GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_COMMAND_LIST_TYPE types[QUEUE_NUM] = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COPY};

	for (int i = 0; i < QUEUE_NUM; i++)
	{
		throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queues[i].d3dFence)));
		x12::d3d12::set_name(queues[i].d3dFence, L"fence");

		queues[i].fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		assert(queues[i].fenceEvent && "Failed to create fence event.");

		// Command queue
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = types[i];

		throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queues[i].d3dCommandQueue)));
	}

	x12::d3d12::set_name(queues[QUEUE_GRAPHIC].d3dCommandQueue, L"dx12render QUEUE_GRAPHIC");
	x12::d3d12::set_name(queues[QUEUE_COPY].d3dCommandQueue, L"dx12render QUEUE_COPY");

	uint64_t gpuFrequency;
	queues[QUEUE_GRAPHIC].d3dCommandQueue->GetTimestampFrequency(&gpuFrequency);
	gpuTickDelta = float(1000.0 / static_cast<double>(gpuFrequency));

	copyCommandContext = new Dx12CopyCommandList();

	tearingSupported = x12::d3d12::CheckTearingSupport();

	auto frameFn = std::bind(&ICoreRenderer::Frame, this);
	SRVdescriptorAllocator = new x12::descriptorheap::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, frameFn);
	RTVdescriptorAllocator = new x12::descriptorheap::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, frameFn);
	DSVdescriptorAllocator = new x12::descriptorheap::Allocator(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, frameFn);

	frameMemory = std::make_unique<DirectX::GraphicsMemory>(device);

	srv.Init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);}

void x12::Dx12CoreRenderer::DescriptorHeap::Init(D3D12_DESCRIPTOR_HEAP_TYPE type, bool gpuVisible)
{
	heap = d3d12::CreateDescriptorHeap(d3d12::CR_GetD3DDevice(), x12::NumRenderDescriptors, type, gpuVisible);
	x12::d3d12::set_name(heap.Get(), L"descriptor heap for %lu descriptors", x12::NumRenderDescriptors);
	cpuStart = heap->GetCPUDescriptorHandleForHeapStart();

	if (gpuVisible)
		gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
}

void x12::Dx12CoreRenderer::Free()
{
	PresentSurfaces();
	WaitGPUAll();

	commandLists.clear();

	copyCommandContext->Free();

	delete copyCommandContext;
	copyCommandContext = nullptr;

	for (int i = 0; i < commandLists.size(); i++)
		delete commandLists[i];

	commandLists.clear();

	for (int i = 0; i < QUEUE_NUM; i++)
	{
		Release(queues[i].d3dCommandQueue);
		Release(queues[i].d3dFence);
	}

	psoMap.clear();

	srv.Free();

	surfaces.clear();

	IResourceUnknown::CheckResources();

	delete SRVdescriptorAllocator;
	SRVdescriptorAllocator = nullptr;
	delete RTVdescriptorAllocator;
	RTVdescriptorAllocator = nullptr;
	delete DSVdescriptorAllocator;
	DSVdescriptorAllocator = nullptr;

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

ICoreGraphicCommandList* x12::Dx12CoreRenderer::CreateCommandList(int32_t id = -1)
{
	auto cmdListPtr = new Dx12GraphicCommandList(this, id);
	commandLists.insert(commandLists.begin(), cmdListPtr);

	return cmdListPtr;
}

auto x12::Dx12CoreRenderer::GetGraphicCommandList() -> ICoreGraphicCommandList*
{
	uint64_t queueCompletedValue = queues[QUEUE_GRAPHIC].completedValue;

	for (int i = 0; i < commandLists.size(); i++)
	{
		if (commandLists[i]->ReadyForOpening() &&
			commandLists[i]->Unnamed() &&
			queues[QUEUE_GRAPHIC].IsCompleted(commandLists[i]->SubmitedValue()))
		{
			return commandLists[i];
		}
	}

	return CreateCommandList();
}

auto x12::Dx12CoreRenderer::GetGraphicCommandList(int32_t id) -> ICoreGraphicCommandList*
{
	uint64_t queueCompletedValue1 = queues[QUEUE_GRAPHIC].completedValue;

	for (int i = 0; i < commandLists.size(); i++)
	{
		if (commandLists[i]->ReadyForOpening() &&
			commandLists[i]->ID() == id &&
			queues[QUEUE_GRAPHIC].IsCompleted(commandLists[i]->SubmitedValue()))
		{
			return commandLists[i];
		}
	}

	return CreateCommandList(id);
}

auto x12::Dx12CoreRenderer::RefreshFencesStatus() -> void
{
	uint64_t valueCompleted = queues[QUEUE_GRAPHIC].completedValue;

	SRVdescriptorAllocator->ReclaimMemory(valueCompleted);
	RTVdescriptorAllocator->ReclaimMemory(valueCompleted);
	DSVdescriptorAllocator->ReclaimMemory(valueCompleted);

	for (int i = 0; i < commandLists.size(); i++)
	{
		if (commandLists[i]->IsSubmited() && commandLists[i]->SubmitedValue() <= valueCompleted)
			commandLists[i]->NotifyFrameCompleted(valueCompleted);
	}
}

auto x12::Dx12CoreRenderer::FrameEnd() -> void
{
	frameMemory->Commit(CommandQueue());

	for (auto& cmdList : commandLists)
		cmdList->FrameEnd();

	copyCommandContext->FrameEnd();

	DirectX::GraphicsMemoryStatistics memStat = FrameMemory()->GetStatistics();

	stat.committedMemory = memStat.committedMemory;
	stat.totalMemory = memStat.totalMemory;
	stat.totalPages = memStat.totalPages;
	stat.peakCommitedMemory = memStat.peakCommitedMemory;
	stat.peakTotalMemory = memStat.peakTotalMemory;
	stat.peakTotalPages = memStat.peakTotalPages;

	++frame;
	frameIndex = (frameIndex + 1u) % engine::DeferredBuffers;

	//_ReleaseGraphicQueueResources();
}

auto x12::Dx12CoreRenderer::WaitGPU() -> void
{
	for (int i = 0; i < QUEUE_NUM; i++)
	{
		uint64_t valueToWait = queues[i].submitedValues[frameIndex];
		uint64_t valueCompleted = queues[i].completedValue;

		if (valueCompleted < valueToWait)
		{
			WaitForFenceValue(queues[i].d3dFence, valueToWait, queues[i].fenceEvent);
			queues[i].completedValue = valueToWait;
		}
	}

	RefreshFencesStatus();
}

auto x12::Dx12CoreRenderer::WaitGPUAll() -> void
{
	for (int i = 0; i < QUEUE_NUM; ++i)
	{
		uint64_t valueToWait = 0;

		for (int j = 0; j < engine::DeferredBuffers; ++j)
			valueToWait = std::max(queues[i].submitedValues[j], valueToWait);
		uint64_t valueCompleted = queues[i].completedValue;

		if (valueCompleted < valueToWait)
		{
			WaitForFenceValue(queues[i].d3dFence, valueToWait, queues[i].fenceEvent);
			queues[i].completedValue = valueToWait;
		}
	}

	RefreshFencesStatus();
}

auto x12::Dx12CoreRenderer::_FetchSurface(HWND hwnd) -> surface_ptr
{
	if (auto it = surfaces.find(hwnd); it == surfaces.end())
	{
		surface_ptr surface = std::make_shared<Dx12WindowSurface>();
		surface->Init(hwnd, this);

		surfaces[hwnd] = surface;

		return std::move(surface);
	}
	else
		return it->second;
}

void x12::Dx12CoreRenderer::RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight)
{
	WaitGPUAll();

	queues[0].Signal(frameIndex);

	uint64_t valueToWait = 0;
	for (int j = 0; j < engine::DeferredBuffers; ++j)
		valueToWait = std::max(queues[0].submitedValues[j], valueToWait);
	uint64_t valueCompleted = queues[0].completedValue;

	if (valueCompleted < valueToWait)
	{
		WaitForFenceValue(queues[0].d3dFence, valueToWait, queues[0].fenceEvent);
		queues[0].completedValue = valueToWait;
	}

	surface_ptr surf = _FetchSurface(hwnd);
	surf->ResizeBuffers(newWidth, newHeight);

	// after recreating swapchain's buffers frameIndex should be 0??
	frameIndex = 0; // TODO: make frameIndex private
}

auto x12::Dx12CoreRenderer::GetWindowSurface(HWND hwnd) -> surface_ptr
{
	surface_ptr surf = _FetchSurface(hwnd);
	surfacesForPresenting.insert(surf);

	return surf;
}

auto x12::Dx12CoreRenderer::PresentSurfaces() -> void
{
	for (auto& s : surfacesForPresenting)
		s->Present();

	surfacesForPresenting.clear();
}

auto x12::Dx12CoreRenderer::ExecuteCommandList(ICoreCopyCommandList* cmdList) -> void
{
	if (dynamic_cast<Dx12GraphicCommandList*>(cmdList))
	{
		for (int i = 0; i < commandLists.size(); i++)
		{
			if (commandLists[i] == cmdList)
			{
				commandLists[i]->NotifySubmited(queues[QUEUE_GRAPHIC].nextFenceValue);
				break;
			}
		}

		ID3D12CommandList* const commandLists_[] = {static_cast<Dx12GraphicCommandList*>(cmdList)->GetD3D12CmdList()};
		queues[QUEUE_GRAPHIC].d3dCommandQueue->ExecuteCommandLists(1, commandLists_);
		queues[QUEUE_GRAPHIC].Signal(frameIndex);
	}
	else if (dynamic_cast<Dx12CopyCommandList*>(cmdList))
	{
		ID3D12CommandList* const commandLists_[] = {static_cast<Dx12CopyCommandList*>(cmdList)->GetD3D12CmdList()};
		queues[QUEUE_COPY].d3dCommandQueue->ExecuteCommandLists(1, commandLists_);
		queues[QUEUE_COPY].Signal(frameIndex);
	}
}

bool x12::Dx12CoreRenderer::CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
											const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	auto* ptr = new Dx12CoreShader{};
	ptr->InitGraphic(name, vertText, fragText, variabledesc, varNum);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	auto* ptr = new Dx12CoreShader{};
	ptr->InitCompute(name, text, variabledesc, varNum);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
	const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE mem)
{
	auto* ptr = new Dx12CoreVertexBuffer{};
	ptr->Init(name, vbData, vbDesc, idxData, idxDesc, mem);
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, BUFFER_FLAGS flags, MEMORY_TYPE mem, const void* data, size_t num)
{
	if (flags & BUFFER_FLAGS::CONSTANT_BUFFER_VIEW)
		size = AlignConstantBufferSize(size);

	auto* ptr = new Dx12CoreBuffer(size * num, data, mem, flags, name);

	const bool rawBuffer = flags & BUFFER_FLAGS::RAW_BUFFER;

	if (flags & BUFFER_FLAGS::SHADER_RESOURCE_VIEW)
		ptr->initSRV((UINT)num, size, rawBuffer);

	if (flags & BUFFER_FLAGS::UNORDERED_ACCESS_VIEW)
		ptr->initUAV((UINT)num, size, rawBuffer);

	if (flags & BUFFER_FLAGS::CONSTANT_BUFFER_VIEW)
		ptr->initCBV((UINT)size);

	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateTexture(ICoreTexture** out, LPCWSTR name, const uint8_t* data, size_t size,
	int32_t width, int32_t height, uint32_t mipCount, uint32_t layerCount, TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags)
{
	auto ptr = new Dx12CoreTexture();
	ptr->Init(name, data, size, width, height, mipCount, layerCount, type, format, flags);

	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::vector<D3D12_SUBRESOURCE_DATA> subresources,
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

	x12::d3d12::set_name(d3dTextureUploadHeap.Get(), L"Upload buffer for cpu->gpu copying %u bytes for '%s' texture", uploadBufferSize, name);

	copyCommandContext->CommandsBegin();
		UpdateSubresources(copyCommandContext->GetD3D12CmdList(), d3dexistingtexture, d3dTextureUploadHeap.Get(), 0, 0, 1, &subresources[0]);
	copyCommandContext->CommandsEnd();
	ExecuteCommandList(copyCommandContext);
	WaitGPUAll(); // wait GPU copying upload -> default heap

	auto* ptr = new Dx12CoreTexture();
	ptr->InitFromExisting(name, d3dexistingtexture);
	ptr->AddRef();

	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateTextureFrom(ICoreTexture** out, LPCWSTR name, ID3D12Resource* existingTexture)
{
	auto* ptr = new Dx12CoreTexture();
	ptr->InitFromExisting(name, existingTexture);
	ptr->AddRef();

	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateResourceSet(IResourceSet** out, const ICoreShader* shader)
{
	auto* dxShader = static_cast<const Dx12CoreShader*>(shader);

	auto* ptr = new Dx12ResourceSet(dxShader);
	ptr->AddRef();

	*out = ptr;

	return ptr != nullptr;
}

bool x12::Dx12CoreRenderer::CreateQuery(ICoreQuery** out)
{
	auto* ptr = new Dx12CoreQuery;
	ptr->AddRef();
	*out = ptr;

	return ptr != nullptr;
}

ComPtr<ID3D12RootSignature> x12::Dx12CoreRenderer::GetDefaultRootSignature()
{
	if (defaultRootSignature)
		return defaultRootSignature;

	CD3DX12_ROOT_SIGNATURE_DESC desc;
	desc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ID3DBlob* blob;
	throwIfFailed(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, nullptr));

	throwIfFailed(GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&defaultRootSignature)));

	return defaultRootSignature;
}

ComPtr<ID3D12PipelineState> x12::Dx12CoreRenderer::GetGraphicPSO(const GraphicPipelineState& pso, psomap_checksum_t checksum)
{
	std::unique_lock lock(psoMutex);

	if (auto it = psoMap.find(checksum); it != psoMap.end())
		return it->second;

	lock.unlock();

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader.get());
	Dx12CoreVertexBuffer* dx12vb = static_cast<Dx12CoreVertexBuffer*>(pso.vb.get());

	// Create PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.InputLayout = { &dx12vb->inputLayout[0], (UINT)dx12vb->inputLayout.size() };
	desc.pRootSignature = dx12Shader->HasResources() ? dx12Shader->resourcesRootSignature.Get() :
		d3d12::D3D12GetCoreRender()->GetDefaultRootSignature().Get();
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
	desc.RasterizerState.FrontCounterClockwise = 1;

	ComPtr<ID3D12PipelineState> d3dPipelineState;
	throwIfFailed(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(d3dPipelineState.GetAddressOf())));

	x12::d3d12::set_name(d3dPipelineState.Get(), L"PSO (graphic) #" PRIu64, psoNum);

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

auto x12::Dx12CoreRenderer::GetComputePSO(const ComputePipelineState& pso, psomap_checksum_t checksum) -> ComPtr<ID3D12PipelineState>
{
	std::unique_lock lock(psoMutex);

	if (auto it = psoMap.find(checksum); it != psoMap.end())
		return it->second;

	lock.unlock();

	Dx12CoreShader* dx12Shader = static_cast<Dx12CoreShader*>(pso.shader);

	// Create PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = dx12Shader->HasResources() ? dx12Shader->resourcesRootSignature.Get() :
		d3d12::D3D12GetCoreRender()->GetDefaultRootSignature().Get();
	desc.CS = CD3DX12_SHADER_BYTECODE(dx12Shader->cs.Get());

	ComPtr<ID3D12PipelineState> d3dPipelineState;
	throwIfFailed(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(d3dPipelineState.GetAddressOf())));

	x12::d3d12::set_name(d3dPipelineState.Get(), L"PSO (compute) #" PRIu64, psoNum);

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

std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> Dx12CoreRenderer::AllocateSRVDescriptor(UINT num)
{
	assert(srv.offset_ < NumRenderDescriptors);

	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> ret;
	ret.first.ptr = srv.cpuStart.ptr + size_t(srv.offset_) * descriptorSizeCBSRV;
	ret.second.ptr = srv.gpuStart.ptr + size_t(srv.offset_) * descriptorSizeCBSRV;
	srv.offset_ += num;
	return ret;
}

//D3D12_CPU_DESCRIPTOR_HANDLE x12::Dx12CoreRenderer::AllocateRTVDescriptor(UINT num)
//{
//	assert(rtv.offset_ < NumRenderDescriptors);
//
//	D3D12_CPU_DESCRIPTOR_HANDLE ret;
//	ret.ptr = rtv.cpuStart.ptr + SIZE_T(rtv.offset_) * descriptorSizeRTV;
//	rtv.offset_ += num;
//	return ret;
//
//}
//
//D3D12_CPU_DESCRIPTOR_HANDLE x12::Dx12CoreRenderer::AllocateDSVDescriptor(UINT num)
//{
//	assert(dsv.offset_ < NumRenderDescriptors);
//
//	D3D12_CPU_DESCRIPTOR_HANDLE ret;
//	ret.ptr = dsv.cpuStart.ptr + SIZE_T(dsv.offset_) * descriptorSizeDSV;
//	dsv.offset_ += num;
//	return ret;
//}

x12::descriptorheap::Alloc x12::Dx12CoreRenderer::AllocateStaticDescriptor(UINT num)
{
	return SRVdescriptorAllocator->Allocate(num);
}

auto x12::Dx12CoreRenderer::AllocateStaticRTVDescriptor(UINT num) -> x12::descriptorheap::Alloc
{
	return RTVdescriptorAllocator->Allocate(num);
}

auto x12::Dx12CoreRenderer::AllocateStaticDSVDescriptor(UINT num) -> x12::descriptorheap::Alloc
{
	return DSVdescriptorAllocator->Allocate(num);
}

