#pragma once
#include "common.h"
#include "icorerender.h"


struct Dx12CoreShader : public ICoreShader
{
	Dx12CoreShader(ComPtr<ID3DBlob> vertex_, ComPtr<ID3DBlob> frag_, const ConstantBuffersDesc* variabledesc, uint32_t varNum);
	~Dx12CoreShader() override = default;

	enum class PARAMETER_TYPE
	{
		NONE,
		TABLE,
		INLINE_DESCRIPTOR,
		//INLINE_CONSTANT
	};

	//struct Range
	//{
	//	int slot{-1};
	//	int num{0};
	//};

	struct RootSignatureParameter
	{
		PARAMETER_TYPE type;
		int slot{-1};
		SHADER_TYPE shaderType;
		int buffersNum;
		std::vector<int> buffers;
	};

	std::vector<RootSignatureParameter> rootSignatureParameters;

	ComPtr<ID3DBlob> vs;
	ComPtr<ID3DBlob> ps;

	bool HasResources() { return hasResources; }
	ComPtr<ID3D12RootSignature> resourcesRootSignature;

	uint16_t ID() { return id; }

	inline void AddRef() override { refs++; }
	inline int GetRefs() override { return refs; }
	void Release() override;

private:

	struct ShaderReflectionResource
	{
		std::string name;
		int slot;
		SHADER_TYPE shader;
	};
	std::vector<ShaderReflectionResource> fetchShaderReources(ComPtr<ID3DBlob> shader, SHADER_TYPE type);

	bool hasResources{false};

	int refs{};

	static IdGenerator<uint16_t> idGen;
	uint16_t id;
};

