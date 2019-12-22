#pragma once
#include "common.h"
#include "icorerender.h"
#include "intrusiveptr.h"

typedef std::map<uint64_t, ComPtr<ID3D12PipelineState>> psomap_t;
typedef std::vector<Dx12UniformBuffer*> uniformbuffers_t;

class gpuProfiler;

// Resources associated with window
struct Dx12WindowSurface
{
	void Init(HWND hwnd, ID3D12CommandQueue* queue);

	unsigned width, height;

	ComPtr<swapchain_t> swapChain;

	ComPtr<ID3D12Resource> colorBuffers[DeferredBuffers];
	ComPtr<ID3D12Resource> depthBuffer;

	ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
	ComPtr<ID3D12DescriptorHeap> descriptorHeapDSV;

	void ResizeBuffers(unsigned width_, unsigned height_);
};

class Dx12CoreRenderer
{
	device_t *device;
	adapter_t *adapter;
	Dx12WindowSurface *surface;

	gpuProfiler* gpuprofiler;

	Dx12GraphicCommandContext* graphicCommandContext;
	Dx12CopyCommandContext* copyCommandContext;

	std::vector<intrusive_ptr<IResourceUnknown>> resources;					// All resources
	uniformbuffers_t uniformBufferVec;										// All uniform buffers
	psomap_t psoMap;														// All Pipeline State Objects. checksum -> PSO
	std::map<UINT, FastFrameAllocator::PagePool*> fastAllocatorPagePools;	// Page pools for dynamic upload resources. allocation size -> pool
	DescriptorHeap::Allocator* descriptorAllocator;							// Descriptors for static long-lived resources

	ComPtr<ID3D12RootSignature> defaultRootSignature;						// Root signature for shaders "without input resources"

	bool Vsync{false};
	bool tearingSupported;

	UINT descriptorSizeCBSRV;
	UINT descriptorSizeRTV;
	UINT descriptorSizeDSV;

	void UpdateBufferResource(ID3D12Resource** dest, ID3D12Resource** intermediate,	UINT64 size,
		const void* data, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

	void ReleaseFrame(uint64_t fenceID);
	static void sReleaseFrame(uint64_t fenceID);

public:
	Dx12CoreRenderer();
	~Dx12CoreRenderer();

	// Internal API
	auto Init(HWND hwnd) -> void;
	auto Free() -> void;

	auto RecreateBuffers(UINT w, UINT h) -> void;
	auto GetFastFrameAllocatorPool(UINT bufferSize) -> FastFrameAllocator::PagePool*;
	auto GetDevice() -> device_t* { return device; }
	auto IsTearingSupport() -> bool { return tearingSupported; }
	auto IsVSync() -> bool { return Vsync; }
	ID3D12RootSignature*		GetDefaultRootSignature();
	psomap_t&					GetGloablPSOMap() { return psoMap; }
	uniformbuffers_t&			GetGlobalUnifromBuffers() { return uniformBufferVec; }
	DescriptorHeap::Allocator*	GetDescriptorAllocator() { return descriptorAllocator; }

	UINT CBSRV_DescriptorsSize() { return descriptorSizeCBSRV; }
	UINT RTV_DescriptorsSize() { return descriptorSizeRTV; }
	UINT DSV_DescriptorsSize() { return descriptorSizeDSV; }

	void ReleaseResource(int& refs, IResourceUnknown* ptr);

	void GPUProfileRender();

	// API
	auto GetMainCommmandContext() const -> Dx12GraphicCommandContext* { return graphicCommandContext; };
	auto CreateShader(const char* vertText, const char* fragText, const ConstantBuffersDesc *variabledesc, uint32_t varNum) -> Dx12CoreShader*;
	auto CreateVertexBuffer(const void* vbData, const VeretxBufferDesc* vbDesc, const void* idxData, const IndexBufferDesc* idxDesc) -> Dx12CoreVertexBuffer*;
	auto CreateUniformBuffer(size_t size) -> Dx12UniformBuffer*;
};


// helpers

extern Dx12CoreRenderer*	_coreRender;
inline Dx12CoreRenderer*	GetCoreRender() { return _coreRender; }

inline device_t*			CR_GetDevice() { return GetCoreRender()->GetDevice(); }
inline psomap_t&			CR_GetGlobalPSOMap() { return GetCoreRender()->GetGloablPSOMap(); }
inline uniformbuffers_t&	CR_GetGlobalUniformBuffers() { return GetCoreRender()->GetGlobalUnifromBuffers(); }
inline DescriptorHeap::Allocator* CR_GetDescriptorAllocator() { return GetCoreRender()->GetDescriptorAllocator(); }

inline bool					CR_IsTearingSupport() { return GetCoreRender()->IsTearingSupport(); }
inline bool					CR_IsVSync() { return GetCoreRender()->IsVSync(); }

inline void					CR_ReleaseResource(int& refs, IResourceUnknown* ptr) { GetCoreRender()->ReleaseResource(refs, ptr); }

inline UINT					CR_CBSRV_DescriptorsSize() { return GetCoreRender()->CBSRV_DescriptorsSize(); }
inline UINT					CR_RTV_DescriptorsSize() { return GetCoreRender()->RTV_DescriptorsSize(); }
inline UINT					CR_DSV_DescriptorsSize() { return GetCoreRender()->DSV_DescriptorsSize(); }

