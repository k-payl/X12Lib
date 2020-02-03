#pragma once
#include "common.h"
#include "icorerender.h"
#include "intrusiveptr.h"

typedef std::map<uint64_t, ComPtr<ID3D12PipelineState>> psomap_t;
typedef std::vector<Dx12UniformBuffer*> uniformbuffers_t;

class GpuProfiler;

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
	void Present();
};

using surface_ptr = std::shared_ptr<Dx12WindowSurface>;

uint64_t CalculateChecksum(const PipelineState& pso);

class Dx12CoreRenderer
{
	device_t *device{nullptr};
	adapter_t *adapter{nullptr};

	std::map<HWND, surface_ptr> surfaces;
	surface_ptr currentSurface;
	unsigned currentSurfaceWidth{}, currentSurfaceHeight{};
	std::vector<surface_ptr> surfacesForPresenting;

	Dx12GraphicCommandContext* graphicCommandContext;
	Dx12CopyCommandContext* copyCommandContext;

	std::vector<intrusive_ptr<IResourceUnknown>> resources;					// All resources
	uniformbuffers_t uniformBufferVec;										// All uniform buffers
	psomap_t psoMap;														// All Pipeline State Objects. checksum -> PSO
	std::map<UINT, FastFrameAllocator::PagePool*> fastAllocatorPagePools;	// Page pools for uniform buffers. allocation size -> pool
	DescriptorHeap::Allocator* descriptorAllocator;							// Descriptors for static long-lived resources
	ComPtr<ID3D12RootSignature> defaultRootSignature;						// Root signature for shaders without input resources
	bool Vsync{false};
	bool tearingSupported;
	UINT descriptorSizeCBSRV;
	UINT descriptorSizeRTV;
	UINT descriptorSizeDSV;

	void ReleaseFrame(uint64_t fenceID);
	static void sReleaseFrameCallback(uint64_t fenceID);

public:
	Dx12CoreRenderer();
	~Dx12CoreRenderer();
	
	auto Init() -> void;
	auto Free() -> void;

	inline auto GetDevice() -> device_t* { return device; }
	inline auto IsTearingSupport() -> bool { return tearingSupported; }
	inline auto IsVSync() -> bool { return Vsync; }
	inline auto CBSRV_DescriptorsSize() -> UINT { return descriptorSizeCBSRV; }
	inline auto RTV_DescriptorsSize() -> UINT { return descriptorSizeRTV; }
	inline auto DSV_DescriptorsSize() -> UINT { return descriptorSizeDSV; }
	auto GetFastFrameAllocatorPool(UINT bufferSize)->FastFrameAllocator::PagePool*;
	auto GetDefaultRootSignature()->ID3D12RootSignature*;
	auto getPSO(const PipelineState& pso)->ID3D12PipelineState*;
	auto AllocateDescriptor(UINT num = 1)->DescriptorHeap::Alloc;
	auto ReleaseResource(int& refs, IResourceUnknown* ptr) -> void;
	auto GetGraphicCommmandContext() const -> Dx12GraphicCommandContext* { return graphicCommandContext; };
	auto GetCopyCommandContext() const -> Dx12CopyCommandContext* { return copyCommandContext; }

	// Surfaces
	auto fetchSurface(HWND hwnd)->surface_ptr;
	auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void;
	auto MakeCurrent(HWND hwnd) -> surface_ptr;
	auto GetSurfaceSize(unsigned& w, unsigned& h) -> void { w = currentSurfaceWidth; h = currentSurfaceHeight; }
	auto PresentSurfaces() -> void;

	// Statistic
	auto UniformBufferUpdates()->uint64_t;
	auto StateChanges()->uint64_t;
	auto Triangles()->uint64_t;
	auto DrawCalls()->uint64_t;

	// Resurces creation
	bool CreateShader(Dx12CoreShader **out, const char* vertText, const char* fragText,
							const ConstantBuffersDesc *variabledesc = nullptr, uint32_t varNum = 0);
	bool CreateVertexBuffer(Dx12CoreVertexBuffer** out, const void* vbData, const VeretxBufferDesc* vbDesc,
							const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_USAGE usage = BUFFER_USAGE::GPU_READ);
	bool CreateUniformBuffer(Dx12UniformBuffer** out, size_t size);
	bool CreateStructuredBuffer(Dx12CoreStructuredBuffer **out, size_t structureSize, size_t num, const void* data);
	bool CreateTexture(Dx12CoreTexture **out, std::unique_ptr<uint8_t[]> ddsData,
							std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dtexture);
};


// helpers

extern Dx12CoreRenderer*			_coreRender;
inline Dx12CoreRenderer*			GetCoreRender() { return _coreRender; }

inline device_t*					CR_GetD3DDevice() { return GetCoreRender()->GetDevice(); }
inline bool							CR_IsTearingSupport() { return GetCoreRender()->IsTearingSupport(); }
inline bool							CR_IsVSync() { return GetCoreRender()->IsVSync(); }
inline void							CR_ReleaseResource(int& refs, IResourceUnknown* ptr) { GetCoreRender()->ReleaseResource(refs, ptr); }
inline UINT							CR_CBSRV_DescriptorsSize() { return GetCoreRender()->CBSRV_DescriptorsSize(); }
inline UINT							CR_RTV_DescriptorsSize() { return GetCoreRender()->RTV_DescriptorsSize(); }
inline UINT							CR_DSV_DescriptorsSize() { return GetCoreRender()->DSV_DescriptorsSize(); }

