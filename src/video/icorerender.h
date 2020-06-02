#pragma once
#include "common.h"
#include "intrusiveptr.h"

namespace x12
{
	inline constexpr size_t MaxBindedResourcesPerFrame = 1'024;
	inline constexpr unsigned MaxResourcesPerShader = 8;

	enum class TEXTURE_FORMAT;
	enum class TEXTURE_CREATE_FLAGS : uint32_t;
	enum class TEXTURE_TYPE;
	enum class VERTEX_BUFFER_FORMAT;
	enum class VERTEX_BUFFER_FORMAT;
	enum class INDEX_BUFFER_FORMAT;
	enum class BUFFER_FLAGS;
	class IWidowSurface;
	class ICoreRenderer;
	struct GraphicPipelineState;
	struct ComputePipelineState;
	struct ICoreVertexBuffer;
	struct IResourceSet;
	struct ICoreBuffer;
	struct ICoreShader;
	struct ConstantBuffersDesc;
	struct VeretxBufferDesc;
	struct IndexBufferDesc;
	struct ICoreTexture;

	extern ICoreRenderer* _coreRender;
	inline ICoreRenderer* GetCoreRender() { return _coreRender; }

	using surface_ptr = std::shared_ptr<IWidowSurface>;

	class ICoreGraphicCommandList
	{
	public:
		virtual ~ICoreGraphicCommandList() = default;
		virtual void BindSurface(surface_ptr& surface_) = 0; // TODO: bind arbitary textures
		virtual void CommandsBegin() = 0;
		virtual void CommandsEnd() = 0;
		virtual void FrameEnd() = 0;
		virtual void Submit() = 0;
		virtual void WaitGPUFrame() = 0;
		virtual void WaitGPUAll() = 0;
		virtual void PushState() = 0;
		virtual void PopState() = 0;
		virtual void SetGraphicPipelineState(const GraphicPipelineState& gpso) = 0;
		virtual void SetComputePipelineState(const ComputePipelineState& cpso) = 0;
		virtual void SetVertexBuffer(ICoreVertexBuffer* vb) = 0;
		virtual void SetViewport(unsigned width, unsigned heigth) = 0;
		virtual void GetViewport(unsigned& width, unsigned& heigth) = 0;
		virtual void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth) = 0;
		virtual void Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0) = 0;
		virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1) = 0;
		virtual void Clear() = 0;
		virtual void BuildResourceSet(IResourceSet* set_) = 0;
		virtual void BindResourceSet(IResourceSet* set_) = 0;
		virtual void UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size) = 0;
		virtual void EmitUAVBarrier(ICoreBuffer* buffer) = 0;
		virtual void TimerBegin(uint32_t timerID) = 0;
		virtual void TimerEnd(uint32_t timerID) = 0;
		virtual float TimerGetTimeInMs(uint32_t timerID) = 0;
	};

	class ICoreCopyCommandList
	{
	public:
		virtual ~ICoreCopyCommandList() = default;
		virtual void Free() = 0;
		virtual void CommandsBegin() = 0;
		virtual void CommandsEnd() = 0;
		virtual void Submit() = 0;
		virtual void WaitGPUAll() = 0;
	};

	class IWidowSurface
	{
	protected:
		unsigned width, height;
	public:
		virtual ~IWidowSurface() = default;
		void GetSubstents(unsigned& w, unsigned& h) { w = width; h = height; }
		virtual void Init(HWND hwnd, ICoreRenderer* render) = 0;
		virtual void ResizeBuffers(unsigned width_, unsigned height_) = 0;
		virtual void Present() = 0;
	};

	struct CoreRenderStat
	{
		uint64_t UniformBufferUpdates;
		uint64_t StateChanges;
		uint64_t Triangles;
		uint64_t DrawCalls;
		size_t committedMemory{};     // Bytes of memory currently committed/in-flight
		size_t totalMemory{};         // Total bytes of memory used by the allocators
		size_t totalPages{};          // Total page count
		size_t peakCommitedMemory{};  // Peak commited memory value since last reset
		size_t peakTotalMemory{};     // Peak total bytes
		size_t peakTotalPages{};      // Peak total page count
	};

	enum class BUFFER_FLAGS
	{
		NONE = 0,
		CPU_WRITE = 1 << 0,
		GPU_READ = 1 << 1,
		UNORDERED_ACCESS = 1 << 2,
		CONSTNAT_BUFFER = 1 << 3,
		RAW_BUFFER = 1 << 4,
	};
	DEFINE_ENUM_OPERATORS(BUFFER_FLAGS)

	class ICoreRenderer
	{
	protected:
		uint64_t frame{};
		bool Vsync{false};

	public:
		virtual ~ICoreRenderer() = default;
		virtual auto Init() -> void = 0;
		virtual auto Free() -> void = 0;
		virtual auto GetGraphicCommandContext()->ICoreGraphicCommandList* = 0;
		virtual auto GetCopyCommandContext()->ICoreCopyCommandList* = 0;

		// Surfaces
		virtual auto _FetchSurface(HWND hwnd)->surface_ptr = 0;
		virtual auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void = 0;
		virtual auto GetWindowSurface(HWND hwnd)->surface_ptr = 0;
		virtual auto PresentSurfaces() -> void = 0;

		virtual auto GetStat(CoreRenderStat& stat) -> void = 0;

		virtual auto IsVSync() -> bool = 0;

		// Resurces
		virtual bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
								  const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) = 0;

		virtual bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
										 const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) = 0;

		virtual bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
										const void* idxData, const IndexBufferDesc* idxDesc, BUFFER_FLAGS flags = BUFFER_FLAGS::GPU_READ) = 0;

		virtual bool CreateConstantBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, bool FastGPUread = false) = 0;

		virtual bool CreateStructuredBuffer(ICoreBuffer** out, LPCWSTR name, size_t structureSize, size_t num,
											const void* data = nullptr, BUFFER_FLAGS flags = BUFFER_FLAGS::NONE) = 0;

		virtual bool CreateRawBuffer(ICoreBuffer** out, LPCWSTR name, size_t size) = 0;

		//virtual bool CreateTexture(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> data, int32_t width, int32_t height,
		//				   TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags) = 0;

		virtual bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::unique_ptr<uint8_t[]> ddsData,
									   std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture) = 0;

		virtual bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) = 0;
	};

	enum class TEXTURE_FORMAT
	{
		// normalized
		R8,
		RG8,
		RGBA8,
		BGRA8,

		// float
		R16F,
		RG16F,
		RGBA16F,
		R32F,
		RG32F,
		RGBA32F,

		// integer
		R32UI,

		// compressed
		DXT1,
		DXT3,
		DXT5,

		// depth/stencil
		D24S8,

		UNKNOWN
	};

	enum class TEXTURE_CREATE_FLAGS : uint32_t
	{
		NONE = 0x00000000,

		FILTER = 0x0000000F,
		FILTER_POINT = 1, // magn = point,	min = point,	mip = point
		FILTER_BILINEAR = 2, // magn = linear,	min = linear,	mip = point
		FILTER_TRILINEAR = 3, // magn = linear,	min = linear,	mip = lenear
		FILTER_ANISOTROPY_2X = 4,
		FILTER_ANISOTROPY_4X = 5,
		FILTER_ANISOTROPY_8X = 6,
		FILTER_ANISOTROPY_16X = 7,

		COORDS = 0x00000F00,
		COORDS_WRAP = 1 << 8,
		//COORDS_MIRROR
		//COORDS_CLAMP
		//COORDS_BORDER

		USAGE = 0x0000F000,
		USAGE_RENDER_TARGET = 1 << 12,
		USAGE_UNORDRED_ACCESS = 1 << 13,

		MSAA = 0x000F0000,
		MSAA_2x = 2 << 16,
		MSAA_4x = 3 << 16,
		MSAA_8x = 4 << 16,

		MIPMAPS = 0xF0000000,
		GENERATE_MIPMAPS = 1 << 28,
		MIPMPAPS_PRESENTED = (1 << 28) + 1,
	};
	enum class TEXTURE_TYPE
	{
		TYPE_2D = 0x00000001,
		//TYPE_3D					= 0x00000001,
		TYPE_CUBE = 0x00000002,
		//TYPE_2D_ARRAY			= 0x00000003,
		//TYPE_CUBE_ARRAY			= 0x00000004
	};

	enum class VERTEX_BUFFER_FORMAT
	{
		FLOAT4,
	};

	enum class INDEX_BUFFER_FORMAT
	{
		UNSIGNED_16,
		UNSIGNED_32,
	};

	struct VertexAttributeDesc
	{
		uint32_t offset;
		VERTEX_BUFFER_FORMAT format;
		const char* semanticName;
	};

	struct VeretxBufferDesc
	{
		uint32_t vertexCount;
		int attributesCount;
		VertexAttributeDesc* attributes;
	};

	struct IndexBufferDesc
	{
		uint32_t vertexCount;
		INDEX_BUFFER_FORMAT format;
	};

	enum class CONSTANT_BUFFER_UPDATE_FRIQUENCY
	{
		PER_FRAME = 0,
		PER_DRAW
	};

	struct ConstantBuffersDesc
	{
		const char* name;
		CONSTANT_BUFFER_UPDATE_FRIQUENCY mode;
	};

	struct IResourceUnknown
	{
	private:
		uint16_t id;
		mutable int refs{};

		static std::vector<IResourceUnknown*> resources;
		static void ReleaseResource(int& refs, IResourceUnknown* ptr);

	public:
		IResourceUnknown(uint16_t id_);
		virtual ~IResourceUnknown() = default;

		void AddRef() const { refs++; }
		int GetRefs() { return refs; }
		void Release();
		uint16_t ID() const { return id; }
		static void CheckResources();
	};

	struct ICoreShader : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreShader();
	};

	struct ICoreVertexBuffer : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreVertexBuffer();
		virtual void SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset) = 0;
	};

	struct ICoreTexture : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreTexture();
	};

	struct ICoreBuffer : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreBuffer();
		virtual void GetData(void* data) = 0;
		virtual void SetData(const void* data, size_t size) = 0;
	};

	struct IResourceSet : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		IResourceSet();
		virtual void BindConstantBuffer(const char* name, ICoreBuffer* buffer) = 0;
		virtual void BindStructuredBufferSRV(const char* name, ICoreBuffer* buffer) = 0;
		virtual void BindStructuredBufferUAV(const char* name, ICoreBuffer* buffer) = 0;
		virtual void BindTextueSRV(const char* name, ICoreTexture* texture) = 0;
		virtual size_t FindInlineBufferIndex(const char* name) = 0;
	};

	enum class PRIMITIVE_TOPOLOGY
	{
		UNDEFINED = 0,
		POINT = 1,
		LINE = 2,
		TRIANGLE = 3,
		PATCH = 4
	};

	enum class BLEND_FACTOR
	{
		NONE = 0,
		ZERO,
		ONE,
		SRC_COLOR,
		ONE_MINUS_SRC_COLOR,
		SRC_ALPHA,
		ONE_MINUS_SRC_ALPHA,
		DEST_ALPHA,
		ONE_MINUS_DEST_ALPHA,
		DEST_COLOR,
		ONE_MINUS_DEST_COLOR,
		NUM
	};

	struct GraphicPipelineState
	{
		intrusive_ptr<ICoreShader> shader;
		intrusive_ptr<ICoreVertexBuffer> vb;
		PRIMITIVE_TOPOLOGY primitiveTopology;
		BLEND_FACTOR src;
		BLEND_FACTOR dst;
	};

	struct ComputePipelineState
	{
		ICoreShader* shader;
	};

	enum class SHADER_TYPE
	{
		SHADER_VERTEX,
		SHADER_FRAGMENT,
		SHADER_COMPUTE,
		NUM
	};

	enum RESOURCE_DEFINITION
	{
		RBF_NO_RESOURCE = 0,

		// Shader constants
		RBF_UNIFORM_BUFFER = 1 << 1,

		// Shader read-only resources
		RBF_TEXTURE_SRV = 1 << 2,
		RBF_BUFFER_SRV = 1 << 3,
		RBF_SRV = RBF_TEXTURE_SRV | RBF_BUFFER_SRV,

		// Shader unordered access resources
		RBF_TEXTURE_UAV = 1 << 4,
		RBF_BUFFER_UAV = 1 << 5,
		RBF_UAV = RBF_TEXTURE_UAV | RBF_BUFFER_UAV,
	};
	DEFINE_ENUM_OPERATORS(RESOURCE_DEFINITION)

		static UINT formatInBytes(VERTEX_BUFFER_FORMAT format)
	{
		switch (format)
		{
			case VERTEX_BUFFER_FORMAT::FLOAT4: return 16;
			default: assert(0);
		}
		return 0;
	}
	static UINT formatInBytes(INDEX_BUFFER_FORMAT format)
	{
		switch (format)
		{
			case INDEX_BUFFER_FORMAT::UNSIGNED_16: return 2;
			case INDEX_BUFFER_FORMAT::UNSIGNED_32: return 4;
			default: assert(0);
		}
		return 0;
	}

	psomap_checksum_t CalculateChecksum(const GraphicPipelineState& pso);
	psomap_checksum_t CalculateChecksum(const ComputePipelineState& pso);
}

