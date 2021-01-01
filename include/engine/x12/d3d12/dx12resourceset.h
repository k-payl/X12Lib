#pragma once
#include "dx12common.h"
#include "dx12memory.h"
#include "intrusiveptr.h"
#include "dx12texture.h"
#include "dx12buffer.h"

namespace x12
{
	// Prebuild combination of static resources.
	//	that binds to context fast.
	//	Dynamic constant buffer can be updated through Dx12GraphicCommandList::UpdateInlineConstantBuffer().
	//
	struct Dx12ResourceSet : IResourceSet
	{
		using resource_index = std::pair<size_t, int>;

		struct BindedResource : x12::d3d12::ResourceDefinition
		{
			intrusive_ptr<Dx12CoreBuffer> buffer;
			intrusive_ptr<Dx12CoreTexture> texture;

			BindedResource& operator=(const ResourceDefinition& r)
			{
				static_cast<ResourceDefinition&>(*this) = r;
				return *this;
			}
		};

		std::unordered_map<std::string, std::pair<size_t, int>> resourcesMap; // {parameter index, table index}. (table index=-1 if inline)

		bool dirty{false};

		size_t rootParametersNum;

		// parallel arrays
		std::vector<x12::d3d12::RootSignatureParameter<BindedResource>> resources;
		std::vector<bool> resourcesDirty;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpuDescriptors;

	public:
		Dx12ResourceSet(const Dx12CoreShader* shader);

		X12_API void BindConstantBuffer(const char* name, ICoreBuffer* buffer) override;
		X12_API void BindStructuredBufferSRV(const char* name, ICoreBuffer* buffer) override;
		X12_API void BindStructuredBufferUAV(const char* name, ICoreBuffer* buffer) override;
		X12_API void BindTextueSRV(const char* name, ICoreTexture* texture) override;
		X12_API void BindTextueUAV(const char* name, ICoreTexture* texture) override;

		size_t FindInlineBufferIndex(const char* name) override;

	private:
		inline void checkResourceIsTable(const resource_index& index);
		inline void checkResourceIsInlineDescriptor(const resource_index& index);

		resource_index& findResourceIndex(const char* name);

		template<class TRes, class TDx12Res>
		void Bind(const char* name, TRes* resource, d3d12::RESOURCE_DEFINITION type)
		{
			auto dx12resource = static_cast<TDx12Res*>(resource);

			auto& index = findResourceIndex(name);
			checkResourceIsTable(index);

			Dx12ResourceSet::BindedResource& resourceSlot = resources[index.first].tableResources[index.second];

			verify(type == resourceSlot.resources && "Invalid resource type");

			if constexpr (std::is_same<TRes, ICoreBuffer>::value)
				resourceSlot.buffer = dx12resource;
			else if constexpr (std::is_same<TRes, ICoreTexture>::value)
				resourceSlot.texture = dx12resource;
			else
				verify(0);

			dirty = true;
			gpuDescriptors[index.first] = {};
			resourcesDirty[index.first] = true;
		}
	};
}
