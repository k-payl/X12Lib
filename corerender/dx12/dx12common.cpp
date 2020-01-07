#include "pch.h"
#include "dx12common.h"

DXGI_FORMAT engineToDXGIFormat(VERTEX_BUFFER_FORMAT format)
{
	switch (format)
	{
		case VERTEX_BUFFER_FORMAT::FLOAT4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		default: assert(0);
	}
	return DXGI_FORMAT_UNKNOWN;
}

