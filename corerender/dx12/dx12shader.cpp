#include "pch.h"
#include "dx12shader.h"
#include "dx12render.h"
#include "dx12uniformbuffer.h"
#include <d3dcompiler.h>
#include <algorithm>

#define HLSL_VER "5_1"

IdGenerator<uint16_t> Dx12CoreShader::idGen;

struct ShaderReflectionResource
{
	std::string name;
	int slot;
	SHADER_TYPE shader;
	D3D_SHADER_INPUT_TYPE resourceType;
};

static D3D12_DESCRIPTOR_RANGE_TYPE ResourceToView(D3D_SHADER_INPUT_TYPE resource)
{
	switch (resource)
	{
		case D3D_SIT_CBUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case D3D_SIT_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case D3D_SIT_STRUCTURED: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		default:
			assert(0 && "Not impl");
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			break;
	}
}

static RESOURCE_BIND_FLAGS ResourceToBindFlag(D3D_SHADER_INPUT_TYPE resource)
{
	switch (resource)
	{
		case D3D_SIT_CBUFFER: return RESOURCE_BIND_FLAGS::UNIFORM_BUFFER;
		case D3D_SIT_TEXTURE: return RESOURCE_BIND_FLAGS::TEXTURE_SRV;
		case D3D_SIT_STRUCTURED: return RESOURCE_BIND_FLAGS::STRUCTURED_BUFFER_SRV;
		default:
			assert(0 && "Not impl");
			return RESOURCE_BIND_FLAGS::NONE;
			break;
	}
}

static std::vector<ShaderReflectionResource> fetchShaderReources(ComPtr<ID3DBlob> shader, SHADER_TYPE shaderType)
{
	std::vector<ShaderReflectionResource> resources;

	ID3D12ShaderReflection* reflection;
	ThrowIfFailed(D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)& reflection));

	D3D12_SHADER_DESC desc;
	reflection->GetDesc(&desc);
	
	// Each resource
	for (unsigned int i = 0; i < desc.BoundResources; ++i)
	{
		D3D12_SHADER_INPUT_BIND_DESC ibdesc;
		reflection->GetResourceBindingDesc(i, &ibdesc);

		resources.push_back({std::string(ibdesc.Name), (int)ibdesc.BindPoint, shaderType, ibdesc.Type});
	}

	return resources;
}

static const char* getShaderProfileName(SHADER_TYPE type)
{
	switch (type)
	{
		case SHADER_TYPE::SHADER_VERTEX: return "vs_" HLSL_VER;
		case SHADER_TYPE::SHADER_FRAGMENT: return "ps_" HLSL_VER;
	}
	assert(false);
	return nullptr;
}
static D3D_SHADER_MACRO* getShaderMacro(SHADER_TYPE type)
{
	static D3D_SHADER_MACRO vertDefines[] =
	{
		"VERTEX", "1",
		nullptr, nullptr
	};
	static D3D_SHADER_MACRO fragmentDefins[] =
	{
		"FRAGMENT", "1",
		nullptr, nullptr
	};

	switch (type)
	{
		case SHADER_TYPE::SHADER_VERTEX: return vertDefines;
		case SHADER_TYPE::SHADER_FRAGMENT: return fragmentDefins;
	}
	return nullptr;
}

static ComPtr<ID3DBlob> compileShader(const char* src, SHADER_TYPE type)
{
	ComPtr<ID3DBlob> shader;
	ComPtr<ID3DBlob> errorBlob;

	if (!src)
		return shader;

	constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR
	#if _DEBUG
		| D3DCOMPILE_DEBUG
		| D3DCOMPILE_SKIP_OPTIMIZATION;
	#else
		| D3DCOMPILE_OPTIMIZATION_LEVEL3;
	#endif

	if (FAILED(D3DCompile(src, strlen(src), "", getShaderMacro(type), NULL, "main", getShaderProfileName(type),
						  flags, 0, shader.GetAddressOf(), errorBlob.GetAddressOf())))
	{
		if (errorBlob)
		{
			unsigned char* error = (unsigned char*)errorBlob->GetBufferPointer();
			printf("%s\n", error);
			assert(0);
		}
	}
	return shader;
}

