#include "mesh.h"
#include "core.h"
#include "console.h"
#include "filesystem.h"
#include "icorerender.h"

static float vertexPlane[40] =
{
	-1.0f, 1.0f, 0.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,		0.0f, 1.0f,
	 1.0f,-1.0f, 0.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,		1.0f, 0.0f,
	 1.0f, 1.0f, 0.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,		1.0f, 1.0f,
	-1.0f,-1.0f, 0.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,		0.0f, 0.0f
};

static float vertexCube[] =
{
	-1.0f, 1.0f, 1.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,			0.0f, 1.0f, // top
	 1.0f,-1.0f, 1.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,			1.0f, 0.0f,
	 1.0f, 1.0f, 1.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,			1.0f, 1.0f,
	-1.0f,-1.0f, 1.0f, 1.0f,	0.0f, 0.0f, 1.0f, 0.0f,			0.0f, 0.0f,

	-1.0f, 1.0f, -1.0f, 1.0f,	0.0f, 0.0f, -1.0f, 0.0f,		0.0f, 1.0f, // bottom
	 1.0f,-1.0f, -1.0f, 1.0f,	0.0f, 0.0f, -1.0f, 0.0f,		1.0f, 0.0f,
	 1.0f, 1.0f, -1.0f, 1.0f,	0.0f, 0.0f, -1.0f, 0.0f,		1.0f, 1.0f,
	-1.0f,-1.0f, -1.0f, 1.0f,	0.0f, 0.0f, -1.0f, 0.0f,		0.0f, 0.0f,

	 -1.0f, -1.0f, 1.0f,1.0f,	-1.0f, 0.0f, 0.0f, 0.0f,		0.0f, 1.0f, // left
	 -1.0f,  1.0f,-1.0f,1.0f,	-1.0f, 0.0f, 0.0f, 0.0f,		1.0f, 0.0f,
	 -1.0f,  1.0f, 1.0f,1.0f,	-1.0f, 0.0f, 0.0f, 0.0f,		1.0f, 1.0f,
	 -1.0f, -1.0f,-1.0f,1.0f,	-1.0f, 0.0f, 0.0f, 0.0f,		0.0f, 0.0f,

	 1.0f, -1.0f, 1.0f,1.0f,	1.0f, 0.0f, 0.0f, 0.0f,			0.0f, 1.0f, // right
	 1.0f,  1.0f,-1.0f,1.0f,	1.0f, 0.0f, 0.0f, 0.0f,			1.0f, 0.0f,
	 1.0f,  1.0f, 1.0f,1.0f,	1.0f, 0.0f, 0.0f, 0.0f,			1.0f, 1.0f,
	 1.0f, -1.0f,-1.0f,1.0f,	1.0f, 0.0f, 0.0f, 0.0f,			0.0f, 0.0f,

	-1.0f, 1.0f,  1.0f,1.0f,	0.0f, 1.0f, 0.0f, 0.0f,			0.0f, 1.0f, // front
	 1.0f, 1.0f, -1.0f,1.0f,	0.0f, 1.0f, 0.0f, 0.0f,			1.0f, 0.0f,
	 1.0f, 1.0f,  1.0f,1.0f,	0.0f, 1.0f, 0.0f, 0.0f,			1.0f, 1.0f,
	-1.0f, 1.0f, -1.0f,1.0f,	0.0f, 1.0f, 0.0f, 0.0f,			0.0f, 0.0f,

	-1.0f, -1.0f,  1.0f,1.0f,	0.0f, -1.0f, 0.0f, 0.0f,		0.0f, 1.0f, // back
	 1.0f, -1.0f, -1.0f,1.0f,	0.0f, -1.0f, 0.0f, 0.0f,		1.0f, 0.0f,
	 1.0f, -1.0f,  1.0f,1.0f,	0.0f, -1.0f, 0.0f, 0.0f,		1.0f, 1.0f,
	-1.0f, -1.0f, -1.0f,1.0f,	0.0f, -1.0f, 0.0f, 0.0f,		0.0f, 0.0f,
};

static unsigned short indexPlane[6]
{
	0 + 0, 0 + 2, 0 + 1,
	0 + 0, 0 + 1, 0 + 3
};

static const char* stdMeshses[] =
{
	"std#plane",
	"std#grid",
	"std#line",
	"std#axes_arrows",
	"std#cube"
};

bool engine::Mesh::isSphere()
{
	return strcmp(path_.c_str(), "standard\meshes\sphere.mesh") == 0;
}

bool engine::Mesh::isPlane()
{
	return strcmp(path_.c_str(), "std#plane") == 0;
}

bool engine::Mesh::isStd()
{
	for (int i = 0; i < _countof(stdMeshses); ++i)
		if (strcmp(path_.c_str(), stdMeshses[i]) == 0)
			return true;
	return false;
}

