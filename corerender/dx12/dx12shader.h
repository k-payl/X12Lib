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
		std::vector<RootSignatureParameter<ResourceDefinition>> rootSignatureParameters;

		ComPtr<ID3DBlob> vs;
		ComPtr<ID3DBlob> ps;

		ComPtr<ID3DBlob> cs;

		bool HasResources() { return hasResources; }
		ComPtr<ID3D12RootSignature> resourcesRootSignature;

		uint16_t ID() { return id; }

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
										   ComPtr<ID3DBlob> shaderIn,
										   D3D12_SHADER_VISIBILITY visibilityIn,
										   SHADER_TYPE shaderTypeIn);

		void addInlineDescriptors(std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters, const std::vector<ShaderReflectionResource>& perDrawResources);
		void initRootSignature(const std::vector<D3D12_ROOT_PARAMETER>& d3dRootParameters, D3D12_ROOT_SIGNATURE_FLAGS flags);
		void initResourcesMap();

		bool hasResources{false};

		static IdGenerator<uint16_t> idGen;
		uint16_t id;

		std::wstring name;
	};
}

