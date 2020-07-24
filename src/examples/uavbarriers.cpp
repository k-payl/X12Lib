#include "core.h"
#include "filesystem.h"
#include <string>
#include <iostream>

using namespace x12;

constexpr inline UINT float4chunks = 15;

std::vector<float> ExecuteGPU();

int main()
{
	engine::Core *core = engine::CreateCore();
	core->Init(engine::INIT_FLAGS::NO_WINDOW | engine::INIT_FLAGS::NO_INPUT | engine::INIT_FLAGS::NO_CONSOLE | engine::INIT_FLAGS::DIRECTX12_RENDERER);

	std::vector<float> result = ExecuteGPU();

	{
		auto* fs = core->GetFS();

		engine::File file = fs->OpenFile("out\\test3_out.txt", engine::FILE_OPEN_MODE::WRITE);

		std::string str = std::to_string(float4chunks);
		str += '\n';

		file.WriteStr(str.c_str());

		str.clear();

		std::cout << "Result (pow of two): ";

		for (size_t c = 0; c < float4chunks; c++)
		{
			str += std::to_string((int)result[c * 4]) + ' ';
			std::cout << result[c * 4] << ' ';
		}
		file.WriteStr(str.c_str());
	}

	core->Free();
	engine::DestroyCore(core);

	system("pause");

	return 0;
}

std::vector<float> ExecuteGPU()
{
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<ICoreBuffer> buffer;
	intrusive_ptr<IResourceSet> resources;

	ICoreRenderer* renderer = engine::GetCoreRenderer();
	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();	

	{
		auto text = engine::GetFS()->LoadFile(SHADER_DIR "uav.shader");

		const ConstantBuffersDesc buffersdesc[] =
		{
			"ChunkNumber",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW
		};
		renderer->CreateComputeShader(shader.getAdressOf(), L"uav.shader", text.get(), buffersdesc,
									  _countof(buffersdesc));
	}

	renderer->CreateStructuredBuffer(buffer.getAdressOf(), L"Unordered buffer for test barriers", 16, float4chunks, nullptr, BUFFER_FLAGS::UNORDERED_ACCESS);

	renderer->CreateResourceSet(resources.getAdressOf(), shader.get());
	resources->BindStructuredBufferUAV("tex_out", buffer.get());
	cmdList->CompileSet(resources.get());
	size_t chunkIdx = resources->FindInlineBufferIndex("ChunkNumber");

	cmdList->CommandsBegin();
		ComputePipelineState cpso{};
		cpso.shader = shader.get();

		cmdList->SetComputePipelineState(cpso);

		cmdList->BindResourceSet(resources.get());

		for (UINT i = 0; i< float4chunks; ++i)
		{
			cmdList->UpdateInlineConstantBuffer(chunkIdx, &i, 4);
			cmdList->Dispatch(1, 1);

			// Comment this and you'll get random values
			cmdList->EmitUAVBarrier(buffer.get());
		}
	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);

	std::vector<float> ret(4 * float4chunks);
	buffer->GetData(ret.data());

	return ret;
}

