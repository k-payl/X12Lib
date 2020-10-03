#include "shader.h"
#include "core.h"
#include "filesystem.h"

bool engine::Shader::Load()
{
	auto text = engine::GetFS()->LoadFile(path.c_str());

	std::vector<x12::ConstantBuffersDesc> buffersdesc;
	buffersdesc.resize(vardesc.size());

	for (int i = 0 ; i < vardesc.size(); i++)
	{
		buffersdesc[i].mode = vardesc[i].mode;
		buffersdesc[i].name = vardesc[i].name.c_str();
	}

	if (compute_)
		return GetCoreRenderer()->CreateComputeShader(coreShader.getAdressOf(), ConvertFromUtf8ToUtf16(path).c_str(), text.get(), buffersdesc.empty()? nullptr : &buffersdesc[0], buffersdesc.size());
	else
		return GetCoreRenderer()->CreateShader(coreShader.getAdressOf(), ConvertFromUtf8ToUtf16(path).c_str(), text.get(), text.get(), buffersdesc.empty()? nullptr : &buffersdesc[0], buffersdesc.size());
}
