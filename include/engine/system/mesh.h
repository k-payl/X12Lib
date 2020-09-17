#pragma once
#include "common.h"
#include "icorerender.h"

namespace engine
{
	class Mesh
	{
		intrusive_ptr<x12::ICoreBuffer> vertexBuffer;
		intrusive_ptr<x12::ICoreBuffer> indexBuffer;
		intrusive_ptr<x12::ICoreVertexBuffer> renderVetexBuffer;

		std::string path_;
		math::vec3 center_;
		engine::MeshDataDesc desc{};
		engine::MeshIndexDesc indexDesc{};

		inline void checksStride();
		bool createStdMesh(const char* path);

	public:
		X12_API Mesh(const std::string& path);
		X12_API ~Mesh();

		X12_API bool Load();
		X12_API bool isSphere();
		X12_API bool isPlane();
		X12_API bool isStd();
		X12_API auto GetPath() -> const char* const { return path_.c_str(); }
		X12_API auto GetCenter()->math::vec3;
		X12_API unsigned GetPositionStride() { return desc.positionStride; }
		X12_API unsigned GetVertexCount() { return desc.numberOfVertex; }
		X12_API x12::ICoreBuffer* VertexBuffer() { return vertexBuffer.get(); }
		X12_API x12::ICoreVertexBuffer* RenderVertexBuffer() { return renderVetexBuffer.get(); }
	};
}
