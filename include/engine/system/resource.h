#pragma once
#include "common.h"

namespace engine
{
	template<typename T>
	class IResource
	{
	public:
		X12_API virtual ~IResource() = default;
		X12_API virtual void AddRef() = 0;
		X12_API virtual void DecRef() = 0;
		X12_API virtual int GetRefs() const = 0;
		X12_API virtual T* Get() = 0;
		X12_API virtual std::string& GetPath() = 0;
		X12_API virtual void Free() = 0;
		X12_API virtual bool IsLoaded() = 0;
		X12_API virtual void Reload() = 0;
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
		//int64_t timestamp{}; // not used yet

		virtual T* create() = 0;

	public:
		X12_API Resource(const std::string& path);
		X12_API ~Resource() = default;

		X12_API void AddRef() override { refs_++; }
		X12_API void DecRef() override { refs_--; }
		X12_API int GetRefs() const override { return refs_; }
		X12_API std::string& GetPath() override { return path_; }
		X12_API T* Get() override;
		X12_API void Free() override { pointer_ = nullptr; }
		X12_API bool IsLoaded() override { return static_cast<bool>(pointer_); }
		X12_API void Reload();
		X12_API uint64_t Frame() { return frame_; }
		X12_API size_t GetVideoMemoryUsage()
		{
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
				resource_->AddRef();
		}

		void release();

	public:
		StreamPtr() = default;
		StreamPtr(IResource<T>* resource, bool attach = false) : resource_(resource)
		{
			if (!attach)
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
			return resource_ ? resource_->Get() : nullptr;
		}
		bool isLoaded()
		{
			return resource_ ? resource_->IsLoaded() : false;
		}
		const std::string& path()
		{
			static std::string empty;
			if (!resource_)
				return empty;
			return resource_->GetPath();
		}
	};
}

