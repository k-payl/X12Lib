#include "pch.h"
#include "Dx11GpuProfiler.h"
#include "core.h"
#include "filesystem.h"
#include "dx11.h"
#include "3rdparty/DirectXTex/DDSTextureLoader.h"

struct Dx11Graph : public Graph
{
	ComPtr<ID3D11Buffer> offsetUniformBuffer;
	ComPtr<ID3D11Buffer> colorUniformBuffer;
	ComPtr<ID3D11Buffer> vertexBuffer;
	UINT vertexCount;
	ComPtr<ID3D11InputLayout> inputLayout;
	UINT stride;
	std::vector<vec4> data;

	Dx11Graph()
	{
		offsetUniformBuffer = dx11::CreateConstanBuffer(16);
		colorUniformBuffer = dx11::CreateConstanBuffer(16);
	}

	void Render(void* c, vec4 color, float value, unsigned w, unsigned h) override
	{
		ID3D11DeviceContext* context = (ID3D11DeviceContext*)c;

		context->PSSetConstantBuffers(0, 1, colorUniformBuffer.GetAddressOf());
		context->VSSetConstantBuffers(1, 1, offsetUniformBuffer.GetAddressOf());

		context->IASetInputLayout(inputLayout.Get());

		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

		if (!lastColor.Aproximately(color))
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			context->Map(colorUniformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &color, 16);
			context->Unmap(colorUniformBuffer.Get(), 0);

			lastColor = color;
		}

		//vec4 cpuv[2];
		//cpuv[0] = lastGraphValue;
		//cpuv[1] = vec4((float)graphRingBufferOffset, h - value, 0, 0);

		data[graphRingBufferOffset * 2] = lastGraphValue;
		data[graphRingBufferOffset * 2 + 1] = vec4((float)graphRingBufferOffset, h - value, 0, 0);

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		context->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, data.data(), sizeof(vec4) * 2 * w);
		context->Unmap(vertexBuffer.Get(), 0);

		//graphVertexBuffer->SetData(&cpuv, sizeof(cpuv), graphRingBufferOffset * sizeof(cpuv), nullptr, 0, 0);

		auto Draw = [this, context](float offset, uint32_t len, uint32_t o)
		{
			D3D11_MAPPED_SUBRESOURCE mappedResource;
			context->Map(offsetUniformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &offset, 4);
			context->Unmap(offsetUniformBuffer.Get(), 0);

			context->Draw(len, o);
		};

		if (graphRingBufferOffset)
			Draw(float(graphRingBufferOffset), graphRingBufferOffset * 2, 0);

		Draw(float(graphRingBufferOffset + w), (w - graphRingBufferOffset) * 2, graphRingBufferOffset * 2);
	}

	void RecreateVB(unsigned w) override
	{
		lastGraphValue = {};
		lastGraphValue.y = (float)w;
		graphRingBufferOffset = 0;

		vertexCount = w * 2;

		for (uint32_t i = 0; i < vertexCount; i++)
			graphData[i] = vec4(float(i / 2), 100, 0, 0);

		data.resize(vertexCount);
		memcpy(data.data(), graphData, sizeof(vec4) * vertexCount);

		if (vertexBuffer)
			vertexBuffer = nullptr;

		ComPtr<ID3D11Buffer> ib;
		dx11::CreateVertexBuffer(nullptr, nullptr, w * 2, 0, vertexBuffer, ib, inputLayout, stride, true);
	}
};

struct Dx11RenderProfilerRecord : public RenderProfilerRecord
{
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11InputLayout> inputLayout;
	UINT size;
	UINT stride;

	void CreateBuffer() override
	{
		if (vertexBuffer)
			return;

		size = 6 * (uint32_t)text.size();

		ComPtr<ID3D11Buffer> ib;
		
		dx11::CreateVertexBuffer(nullptr, nullptr, size, 0, vertexBuffer, ib, inputLayout, stride, true);
	}

	void UpdateBuffer(void* data) override
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource{};
		ThrowIfFailed(dx11::GetDx11Context()->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
		memcpy(mappedResource.pData, data, size * 16);
		dx11::GetDx11Context()->Unmap(vertexBuffer.Get(), 0);
	}
};

