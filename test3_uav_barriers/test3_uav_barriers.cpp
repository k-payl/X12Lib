#include "core.h"
#include "dx12render.h"
#include "dx12shader.h"
#include "dx12context.h"
#include "dx12buffer.h"
#include "filesystem.h"
#include <string>

constexpr inline UINT float4chunks = 15;

std::vector<float> ExecuteGPU();

int main()
{
	Core *core = new Core();
	core->Init(nullptr, nullptr, INIT_FLAGS::NO_WINDOW | INIT_FLAGS::NO_INPUT | INIT_FLAGS::NO_CONSOLE | INIT_FLAGS::BUILT_IN_DX12_RENDERER);

	std::vector<float> result = ExecuteGPU();

	{
		auto* fs = core->GetFS();

		File file = fs->OpenFile("out\\test3_out.txt", FILE_OPEN_MODE::WRITE);

		std::string str = std::to_string(float4chunks);
		str += '\n';

		file.WriteStr(str.c_str());

		str.clear();

		for (size_t c = 0; c < float4chunks; c++)
			str += std::to_string((int)result[c * 4]) + ' ';

		file.WriteStr(str.c_str());
	}

	core->Free();
	delete core;

	return 0;
}

std::vector<float> ExecuteGPU()
{
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<ICoreBuffer> buffer;
	intrusive_ptr<IResourceSet> resources;

	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();
	Dx12GraphicCommandContext* context = renderer->GetGraphicCommmandContext();	

	{
		auto text = CORE->GetFS()->LoadFile("uav.shader");

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
	context->BuildResourceSet(resources.get());
	size_t chunkIdx = resources->FindInlineBufferIndex("ChunkNumber");

	context->CommandsBegin();

		ComputePipelineState cpso{};
		cpso.shader = shader.get();

		context->SetComputePipelineState(cpso);

		context->BindResourceSet(resources.get());

		for (UINT i = 0; i< float4chunks; ++i)
		{
			context->UpdateInlineConstantBuffer(chunkIdx, &i, 4);
			context->Dispatch(1, 1);

			// Comment this and you'll get random values
			context->EmitUAVBarrier(buffer.get());
		}

		std::vector<float> ret(4 * float4chunks);
		buffer->GetData(ret.data());

	context->CommandsEnd();
	context->Submit();

	return ret;
}

