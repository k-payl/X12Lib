#include "vkshader.h"
#include "vkrender.h"
#include <dxcapi.h>

static ID3D12Resource* scratchResource;
static IDxcCompiler* pCompiler;
static IDxcLibrary* pLibrary;
static IDxcIncludeHandler* dxcIncludeHandler;

static VkShaderModule createShaderModule(const void* code, uint32_t size)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = size;
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code);

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(x12::vk::GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	return shaderModule;
}

x12::VkCoreShader::VkCoreShader()
{
}

x12::VkCoreShader::~VkCoreShader()
{
	vkDestroyShaderModule(x12::vk::GetDevice(), vkmodule_, nullptr);
}

void x12::VkCoreShader::InitCompute(LPCWSTR name, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum)
{
	name_ = name;

	// Initialize the DXC compiler and compiler helper
	if (!pCompiler)
	{
		throwIfFailed(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)&pCompiler));
		throwIfFailed(DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)&pLibrary));
		throwIfFailed(pLibrary->CreateIncludeHandler(&dxcIncludeHandler));
	}

	const auto dir = engine::ConvertFromUtf8ToUtf16(SHADER_DIR);
	//const auto path = dir + fileName;

	//// Open and read the file
	//std::ifstream shaderFile(path);
	//if (shaderFile.good() == false)
	//{
	//	throw std::logic_error("Cannot find shader file");
	//}
	//std::stringstream strStream;
	//strStream << shaderFile.rdbuf();
	//std::string sShader = strStream.str();

	// Create blob from the string
	ComPtr<IDxcBlobEncoding> pTextBlob;
	throwIfFailed(pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)text, (uint32_t)strlen(text), 0, pTextBlob.GetAddressOf()));

	std::vector<DxcDefine> dxDefines(/*defines.size()*/);
	//for (int i = 0; i < defines.size(); i++)
	//{
	//	dxDefines[i].Name = defines[i].first.c_str();
	//	dxDefines[i].Value = defines[i].second.c_str();
	//}
	const wchar_t* pArgs[] =
	{
		L"-spirv",			// Vulkan SPIR-V
		L"-Zpr",			//Row-major matrices
		L"-WX",				//Warnings as errors
#ifdef _DEBUG
		L"-Zi",				//Debug info
		L"-Qembed_debug",	//Embed debug info into the shader
		L"-Od",				//Disable optimization
#else
		L"-O3",				//Optimization level 3
#endif
	};

	bool compute = 1;

	// Compile
	ComPtr<IDxcOperationResult> pResult;

	throwIfFailed(pCompiler->Compile(
		pTextBlob.Get(),
		nullptr,//path.c_str(),
		compute ? L"main" : L"",
		compute ? L"cs_6_5" : L"lib_6_5",
		&pArgs[0], sizeof(pArgs) / sizeof(pArgs[0]),
		/*dxDefines.empty() ? : &dxDefines[0], dxDefines.size()*/nullptr, 0,
		dxcIncludeHandler,
		pResult.GetAddressOf()));

	// Verify the result
	HRESULT resultCode;
	throwIfFailed(pResult->GetStatus(&resultCode));

	if (FAILED(resultCode))
	{
		ComPtr<IDxcBlobEncoding> pError;
		throwIfFailed(pResult->GetErrorBuffer(pError.GetAddressOf()));

		// Convert error blob to a string
		std::vector<char> infoLog(pError->GetBufferSize() + 1);
		memcpy(infoLog.data(), pError->GetBufferPointer(), pError->GetBufferSize());
		infoLog[pError->GetBufferSize()] = 0;

		std::string errorMsg = "Shader Compiler Error:\n";
		errorMsg.append(infoLog.data());

		MessageBoxA(nullptr, errorMsg.c_str(), "Error!", MB_OK);
		throw std::logic_error("Failed compile shader");
	}

	ComPtr<IDxcBlob> pBlob;
	throwIfFailed(pResult->GetResult(pBlob.GetAddressOf()));

	vkmodule_ = createShaderModule(pBlob->GetBufferPointer(), pBlob->GetBufferSize());
}
