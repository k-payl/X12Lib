#pragma once
#include "gpuprofiler.h"
#include <d3d11_3.h>

class Dx11GpuProfiler :	public GpuProfiler
{
	ID3D11Device* device;
	ID3D11DeviceContext* context;

	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11VertexShader> vertexShader;

	ComPtr<ID3D11PixelShader> graphPixelShader;
	ComPtr<ID3D11VertexShader> graphVertexShader;

	ComPtr<ID3D11Buffer> viewportUniformBuffer;
	ComPtr<ID3D11Buffer> transformUniformBuffer;
	ComPtr<ID3D11Buffer> colorUniformBuffer;

	ComPtr<ID3D11ShaderResourceView> fontSRV;
	ComPtr<ID3D11Buffer> fontBuffer;

	ComPtr<ID3D11Resource> d3dtexture;
	ComPtr<ID3D11ShaderResourceView> textureView;

	ComPtr<ID3D11BlendState> blendState;
	ComPtr<ID3D11BlendState> blendStateDefault;

	void Begin() override;
	void BeginGraph() override;
	void UpdateViewportConstantBuffer() override;
	void DrawRecords(size_t maxRecords) override;
	void* getContext() override { return context; }

public:
	Dx11GpuProfiler(vec4 color_, float verticalOffset_)
		: GpuProfiler(color_, verticalOffset_) {}
	void Init() override;
	void Free() override;
	void AddRecord(const char* format, bool isFloat, bool renderGraph) override;
};

