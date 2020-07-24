
#include "dx12resourceset.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12commandlist.h"

using namespace x12;

Dx12ResourceSet::Dx12ResourceSet(const Dx12CoreShader* shader)
{
	resourcesMap = shader->resourcesMap;

	rootParametersNum = shader->rootSignatureParameters.size();
	resources.resize(rootParametersNum);
	resourcesDirty.resize(rootParametersNum);
	gpuDescriptors.resize(rootParametersNum);

	for (size_t i = 0; i < shader->rootSignatureParameters.size(); ++i)
	{
		const d3d12::RootSignatureParameter<d3d12::ResourceDefinition>& in = shader->rootSignatureParameters[i];
		d3d12::RootSignatureParameter<BindedResource>& out = resources[i];

		out.type = in.type;
		out.shaderType = in.shaderType;
		out.tableResourcesNum = in.tableResourcesNum;

		if (in.type == d3d12::ROOT_PARAMETER_TYPE::INLINE_DESCRIPTOR)
		{
			out.inlineResource = in.inlineResource;
		}
		else if (in.type == d3d12::ROOT_PARAMETER_TYPE::TABLE)
		{
			out.tableResources.resize(in.tableResources.size());

			for (int j = 0; j < out.tableResources.size(); ++j)
				out.tableResources[j] = in.tableResources[j];
		}
		else
			unreacheble();
	}
}

void Dx12ResourceSet::BindConstantBuffer(const char* name, ICoreBuffer* buffer)
{
	Bind<ICoreBuffer, Dx12CoreBuffer>(name, buffer, RESOURCE_DEFINITION::RBF_UNIFORM_BUFFER);
}

void Dx12ResourceSet::BindStructuredBufferSRV(const char* name, ICoreBuffer* buffer)
{
	Bind<ICoreBuffer, Dx12CoreBuffer>(name, buffer, RESOURCE_DEFINITION::RBF_BUFFER_SRV);
}

void Dx12ResourceSet::BindStructuredBufferUAV(const char* name, ICoreBuffer* buffer)
{
	Bind<ICoreBuffer, Dx12CoreBuffer>(name, buffer, RESOURCE_DEFINITION::RBF_BUFFER_UAV);
}

void Dx12ResourceSet::BindTextueSRV(const char* name, ICoreTexture* texture)
{
	Bind<ICoreTexture, Dx12CoreTexture>(name, texture, RESOURCE_DEFINITION::RBF_TEXTURE_SRV);
}

void x12::Dx12ResourceSet::checkResourceIsTable(const resource_index& index)
{
	assert(index.second != -1 && "Resource is not in table");
}

void x12::Dx12ResourceSet::checkResourceIsInlineDescriptor(const resource_index& index)
{
	assert(index.second == -1 && "Resource in some table. Inline resource can not be in table");
}

Dx12ResourceSet::resource_index& Dx12ResourceSet::findResourceIndex(const char* name)
{
	auto it = resourcesMap.find(name);
	assert(it != resourcesMap.end());
	return it->second;
}

size_t Dx12ResourceSet::FindInlineBufferIndex(const char* name)
{
	auto& index = findResourceIndex(name);
	checkResourceIsInlineDescriptor(index);
	return index.first;
}
