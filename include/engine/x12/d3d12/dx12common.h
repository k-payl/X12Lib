#pragma once
#include "common.h"
#include "icorerender.h"
#include "dx12descriptorheap.h"

namespace DirectX {
	struct GraphicsMemoryStatistics;
	class GraphicsMemory;
};

namespace x12
{
	inline constexpr size_t NumRenderDescriptors = 1'024;

	x12::TEXTURE_FORMAT D3DToEng(DXGI_FORMAT format);
	DXGI_FORMAT EngToD3D(x12::TEXTURE_FORMAT format);
	size_t BitsPerPixel(DXGI_FORMAT format);

	// Resources associated with window
	struct Dx12WindowSurface : public IWidowSurface
	{
		ComPtr<swapchain_t> swapChain;

		intrusive_ptr<x12::ICoreTexture> colorBuffers[engine::DeferredBuffers];
		intrusive_ptr<x12::ICoreTexture> depthBuffer;

		void Init(HWND hwnd, ICoreRenderer* render) override;
		void ResizeBuffers(unsigned width_, unsigned height_) override;
		void Present() override;
		void* GetNativeResource(int i) override;
	};

	namespace d3d12
	{
		enum class ROOT_PARAMETER_TYPE
		{
			NONE,
			TABLE,
			INLINE_DESCRIPTOR,
			//INLINE_CONSTANT // TODO
		};

		// TODO: move to private d3d namespace
		enum RESOURCE_DEFINITION
		{
			RBF_NO_RESOURCE,

			// Shader constants
			RBF_UNIFORM_BUFFER = 1 << 0,

			// Shader read-only resources
			RBF_TEXTURE_SRV = 1 << 1,
			RBF_BUFFER_SRV = 1 << 2,
			RBF_SRV = RBF_TEXTURE_SRV | RBF_BUFFER_SRV,

			// Shader unordered access resources
			RBF_TEXTURE_UAV = 1 << 3,
			RBF_BUFFER_UAV = 1 << 4,
			RBF_UAV = RBF_TEXTURE_UAV | RBF_BUFFER_UAV,

			RBF_SAMPLER = 1 << 5,
		};
		DEFINE_ENUM_OPERATORS(RESOURCE_DEFINITION)

		struct ResourceDefinition
		{
			int slot;
			std::string name;
			RESOURCE_DEFINITION resources;
		};

		template<class T>
		struct RootSignatureParameter
		{
			ROOT_PARAMETER_TYPE type;
			SHADER_TYPE shaderType;
			int tableResourcesNum; // cache for tableResources.size()
			std::vector<T> tableResources;
			T inlineResource;
		};

		bool CheckTearingSupport();
		DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format);

		template<typename... Arguments>
		void set_name(ID3D12Object* obj, LPCWSTR format, Arguments ...args)
		{
			WCHAR wstr[256];
			wsprintf(wstr, format, args...);
			obj->SetName(wstr);
		}

		static ID3D12DescriptorHeap* CreateDescriptorHeap(device_t* device, UINT numDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, bool gpu = false)
		{
			ID3D12DescriptorHeap* heap;

			D3D12_DESCRIPTOR_HEAP_DESC desc = {};
			desc.NumDescriptors = numDescriptors;
			desc.Type = type;
			desc.Flags = gpu ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			throwIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)));

			return heap;
		}
	}
}
