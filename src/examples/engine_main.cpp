#include "core.h"
#include "scenemanager.h"
#include "materialmanager.h"
#include "scenemanager.h"
#include "model.h"

#define VIDEO_API engine::INIT_FLAGS::DIRECTX12_RENDERER

// ----------------------------
// Main
// ----------------------------

int main()
{
	auto core = engine::CreateCore();
	core->Init("", engine::INIT_FLAGS::HIGH_LEVEL_RENDERER | VIDEO_API);

#if 1
	engine::GetSceneManager()->LoadScene("sponza/sponza.yaml");
#else
	engine::GetSceneManager()->LoadScene("scene.yaml");
#endif

	core->Start();
	core->Free();

	engine::DestroyCore(core);

	return 0;
}
