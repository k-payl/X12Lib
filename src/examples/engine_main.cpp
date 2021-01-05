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

#if 0
	engine::GetSceneManager()->LoadScene("sponza/sponza.yaml");
#else
	engine::GetSceneManager()->LoadScene("scene.yaml");
	//engine::GetSceneManager()->LoadScene("gto67/Scene.yaml");
	//engine::GetSceneManager()->LoadScene("Grey & White Room 03 BS/Scene.yaml");
#endif

	//engine::Model *model = engine::GetSceneManager()->CreateModel("std#plane");

	//model->SetWorldRotation(math::vec3(0, 90, 0));
	//model->SetLocalPosition(math::vec3(10, 0, 0));
	//model->SetLocalScale(math::vec3(10, 10, 10));

	//engine::GetSceneManager()->SaveScene("scene.yaml");

	core->Start();
	core->Free();

	engine::DestroyCore(core);

	return 0;
}
