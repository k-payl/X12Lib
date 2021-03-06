#include "resource.h"
#include "mesh.h"
#include "texture.h"
#include "shader.h"
#include "core.h"

template <class T>
void engine::StreamPtr<T>::release()
{
	if (resource_)
	{
		resource_->DecRef();
		if (resource_->GetRefs() == 0)
		{
			resource_->Free(); // destroy resource here if refs==0
		}
		assert(resource_->GetRefs() >= 0);
		resource_ = nullptr;
	}
}

template<typename T>
engine::Resource<T>::Resource(const std::string& path) :
	path_(path)
{
	engine::Log("Resource created '%s'", path.c_str());
}

template <class T>
T* engine::Resource<T>::Get()
{
	if (pointer_)
	{
		frame_ = core__->frame;
		return pointer_.get();
	}
	if (loadingFailed)
		return nullptr;

	pointer_.reset(create());
	loadingFailed = !pointer_->Load();

	if (loadingFailed)
		pointer_ = nullptr;

	frame_ = core__->frame;
	return pointer_.get();
}

template <class T>
void engine::Resource<T>::Reload()
{
	if (!pointer_)
		return;

	Free();

	Get(); // force load 
}



template class engine::Resource<engine::Mesh>;
template class engine::Resource<engine::Texture>;
template class engine::Resource<engine::Shader>;
template class X12_API engine::StreamPtr<engine::Mesh>;
template class X12_API engine::StreamPtr<engine::Texture>;
template class X12_API engine::StreamPtr<engine::Shader>;
