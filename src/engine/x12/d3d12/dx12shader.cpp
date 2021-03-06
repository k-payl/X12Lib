
#include "dx12shader.h"
#include "dx12render.h"
#include "core.h"
#include "filesystem.h"
#include <d3dcompiler.h>
#include <algorithm>

#define HLSL_VER "5_1"

using namespace x12;
using namespace x12::d3d12;

static D3D12_DESCRIPTOR_RANGE_TYPE ResourceToView(D3D_SHADER_INPUT_TYPE resource)
{
	switch (resource)
	{
		case D3D_SIT_CBUFFER: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case D3D_SIT_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case D3D_SIT_STRUCTURED: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		case D3D_SIT_UAV_RWSTRUCTURED: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case D3D_SIT_UAV_RWTYPED: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case D3D_SIT_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		default:
			notImplemented();
			return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			break;
	}
}

static RESOURCE_DEFINITION ResourceToBindFlag(D3D_SHADER_INPUT_TYPE resource)
{
	switch (resource)
	{
		case D3D_SIT_CBUFFER: return RESOURCE_DEFINITION::RBF_UNIFORM_BUFFER;
		case D3D_SIT_TEXTURE: return RESOURCE_DEFINITION::RBF_TEXTURE_SRV;
		case D3D_SIT_STRUCTURED: return RESOURCE_DEFINITION::RBF_BUFFER_SRV;
		case D3D_SIT_UAV_RWSTRUCTURED: return RESOURCE_DEFINITION::RBF_BUFFER_UAV;
		case D3D_SIT_UAV_RWTYPED: return RESOURCE_DEFINITION::RBF_TEXTURE_UAV;
		case D3D_SIT_SAMPLER: return RBF_SAMPLER;
		default:
			notImplemented();
			return RESOURCE_DEFINITION::RBF_NO_RESOURCE;
			break;
	}
}

static std::vector<x12::Dx12CoreShader::ShaderReflectionResource> fetchShaderReources(ComPtr<ID3DBlob> shader, SHADER_TYPE shaderType)
{
	std::vector<x12::Dx12CoreShader::ShaderReflectionResource> resources;

	ID3D12ShaderReflection* reflection;
	throwIfFailed(D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)& reflection));

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
		case SHADER_TYPE::SHADER_COMPUTE: return "cs_" HLSL_VER;
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

	static D3D_SHADER_MACRO computeDefins[] =
	{
		"COMPUTE", "1",
		nullptr, nullptr
	};

	switch (type)
	{
		case SHADER_TYPE::SHADER_VERTEX: return vertDefines;
		case SHADER_TYPE::SHADER_FRAGMENT: return fragmentDefins;
		case SHADER_TYPE::SHADER_COMPUTE: return computeDefins;
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

	static struct IncludeHandler : public ID3DInclude
	{
		std::shared_ptr<char[]> text;

		STDMETHOD(Open)(THIS_ D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes) override
		{
			std::string s = SHADER_DIR;
			s += pFileName;

			text = engine::GetFS()->LoadFile(s.c_str());
			*pBytes = (UINT)strlen(text.get());
			*ppData = text.get();
			return 0;
		};
		STDMETHOD(Close)(THIS_ LPCVOID pData) override
		{
			text = nullptr;
			return 0;
		};
	}includeH;

	if (FAILED(D3DCompile(src, strlen(src), "", getShaderMacro(type), &includeH, "main", getShaderProfileName(type),
						  flags, 0, shader.GetAddressOf(), errorBlob.GetAddressOf())))
	{
		if (errorBlob)
		{
			unsigned char* error = (unsigned char*)errorBlob->GetBufferPointer();
			printf("%s\n", error);
			throw std::runtime_error("");
		}
	}
	return shader;
}

x12::Dx12CoreShader::Dx12CoreShader()
{
}

bool x12::Dx12CoreShader::processShader(const ConstantBuffersDesc* buffersDesc,
								   uint32_t descNum,
								   std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters,
								   std::vector<ShaderReflectionResource>& perDrawResources,
								   std::vector<D3D12_DESCRIPTOR_RANGE>& d3dRangesOut,
								   std::vector< D3D12_STATIC_SAMPLER_DESC>& staticSamplers,
								   ComPtr<ID3DBlob> shaderIn,
								   D3D12_SHADER_VISIBILITY visibilityIn,
								   SHADER_TYPE shaderTypeIn)
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
		// SRV UAV CBV
		{
			std::vector<d3d12::ResourceDefinition> srv_uav_cbv_table;

			for (auto i = 0; i < tableResources.size(); i++)
			{
				const ShaderReflectionResource& shaderResource = tableResources[i];
				if (shaderResource.resourceType == D3D_SHADER_INPUT_TYPE::D3D10_SIT_SAMPLER)
				{
					D3D12_STATIC_SAMPLER_DESC sampler{};
					sampler.Filter = D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
					sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
					FLOAT MipLODBias = 0;
					//UINT MaxAnisotropy;
					//D3D12_COMPARISON_FUNC ComparisonFunc;
					//D3D12_STATIC_BORDER_COLOR BorderColor;
					//FLOAT MinLOD;
					//FLOAT MaxLOD;
					sampler.ShaderRegister = shaderResource.slot;
					sampler.RegisterSpace = 0;
					sampler.ShaderVisibility= visibilityIn;
					staticSamplers.push_back(sampler);
				}
				else
				{

					D3D12_DESCRIPTOR_RANGE Ranges{};
					Ranges.RangeType = ResourceToView(shaderResource.resourceType);
					Ranges.NumDescriptors = 1; // TODO: group
					Ranges.BaseShaderRegister = shaderResource.slot;
					Ranges.RegisterSpace = 0;
					Ranges.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					d3dRangesOut.push_back(Ranges);

					srv_uav_cbv_table.push_back({ shaderResource.slot, shaderResource.name, ResourceToBindFlag(shaderResource.resourceType) });
				}
			}

			// Add root parameter
			rootSignatureParameters.push_back({ d3d12::ROOT_PARAMETER_TYPE::TABLE, shaderTypeIn, (int)srv_uav_cbv_table.size(), srv_uav_cbv_table, {-1} });
		}

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
}
void x12::Dx12CoreShader::addInlineDescriptors(std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters,
										  const std::vector<ShaderReflectionResource>& perDrawResources)
{
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
				notImplemented();
				break;
		}

		d3dParam.Descriptor.ShaderRegister = perDrawResources[i].slot;

		switch (perDrawResources[i].shader)
		{
			case SHADER_TYPE::SHADER_VERTEX:
				d3dParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case SHADER_TYPE::SHADER_FRAGMENT:
				d3dParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case SHADER_TYPE::SHADER_COMPUTE:
				d3dParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				break;
			default:
				assert(false);
				break;
		}

		d3dRootParameters.insert(d3dRootParameters.begin(), d3dParam);

		d3d12::ResourceDefinition r;
		r.slot = perDrawResources[i].slot;
		r.resources = ResourceToBindFlag(perDrawResources[i].resourceType);
		r.name = perDrawResources[i].name;

		rootSignatureParameters.insert(rootSignatureParameters.begin(), {d3d12::ROOT_PARAMETER_TYPE::INLINE_DESCRIPTOR, perDrawResources[i].shader, {}, {}, r });
	}

}

