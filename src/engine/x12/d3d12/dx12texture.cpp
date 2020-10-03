
#include "core.h"
#include "dx12render.h"
#include "dx12texture.h"
#include "dx12render.h"
#include "dx12descriptorheap.h"
#include "dx12commandlist.h"

static HRESULT GetSurfaceInfo(
	_In_ size_t width,
	_In_ size_t height,
	_In_ DXGI_FORMAT fmt,
	size_t* outNumBytes,
	_Out_opt_ size_t* outRowBytes,
	_Out_opt_ size_t* outNumRows);

static HRESULT FillInitData(_In_ size_t width,
	_In_ size_t height,
	_In_ size_t depth,
	_In_ size_t mipCount,
	_In_ size_t arraySize,
	_In_ size_t numberOfPlanes,
	_In_ DXGI_FORMAT format,
	_In_ size_t maxsize,
	_In_ size_t bitSize,
	_In_reads_bytes_(bitSize) const uint8_t* bitData,
	_Out_ size_t& twidth,
	_Out_ size_t& theight,
	_Out_ size_t& tdepth,
	_Out_ size_t& skipMip,
	std::vector<D3D12_SUBRESOURCE_DATA>& initData);

void x12::Dx12CoreTexture::InitFromExisting(ID3D12Resource* resource_)
{
	resource.Attach(resource_);

	desc = resource->GetDesc();

	format_ = D3DToEng(desc.Format);

	if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		flags_ = flags_ | TEXTURE_CREATE_FLAGS::USAGE_RENDER_TARGET;

	if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		flags_ = flags_ | TEXTURE_CREATE_FLAGS::USAGE_UNORDRED_ACCESS;

	if (!(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
		flags_ = flags_ | TEXTURE_CREATE_FLAGS::USAGE_SHADER_RESOURCE;

	InitSRV();
	InitRTV();
	InitDSV();
	InitUAV();
}

void x12::Dx12CoreTexture::TransiteToState(D3D12_RESOURCE_STATES newState, ID3D12GraphicsCommandList* cmdList)
{
	if (state == newState)
		return;
	
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), state, newState);
	
	cmdList->ResourceBarrier(1, &barrier);
	
	state = newState;
}

void x12::Dx12CoreTexture::_GPUCopyToStaging(ICoreGraphicCommandList* cmdList)
{
	if (!stagingResource)
	{

		x12::memory::CreateCommittedBuffer(stagingResource.GetAddressOf(), WholeSize(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
		//CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_READBACK);

		//throwIfFailed(d3d12::CR_GetD3DDevice()->CreateCommittedResource(
		//	&defaultHeapProperties,
		//	D3D12_HEAP_FLAG_NONE,
		//	&desc,
		//	D3D12_RESOURCE_STATE_COPY_DEST,
		//	nullptr,
		//	IID_ID3D12Resource, reinterpret_cast<void**>(stagingResource.GetAddressOf())));

		x12::d3d12::set_name(stagingResource.Get(), L"Staging buffer for gpu->cpu copying %u bytes for '%s'", WholeSize(), L"");
	}

	{
		Dx12GraphicCommandList* dx12ctx = static_cast<Dx12GraphicCommandList*>(cmdList);
		auto d3dCmdList = dx12ctx->GetD3D12CmdList(); // TODO: avoid

		D3D12_RESOURCE_STATES oldState = state;
		if (state != D3D12_RESOURCE_STATE_COPY_SOURCE)
		{
			d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), state, D3D12_RESOURCE_STATE_COPY_SOURCE));
			state = D3D12_RESOURCE_STATE_COPY_SOURCE;
		}
		{
			size_t bytes;
			size_t outRow, outNUmRow;
			GetSurfaceInfo(desc.Width, desc.Height, EngToD3D(format_), &bytes, &outRow, &outNUmRow);

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
			footprint.Footprint.Width = desc.Width;
			footprint.Footprint.Height = desc.Height;
			footprint.Footprint.Depth = 1;
			footprint.Footprint.RowPitch = outRow;
			footprint.Footprint.Format = desc.Format;

			CD3DX12_TEXTURE_COPY_LOCATION Dst(stagingResource.Get(), footprint);
			CD3DX12_TEXTURE_COPY_LOCATION Src(resource.Get(), 0);

			d3dCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
		}
		if (oldState != state)
		{
			d3dCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), state, oldState));
			state = oldState;
		}
	}
}
void x12::Dx12CoreTexture::_GetStagingData(void* data)
{
	void* ptr;
	stagingResource->Map(0, nullptr, &ptr);

	memcpy(data, ptr, WholeSize());

	stagingResource->Unmap(0, nullptr);
}

