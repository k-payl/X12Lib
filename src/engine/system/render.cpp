
#include "render.h"
#include "core.h"

using namespace engine;

void engine::Render::Init()
{
}
void engine::Render::Update()
{
}
void engine::Render::RenderFrame(const ViewportData& viewport, const engine::CameraData& camera)
{
	x12::ICoreRenderer* renderer = GetCoreRenderer();
	x12::surface_ptr surface = renderer->GetWindowSurface(*viewport.hwnd);
	x12::ICoreGraphicCommandList* cmdList = renderer->GetGraphicCommandList();

	cmdList->CommandsBegin();
	cmdList->BindSurface(surface);
	cmdList->Clear();
	cmdList->CommandsEnd();
	renderer->ExecuteCommandList(cmdList);
}
void engine::Render::Free()
{
}
