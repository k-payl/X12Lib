#pragma once
#include "common.h"

namespace engine
{
	class MaterialManager
	{	
	public:
		void Free();
		void Init();

	public:
		auto X12_API LoadMaterial(const char *path) -> Material*;
		auto X12_API FindMaterial(const char* name)->Material*;
	};
}