UINT x12::Dx12CoreTexture::WholeSize()
{
	size_t bytes;
	size_t outRow, outNUmRow;
	GetSurfaceInfo(desc.Width, desc.Height, EngToD3D(format_), &bytes, &outRow, &outNUmRow);
	return UINT(bytes);
}

void x12::Dx12CoreTexture::GetData(void* data)
{
	ICoreRenderer* renderer = engine::GetCoreRenderer();
	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	_GPUCopyToStaging(cmdList);
	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);

	renderer->WaitGPUAll(); // execute current GPU work and wait

	_GetStagingData(data);
}

void x12::Dx12CoreTexture::InitSRV()
{
	if (!(flags_ & TEXTURE_CREATE_FLAGS::USAGE_SHADER_RESOURCE))
		return;

	SRVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticDescriptor();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // TODO
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	d3d12::CR_GetD3DDevice()->CreateShaderResourceView(resource.Get(), &srvDesc, SRVdescriptor.descriptor);
}

void x12::Dx12CoreTexture::InitRTV()
{
	if (flags_ & TEXTURE_CREATE_FLAGS::USAGE_RENDER_TARGET && !IsDepthStencil(format_))
	{
		RTVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticRTVDescriptor();
		d3d12::CR_GetD3DDevice()->CreateRenderTargetView(resource.Get(), nullptr, RTVdescriptor.descriptor);
	}
}

void x12::Dx12CoreTexture::InitUAV()
{
	if (flags_ & TEXTURE_CREATE_FLAGS::USAGE_UNORDRED_ACCESS)
	{
		UAVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticDescriptor();

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = desc.Format;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; // TODO
		uavDesc.Texture2D.MipSlice = 0; // TODO

		d3d12::CR_GetD3DDevice()->CreateUnorderedAccessView(resource.Get(), nullptr, &uavDesc, UAVdescriptor.descriptor);
	}
}

void x12::Dx12CoreTexture::InitDSV()
{
	if (flags_ & TEXTURE_CREATE_FLAGS::USAGE_RENDER_TARGET && IsDepthStencil(format_))
	{
		DSVdescriptor = d3d12::D3D12GetCoreRender()->AllocateStaticDSVDescriptor();

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = EngToD3D(format_);
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // TODO
		dsv.Texture2D.MipSlice = 0; // TODO
		dsv.Flags = D3D12_DSV_FLAG_NONE;

		d3d12::CR_GetD3DDevice()->CreateDepthStencilView(resource.Get(), &dsv, DSVdescriptor.descriptor);
	}
}

