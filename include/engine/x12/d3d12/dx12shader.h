#pragma once
#include "dx12common.h"

namespace x12
{
	struct Dx12CoreShader final : public ICoreShader
	{
		Dx12CoreShader();

		void InitGraphic(LPCWSTR name, const char* vertText, const char* fragText, const ConstantBuffersDesc* variabledesc, uint32_t varNum);
		void InitCompute(LPCWSTR name, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum);

		std::unordered_map<std::string, std::pair<size_t, int>> resourcesMap;
		std::vector<d3d12::RootSignatureParameter<d3d12::ResourceDefinition>> rootSignatureParameters;

		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;

		ComPtr<ID3DBlob> cs;

		bool HasResources() { return hasResources; }
		ComPtr<ID3D12RootSignature> resourcesRootSignature;

		struct ShaderReflectionResource
		{
			std::string name;
			int slot;
			SHADER_TYPE shader;
			D3D_SHADER_INPUT_TYPE resourceType;
		};

	private:

		bool processShader(const ConstantBuffersDesc* buffersDesc, uint32_t descNum,
										   std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters,
										   std::vector<ShaderReflectionResource>& perDrawResources,
										   std::vector<D3D12_DESCRIPTOR_RANGE>& d3dRangesOut,
										   std::vector< D3D12_STATIC_SAMPLER_DESC>& staticSamplers,
										   ComPtr<ID3DBlob> shaderIn,
										   D3D12_SHADER_VISIBILITY visibilityIn,
										   SHADER_TYPE shaderTypeIn);

		void addInlineDescriptors(std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters, const std::vector<ShaderReflectionResource>& perDrawResources);
		void initRootSignature(const std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters, D3D12_ROOT_SIGNATURE_FLAGS flags,
			const std::vector< D3D12_STATIC_SAMPLER_DESC>& staticSamplers);
		void initResourcesMap();

		bool hasResources{false};

		std::wstring name;
	};
}

