#include "core.h"
#include "mainwindow.h"
#include "filesystem.h"
#include "resourcemanager.h"
#include "scenemanager.h"
#include "camera.h"
#include "gtest/gtest.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace x12;

class TestX12 : public ::testing::TestWithParam<engine::INIT_FLAGS>
{	
	engine::Core *core;

protected:
	engine::INIT_FLAGS api;

	const int w = 512;
	const int h = 512;
	const float aspect = float(w) / h;
	const std::string ext = ".png";
	std::unique_ptr<uint8_t[]> image;
	intrusive_ptr<ICoreTexture> colorTexture;
	intrusive_ptr<ICoreTexture> depthTexture;
	ICoreRenderer* renderer;

	void CaptureImage()
	{
		image = std::move(std::unique_ptr<uint8_t[]>(new uint8_t[w * h * 4]));
		colorTexture->GetData(image.get());

		for (int i = 0; i < h; i++)
			for (int j = 0; j < w; j++)
			{
				image[4 * (i * w + j) + 3] = 255;
			}
	}

	bool Exists(const std::string& name)
	{
		if (FILE* file = fopen(name.c_str(), "r")) {
			fclose(file);
			return true;
		}
		else
		{
			return false;
		}
	}

	std::pair<std::string, std::string> ImageNames()
	{
		const ::testing::TestInfo* const test_info =
			::testing::UnitTest::GetInstance()->current_test_info();

		printf("We are in test %s of test suite %s.\n",
			test_info->name(),
			test_info->test_suite_name());

		auto nameRef = "../../resources/tests_ref/" + std::string(test_info->name()) + ext;
		auto nameResult = "../../resources/tests_out/" + std::string(test_info->name());

		return { nameRef, nameResult };
	}

	int Compare()
	{
		auto names = ImageNames();

		try
		{
			if (Exists(names.first))
			{
				printf("File '%s' found\n", names.first.c_str());

				int w_;
				int h_;
				int comp;
				unsigned char* img = stbi_load(names.first.c_str(), &w_, &h_, &comp, STBI_rgb);

				if (w_ != w || h_ != h)
				{
					throw img;
				}

				std::unique_ptr<uint8_t[]> diff = std::move(std::unique_ptr<uint8_t[]>(new uint8_t[w * h * 4]));
				memset(diff.get(), 0, w * h * 4);

				int failed = 0;
				for (int i = 0; i < h; i++)
					for (int j = 0; j < w; j++)
					{
						char r = img[3 * (i * w + j) + 0];
						char g = img[3 * (i * w + j) + 1];
						char b = img[3 * (i * w + j) + 2];

						char rc = image[4 * (i * w + j) + 0];
						char gc = image[4 * (i * w + j) + 1];
						char bc = image[4 * (i * w + j) + 2];

						if (r != rc || g != gc || b != bc)
						{
							++failed;

							diff[4 * (i * w + j) + 0] = 255;
							diff[4 * (i * w + j) + 3] = 255;
						}
					}

				if (failed)
				{
					printf("Failed %d of %d (%f%%)\n", failed, w * h, 100.f * float(failed) / (w * h));
					printf("src[0].r=%c src1[0].r=%c\n", img[0], image[0]);

					std::string diffName = names.second + "_diff" + ext;
					stbi_write_png(diffName.c_str(), w, h, 4, diff.get(), 4 * w);
					printf("File '%s' saved", diffName.c_str());

					throw img;
				}

				stbi_image_free(img);
				printf("Comparision with '%s' passed\n", names.first.c_str());
			}
			else
			{
				stbi_write_png(names.first.c_str(), w, h, 4, image.get(), 4 * w);
				printf("File '%s' is not found. Create new reference\n", names.first.c_str());
			}
		}
		catch (unsigned char* img)
		{
			std::string failName = names.second + "_failed" + ext;
			stbi_write_png(failName.c_str(), w, h, 4, image.get(), 4 * w);
			stbi_image_free(img);
			return 1;
		}

		return 0;
	}