engine::Mesh::Mesh(const std::string& path) : path_(path)
{
}

engine::Mesh::~Mesh()
{
	//Log("Mesh unloaded: '%s'", path_.c_str());
}

/*ICoreMesh* createStdMesh(const char* path)
{
	ICoreMesh* ret = nullptr;

	if (!strcmp(path, "std#plane"))
	{
		MeshDataDesc desc;
		desc.pData = reinterpret_cast<uint8*>(vertexPlane);
		desc.numberOfVertex = 4;
		desc.positionStride = 40;
		desc.normalsPresented = true;
		desc.normalOffset = 16;
		desc.normalStride = 40;
		desc.texCoordPresented = true;
		desc.texCoordOffset = 32;
		desc.texCoordStride = 40;

		MeshIndexDesc indexDesc;
		indexDesc.pData = reinterpret_cast<uint8*>(indexPlane);
		indexDesc.number = 6;
		indexDesc.format = MESH_INDEX_FORMAT::INT16;

		ret = CORE_RENDER->CreateMesh(&desc, &indexDesc, VERTEX_TOPOLOGY::TRIANGLES);
	}
	else if (!strcmp(path, "std#grid"))
	{
		const float linesInterval = 10.0f;
		const int linesNumber = 21;
		const float startOffset = linesInterval * (linesNumber / 2);
		vec4 vertexGrid[4 * linesNumber];

		for (int i = 0; i < linesNumber; i++)
		{
			vertexGrid[i * 4] = vec4(-startOffset + i * linesInterval, -startOffset, 0.0f, 1.0f);
			vertexGrid[i * 4 + 1] = vec4(-startOffset + i * linesInterval, startOffset, 0.0f, 1.0f);
			vertexGrid[i * 4 + 2] = vec4(startOffset, -startOffset + i * linesInterval, 0.0f, 1.0f);
			vertexGrid[i * 4 + 3] = vec4(-startOffset, -startOffset + i * linesInterval, 0.0f, 1.0f);
		}

		MeshIndexDesc indexEmpty;
		MeshDataDesc descGrid;
		descGrid.pData = reinterpret_cast<uint8*>(vertexGrid);
		descGrid.numberOfVertex = 4 * linesNumber;
		descGrid.positionStride = 16;

		ret = CORE_RENDER->CreateMesh(&descGrid, &indexEmpty, VERTEX_TOPOLOGY::LINES);
	}
	else if (!strcmp(path, "std#line"))
	{
		float vertexAxes[] = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f};
		MeshIndexDesc indexEmpty;
		MeshDataDesc descAxes;
		descAxes.pData = reinterpret_cast<uint8*>(vertexAxes);
		descAxes.numberOfVertex = 2;
		descAxes.positionStride = 16;

		ret = CORE_RENDER->CreateMesh(&descAxes, &indexEmpty, VERTEX_TOPOLOGY::LINES);
	}
	else if (!strcmp(path, "std#axes_arrows"))
	{
		// Layout: position, color, position, color, ...
		const float arrowRadius = 0.052f;
		const float arrowLength = 0.28f;
		const int segments = 12;
		const int numberOfVeretex = 3 * segments;
		const int floats = 4 * numberOfVeretex;

		float vertexAxesArrows[floats];
		{
			for (int j = 0; j < segments; j++)
			{
				constexpr float pi2 = 3.141592654f * 2.0f;
				float alpha = pi2 * (float(j) / segments);
				float dAlpha = pi2 * (1.0f / segments);

				vec4 v1, v2, v3;

				v1.xyzw[0] = 1.0f;
				v1.w = 1.0f;

				v2.xyzw[0] = 1.0f - arrowLength;
				v2.xyzw[1] = cos(alpha) * arrowRadius;
				v2.xyzw[2] = sin(alpha) * arrowRadius;
				v2.w = 1.0f;

				v3.xyzw[0] = 1.0f - arrowLength;
				v3.xyzw[1] = cos(alpha + dAlpha) * arrowRadius;
				v3.xyzw[2] = sin(alpha + dAlpha) * arrowRadius;
				v3.w = 1.0f;

				memcpy(vertexAxesArrows + j * 12 + 0L, &v1.x, 16);
				memcpy(vertexAxesArrows + j * 12 + 4L, &v2.x, 16);
				memcpy(vertexAxesArrows + j * 12 + 8L, &v3.x, 16);
			}
		}
		MeshIndexDesc indexEmpty;
		MeshDataDesc descArrows;
		descArrows.pData = reinterpret_cast<uint8*>(vertexAxesArrows);
		descArrows.numberOfVertex = numberOfVeretex;
		descArrows.positionStride = 16;

		ret = CORE_RENDER->CreateMesh(&descArrows, &indexEmpty, VERTEX_TOPOLOGY::TRIANGLES);
	}
	else if (!strcmp(path, "std#cube"))
	{
		MeshDataDesc desc;
		desc.pData = reinterpret_cast<uint8*>(vertexCube);
		desc.numberOfVertex = sizeof(vertexCube) / (sizeof(float) * 10);
		desc.positionStride = 40;
		desc.normalsPresented = true;
		desc.normalOffset = 16;
		desc.normalStride = 40;
		desc.texCoordPresented = true;
		desc.texCoordOffset = 32;
		desc.texCoordStride = 40;

		unsigned short indexCube_[6 * sizeof(indexPlane) / sizeof(indexPlane[0])];
		for (int i = 0; i < 6; i++)
		{
			for (int j = 0; j < 6; j++)
			{
				indexCube_[i * 6 + j] = indexPlane[j] + 4 * i;
			}
		}

		MeshIndexDesc indexDesc;
		indexDesc.pData = reinterpret_cast<uint8*>(indexCube_);
		indexDesc.number = sizeof(indexCube_) / sizeof(indexCube_[0]);
		indexDesc.format = MESH_INDEX_FORMAT::INT16;

		ret = CORE_RENDER->CreateMesh(&desc, &indexDesc, VERTEX_TOPOLOGY::TRIANGLES);
	}

	return ret;
}
*/

