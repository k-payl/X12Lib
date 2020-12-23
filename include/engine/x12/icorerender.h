#pragma once
#include "common.h"
#include "intrusiveptr.h"

namespace x12
{
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
	struct ICoreQuery;

	extern ICoreRenderer* _coreRender;
	FORCEINLINE ICoreRenderer* GetCoreRender() { return _coreRender; }

	using surface_ptr = std::shared_ptr<IWidowSurface>;

	class ICoreCopyCommandList
	{
	protected:
		enum class State
		{
			Free,
			Opened,
			Recordered,
			Submited,
		} state_{ State::Free };

		uint64_t submitedValue{};
		int32_t id{ -1 };

	public:
		ICoreCopyCommandList(int32_t id_) { id = id_; }

		uint64_t SubmitedValue() const { return submitedValue; }
		bool	 ReadyForOpening() { return state_ == State::Free || state_ == State::Recordered; }
		bool	 IsSubmited() { return state_ == State::Submited; }
		bool	 Unnamed() { return id == -1; }
		int32_t	 ID() { return id; }

		virtual void NotifySubmited(uint64_t submited)
		{
			assert(state_ == State::Recordered);
			state_ = State::Submited;
			submitedValue = submited;
		}
		virtual void NotifyFrameCompleted(uint64_t completed)
		{
			assert(state_ == State::Submited);
			state_ = State::Recordered;
			submitedValue = 0;
		}

	public:
		virtual ~ICoreCopyCommandList() = default;
		X12_API virtual void FrameEnd() = 0;
		X12_API virtual void Free() = 0;
		X12_API virtual void CommandsBegin() = 0;
		X12_API virtual void CommandsEnd() = 0;
	};

	class ICoreGraphicCommandList : public ICoreCopyCommandList
	{
	public:
		ICoreGraphicCommandList(int32_t id_) : ICoreCopyCommandList(id_) {}

