#include "resource.h"

template <class T>
void engine::StreamPtr<T>::release()
{
	if (resource_)
	{
		resource_->decRef();
		if (resource_->getRefs() == 0)
		{
			resource_->free();
		}
		assert(resource_->getRefs() >= 0);
		resource_ = nullptr;
	}
}

template<typename T>
engine::Resource<T>::Resource(const std::string& path) :
	path_(path)
{
	Log("Resource created '%s'", path.c_str());
}

template <class T>
T* engine::Resource<T>::get()
{
	if (pointer_)
	{
		frame_ = _core->frame();
		return pointer_.get();
	}
	if (loadingFailed)
		return nullptr;

	pointer_.reset(create());
	loadingFailed = !pointer_->Load();

	if (loadingFailed)
		pointer_ = nullptr;

	frame_ = _core->frame();
	return pointer_.get();
}

//template class engine::Resource<engine::Mesh>;
//template class engine::Resource<engine::Texture>;
//template class DLLEXPORT engine::StreamPtr<engine::Mesh>;
//template class DLLEXPORT engine::StreamPtr<engine::Texture>;
