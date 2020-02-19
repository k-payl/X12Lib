#pragma once
#include "common.h"
#include "icorerender.h"

struct Dx12CoreShader final : public ICoreShader
{
	void Init(const char* vertText, const char* fragText, const ConstantBuffersDesc* variabledesc, uint32_t varNum);
	~Dx12CoreShader() override = default;

	enum class PARAMETER_TYPE
	{
		NONE,
		TABLE,
		INLINE_DESCRIPTOR,
		//INLINE_CONSTANT // TODO
	};

	struct RootSignatueResource
	{
		int slot;
		RESOURCE_BIND_FLAGS resources;
	};

	struct RootSignatureParameter
	{
		PARAMETER_TYPE type;
		SHADER_TYPE shaderType;
		int tableResourcesNum;
		std::vector<RootSignatueResource> tableResources;
		RootSignatueResource inlineResource;
	};

	std::vector<RootSignatureParameter> rootSignatureParameters;

	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;

	bool HasResources() { return hasResources; }
	ComPtr<ID3D12RootSignature> resourcesRootSignature;

	uint16_t ID() { return id; }

private:

	bool hasResources{false};

	static IdGenerator<uint16_t> idGen;
	uint16_t id;
};

