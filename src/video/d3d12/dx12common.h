#pragma once
#include "common.h"
#include "icorerender.h"

namespace DirectX {
	struct GraphicsMemoryStatistics;
	class GraphicsMemory;
};

namespace x12
{
	// Resources associated with window
	struct Dx12WindowSurface : public IWidowSurface
	{
		ComPtr<swapchain_t> swapChain;

		ComPtr<ID3D12Resource> colorBuffers[DeferredBuffers];
		ComPtr<ID3D12Resource> depthBuffer;

		ComPtr<ID3D12DescriptorHeap> descriptorHeapRTV;
		ComPtr<ID3D12DescriptorHeap> descriptorHeapDSV;

		D3D12_RESOURCE_STATES state{};

		void Init(HWND hwnd, ICoreRenderer* render) override;
		void ResizeBuffers(unsigned width_, unsigned height_) override;
		void Present() override;
	};

	namespace d3d12
	{
		bool CheckTearingSupport();
		DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format);

		template<typename... Arguments>
		void set_name(ID3D12Object* obj, LPCWSTR format, Arguments ...args)
		{
			WCHAR wstr[256];
			wsprintf(wstr, format, args...);
			obj->SetName(wstr);
		}
	}

	enum class ROOT_PARAMETER_TYPE
	{
		NONE,
		TABLE,
		INLINE_DESCRIPTOR,
		//INLINE_CONSTANT // TODO
	};

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
}
