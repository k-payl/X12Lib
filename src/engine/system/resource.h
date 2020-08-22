#pragma once
#include "common.h"

namespace engine
{
	template<typename T>
	class IResource
	{
	public:
		X12_API virtual ~IResource() = default;
		X12_API virtual void addRef() = 0;
		X12_API virtual void decRef() = 0;
		X12_API virtual int getRefs() const = 0;
		X12_API virtual T* get() = 0;
		X12_API virtual std::string& getPath() = 0;
		X12_API virtual void free() = 0;
		X12_API virtual bool isLoaded() = 0;
	};

	template<typename T>
	class Resource : public IResource<T>
	{
	protected:
		std::string path_;
		std::unique_ptr<T> pointer_;
		int refs_{0};
		bool loadingFailed{false};
		uint64_t frame_{};

		virtual T* create() = 0;

	public:
		X12_API Resource(const std::string& path);
		X12_API ~Resource() = default;

		X12_API void addRef() override { refs_++; }
		X12_API void decRef() override { refs_--; }
		X12_API int getRefs() const override { return refs_; }
		X12_API std::string& getPath() override { return path_; }
		X12_API T* get() override;
		X12_API void free() override { pointer_ = nullptr; }
		X12_API bool isLoaded() override { return static_cast<bool>(pointer_); }
		X12_API uint64_t frame() { return frame_; }
		X12_API size_t getVideoMemoryUsage()
		{
			//if (pointer_)
			//	return pointer_->GetVideoMemoryUsage();
			return 0;
		}
	};

	template<typename T>
	class StreamPtr
	{
		IResource<T>* resource_{nullptr};

		inline void grab()
		{
			if (resource_)
				resource_->addRef();
		}

		void release();

	public:
		StreamPtr() = default;
		StreamPtr(IResource<T>* resource) : resource_(resource)
		{
			grab();
		}
		StreamPtr(StreamPtr& r)
		{
			resource_ = r.resource_;
			grab();
		}
		StreamPtr& operator=(StreamPtr& r)
		{
			release();
			resource_ = r.resource_;
			grab();

			return *this;
		}
		StreamPtr(StreamPtr&& r)
		{
			resource_ = r.resource_;
			r.resource_ = nullptr;
		}
		StreamPtr& operator=(StreamPtr&& r)
		{
			release();
			resource_ = r.resource_;
			r.resource_ = nullptr;

			return *this;
		}
		~StreamPtr()
		{
			release();
		}
		T* get()
		{
			return resource_ ? resource_->get() : nullptr;
		}
		bool isLoaded()
		{
			return resource_ ? resource_->isLoaded() : false;
		}
		const std::string& path()
		{
			static std::string empty;
			if (!resource_)
				return empty;
			return resource_->getPath();
		}
	};
}

