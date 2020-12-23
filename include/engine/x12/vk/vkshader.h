#pragma once
#include "vkcommon.h"

namespace x12
{
	struct VkCoreShader final : public ICoreShader
	{
		VkCoreShader();
		~VkCoreShader();

		void InitCompute(LPCWSTR name, const char* text, const ConstantBuffersDesc* variabledesc, uint32_t varNum);

	private:
		VkShaderModule vkmodule_{};
		std::wstring name_;
	};
}
