#pragma once
#include "common.h"
#include "icorerender.h"

namespace engine
{
	struct Shader
	{
	public:
		struct SafeConstantBuffersDesc
		{
			std::string name;
			x12::CONSTANT_BUFFER_UPDATE_FRIQUENCY mode;
		};

	public:
		Shader(std::string path_, const std::vector<Shader::SafeConstantBuffersDesc>& vardesc_, bool compute) : path(path_), vardesc(vardesc_), compute_(compute) {}

		bool Load();
		x12::ICoreShader* GetCoreShader() { return coreShader.get(); }

	private:
		intrusive_ptr<x12::ICoreShader> coreShader;
		std::string path;
		std::vector<Shader::SafeConstantBuffersDesc> vardesc;
		bool compute_{ false };
	};
}
