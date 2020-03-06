#pragma once
#include "common.h"
#include "icorerender.h"

struct Dx12CoreShader final : public ICoreShader
{
	Dx12CoreShader();;

	void InitGraphic(const char* vertText, const char* fragText, const ConstantBuffersDesc* variabledesc, uint32_t varNum);
	void InitCompute(const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum);

	enum class ROOT_PARAMETER_TYPE
	{
		NONE,
		TABLE,
		INLINE_DESCRIPTOR,
		//INLINE_CONSTANT // TODO
	};

	struct RootSignatueResource
	{
		int slot;
		RESOURCE_DEFINITION resources;
	};

	struct RootSignatureParameter
	{
		ROOT_PARAMETER_TYPE type;
		SHADER_TYPE shaderType;
		int tableResourcesNum; // cache for tableResources.size()
		std::vector<RootSignatueResource> tableResources;
		RootSignatueResource inlineResource;
	};

	std::vector<RootSignatureParameter> rootSignatureParameters;

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

	bool hasResources{false};

	static IdGenerator<uint16_t> idGen;
	uint16_t id;
};

