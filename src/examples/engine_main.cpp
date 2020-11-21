#include "core.h"
#include "scenemanager.h"
#include "materialmanager.h"

#define VIDEO_API engine::INIT_FLAGS::DIRECTX12_RENDERER

// ----------------------------
// Main
// ----------------------------

int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	auto core = engine::CreateCore();
	core->Init("", engine::INIT_FLAGS::HIGH_LEVEL_RENDER | VIDEO_API);

	engine::GetSceneManager()->LoadScene("sponza//sponza.yaml");

	core->Start();
	core->Free();

	engine::DestroyCore(core);

	return 0;
}
