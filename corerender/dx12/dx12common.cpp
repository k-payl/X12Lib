#include "pch.h"
#include "dx12common.h"
#include "dx12render.h"

static std::mutex resourcesMutex;
std::vector<IResourceUnknown*> IResourceUnknown::resources;

IResourceUnknown::IResourceUnknown()
{
	AddRef();
	std::scoped_lock guard(resourcesMutex);
	resources.push_back(this);
}

void IResourceUnknown::ReleaseResource(int& refs, IResourceUnknown* ptr)
{
	assert(refs == 1);

	std::scoped_lock guard(resourcesMutex);

	auto it = std::find_if(resources.begin(), resources.end(), [ptr](const IResourceUnknown* r) -> bool
	{
		return r == ptr;
	});

	assert(it != resources.end());

	resources.erase(it);
	delete ptr;
}

DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: assert(0);
	}
	return DXGI_FORMAT_UNKNOWN;
}

void IResourceUnknown::Release()
{
	--refs;
	assert(refs > 0);

	if (refs == 1)
		ReleaseResource(refs, this);
}

void IResourceUnknown::CheckResources()
{
	for (auto& r : resources)
	{
		if (r->GetRefs() != 1)
			throw std::exception("Resource is not released properly");
	}
}