void Dx11GpuProfiler::Init()
{
	FileSystem* fs = CORE->GetFS();
	context = dx11::GetDx11Context();
	device = dx11::GetDx11Device();

	// Font shader
	{
		auto text = fs->LoadFile("gpuprofiler_font.shader");
		dx11::CreateVertexShader(text, vertexShader);
		dx11::CreatePixelShader(text, pixelShader);
	}

	// Graph
	graphs.resize(GraphsCount);
	for (int i = 0; i < GraphsCount; ++i)
		graphs[i] = new Dx11Graph;

	for (int i = 0; i < recordsNum; ++i)
		records[i] = new Dx11RenderProfilerRecord;

	{
		auto text = fs->LoadFile("gpuprofiler_graph.shader");
		dx11::CreateVertexShader(text, graphVertexShader);
		dx11::CreatePixelShader(text, graphPixelShader);
	}

	viewportUniformBuffer = dx11::CreateConstanBuffer(16);
	transformUniformBuffer = dx11::CreateConstanBuffer(sizeof(TransformConstantBuffer));

	DirectX::CreateDDSTextureFromFile(
		dx11::GetDx11Device(), dx11::GetDx11Context(),
		fontTexturePath,
		&d3dtexture,
		&textureView);

	// Font
	loadFont();
	dx11::CreateStructuredBuffer(fontSRV, fontBuffer, sizeof(FontChar), fontData.size(), &fontData[0]);
}

void Dx11GpuProfiler::Begin()
{
	w = dx11::GetWidth();
	h = dx11::GetHeight();
}
void Dx11GpuProfiler::BeginGraph()
{
	context->VSSetShader(graphVertexShader.Get(), nullptr, 0);
	context->PSSetShader(graphPixelShader.Get(), nullptr, 0);

	if (!blendStateDefault)
	{
		D3D11_BLEND_DESC blend_desc{};

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = 0;
		blend_desc.RenderTarget[0].BlendEnable = FALSE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		device->CreateBlendState(&blend_desc, &blendStateDefault);
	}
	const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(blendStateDefault.Get(), zero, ~0u);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	context->VSSetConstantBuffers(0, 1, viewportUniformBuffer.GetAddressOf());
}
void Dx11GpuProfiler::UpdateViewportConstantBuffer()
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	context->Map(viewportUniformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &viewport, 16);
	context->Unmap(viewportUniformBuffer.Get(), 0);
}
void Dx11GpuProfiler::DrawFont(int maxRecords)
{
	//context->ClearState();

	context->VSSetShader(vertexShader.Get(), nullptr, 0);
	context->PSSetShader(pixelShader.Get(), nullptr, 0);

	if (!blendState)
	{
		D3D11_BLEND_DESC blend_desc{};

		blend_desc.AlphaToCoverageEnable = FALSE;
		blend_desc.IndependentBlendEnable = 0;
		blend_desc.RenderTarget[0].BlendEnable = TRUE;
		blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND::D3D11_BLEND_SRC_ALPHA;
		blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND::D3D11_BLEND_INV_SRC_ALPHA;
		blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		device->CreateBlendState(&blend_desc, &blendState);
	}
	const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	context->OMSetBlendState(blendState.Get(), zero, ~0u);

	context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	context->VSSetConstantBuffers(0, 1, viewportUniformBuffer.GetAddressOf());
	context->VSSetConstantBuffers(1, 1, transformUniformBuffer.GetAddressOf());

	context->PSSetShaderResources(1, 1, textureView.GetAddressOf());
	context->VSSetShaderResources(0, 1, fontSRV.GetAddressOf());

	float t = 0;
	for (int i = 0; i < maxRecords; ++i)
	{
		auto* r = static_cast<Dx11RenderProfilerRecord*>(records[i]);

		if (i == 0)
			context->IASetInputLayout(r->inputLayout.Get());

		UINT offset = 0;
		context->IASetVertexBuffers(0, 1, r->vertexBuffer.GetAddressOf(), &r->stride, &offset);
		t += fntLineHeight;

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		context->Map(transformUniformBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, &t, 4);
		context->Unmap(transformUniformBuffer.Get(), 0);

		context->Draw(r->size, 0);
	}

}
void Dx11GpuProfiler::Free()
{
}