	void SetUp() override 
	{
		api = GetParam();

		const auto flags =
			api |
			engine::INIT_FLAGS::NO_WINDOW |
			engine::INIT_FLAGS::NO_INPUT |
			engine::INIT_FLAGS::NO_CONSOLE;

		core = engine::CreateCore();
		core->Init("", flags);

		renderer = engine::GetCoreRenderer();

		if (api != engine::INIT_FLAGS::VULKAN_RENDERER) // TODO: create vulkan texture
		{
			renderer->CreateTexture(colorTexture.getAdressOf(), L"target texture",
				nullptr, 0, w, h, 1, TEXTURE_TYPE::TYPE_2D,
				TEXTURE_FORMAT::RGBA8, TEXTURE_CREATE_FLAGS::USAGE_RENDER_TARGET);

			renderer->CreateTexture(depthTexture.getAdressOf(), L"depth texture",
				nullptr, 0, w, h, 1, TEXTURE_TYPE::TYPE_2D,
				TEXTURE_FORMAT::D32, TEXTURE_CREATE_FLAGS::USAGE_RENDER_TARGET);
		}
	}

	void TearDown() override 
	{
		colorTexture = 0;
		depthTexture = 0;
		core->Free();
		engine::DestroyCore(core);
	}
};

INSTANTIATE_TEST_SUITE_P(X12Scope, TestX12,
	::testing::Values(engine::INIT_FLAGS::DIRECTX12_RENDERER, engine::INIT_FLAGS::VULKAN_RENDERER));

/*
 * Render quad to texture.
 */ 

TEST_P(TestX12, X12_Texturing)
{
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<IResourceSet> resourceSet;
	intrusive_ptr<ICoreBuffer> cameraBuffer;
	intrusive_ptr<ICoreVertexBuffer> plane;
	engine::StreamPtr<engine::Mesh> mesh = engine::GetResourceManager()->CreateStreamMesh("std#plane");
	engine::StreamPtr<engine::Texture> tex = engine::GetResourceManager()->CreateStreamTexture(TEXTURES_DIR"chipped-paint-metal-albedo_3_512x512.dds", TEXTURE_CREATE_FLAGS::NONE);

	engine::Camera* cam = engine::GetSceneManager()->CreateCamera();

	{
		const ConstantBuffersDesc buffersdesc[] =
		{
			{"TransformCB",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW},
		};

		auto text = engine::GetFS()->LoadFile(SHADER_DIR "test_mesh.hlsl");

		renderer->CreateShader(shader.getAdressOf(), L"test_mesh.hlsl", text.get(), text.get(), buffersdesc, _countof(buffersdesc));
	}

	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	cmdList->SetRenderTargets(colorTexture.getAdressOf(), 1, depthTexture.get());
	cmdList->Clear();
	cmdList->SetViewport(w, h);
	cmdList->SetScissor(0, 0, w, h);

	GraphicPipelineState pso{};
	pso.shader = shader.get();
	pso.vb = mesh.get()->RenderVertexBuffer();
	pso.primitiveTopology = PRIMITIVE_TOPOLOGY::TRIANGLE;
	cmdList->SetGraphicPipelineState(pso);

	cmdList->SetVertexBuffer(mesh.get()->RenderVertexBuffer());

	struct
	{
		math::mat4 MVP;
		math::vec4 cameraPos;
	}camc;
	cam->GetModelViewProjectionMatrix(camc.MVP, aspect);
	camc.cameraPos = cam->GetWorldPosition();

	renderer->CreateBuffer(cameraBuffer.getAdressOf(),
		L"Camera constant buffer", sizeof(math::mat4) + 16,
		BUFFER_FLAGS::CONSTANT_BUFFER_VIEW, MEMORY_TYPE::CPU, &camc);

	renderer->CreateResourceSet(resourceSet.getAdressOf(), shader.get());
	resourceSet->BindConstantBuffer("CameraCB", cameraBuffer.get());
	resourceSet->BindTextueSRV("texture_", tex.get()->GetCoreTexture());
	cmdList->CompileSet(resourceSet.get());
	auto transformIdx = resourceSet->FindInlineBufferIndex("TransformCB");
	cmdList->BindResourceSet(resourceSet.get());

	struct
	{
		math::mat4 transform;
		math::mat4 NM;
	}cbdata;
	cbdata.NM = cbdata.transform.Inverse().Transpose();
	cmdList->UpdateInlineConstantBuffer(transformIdx, &cbdata, sizeof(cbdata));

	{
		cmdList->Draw(mesh.get()->RenderVertexBuffer());
	}

	cmdList->CommandsEnd();

	renderer->ExecuteCommandList(cmdList);

	CaptureImage();

	ASSERT_EQ(Compare(), 0);
}

