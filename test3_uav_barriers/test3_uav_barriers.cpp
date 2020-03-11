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
	intrusive_ptr<Dx12CoreShader> comp;
	intrusive_ptr<Dx12CoreBuffer> compSB;
	Dx12UniformBuffer* compCB;
	Dx12CoreRenderer* renderer = CORE->GetCoreRenderer();
	Dx12GraphicCommandContext* context = renderer->GetGraphicCommmandContext();

	{
		auto text = CORE->GetFS()->LoadFile("uav.shader");


		renderer->CreateComputeShader(comp.getAdressOf(), L"uav.shader", text.get());
	}

	renderer->CreateUniformBuffer(&compCB, 4);

	renderer->CreateStructuredBuffer(compSB.getAdressOf(), L"Unordered buffer for test barriers", 16, float4chunks, nullptr, BUFFER_FLAGS::UNORDERED_ACCESS);

	context->CommandsBegin();

		ComputePipelineState cpso{};
		cpso.shader = comp.get();

		context->SetComputePipelineState(cpso);
		context->BindUniformBuffer(0, compCB, SHADER_TYPE::SHADER_COMPUTE);
		context->BindUnorderedAccessStructuredBuffer(1, compSB.get(), SHADER_TYPE::SHADER_COMPUTE);

		for (UINT i = 0; i< float4chunks; ++i)
		{
			context->UpdateUniformBuffer(compCB, &i, 0, 4);
			context->Dispatch(1, 1);

			// comment this and you'll get random values
			context->EmitUAVBarrier(compSB.get());
		}

		std::vector<float> ret(4 * float4chunks);
		compSB->GetData(ret.data());

	context->CommandsEnd();
	context->Submit();

	return ret;
}

