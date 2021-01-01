#include "core.h"
#include "scenemanager.h"
#include "materialmanager.h"

#define VIDEO_API engine::INIT_FLAGS::DIRECTX12_RENDERER

// ----------------------------
// Main
// ----------------------------

int main()
{
	auto core = engine::CreateCore();
	core->Init("", engine::INIT_FLAGS::HIGH_LEVEL_RENDERER | VIDEO_API);

	engine::GetSceneManager()->LoadScene("sponza//sponza.yaml");

	core->Start();
	core->Free();

	engine::DestroyCore(core);

	return 0;
}
