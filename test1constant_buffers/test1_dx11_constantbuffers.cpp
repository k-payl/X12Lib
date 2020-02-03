#include "common.h"
#include "core.h"
#include "camera.h"
#include "dx11.h"
#include "mainwindow.h"
#include "test1_shared.h"
#include <memory>

using namespace std::chrono;

static ID3D11Device* device;
static ID3D11DeviceContext* context;
static IDXGISwapChain* swapChain;
static ID3D11RenderTargetView *renderTargetView;
static ID3D11Texture2D* swapChainTexture;
static ID3D11DepthStencilView* dsv;
static ID3D11Texture2D* depthTex;

static UINT width, height;
static unsigned queryFrame = 0;
static const unsigned QueryFrames = 4;

static steady_clock::time_point start;


struct Resources
{
	std::unique_ptr<Camera> cam;

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

	Resources()
	{
		cam = std::make_unique<Camera>();
	}

} *res;

void Resize(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* data);
void Init();
void Render();
void CreateBuffers(UINT w, UINT h);


int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	Core* core = new Core();
	core->AddRenderProcedure(Render);

	res = new Resources;
	core->Init(INIT_FLAGS::SHOW_CONSOLE);

	HWND hwnd = *core->GetWindow()->handle();
	initDX11(&hwnd, device, context, swapChain);

	RECT r;
	GetClientRect(hwnd, &r);
	UINT w = r.right - r.left;
	UINT h = r.bottom - r.top;
	CreateBuffers(w, h);

	core->GetWindow()->AddMessageCallback(Resize);
	core->GetWindow()->SetCaption(L"Test DX11");

	Init();

	core->Start();

	delete res;

	device->Release();
	context->Release();
	swapChain->Release();
	renderTargetView->Release();
	dsv->Release();
	depthTex->Release();

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
	context->ClearRenderTargetView(renderTargetView, &bgColor.x);

	D3D11_VIEWPORT vp;
	vp.Width = (float)width;
	vp.Height = (float)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	context->RSSetViewports(1, &vp);

	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

	context->OMSetDepthStencilState(res->depthState.Get(), 0);

	ID3D11RenderTargetView *rtvs[1] = { renderTargetView };
	context->OMSetRenderTargets(1, rtvs, dsv);

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
	res->cam->GetPerspectiveMat(P, static_cast<float>(width) / height);

	mvpcb.MVP = P * V;

	D3D11_MAPPED_SUBRESOURCE mappedResource;

	context->Map(res->MVPcb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &mvpcb, sizeof(MVPcb));
	context->Unmap(res->MVPcb.Get(), 0);

	for (int i = 0; i < numCubesX; i++)
	{
		for (int j = 0; j < numCubesY; j++)
		{
			ColorCB colorCB;
			colorCB.color_out = cubeColor(i, j);
			colorCB.transform = cubePosition(i, j);

			context->Map(res->colorCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
			memcpy(mappedResource.pData, &colorCB, sizeof(ColorCB));
			context->Unmap(res->colorCB.Get(), 0);

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

#ifdef USE_PROFILER_REALTIME
	static float accum;
	if (accum > UPD_INTERVAL)
	{
		accum = 0;
		CORE->LogProfiler("Render GPU (in ms)", frameGPU);
		CORE->LogProfiler("Render CPU (in ms)", frameCPU);
		CORE->LogProfiler("Frame CPU (in ms)", CORE->dt * 1e3f);
		CORE->LogProfiler("FPS", CORE->fps);
	}
	accum += CORE->dt;
#endif

#ifdef USE_PROFILE_TO_CSV
	// stat
	if (CORE->frame > StartFrame && CORE->frame % SkipFrames == 0 && curFrame < Frames)
	{
		data[curFrame].f = CORE->frame;
		data[curFrame].CPU = frameCPU;
		data[curFrame].GPU = frameGPU;
		curFrame++;
	}
	if (curFrame == Frames)
	{
		CORE->Log("Statistic compltetd");
		{
			std::ofstream file("dx11.csv", std::ios::out);
			for (size_t i = 0; i < Frames; ++i)
			{
				file << data[i].f << ", " << data[i].CPU << ", " << data[i].GPU << "\n";
			}
			file.close();

			std::sort(data.begin(), data.end(), [](const Stat& l, const Stat& r) -> int
			{
				return l.CPU < r.CPU;
			});

			char buf[50];
			sprintf_s(buf, "Median CPU: %f", data[Frames / 2].CPU);
			CORE->Log(buf);

			std::sort(data.begin(), data.end(), [](const Stat& l, const Stat& r) -> int
			{
				return l.GPU < r.GPU;
			});

			sprintf_s(buf, "Median GPU: %f", data[Frames / 2].GPU);
			CORE->Log(buf);

		}
		curFrame = Frames + 1;
	}
#endif

	swapChain->Present(0, 0);
}


void Init()
{
	auto text = loadShader("..//mesh.shader");

	CreateVertexShader(device, text, res->vertexShader);
	CreatePixelShader(device, text, res->pixelShader);

	CreateVertexBuffer(device, vertexData, indexData, veretxCount, idxCount, res->vb, res->ib, res->inputLayout, res->stride);
	res->vertex = veretxCount;

	res->MVPcb = CreateConstanBuffer(device, sizeof(MVPcb));
	res->colorCB = CreateConstanBuffer(device, sizeof(ColorCB));

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	device->CreateDepthStencilState(&depthStencilDesc, res->depthState.GetAddressOf());

	for(int i = 0; i < QueryFrames; ++i)
	{
		res->disjontQuery[i] = CreateQuery(device, D3D11_QUERY_TIMESTAMP_DISJOINT);
		res->beginQuery[i] = CreateQuery(device, D3D11_QUERY_TIMESTAMP);
		res->endQuery[i] = CreateQuery(device, D3D11_QUERY_TIMESTAMP);
	}

#ifdef USE_PROFILE_TO_CSV
	data.resize(Frames);
#endif
}

void _Resize();

void CreateBuffers(UINT w, UINT h)
{
	width = w;
	height = h;

	_Resize();
}

void Resize(HWND hwnd, WINDOW_MESSAGE type, uint32_t param1, uint32_t param2, void* pData)
{
	if (type != WINDOW_MESSAGE::SIZE)
		return;

	width = param1;
	height = param2;
	
	_Resize();
}

void _Resize()
{
	if (renderTargetView)
		renderTargetView->Release();

	ThrowIfFailed(swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0));

	ID3D11Texture2D* pBuffer;
	ThrowIfFailed(swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)& pBuffer));

	ThrowIfFailed(device->CreateRenderTargetView(pBuffer, NULL, &renderTargetView));
	pBuffer->Release();

	if (depthTex)
		depthTex->Release();

	// Depth resource
	D3D11_TEXTURE2D_DESC descDepth{};
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_R32_TYPELESS;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	ThrowIfFailed(device->CreateTexture2D(&descDepth, nullptr, &depthTex));

	if (dsv)
		dsv->Release();

	// Depth DSV
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV{};
	descDSV.Format = DXGI_FORMAT_D32_FLOAT;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	ThrowIfFailed(device->CreateDepthStencilView(depthTex, &descDSV, &dsv));
}