void x12::Dx12CoreShader::initRootSignature(const std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters, D3D12_ROOT_SIGNATURE_FLAGS flags,
	const std::vector< D3D12_STATIC_SAMPLER_DESC>& staticSamplers)
{
	if (d3dRootParameters.empty())
		hasResources = false;
	else
	{
		hasResources = true;

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init((UINT)d3dRootParameters.size(),
							   &d3dRootParameters[0],
							   (UINT)staticSamplers.size(),
								staticSamplers.empty() ? nullptr : &staticSamplers[0],
							   flags);

		ID3DBlob* signature;
		throwIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));

		throwIfFailed(d3d12::CR_GetD3DDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&resourcesRootSignature)));
		x12::d3d12::set_name(resourcesRootSignature.Get(), L"Root signature for shader '%s'", name.c_str());
	}
}

void x12::Dx12CoreShader::initResourcesMap()
{
	resourcesMap.clear();

	for (size_t i = 0; i < rootSignatureParameters.size(); ++i)
	{
		const d3d12::RootSignatureParameter<d3d12::ResourceDefinition>& in = rootSignatureParameters[i];

		if (in.type == d3d12::ROOT_PARAMETER_TYPE::INLINE_DESCRIPTOR)
		{
			resourcesMap[in.inlineResource.name] = {i, -1};
		}
		else if (in.type == d3d12::ROOT_PARAMETER_TYPE::TABLE)
		{
			for (int j = 0; j < in.tableResources.size(); ++j)
				resourcesMap[in.tableResources[j].name] = { i, j };
		}
		else
			unreacheble();
	}
}

void x12::Dx12CoreShader::InitGraphic(LPCWSTR name_, const char* vertText, const char* fragText, const ConstantBuffersDesc* buffersDesc, uint32_t descNum)
{
	name = name_;

	vs = compileShader(vertText, SHADER_TYPE::SHADER_VERTEX);
	ps = compileShader(fragText, SHADER_TYPE::SHADER_FRAGMENT);	

	std::vector<ShaderReflectionResource> perDrawResources;
	std::vector<D3D12_ROOT_PARAMETER> d3dRootParameters;
	std::vector< D3D12_STATIC_SAMPLER_DESC> staticSamplers;

	D3D12_ROOT_SIGNATURE_FLAGS flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	std::vector<D3D12_DESCRIPTOR_RANGE> d3dRangesVS;
	if (!processShader(buffersDesc, descNum, d3dRootParameters, perDrawResources, d3dRangesVS, staticSamplers,
					   vs, D3D12_SHADER_VISIBILITY_VERTEX, SHADER_TYPE::SHADER_VERTEX))
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

	std::vector<D3D12_DESCRIPTOR_RANGE> d3dRangesPS;
	if (!processShader(buffersDesc, descNum, d3dRootParameters, perDrawResources, d3dRangesPS, staticSamplers,
					   ps, D3D12_SHADER_VISIBILITY_PIXEL, SHADER_TYPE::SHADER_FRAGMENT))
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	addInlineDescriptors(d3dRootParameters, perDrawResources);

	initRootSignature(d3dRootParameters, flags, staticSamplers);

	initResourcesMap();
}

void x12::Dx12CoreShader::InitCompute(LPCWSTR name_, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	name = name_;

	cs = compileShader(text, SHADER_TYPE::SHADER_COMPUTE);

	std::vector<ShaderReflectionResource> perDrawResources;
	std::vector<D3D12_ROOT_PARAMETER> d3dRootParameters;
	std::vector< D3D12_STATIC_SAMPLER_DESC> staticSamplers;

	D3D12_ROOT_SIGNATURE_FLAGS flags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	std::vector<D3D12_DESCRIPTOR_RANGE> d3dRangesCS;
	processShader(variabledesc, varNum, d3dRootParameters, perDrawResources, d3dRangesCS, staticSamplers,
				  cs, D3D12_SHADER_VISIBILITY_ALL, SHADER_TYPE::SHADER_COMPUTE); // compute uses D3D12_SHADER_VISIBILITY_ALL

	addInlineDescriptors(d3dRootParameters, perDrawResources);

	initRootSignature(d3dRootParameters, flags, staticSamplers);

	initResourcesMap();
}