/*
 * Set and get data for buffer.
 */
TEST_P(TestX12, X12_BufferGetSet)
{
	for (int setData = 0; setData < 2; ++setData)
	{
		for (int fooInt = (int)MEMORY_TYPE::CPU; fooInt != (int)MEMORY_TYPE::NUM; fooInt++)
		{
			MEMORY_TYPE bufferMemory = static_cast<MEMORY_TYPE>(fooInt);
			bool immidatelySetData = setData == 0;

			float data[] = { 1, 55, 123.323f, 0, -3232.3f, 444 };

			intrusive_ptr<ICoreBuffer> buffer;

			if (immidatelySetData)
			{
				renderer->CreateBuffer(buffer.getAdressOf(), L"Set get buffer",
					sizeof(data), BUFFER_FLAGS::NONE, bufferMemory, data);
			}
			else
			{
				renderer->CreateBuffer(buffer.getAdressOf(), L"Set get buffer",
					sizeof(data), BUFFER_FLAGS::NONE, bufferMemory, nullptr);

				buffer->SetData(data, sizeof(data));
			}

			decltype(data) readData{};
			buffer->GetData(readData);

			for (size_t i = 0; i < std::size(data); ++i)
			{
				EXPECT_EQ(readData[i], data[i]);
			}
		}
	}
}

/*
 * Calculate power of two.
 */
TEST_P(TestX12, X12_ComputeShader)
{
	constexpr UINT float4chunks = 15;
	std::vector<float> result;
	intrusive_ptr<ICoreShader> shader;
	intrusive_ptr<ICoreBuffer> buffer;
	intrusive_ptr<IResourceSet> resources;

	ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	{
		auto text = engine::GetFS()->LoadFile(SHADER_DIR "uav.hlsl");

		const ConstantBuffersDesc buffersdesc[] =
		{
			{"ChunkNumber",	CONSTANT_BUFFER_UPDATE_FRIQUENCY::PER_DRAW}
		};

		renderer->CreateComputeShader(shader.getAdressOf(), L"uav.hlsl", text.get(), buffersdesc,
			_countof(buffersdesc));
	}

	renderer->CreateBuffer(buffer.getAdressOf(), L"Power of two result buffer",
		16, BUFFER_FLAGS::UNORDERED_ACCESS_VIEW, MEMORY_TYPE::GPU_READ, nullptr, float4chunks);

	renderer->CreateResourceSet(resources.getAdressOf(), shader.get());
	resources->BindStructuredBufferUAV("powerOfTwo", buffer.get());
	cmdList->CompileSet(resources.get());
	const size_t chunkIdx = resources->FindInlineBufferIndex("ChunkNumber");

	cmdList->CommandsBegin();
	ComputePipelineState cpso{};
	cpso.shader = shader.get();

	cmdList->SetComputePipelineState(cpso);
	cmdList->BindResourceSet(resources.get());

	for (UINT i = 0; i < float4chunks; ++i)
	{
		cmdList->UpdateInlineConstantBuffer(chunkIdx, &i, 4);
		cmdList->Dispatch(1, 1);
		cmdList->EmitUAVBarrier(buffer.get());
	}

	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);

	result.resize(4 * float4chunks);
	buffer->GetData(result.data());

	size_t x = 1;
	for (size_t i = 0; i < result.size(); i += 4)
	{
		EXPECT_EQ(float(x), result[i]);
		x *= 2;
	}
}

class TestFS : public ::testing::Test
{
	engine::Core* core;

protected:
	void SetUp() override
	{
		const auto flags =
			engine::INIT_FLAGS::DIRECTX12_RENDERER |
			engine::INIT_FLAGS::NO_WINDOW |
			engine::INIT_FLAGS::NO_INPUT |
			engine::INIT_FLAGS::NO_CONSOLE;

		core = engine::CreateCore();
		core->Init("", flags);
	}

	void TearDown() override
	{
		core->Free();
		engine::DestroyCore(core);
	}
};

