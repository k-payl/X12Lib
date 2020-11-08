#include "common.h"
#include "core.h"
#include "mesh.h"
#include "filesystem.h"
#include "scenemanager.h"
#include "model.h"
#include "light.h"
#include "cpp_hlsl_shared.h"

#include <set>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "tiny_gltf.h"

#include "yaml-cpp/yaml.h"


int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	std::string path = "..\\..\\sponza-gltf-pbr\\sponza.glb";

	engine::Core* core = engine::CreateCore();
	core->Init("", engine::INIT_FLAGS::NONE);

	std::string ext = engine::GetFS()->FileExtension(path.c_str());
	std::string name = engine::GetFS()->GetFileName(path.c_str(), false);
	engine::GetFS()->CreateDirectory_(name.c_str());

	bool binary = ext == "glb";

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = binary ?
		loader.LoadBinaryFromFile(&model, &err, &warn, path) :
		loader.LoadASCIIFromFile(&model, &err, &warn, path);

	if (!warn.empty()) {
		printf("Warn: %s\n", warn.c_str());
	}

	if (!err.empty()) {
		printf("Err: %s\n", err.c_str());
	}

	if (!ret) {
		printf("Failed to parse glTF\n");
		return -1;
	}

	if (model.lights.empty())
	{
		auto l = engine::GetSceneManager()->CreateLight();

		l->SetWorldScale(math::vec3(10, 10, 10));
		l->SetWorldRotation(math::quat(0, 0, -180));
		l->SetWorldPosition(math::vec3(0,0,20));
	}

	const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];

	struct MeshInfo
	{
		int index;
		math::vec4 transform;
	};

	std::map<int, math::mat4> meshes;

	for (size_t i = 0; i < scene.nodes.size(); i++)
	{
		const tinygltf::Node& node = model.nodes[scene.nodes[i]];

		if (!node.scale.empty())
		{
			math::mat4 tt;
			math::compositeTransform(tt, math::vec3(), math::quat(),
				math::vec3(node.scale[0], node.scale[1], node.scale[2]));

			meshes.emplace(node.mesh, tt);

		}
		if (!node.matrix.empty())
		{
			float t[16];
			for (int i = 0; i < 16; i++)
				t[i] = node.matrix[i];
			math::mat4 tt(t);

			meshes.emplace(node.mesh, tt);
		} else
			meshes.emplace(node.mesh, math::mat4());
	}

	std::set<size_t> bufferViews;

	for (auto& m : meshes)
	{
		const tinygltf::Mesh& mesh = model.meshes[m.first];

		for (size_t i = 0; i < mesh.primitives.size(); ++i)
		{
			tinygltf::Primitive primitive = mesh.primitives[i];
			tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

			engine::MeshHeader header{};
			header.magic[0] = 'M';
			header.magic[1] = 'F';
			header.version = 0;

			int is_normals = 0;
			int is_uv = 0;
			int is_tangent = 0;
			int is_binormal = 0;
			int is_color = 0;
			uint32_t vertecies = 0;

			std::vector<engine::Shaders::Vertex> data;

			// normal, poistion,...
			for (auto& attrib : primitive.attributes)
			{
				tinygltf::Accessor accessor = model.accessors[attrib.second];
				int byteStride =
					accessor.ByteStride(model.bufferViews[accessor.bufferView]);				

				tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
				tinygltf::Buffer& buffer = model.buffers[view.buffer];

				vertecies = indexAccessor.count? indexAccessor.count : accessor.count;

				if (attrib.first.compare("POSITION") == 0)
				{
					bufferViews.insert(view.byteOffset);

					if (data.empty())
						data.resize(vertecies);

					if (indexAccessor.count)
					{
						tinygltf::BufferView& indexview = model.bufferViews[indexAccessor.bufferView];
						tinygltf::Buffer& indexbuffer = model.buffers[indexview.buffer];

						for (size_t j = 0; j < indexAccessor.count; ++j)
						{
							uint16_t* ind = (uint16_t*)(&indexbuffer.data[indexview.byteOffset + j * 2]);
							math::vec3 pos = *(math::vec3*)(&buffer.data[view.byteOffset + *ind * byteStride]);
							std::swap(pos.y, pos.z);
							data[j].Position = pos;
						}
					} else
					for (size_t j = 0; j < vertecies; ++j)
					{
						math::vec3 *pos = (math::vec3*)(&buffer.data[view.byteOffset + j * byteStride]);
						data[j].Position = *pos;
					}

					header.positionOffset = 0;
					header.positionStride = sizeof(math::vec3) + sizeof(math::vec3) + sizeof(math::vec2);
				}
				if (attrib.first.compare("NORMAL") == 0)
				{
					is_normals = 1;

					if (data.empty())
						data.resize(vertecies);

					if (indexAccessor.count)
					{
						tinygltf::BufferView& indexview = model.bufferViews[indexAccessor.bufferView];
						tinygltf::Buffer& indexbuffer = model.buffers[indexview.buffer];

						for (size_t j = 0; j < indexAccessor.count; ++j)
						{
							uint16_t* ind = (uint16_t*)(&indexbuffer.data[indexview.byteOffset + j * 2]);
							math::vec3 pos = *(math::vec3*)(&buffer.data[view.byteOffset + *ind * byteStride]);
							std::swap(pos.y, pos.z);
							data[j].Normal = pos;
						}
					}
					else
					for (size_t j = 0; j < vertecies; ++j)
					{
						math::vec3* pos = (math::vec3*)(&buffer.data[view.byteOffset + j * byteStride]);
						data[j].Normal = *pos;
					}

					header.normalOffset = sizeof(math::vec3);
					header.normalStride = sizeof(math::vec3) + sizeof(math::vec3) + sizeof(math::vec2);

				}
				if (attrib.first.compare("TEXCOORD_0") == 0)
				{
					is_uv = 1;

					if (data.empty())
						data.resize(vertecies);

					if (indexAccessor.count)
					{
						tinygltf::BufferView& indexview = model.bufferViews[indexAccessor.bufferView];
						tinygltf::Buffer& indexbuffer = model.buffers[indexview.buffer];

						for (size_t j = 0; j < indexAccessor.count; ++j)
						{
							uint16_t* ind = (uint16_t*)(&indexbuffer.data[indexview.byteOffset + j * 2]);
							math::vec2* pos = (math::vec2*)(&buffer.data[view.byteOffset + *ind * byteStride]);
							data[j].UV = *pos;
						}
					}
					else
					for (size_t j = 0; j < vertecies; ++j)
					{
						math::vec2* pos = (math::vec2*)(&buffer.data[view.byteOffset + j * byteStride]);
						data[j].UV = *pos;
					}

					header.uvOffset = sizeof(math::vec3) + sizeof(math::vec3);
					header.uvStride = sizeof(math::vec3) + sizeof(math::vec3) + sizeof(math::vec2);
				}
			}
			header.attributes = ((vertecies > 0) << 0)
				| (is_normals << 1)
				| (is_uv << 2)
				| (is_tangent << 3)
				| (is_binormal << 4)
				| (is_color << 5);

			header.numberOfVertex = vertecies;

			std::string p = name + "//" + (mesh.name.empty() ? std::to_string(i) + ".mesh" : +".mesh");

			engine::Model* modelPtr = engine::GetSceneManager()->CreateModel(p.c_str());
			modelPtr->SetWorldTransform(m.second);

			engine::File f = engine::GetFS()->OpenFile(p.c_str(), engine::FILE_OPEN_MODE::WRITE | engine::FILE_OPEN_MODE::BINARY);
			f.Write(reinterpret_cast<uint8_t*>(&header), sizeof(engine::MeshHeader));
			f.Write(reinterpret_cast<uint8_t*>(&data[0]), data.size() * sizeof(decltype(data[0])));

			/*
			if (model.textures.size() > 0) {
				// fixme: Use material's baseColor
				tinygltf::Texture& tex = model.textures[0];

				if (tex.source > -1) {

					GLuint texid;
					glGenTextures(1, &texid);

					tinygltf::Image& image = model.images[tex.source];

					glBindTexture(GL_TEXTURE_2D, texid);
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					GLenum format = GL_RGBA;

					if (image.component == 1) {
						format = GL_RED;
					}
					else if (image.component == 2) {
						format = GL_RG;
					}
					else if (image.component == 3) {
						format = GL_RGB;
					}
					else {
						// ???
					}

					GLenum type = GL_UNSIGNED_BYTE;
					if (image.bits == 8) {
						// ok
					}
					else if (image.bits == 16) {
						type = GL_UNSIGNED_SHORT;
					}
					else {
						// ???
					}

					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
						format, type, &image.image.at(0));
				}
			}*/
		}

	}

	std::string sceneName = name + "//" + (scene.name.empty() ? name : scene.name);
	sceneName += ".yaml";

	engine::GetSceneManager()->SaveScene(sceneName.c_str());
	engine::GetSceneManager()->DestroyObjects();

	core->Free();
	engine::DestroyCore(core);

	return 0;
}
