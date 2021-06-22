#pragma once
#include "d3dx12.h"
#include "raytracing_utils.h"
#include "raytracing_d3dx12.h"
#include "icorerender.h"
#include "resourcemanager.h"

namespace engine
{
	using namespace x12;

	/*
	* Hight-level engine renderer.
	* 
	* Now implemented through native D3D12
	* TODO: implement through ICoreRender
	*/
	class Renderer
	{
		ComPtr<ID3D12Device5> dxrDevice;
		ComPtr<ID3D12CommandQueue> rtxQueue;
		ComPtr<ID3D12CommandAllocator> commandAllocators[DeferredBuffers];
		ComPtr<ID3D12GraphicsCommandList4> dxrCommandList;
		ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		ComPtr<ID3D12StateObject> dxrPrimaryStateObject;
		ComPtr<ID3D12StateObject> dxrSecondaryStateObject;
		ComPtr<ID3D12PipelineState> clearRayInfoPSO;
		ComPtr<ID3D12PipelineState> regroupRayInfoPSO;
		ComPtr<ID3D12PipelineState> clearHitCounterPSO;
		ComPtr<ID3D12PipelineState> accumulationPSO;
		ComPtr<ID3D12PipelineState> copyHitCounterPSO;
		ComPtr<ID3D12Resource> outputTexture; // final image accumulation (2d rgba32f texture)

		ComPtr<IDxcBlob> raygen;
		ComPtr<IDxcBlob> hit;
		ComPtr<IDxcBlob> miss;
		ComPtr<IDxcBlob> raygenSecondary;
		ComPtr<IDxcBlob> hitSecondary;
		ComPtr<IDxcBlob> missSecondary;

		ComPtr<ID3D12RootSignature> raytracingGlobalRootSignature;
		ComPtr<ID3D12RootSignature> raytracingLocalRootSignature;
		ComPtr<ID3D12RootSignature> clearRayInfoRootSignature;
		ComPtr<ID3D12RootSignature> regroupRayInfoRootSignature;
		ComPtr<ID3D12RootSignature> clearHitCounterRootSignature;
		ComPtr<ID3D12RootSignature> accumulationRootSignature;
		ComPtr<ID3D12RootSignature> copyrHitCounterRootSignature;

		intrusive_ptr<ICoreBuffer> rayInfoBuffer; // w * h * sizeof(RayInfo)
		intrusive_ptr<ICoreBuffer> regroupedIndexesBuffer; // w * h * sizeof(uint32_t)
		intrusive_ptr<ICoreBuffer> iterationRGBA32f; // w * h * sizeof(float4). primary + secondary + ... + N-th bounce
		intrusive_ptr<ICoreBuffer> hitCointerBuffer; // 4 byte
		intrusive_ptr<ICoreTexture> outputRGBA32fcoreWeakPtr; // ICoreTexture of outputTexture
		intrusive_ptr<IResourceSet> copyResourceSet;
		intrusive_ptr<ICoreVertexBuffer> plane;
		intrusive_ptr<IResourceSet> clearResources;
		intrusive_ptr<ICoreBuffer> cameraBuffer;
		StreamPtr<Shader> clearShader;
		StreamPtr<Shader> copyShader;
		StreamPtr<Mesh> planeMesh;
		StreamPtr<Shader> clearRayInfoBuffer;
		StreamPtr<Texture> blueNoise;

		struct RTXscene
		{
			UINT totalInstances = 0; // If 0, render black screen
			ComPtr<ID3D12Resource> topLevelAccelerationStructure;
			intrusive_ptr<x12::ICoreBuffer> indirectArgBuffer; // sizeof(D3D12_DISPATCH_RAYS_DESC)
			ComPtr<ID3D12CommandSignature> dxrSecondaryCommandIndirect;
			ComPtr<ID3D12Resource> missShaderTable;
			ComPtr<ID3D12Resource> hitGroupShaderTable;
			ComPtr<ID3D12Resource> rayGenShaderTable;
			ComPtr<ID3D12Resource> missShaderTable_secondary;
			ComPtr<ID3D12Resource> hitGroupShaderTable_secondary;
			ComPtr<ID3D12Resource> rayGenShaderTable_secondary;
			std::vector<BLAS> blases;
			intrusive_ptr<x12::ICoreBuffer> sceneBuffer;
			intrusive_ptr<x12::ICoreBuffer> perInstanceBuffer;
			intrusive_ptr<x12::ICoreBuffer> materialsBuffer;
		};
		std::unique_ptr<RTXscene> rtxScene;
		
		Camera* cam;
		HWND hwnd;
		uint32_t width, height;
		uint32_t backBufferIndex = 0;
		uint32_t frame = 0;
		bool needClearBackBuffer = true;
		size_t cameraBufferIndex;
		math::mat4 cameraTransform;
		uint32_t descriptorSize;
		uint32_t descriptorsAllocated;
		HANDLE event;
		D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputResourceUAVGpuDescriptor;
		D3D12_GPU_DESCRIPTOR_HANDLE texturesHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE blueNoiseHandle;

		struct Fence
		{
			ComPtr<ID3D12Fence> fence[DeferredBuffers];
			UINT64 fenceValues[DeferredBuffers];
		};
		Fence rtToSwapchainFence;
		Fence clearToRTFence;
		Fence frameEndFence;

		void InitFence(Fence& fence);
		void SignalFence(ID3D12CommandQueue* ToQueue, Fence& fence);
		void Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, Fence& fence);
		void OnSceneChanged();
		void CreateShaderBindingTable(ComPtr<ID3D12StateObject> state, UINT totalInstances, ComPtr<ID3D12Resource>& r, ComPtr<ID3D12Resource>& h, ComPtr<ID3D12Resource>& m);
		auto CreatePSO(ComPtr<IDxcBlob> r, ComPtr<IDxcBlob> h, ComPtr<IDxcBlob> m) -> ComPtr<ID3D12StateObject>;
		void CreateRaytracingOutputResource(UINT width, UINT height);
		void CreateBuffers(UINT w, UINT h);
		void CreateIndirectBuffer(UINT w, UINT h);
		void BindRaytracingResources();

		static void OnSceneLoaded();

	public:
		void Init();
		void Free();
		void RenderFrame(const ViewportData& viewport, const CameraData& camera);
		void WaitForGpuCurrent();
		void WaitForGpu(UINT index);
		void WaitForGpuAll();
		void Resize(UINT w, UINT h);
		void CompileRTShaders();
	};
}