bool engine::Mesh::Load()
{
	//if (isStd())
	//{
	//	coreMeshPtr.reset(createStdMesh(path_.c_str()));
	//	return true;
	//}

	Log("Mesh loading: '%s'", path_.c_str());

	if (!GetFS()->FileExist(path_.c_str()))
	{
		LogCritical("Mesh::Load(): file '%s' not found", path_);
		return false;
	}

	FileMapping mappedFile = GetFS()->CreateMemoryMapedFile(path_.c_str());

	engine::MeshHeader header;
	memcpy(&header, mappedFile.ptr, sizeof(header));

	size_t bPositions = (header.attributes & 1u) > 0;
	size_t bNormals = (header.attributes & 2u) > 0;
	size_t bUv = (header.attributes & 4u) > 0;
	size_t bTangent = (header.attributes & 8u) > 0;
	size_t bBinormal = (header.attributes & 16u) > 0;
	size_t bColor = (header.attributes & 32u) > 0;

	using namespace math;

	desc.numberOfVertex = header.numberOfVertex;
	desc.positionOffset = header.positionOffset;
	desc.positionStride = header.positionStride;
	desc.normalsPresented = bNormals;
	desc.normalOffset = header.normalOffset;
	desc.normalStride = header.normalOffset;
	desc.tangentPresented = bTangent;
	desc.tangentOffset = header.tangentOffset;
	desc.tangentStride = header.tangentStride;
	desc.binormalPresented = bBinormal;
	desc.binormalOffset = header.binormalOffset;
	desc.binormalStride = header.binormalStride;
	desc.texCoordPresented = bUv;
	desc.texCoordOffset = header.uvOffset;
	desc.texCoordStride = header.uvStride;
	desc.colorPresented = bColor;
	desc.colorOffset = header.colorOffset;
	desc.colorStride = header.colorStride;
	desc.pData = reinterpret_cast<uint8_t*>(mappedFile.ptr + sizeof(MeshHeader));

	size_t bytes = 0;
	bytes = std::max(bytes, header.positionOffset + bPositions * header.numberOfVertex * sizeof(vec4));
	bytes = std::max(bytes, header.normalOffset + bNormals * header.numberOfVertex * sizeof(vec4));
	bytes = std::max(bytes, header.uvOffset + bUv * header.numberOfVertex * sizeof(vec2));
	bytes = std::max(bytes, header.tangentOffset + bTangent * header.numberOfVertex * sizeof(vec4));
	bytes = std::max(bytes, header.binormalOffset + bBinormal * header.numberOfVertex * sizeof(vec4));
	bytes = std::max(bytes, header.colorOffset + bColor * header.numberOfVertex * sizeof(vec4));

#pragma pack(push, 1)
	struct Vertex
	{
		vec3 p;
		vec3 n;
	};
#pragma pack(pop)

	std::vector<Vertex> vs(header.numberOfVertex);

	for (int i = 0; i < header.numberOfVertex; ++i)
	{
		memcpy(&vs[i].p, &desc.pData[i * header.positionStride + header.positionOffset], sizeof(vec3));
		memcpy(&vs[i].n, &desc.pData[i * header.normalStride + header.normalOffset], sizeof(vec3));
	}

	engine::MeshIndexDesc indexDesc;

	GetCoreRenderer()->CreateStructuredBuffer(vertexBuffer.getAdressOf(), ConvertFromUtf8ToUtf16(path_).c_str(), sizeof(Vertex), header.numberOfVertex, vs.data(), x12::BUFFER_FLAGS::SHADER_RESOURCE);

	//if (auto coreMesh = CORE_RENDER->CreateMesh(&desc, &indexDesc, VERTEX_TOPOLOGY::TRIANGLES))
	//	coreMeshPtr.reset(coreMesh);
	//else
	//{
		//LogCritical("Mesh::Load(): error occured");
		//return false;
	//}

	center_.x = header.minX * 0.5f + header.maxX * 0.5f;
	center_.y = header.minY * 0.5f + header.maxY * 0.5f;
	center_.z = header.minZ * 0.5f + header.maxZ * 0.5f;

	return true;
}