void x12::Dx12CoreTexture::Init(LPCWSTR name, const uint8_t* data, size_t size,
	int32_t width, int32_t height, uint32_t mipCount, TEXTURE_TYPE type, TEXTURE_FORMAT format, TEXTURE_CREATE_FLAGS flags)
{
	flags_ = flags;
	format_ = format;

	const UINT arraySize = (type == TEXTURE_TYPE::TYPE_CUBE) ? 6 : 1;
	const UINT depth = 1;

	desc.Width = static_cast<UINT>(width);
	desc.Height = static_cast<UINT>(height);
	desc.MipLevels = static_cast<UINT16>(mipCount);
	desc.DepthOrArraySize = arraySize;
	desc.Format = EngToD3D(format);

	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	if (flags & TEXTURE_CREATE_FLAGS::USAGE_RENDER_TARGET)
	{
		if (IsDepthStencil(format))
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		else
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	if (flags & TEXTURE_CREATE_FLAGS::USAGE_UNORDRED_ACCESS)
		desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	//if (!(flags & TEXTURE_CREATE_FLAGS::USAGE_SHADER_RESOURCE))
	//	desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // TODO: 1d, 3d 

	CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

	//x12::memory::CreateCommitted2DTexture()

	state = IsDepthStencil(format) ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = EngToD3D(format_);
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	throwIfFailed(d3d12::CR_GetD3DDevice()->CreateCommittedResource(
		&defaultHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		state,
		IsDepthStencil(format) ? &optimizedClearValue : nullptr,
		IID_ID3D12Resource, reinterpret_cast<void**>(resource.GetAddressOf())));
	x12::d3d12::set_name(resource.Get(), name);
	
	if (flags_ & TEXTURE_CREATE_FLAGS::USAGE_SHADER_RESOURCE && data)
	{
		// staging
		ComPtr<ID3D12Resource> uploadResource;
		x12::memory::CreateCommittedBuffer(uploadResource.GetAddressOf(), size,
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_UPLOAD);

		x12::d3d12::set_name(uploadResource.Get(), L"Upload buffer for cpu->gpu copying %u bytes for '%s'", size, name);

		size_t maxsize = size;
		size_t bitSize = size;

		// subresource
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		size_t skipMip = 0;
		size_t twidth = 0;
		size_t theight = 0;
		size_t tdepth = 0;
		const auto numberOfPlanes = 1;
		throwIfFailed(FillInitData(width, height, depth, mipCount, arraySize,
			numberOfPlanes, desc.Format,
			maxsize, bitSize, data,
			twidth, theight, tdepth, skipMip, subresources));

		auto* cmdList = GetCoreRender()->GetGraphicCommandList();
		Dx12GraphicCommandList* dx12ctx = static_cast<Dx12GraphicCommandList*>(cmdList);
		cmdList->CommandsBegin();

		UpdateSubresources(dx12ctx->GetD3D12CmdList(), resource.Get(), uploadResource.Get(), 0, 0, 1, &subresources[0]);

		cmdList->CommandsEnd();

		ICoreRenderer* renderer = engine::GetCoreRenderer();
		renderer->ExecuteCommandList(cmdList);

		renderer->WaitGPUAll(); // wait GPU copying upload -> default heap
	}

	InitSRV();
	InitRTV();
	InitDSV();
	InitUAV();
}

// From DDSTextureLoader12.cpp

static void AdjustPlaneResource(
	_In_ DXGI_FORMAT fmt,
	_In_ size_t height,
	_In_ size_t slicePlane,
	_Inout_ D3D12_SUBRESOURCE_DATA& res)
{
	switch (fmt)
	{
	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		if (!slicePlane)
		{
			// Plane 0
			res.SlicePitch = res.RowPitch * static_cast<LONG>(height);
		}
		else
		{
			// Plane 1
			res.pData = reinterpret_cast<const uint8_t*>(res.pData) + uintptr_t(res.RowPitch) * height;
			res.SlicePitch = res.RowPitch * ((static_cast<LONG>(height) + 1) >> 1);
		}
		break;

	case DXGI_FORMAT_NV11:
		if (!slicePlane)
		{
			// Plane 0
			res.SlicePitch = res.RowPitch * static_cast<LONG>(height);
		}
		else
		{
			// Plane 1
			res.pData = reinterpret_cast<const uint8_t*>(res.pData) + uintptr_t(res.RowPitch) * height;
			res.RowPitch = (res.RowPitch >> 1);
			res.SlicePitch = res.RowPitch * static_cast<LONG>(height);
		}
		break;
	}
}

//--------------------------------------------------------------------------------------
	// Get surface information for a particular format
	//--------------------------------------------------------------------------------------
static HRESULT GetSurfaceInfo(
	_In_ size_t width,
	_In_ size_t height,
	_In_ DXGI_FORMAT fmt,
	size_t* outNumBytes,
	_Out_opt_ size_t* outRowBytes,
	_Out_opt_ size_t* outNumRows)
{
	uint64_t numBytes = 0;
	uint64_t rowBytes = 0;
	uint64_t numRows = 0;

	bool bc = false;
	bool packed = false;
	bool planar = false;
	size_t bpe = 0;
	switch (fmt)
	{
	case DXGI_FORMAT_BC1_TYPELESS:
	case DXGI_FORMAT_BC1_UNORM:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC4_TYPELESS:
	case DXGI_FORMAT_BC4_UNORM:
	case DXGI_FORMAT_BC4_SNORM:
		bc = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_BC2_TYPELESS:
	case DXGI_FORMAT_BC2_UNORM:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_TYPELESS:
	case DXGI_FORMAT_BC3_UNORM:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC5_TYPELESS:
	case DXGI_FORMAT_BC5_UNORM:
	case DXGI_FORMAT_BC5_SNORM:
	case DXGI_FORMAT_BC6H_TYPELESS:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
	case DXGI_FORMAT_BC7_TYPELESS:
	case DXGI_FORMAT_BC7_UNORM:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
		bc = true;
		bpe = 16;
		break;

	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_YUY2:
		packed = true;
		bpe = 4;
		break;

	case DXGI_FORMAT_Y210:
	case DXGI_FORMAT_Y216:
		packed = true;
		bpe = 8;
		break;

	case DXGI_FORMAT_NV12:
	case DXGI_FORMAT_420_OPAQUE:
	case DXGI_FORMAT_P208:
		planar = true;
		bpe = 2;
		break;

	case DXGI_FORMAT_P010:
	case DXGI_FORMAT_P016:
		planar = true;
		bpe = 4;
		break;

	default:
		break;
	}

	if (bc)
	{
		uint64_t numBlocksWide = 0;
		if (width > 0)
		{
			numBlocksWide = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
		}
		uint64_t numBlocksHigh = 0;
		if (height > 0)
		{
			numBlocksHigh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
		}
		rowBytes = numBlocksWide * bpe;
		numRows = numBlocksHigh;
		numBytes = rowBytes * numBlocksHigh;
	}
	else if (packed)
	{
		rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		numRows = uint64_t(height);
		numBytes = rowBytes * height;
	}
	else if (fmt == DXGI_FORMAT_NV11)
	{
		rowBytes = ((uint64_t(width) + 3u) >> 2) * 4u;
		numRows = uint64_t(height) * 2u; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
		numBytes = rowBytes * numRows;
	}
	else if (planar)
	{
		rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
		numBytes = (rowBytes * uint64_t(height)) + ((rowBytes * uint64_t(height) + 1u) >> 1);
		numRows = height + ((uint64_t(height) + 1u) >> 1);
	}
	else
	{
		size_t bpp = x12::BitsPerPixel(fmt);
		if (!bpp)
			return E_INVALIDARG;

		rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
		numRows = uint64_t(height);
		numBytes = rowBytes * height;
	}

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
	static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
	if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX)
		return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
#else
	static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
#endif

	if (outNumBytes)
	{
		*outNumBytes = static_cast<size_t>(numBytes);
	}
	if (outRowBytes)
	{
		*outRowBytes = static_cast<size_t>(rowBytes);
	}
	if (outNumRows)
	{
		*outNumRows = static_cast<size_t>(numRows);
	}

	return S_OK;
}
static HRESULT FillInitData(_In_ size_t width,
	_In_ size_t height,
	_In_ size_t depth,
	_In_ size_t mipCount,
	_In_ size_t arraySize,
	_In_ size_t numberOfPlanes,
	_In_ DXGI_FORMAT format,
	_In_ size_t maxsize,
	_In_ size_t bitSize,
	_In_reads_bytes_(bitSize) const uint8_t* bitData,
	_Out_ size_t& twidth,
	_Out_ size_t& theight,
	_Out_ size_t& tdepth,
	_Out_ size_t& skipMip,
	std::vector<D3D12_SUBRESOURCE_DATA>& initData)
{
	if (!bitData)
	{
		return E_POINTER;
	}

	skipMip = 0;
	twidth = 0;
	theight = 0;
	tdepth = 0;

	size_t NumBytes = 0;
	size_t RowBytes = 0;
	const uint8_t* pEndBits = bitData + bitSize;

	initData.clear();

	for (size_t p = 0; p < numberOfPlanes; ++p)
	{
		const uint8_t* pSrcBits = bitData;

		for (size_t j = 0; j < arraySize; j++)
		{
			size_t w = width;
			size_t h = height;
			size_t d = depth;
			for (size_t i = 0; i < mipCount; i++)
			{
				HRESULT hr = GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);
				if (FAILED(hr))
					return hr;

				if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
					return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

				if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
				{
					if (!twidth)
					{
						twidth = w;
						theight = h;
						tdepth = d;
					}

					D3D12_SUBRESOURCE_DATA res =
					{
						reinterpret_cast<const void*>(pSrcBits),
						static_cast<LONG_PTR>(RowBytes),
						static_cast<LONG_PTR>(NumBytes)
					};

					AdjustPlaneResource(format, h, p, res);

					initData.emplace_back(res);
				}
				else if (!j)
				{
					// Count number of skipped mipmaps (first item only)
					++skipMip;
				}

				if (pSrcBits + (NumBytes * d) > pEndBits)
				{
					return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
				}

				pSrcBits += NumBytes * d;

				w = w >> 1;
				h = h >> 1;
				d = d >> 1;
				if (w == 0)
				{
					w = 1;
				}
				if (h == 0)
				{
					h = 1;
				}
				if (d == 0)
				{
					d = 1;
				}
			}
		}
	}

	return initData.empty() ? E_FAIL : S_OK;
}


