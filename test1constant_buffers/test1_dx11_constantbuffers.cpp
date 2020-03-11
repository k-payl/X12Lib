#include "common.h"
#include "core.h"
#include "camera.h"
#include "dx11.h"
#include "mainwindow.h"
#include "filesystem.h"
#include "test1_shared.h"
#include "dx11gpuprofiler.h"
#include <memory>

using namespace std::chrono;

static ID3D11Device* device;
static ID3D11DeviceContext* context;
static IDXGISwapChain* swapChain;

static unsigned queryFrame = 0;
static const unsigned QueryFrames = 4;

static steady_clock::time_point start;

void Resize(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* data);
void Init();
void Render();


struct Resources
{
	std::unique_ptr<Camera> cam = std::make_unique<Camera>();;

	ComPtr<ID3D11PixelShader> pixelShader;
	ComPtr<ID3D11VertexShader> vertexShader;

	UINT stride;
	UINT vertex;
	ComPtr<ID3D11Buffer> vb;
	ComPtr<ID3D11Buffer> ib;
	ComPtr<ID3D11InputLayout> inputLayout;

	ComPtr<ID3D11Buffer> MVPcb;
	ComPtr<ID3D11Buffer> colorCB;

	ComPtr<ID3D11DepthStencilState> depthState;

	ComPtr<ID3D11Query> disjontQuery[QueryFrames];
	ComPtr<ID3D11Query> beginQuery[QueryFrames];
	ComPtr<ID3D11Query> endQuery[QueryFrames];

} *res;

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	Dx11GpuProfiler* gpuprofiler = new Dx11GpuProfiler(vec4(0.6f, 0.6f, 0.6f, 1.0f), 0.0f);
	gpuprofiler->AddRecord("=== D3D11 Render ===");
	gpuprofiler->AddRecord("CPU: % 0.2f ms.");
	gpuprofiler->AddRecord("CPU: % 0.2f ms.");

	Core* core = new Core{};
	core->AddRenderProcedure(Render);

	res = new Resources{};
	core->Init(gpuprofiler, &dx11::InitDX11, INIT_FLAGS::NO_CONSOLE);

	device = dx11::GetDx11Device();
	context = dx11::GetDx11Context();
	swapChain = dx11::GetDx11Swapchain();

	MainWindow* window = core->GetWindow();

	int w, h;
	window->GetClientSize(w, h);
	window->AddMessageCallback(Resize);
	window->SetCaption(L"Test DX11");

	dx11::_Resize(w, h);

	Init();

	core->Start();

	delete res;

	dx11::FreeDX11();

	core->Free();
	delete core;

	return 0;
}

void Render()
{
	context->Begin(res->disjontQuery[queryFrame].Get());
	context->End(res->beginQuery[queryFrame].Get());
	start = high_resolution_clock::now();

	vec4 bgColor(0, 0, 0, 1);
	context->ClearRenderTargetView(dx11::GetDx11RTV(), &bgColor.x);

	D3D11_VIEWPORT vp;
	vp.Width = (float)dx11::GetWidth();
	vp.Height = (float)dx11::GetHeight();
	float aspect = float(vp.Width) / vp.Height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);

	context->ClearDepthStencilView(dx11::GetDx11DSV(), D3D11_CLEAR_DEPTH, 1.0f, 0);

	context->OMSetDepthStencilState(res->depthState.Get(), 0);

	ID3D11RenderTargetView *rtvs[1] = { dx11::GetDx11RTV() };
	context->OMSetRenderTargets(1, rtvs, dx11::GetDx11DSV());

	context->VSSetShader(res->vertexShader.Get(), nullptr, 0);
	context->PSSetShader(res->pixelShader.Get(), nullptr, 0);

	context->IASetInputLayout(res->inputLayout.Get());
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	UINT offset = 0;
	context->IASetVertexBuffers(0, 1, res->vb.GetAddressOf(), &res->stride, &offset);

	context->IASetIndexBuffer(res->ib.Get(), DXGI_FORMAT_R16_UINT, 0);

	context->VSSetConstantBuffers(0, 1, res->MVPcb.GetAddressOf());
	context->VSSetConstantBuffers(3, 1, res->colorCB.GetAddressOf());

	MVPcb mvpcb;
	
	mat4 V;
	res->cam->GetViewMat(V);

	mat4 P;
	res->cam->GetPerspectiveMat(P, aspect);

	mvpcb.MVP = P * V;

	dx11::UpdateUniformBuffer(res->MVPcb.Get(), &mvpcb, sizeof(MVPcb));

	for (int i = 0; i < numCubesX; i++)
	{
		for (int j = 0; j < numCubesY; j++)
		{
			ColorCB colorCB;
			colorCB.color_out = cubeColor(i, j);
			colorCB.transform = cubePosition(i, j);

			dx11::UpdateUniformBuffer(res->colorCB.Get(), &colorCB, sizeof(ColorCB));

			context->DrawIndexed(idxCount, 0, 0);
		}
	}

	context->End(res->endQuery[queryFrame].Get());
	context->End(res->disjontQuery[queryFrame].Get());
	
	queryFrame = (queryFrame + 1) % QueryFrames;

	float frameGPU = 0;
	if (context->GetData(res->disjontQuery[queryFrame].Get(), NULL, 0, 0) != S_FALSE)
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT tsDisjoint;
		context->GetData(res->disjontQuery[queryFrame].Get(), &tsDisjoint, sizeof(tsDisjoint), 0);
		if (!tsDisjoint.Disjoint)
		{
			UINT64 tsBeginPoint, tsEndPoint;

			context->GetData(res->beginQuery[queryFrame].Get(), &tsBeginPoint, sizeof(UINT64), 0);
			context->GetData(res->endQuery[queryFrame].Get(), &tsEndPoint, sizeof(UINT64), 0);

			frameGPU = float(tsEndPoint - tsBeginPoint) / float(tsDisjoint.Frequency) * 1000.0f;
		}
	}

	auto duration = high_resolution_clock::now() - start;
	auto micrs = duration_cast<microseconds>(duration).count();
	float frameCPU = micrs * 1e-3f;

	CORE->RenderProfiler(frameGPU, frameCPU);

	swapChain->Present(0, 0);
}

void Init()
{
	auto text = CORE->GetFS()->LoadFile("mesh.shader");

	dx11::CreateVertexShader(text, res->vertexShader);
	dx11::CreatePixelShader(text, res->pixelShader);

	dx11::CreateVertexBuffer(vertexData, indexData, veretxCount, idxCount, res->vb, res->ib, res->inputLayout, res->stride, false, true);
	res->vertex = veretxCount;

	res->MVPcb = dx11::CreateConstanBuffer(sizeof(MVPcb));
	res->colorCB = dx11::CreateConstanBuffer(sizeof(ColorCB));

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	device->CreateDepthStencilState(&depthStencilDesc, res->depthState.GetAddressOf());

	for(int i = 0; i < QueryFrames; ++i)
	{
		res->disjontQuery[i] = dx11::CreateQuery(D3D11_QUERY_TIMESTAMP_DISJOINT);
		res->beginQuery[i] = dx11::CreateQuery(D3D11_QUERY_TIMESTAMP);
		res->endQuery[i] = dx11::CreateQuery(D3D11_QUERY_TIMESTAMP);
	}
}

void Resize(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	if (type != WINDOW_MESSAGE::SIZE)
		return;
	
	dx11::_Resize(param1, param2);
}