/*
std::shared_ptr<RaytracingData> engine::Mesh::GetRaytracingData()
{
	if (trianglesDataObjectSpace)
		return trianglesDataObjectSpace;

	if (isStd())
	{
		if (isPlane())
		{
			trianglesDataObjectSpace = shared_ptr<RaytracingData>(new RaytracingData());
			trianglesDataObjectSpace->triangles.resize(2);
			vector<GPURaytracingTriangle>& in = trianglesDataObjectSpace->triangles;

			auto index_to_pos = [](int i)->vec4
			{
				size_t s = indexPlane[i];
				return vec4(vertexPlane[s * 10], vertexPlane[s * 10 + 1], vertexPlane[s * 10 + 2], vertexPlane[s * 10 + 3]);
			};

			for (int i = 0; i < 2; ++i)
			{
				in[i].p0 = index_to_pos(i * 3 + 0);
				in[i].p1 = index_to_pos(i * 3 + 1);
				in[i].p2 = index_to_pos(i * 3 + 2);
				in[i].n = triangle_normal(in[i].p0, in[i].p1, in[i].p2);
				in[i].n.w = .0f;
			}

			return trianglesDataObjectSpace;

		}
		else
			throw new std::exception("not impl");
	}
	if (!FS->FileExist(path_.c_str()))
	{
		LogCritical("Mesh::Load(): file '%s' not found", path_);
		trianglesDataObjectSpace = nullptr;
		return trianglesDataObjectSpace;
	}

	FileMapping mappedFile = FS->CreateMemoryMapedFile(path_.c_str());

	MeshHeader header;
	memcpy(&header, mappedFile.ptr, sizeof(header));

	int bPositions = (header.attributes & 1u) > 0;
	int bNormals = (header.attributes & 2u) > 0;
	int bUv = (header.attributes & 4u) > 0;
	int bTangent = (header.attributes & 8u) > 0;
	int bBinormal = (header.attributes & 16u) > 0;
	int bColor = (header.attributes & 32u) > 0;

	size_t bytes = 0;
	bytes += (size_t)bPositions * header.numberOfVertex * sizeof(vec4);
	bytes += (size_t)bNormals * header.numberOfVertex * sizeof(vec4);
	bytes += (size_t)bUv * header.numberOfVertex * sizeof(vec2);
	bytes += (size_t)bTangent * header.numberOfVertex * sizeof(vec4);
	bytes += (size_t)bBinormal * header.numberOfVertex * sizeof(vec4);
	bytes += (size_t)bColor * header.numberOfVertex * sizeof(vec4);

	MeshDataDesc desc;
	desc.numberOfVertex = header.numberOfVertex;
	desc.positionOffset = header.positionOffset;
	uint8_t* data = reinterpret_cast<uint8_t*>(mappedFile.ptr + sizeof(MeshHeader) + desc.positionOffset);

	assert(desc.numberOfVertex % 3 == 0);
	uint32_t stride = header.positionStride;
	triangles = desc.numberOfVertex / 3;

	trianglesDataObjectSpace = shared_ptr<RaytracingData>(new RaytracingData());
	trianglesDataObjectSpace->triangles.resize(triangles);
	vector<GPURaytracingTriangle>& in = trianglesDataObjectSpace->triangles;

	for (int i = 0; i < triangles; ++i)
	{
		in[i].p0 = (vec4)*data; data += stride;
		in[i].p1 = (vec4)*data; data += stride;
		in[i].p2 = (vec4)*data; data += stride;
		in[i].n = triangle_normal(in[i].p0, in[i].p1, in[i].p2);
	}

	return trianglesDataObjectSpace;
}
*/

//auto X12_API engine::Mesh::GetAttributes() -> INPUT_ATTRUBUTE
//{
//	return coreMeshPtr->GetAttributes();
//}
//
//auto X12_API engine::Mesh::GetVideoMemoryUsage() -> size_t
//{
//	if (!coreMeshPtr)
//		return 0;
//
//	return coreMeshPtr->GetVideoMemoryUsage();
//}
//
auto X12_API engine::Mesh::GetCenter() -> math::vec3
{
	return center_;
}