void Dx12CoreShader::Init(const char* vertText, const char* fragText, const ConstantBuffersDesc* buffersDesc, uint32_t descNum)
{
	vs = compileShader(vertText, SHADER_TYPE::SHADER_VERTEX);
	ps = compileShader(fragText, SHADER_TYPE::SHADER_FRAGMENT);

	id = idGen.getId();

	std::vector<ShaderReflectionResource> perDrawResources;
	std::vector<D3D12_ROOT_PARAMETER> d3dRootParameters;

	auto processShader = [this, descNum, buffersDesc, &d3dRootParameters, &perDrawResources]
	(std::vector<D3D12_DESCRIPTOR_RANGE>& d3dRangesOut, ComPtr<ID3DBlob> shaderIn,
	 D3D12_SHADER_VISIBILITY visibilityIn, SHADER_TYPE shaderTypeIn) -> bool
	{
		auto resources = fetchShaderReources(shaderIn, shaderTypeIn);
		if (resources.empty())
			return false;

		auto it = std::stable_partition(resources.begin(), resources.end(), [descNum, buffersDesc](const ShaderReflectionResource& r)
		{
			for (uint32_t i = 0; i < descNum; i++)
			{
				if (r.name == std::string(buffersDesc[i].name) && buffersDesc[i].mode == CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW)
					return false;
			}
			return true;
		});

		std::vector<ShaderReflectionResource> tableResources(resources.begin(), it);
		perDrawResources.insert(perDrawResources.end(), it, resources.end());

		if (!tableResources.empty())
		{
			d3dRangesOut.resize(tableResources.size());
			std::vector<RootSignatueResource> resourcesForTable;

			for (auto i = 0; i < tableResources.size(); i++)
			{
				const ShaderReflectionResource& shaderResource = tableResources[i];

				d3dRangesOut[i].RangeType = ResourceToView(shaderResource.resourceType);
				d3dRangesOut[i].NumDescriptors = 1; // TODO: group
				d3dRangesOut[i].BaseShaderRegister = shaderResource.slot;
				d3dRangesOut[i].RegisterSpace = 0;
				d3dRangesOut[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				resourcesForTable.push_back({ shaderResource.slot, ResourceToBindFlag(shaderResource.resourceType) });
			}

			rootSignatureParameters.push_back({ PARAMETER_TYPE::TABLE, shaderTypeIn, (int)resourcesForTable.size(), resourcesForTable, {-1} });

			D3D12_ROOT_DESCRIPTOR_TABLE d3dTable;
			d3dTable.NumDescriptorRanges = (UINT)d3dRangesOut.size();
			d3dTable.pDescriptorRanges = &d3dRangesOut[0];

			D3D12_ROOT_PARAMETER d3dParam;
			d3dParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			d3dParam.DescriptorTable = d3dTable;
			d3dParam.ShaderVisibility = visibilityIn;

			d3dRootParameters.push_back(d3dParam);
		}
		return true;
	};

	D3D12_ROOT_SIGNATURE_FLAGS flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	std::vector<D3D12_DESCRIPTOR_RANGE> d3dRangesVS;
	std::vector<D3D12_DESCRIPTOR_RANGE> d3dRangesPS;

	if (!processShader(d3dRangesVS, vs, D3D12_SHADER_VISIBILITY_VERTEX, SHADER_TYPE::SHADER_VERTEX))
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

	if (!processShader(d3dRangesPS, ps, D3D12_SHADER_VISIBILITY_PIXEL, SHADER_TYPE::SHADER_FRAGMENT))
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// TODO: other shaders

	// Inline descriptors
	for (int i = 0; i < perDrawResources.size(); ++i)
	{
		D3D12_ROOT_PARAMETER d3dParam{};

		switch (perDrawResources[i].resourceType)
		{
			case D3D_SIT_CBUFFER:
				d3dParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				break;

			case D3D_SIT_TEXTURE:
				d3dParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
				break;

			default:
				assert(0 && "Not impl");
				break;
		}

		d3dParam.Descriptor.RegisterSpace = 0;
		d3dParam.Descriptor.ShaderRegister = perDrawResources[i].slot;

		switch (perDrawResources[i].shader)
		{
			case SHADER_TYPE::SHADER_VERTEX:
				d3dParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case SHADER_TYPE::SHADER_FRAGMENT:
				d3dParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			default:
				assert(false);
				break;
		}

		d3dRootParameters.push_back(d3dParam);

		RootSignatueResource r;
		r.slot = perDrawResources[i].slot;
		r.resources = ResourceToBindFlag(perDrawResources[i].resourceType);

		rootSignatureParameters.push_back({ PARAMETER_TYPE::INLINE_DESCRIPTOR, perDrawResources[i].shader, {}, {}, r });
	}


	if (d3dRootParameters.empty())
		hasResources = false;
	else
	{
		hasResources = true;

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init((UINT)d3dRootParameters.size(),
							   &d3dRootParameters[0],
							   0,
							   nullptr,
							   flags);

		ID3DBlob* signature;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));

		ThrowIfFailed(CR_GetD3DDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&resourcesRootSignature)));
	}

}

void Dx12CoreShader::Release()
{
	--refs;
	assert(refs > 0);

	if (refs == 1)
		CR_ReleaseResource(refs, this);
}