	public:
		virtual ~ICoreGraphicCommandList() = default;
		X12_API virtual void BindSurface(surface_ptr& surface_) = 0;
		X12_API virtual void SetRenderTargets(ICoreTexture** textures, uint32_t count, ICoreTexture* depthStencil) = 0;
		X12_API virtual void PushState() = 0;
		X12_API virtual void PopState() = 0;
		X12_API virtual void SetGraphicPipelineState(const GraphicPipelineState& gpso) = 0;
		X12_API virtual void SetComputePipelineState(const ComputePipelineState& cpso) = 0;
		X12_API virtual void SetVertexBuffer(ICoreVertexBuffer* vb) = 0;
		X12_API virtual void SetViewport(unsigned width, unsigned heigth) = 0;
		X12_API virtual void GetViewport(unsigned& width, unsigned& heigth) = 0;
		X12_API virtual void SetScissor(unsigned x, unsigned y, unsigned width, unsigned heigth) = 0;
		X12_API virtual void Draw(const ICoreVertexBuffer* vb, uint32_t vertexCount = 0, uint32_t vertexOffset = 0) = 0;
		X12_API virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z = 1) = 0;
		X12_API virtual void Clear() = 0;
		X12_API virtual void CompileSet(IResourceSet* set_) = 0;
		X12_API virtual void BindResourceSet(IResourceSet* set_) = 0;
		X12_API virtual void UpdateInlineConstantBuffer(size_t idx, const void* data, size_t size) = 0;
		X12_API virtual void EmitUAVBarrier(ICoreBuffer* buffer) = 0;
		X12_API virtual void StartQuery(ICoreQuery* query) = 0;
		X12_API virtual void StopQuery(ICoreQuery* query) = 0;
		X12_API virtual void* GetNativeResource() = 0;
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
		virtual void* GetNativeResource(int i) = 0;
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
		UNORDERED_ACCESS_VIEW = 1 << 0,
		SHADER_RESOURCE_VIEW = 1 << 1,
		CONSTANT_BUFFER_VIEW = 1 << 2,
		RAW_BUFFER = 1 << 3
	};
	DEFINE_ENUM_OPERATORS(BUFFER_FLAGS)

	enum class MEMORY_TYPE
	{
		CPU = 1,
		GPU_READ,
		READBACK,
		NUM
	};
	DEFINE_ENUM_OPERATORS(MEMORY_TYPE)

	class ICoreRenderer
	{
	protected:
		uint64_t frame{};
		UINT frameIndex{}; // 0, 1, 2
		bool Vsync{false};

	public:
		virtual ~ICoreRenderer() = default;

		uint64_t Frame() const { return frame; }
		uint64_t FrameIndex() const { return frameIndex; }

		X12_API virtual auto Init() -> void = 0;
		X12_API virtual auto Free() -> void = 0;
		X12_API virtual auto GetGraphicCommandList()->ICoreGraphicCommandList* = 0;
		X12_API virtual auto GetGraphicCommandList(int32_t id)->ICoreGraphicCommandList* = 0;
		X12_API virtual auto GetCopyCommandContext()->ICoreCopyCommandList* = 0;

		// Surfaces
		X12_API virtual auto _FetchSurface(HWND hwnd)->surface_ptr = 0;
		X12_API virtual auto RecreateBuffers(HWND hwnd, UINT newWidth, UINT newHeight) -> void = 0;
		X12_API virtual auto GetWindowSurface(HWND hwnd)->surface_ptr = 0;
		X12_API virtual auto PresentSurfaces() -> void = 0;
		X12_API virtual auto FrameEnd() -> void = 0;
		X12_API virtual auto ExecuteCommandList(ICoreCopyCommandList* cmdList) -> void = 0;
		X12_API virtual auto WaitGPU() -> void = 0;
		X12_API virtual auto WaitGPUAll() -> void = 0;

		X12_API virtual auto GetStat(CoreRenderStat& stat) -> void = 0;

		X12_API virtual auto IsVSync() -> bool = 0;

		// Resurces
		X12_API virtual bool CreateShader(ICoreShader** out, LPCWSTR name, const char* vertText, const char* fragText,
								  const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) = 0;

		X12_API virtual bool CreateComputeShader(ICoreShader** out, LPCWSTR name, const char* text,
										 const ConstantBuffersDesc* variabledesc = nullptr, uint32_t varNum = 0) = 0;

		X12_API virtual bool CreateVertexBuffer(ICoreVertexBuffer** out, LPCWSTR name, const void* vbData, const VeretxBufferDesc* vbDesc,
										const void* idxData, const IndexBufferDesc* idxDesc, MEMORY_TYPE mem) = 0;

		X12_API virtual bool CreateBuffer(ICoreBuffer** out, LPCWSTR name, size_t size, BUFFER_FLAGS flags, MEMORY_TYPE mem, const void* data = nullptr, size_t num = 1) = 0;

		X12_API virtual bool CreateTexture(ICoreTexture** out, LPCWSTR name, const uint8_t* data, size_t size, int32_t width, int32_t height, uint32_t mipCount,
						   TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags) = 0;

		// TODO: remove d3d dependency
		X12_API virtual bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, std::vector<D3D12_SUBRESOURCE_DATA> subresources, ID3D12Resource* d3dexistingtexture) = 0;

		X12_API virtual bool CreateTextureFrom(ICoreTexture** out, LPCWSTR name, ID3D12Resource* d3dexistingtexture) = 0;

		X12_API virtual bool CreateResourceSet(IResourceSet** out, const ICoreShader* shader) = 0;

		X12_API virtual bool CreateQuery(ICoreQuery** out) = 0;

		X12_API virtual void* GetNativeDevice() = 0;

		X12_API virtual void* GetNativeGraphicQueue() = 0; // Tmp
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
		D32,
		D24S8,

		UNKNOWN
	};
	inline bool IsDepthStencil(TEXTURE_FORMAT format)
	{
		return format == TEXTURE_FORMAT::D24S8 || format == TEXTURE_FORMAT::D32;
	}
	inline bool HasStencil(TEXTURE_FORMAT format)
	{
		return format == TEXTURE_FORMAT::D24S8;
	}

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
		USAGE_SHADER_RESOURCE = 1 << 11,
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
	DEFINE_ENUM_OPERATORS(TEXTURE_CREATE_FLAGS)

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
		FLOAT3,
		FLOAT2,
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
		PER_FRAME,
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

		X12_API void AddRef() const { refs++; }
		X12_API int GetRefs() { return refs; }
		X12_API void Release();
		X12_API uint16_t ID() const { return id; }
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
		X12_API virtual void SetData(const void* vbData, size_t vbSize, size_t vbOffset, const void* idxData, size_t idxSize, size_t idxOffset) = 0;
	};

	struct ICoreTexture : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreTexture();
		X12_API virtual void* GetNativeResource() = 0;
		X12_API virtual void GetData(void* data) = 0;
	};

	struct ICoreBuffer : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreBuffer();
		X12_API virtual void GetData(void* data) = 0;
		X12_API virtual void SetData(const void* data, size_t size) = 0;
	};

	struct IResourceSet : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		IResourceSet();
		X12_API virtual void BindConstantBuffer(const char* name, ICoreBuffer* buffer) = 0;
		X12_API virtual void BindStructuredBufferSRV(const char* name, ICoreBuffer* buffer) = 0;
		X12_API virtual void BindStructuredBufferUAV(const char* name, ICoreBuffer* buffer) = 0;
		X12_API virtual void BindTextueSRV(const char* name, ICoreTexture* texture) = 0;
		X12_API virtual void BindTextueUAV(const char* name, ICoreTexture* texture) = 0;
		X12_API virtual size_t FindInlineBufferIndex(const char* name) = 0;
	};

	struct ICoreQuery : public IResourceUnknown
	{
		static IdGenerator<uint16_t> idGen;

	public:
		ICoreQuery();
		virtual float GetTime() = 0;
	};

	enum class PRIMITIVE_TOPOLOGY
	{
		UNDEFINED,
		POINT = 1,
		LINE = 2,
		TRIANGLE = 3,
		PATCH = 4
	};

	enum class BLEND_FACTOR
	{
		NONE,
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

	static UINT formatInBytes(VERTEX_BUFFER_FORMAT format)
	{
		switch (format)
		{
			case VERTEX_BUFFER_FORMAT::FLOAT4: return 16;
			case VERTEX_BUFFER_FORMAT::FLOAT3: return 12;
			case VERTEX_BUFFER_FORMAT::FLOAT2: return 8;
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
	UINT64 VbSizeFromDesc(const VeretxBufferDesc* vbDesc);
	UINT64 getBufferStride(const VeretxBufferDesc* vbDesc);
}

