#include "pch.h"
#include "dx12shader.h"
#include "dx12render.h"
#include "dx12uniformbuffer.h"
#include <d3dcompiler.h>
#include <algorithm>

IdGenerator<uint16_t> Dx12CoreShader::idGen;

Dx12CoreShader::Dx12CoreShader(ComPtr<ID3DBlob> vertex_, ComPtr<ID3DBlob> frag_, const ConstantBuffersDesc* buffersDesc, uint32_t descNum) :
	vs(vertex_),
	ps(frag_)
{
	using DxShaderReflectionResource = Dx12CoreShader::ShaderReflectionResource;
	
	id = idGen.getId();

	std::vector<ShaderReflectionResource> perDrawResources;
	std::vector<D3D12_ROOT_PARAMETER> d3dRootParameters;

	auto processShader = [this, descNum, buffersDesc, &d3dRootParameters, &perDrawResources]
	(
		ComPtr<ID3DBlob> shader,
		D3D12_SHADER_VISIBILITY visibility,
		std::vector<D3D12_DESCRIPTOR_RANGE>& ranges,
		SHADER_TYPE type) -> bool
	{
		auto resources = fetchShaderReources(shader, type);
		if (resources.empty())
			return false;

		auto it = std::stable_partition(resources.begin(), resources.end(), [descNum, buffersDesc](const Dx12CoreShader::ShaderReflectionResource& r)
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
			ranges.resize(tableResources.size());

			for (auto i = 0; i < tableResources.size(); i++)
			{
				const ShaderReflectionResource& buf = tableResources[i];

				ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				ranges[i].NumDescriptors = 1;
				ranges[i].BaseShaderRegister = buf.slot;
				ranges[i].RegisterSpace = 0;
				ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}

			D3D12_ROOT_DESCRIPTOR_TABLE vertexDescriptorTable;
			vertexDescriptorTable.NumDescriptorRanges = (UINT)ranges.size();
			vertexDescriptorTable.pDescriptorRanges = &ranges[0];

			D3D12_ROOT_PARAMETER param;
			param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			param.DescriptorTable = vertexDescriptorTable;
			param.ShaderVisibility = visibility;

			d3dRootParameters.push_back(param);
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

	if (!processShader(vs, D3D12_SHADER_VISIBILITY_VERTEX, d3dRangesVS, SHADER_TYPE::SHADER_VERTEX))
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

	std::vector<int> ranges;
	for (int i = 0; i < d3dRangesVS.size(); ++i)
	{
		for(int j = 0; j < d3dRangesVS[i].NumDescriptors; ++j)
			ranges.push_back({ (int)d3dRangesVS[i].BaseShaderRegister + j });
	}
	if (!d3dRangesVS.empty())
		rootSignatureParameters.push_back({ PARAMETER_TYPE::TABLE, -1, SHADER_TYPE::SHADER_VERTEX, (int)ranges.size(), ranges});

	if (!processShader(ps, D3D12_SHADER_VISIBILITY_PIXEL, d3dRangesPS, SHADER_TYPE::SHADER_FRAGMENT))
		flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	
	ranges.clear();
	for (int i = 0; i < d3dRangesPS.size(); ++i)
	{
		for (int j = 0; j < d3dRangesPS[i].NumDescriptors; ++j)
			ranges.push_back({ (int)d3dRangesPS[i].BaseShaderRegister + j });
	}
	if (!d3dRangesPS.empty())
		rootSignatureParameters.push_back({ PARAMETER_TYPE::TABLE, -1, SHADER_TYPE::SHADER_FRAGMENT, (int)ranges.size(), ranges });

	// TODO: other
	
	// inline descriptors...
	for (int i = 0; i < perDrawResources.size(); ++i)
	{
		D3D12_ROOT_PARAMETER d3dParam{};
		d3dParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; //TODO: other
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
		}// TODO: other

		d3dRootParameters.push_back(d3dParam);

		//
		rootSignatureParameters.push_back({ PARAMETER_TYPE::INLINE_DESCRIPTOR, perDrawResources[i].slot, perDrawResources[i].shader, {} });
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

		ThrowIfFailed(CR_GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&resourcesRootSignature)));
	}
}

std::vector<Dx12CoreShader::ShaderReflectionResource> Dx12CoreShader::fetchShaderReources(ComPtr<ID3DBlob> shader, SHADER_TYPE type)
{
	std::vector<Dx12CoreShader::ShaderReflectionResource> ret;

	ID3D12ShaderReflection* reflection;
	ThrowIfFailed(D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)& reflection));

	D3D12_SHADER_DESC desc;
	reflection->GetDesc(&desc);

	// each constant buffer
	for (unsigned int i = 0; i < desc.ConstantBuffers; ++i)
	{
		ID3D12ShaderReflectionConstantBuffer* buffer = reflection->GetConstantBufferByIndex(i);

		D3D12_SHADER_BUFFER_DESC bufferDesc;
		buffer->GetDesc(&bufferDesc);

		UINT registerIndex = 0;
		for (unsigned int k = 0; k < desc.BoundResources; ++k)
		{
			D3D12_SHADER_INPUT_BIND_DESC ibdesc;
			reflection->GetResourceBindingDesc(k, &ibdesc);
		
			if (!strcmp(ibdesc.Name, bufferDesc.Name))
			{
				registerIndex = ibdesc.BindPoint;
				break;
			}
		}

		ret.push_back({std::string(bufferDesc.Name), (int)registerIndex, type});
	}

	return ret;
}

void Dx12CoreShader::Release()
{
	CR_ReleaseResource(refs, this);
}