TEST_F(TestFS, FileExist)
{
	EXPECT_TRUE(engine::GetFS()->FileExist("fs/test.txt"));

	std::u8string str = u8"fs/текстовый файл";
	EXPECT_TRUE(engine::GetFS()->FileExist((char*)str.c_str()));

	str = u8"fs/糞紝.txt";
	EXPECT_TRUE(engine::GetFS()->FileExist((char*)str.c_str()));

	str = u8"fs/紝 тест/ⅳⅷ";
	EXPECT_TRUE(engine::GetFS()->FileExist((char*)str.c_str()));

	str = u8"несущствующий файл.txt";
	EXPECT_FALSE(engine::GetFS()->FileExist((char*)str.c_str()));
}

TEST_F(TestFS, DirectoryExist)
{
	std::u8string str = u8"fs/紝 тест";
	EXPECT_TRUE(engine::GetFS()->DirectoryExist((char*)str.c_str()));

	str = u8"несущствующая дирекория";
	EXPECT_FALSE(engine::GetFS()->DirectoryExist((char*)str.c_str()));
}

TEST_F(TestFS, CreateDeleteDirectory)
{
	std::u8string str = u8"fs/временная директория";
	engine::GetFS()->CreateDirectory_((char*)str.c_str());
	EXPECT_TRUE(engine::GetFS()->DirectoryExist((char*)str.c_str()));

	engine::GetFS()->DeleteDirectory((char*)str.c_str());
	EXPECT_FALSE(engine::GetFS()->DirectoryExist((char*)str.c_str()));
}

TEST_F(TestFS, RelativeDirectory)
{
	std::u8string str = u8"fs/ относительный путь";
	EXPECT_TRUE(engine::GetFS()->IsRelative((char*)str.c_str()));
	EXPECT_TRUE(engine::GetFS()->IsRelative(""));
	EXPECT_TRUE(engine::GetFS()->IsRelative("relative"));

	EXPECT_FALSE(engine::GetFS()->IsRelative("//c/programs not relative"));
	EXPECT_FALSE(engine::GetFS()->IsRelative("//c/programs/zzzzzz"));
	EXPECT_FALSE(engine::GetFS()->IsRelative("C:/programs/"));
	EXPECT_FALSE(engine::GetFS()->IsRelative("C:\\programs\\"));
	EXPECT_FALSE(engine::GetFS()->IsRelative("C:\\programs   "));
}

TEST_F(TestFS, ReadBinaryFile)
{
	std::u8string str = u8"fs/бинарный файл";
	EXPECT_TRUE(engine::GetFS()->FileExist((char*)str.c_str()));
	auto file = engine::GetFS()->OpenFile((char*)str.c_str(), engine::FILE_OPEN_MODE::BINARY | engine::FILE_OPEN_MODE::READ);
	size_t size = file.FileSize();
	std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
	file.Read(data.get(), size);
	EXPECT_TRUE(data[0] == 0);
	EXPECT_TRUE(data[1] == 1);
	EXPECT_TRUE(data[2] == 2);
}

TEST_F(TestFS, ReadTextFile)
{
	std::u8string str = u8"fs/текстовый файл";
	EXPECT_TRUE(engine::GetFS()->FileExist((char*)str.c_str()));
	auto file = engine::GetFS()->OpenFile((char*)str.c_str(), engine::FILE_OPEN_MODE::READ);
	size_t size = file.FileSize();
	std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
	file.Read(data.get(), size);
	EXPECT_TRUE(data[0] == '0');
	EXPECT_TRUE(data[1] == '1');
	EXPECT_TRUE(data[2] == '2');
}

TEST_F(TestFS, GetFilename)
{
	EXPECT_TRUE("file" == engine::GetFS()->GetFileName("F:\\file.txt", false));
	EXPECT_TRUE("file.txt" == engine::GetFS()->GetFileName("F:\\file.txt", true));
	EXPECT_TRUE("file" == engine::GetFS()->GetFileName("//c/dir/file.txt", false));
	EXPECT_TRUE("file.txt" == engine::GetFS()->GetFileName("//c/dir/file.txt", true));
	EXPECT_TRUE("file" == engine::GetFS()->GetFileName("aaa dir/file.txt", false));
	EXPECT_TRUE("file.txt" == engine::GetFS()->GetFileName("aaa dir/file.txt", true));
}

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	auto ret = RUN_ALL_TESTS();
	system("pause");
	return ret;
}
