#pragma once
#include "d3dx12.h"
#include "raytracing_utils.h"
#include "raytracing_d3dx12.h"
#include "core.h"
#include "camera.h"
#include "mainwindow.h"
#include "icorerender.h"
#include "model.h"
#include "light.h"
#include "material.h"
#include "gameobject.h"
#include "scenemanager.h"
#include "resourcemanager.h"
#include "materialmanager.h"
#include "mesh.h"
#include "d3d12/dx12buffer.h"
#include "d3d12/dx12texture.h"
#include "cpp_hlsl_shared.h"

namespace engine
{
	using namespace x12;

	class Render
	{
	public:
		struct Fence
		{
			ComPtr<ID3D12Fence> fence[engine::DeferredBuffers];
			UINT64 fenceValues[engine::DeferredBuffers];
		};

		ComPtr<ID3D12CommandQueue>          commandQueue;
		ComPtr<ID3D12GraphicsCommandList>   commandList;
		ComPtr<ID3D12CommandAllocator>      commandAllocators[engine::DeferredBuffers];

		// DXR
		ComPtr<ID3D12Device5> dxrDevice;
		ComPtr<ID3D12GraphicsCommandList4> dxrCommandList;

		ComPtr<IDxcBlob> raygen;
		ComPtr<IDxcBlob> hit;
		ComPtr<IDxcBlob> miss;

		ComPtr<IDxcBlob> raygen_secondary;
		ComPtr<IDxcBlob> hit_secondary;
		ComPtr<IDxcBlob> miss_secondary;

		ComPtr<ID3D12RootSignature> raytracingGlobalRootSignature;
		ComPtr<ID3D12RootSignature> raytracingLocalRootSignature;
		ComPtr<ID3D12RootSignature> clearRayInfoRootSignature;
		ComPtr<ID3D12RootSignature> regroupRayInfoRootSignature;
		ComPtr<ID3D12RootSignature> clearHitCounterRootSignature;
		ComPtr<ID3D12RootSignature> accumulationRootSignature;
		ComPtr<ID3D12RootSignature> copyrHitCounterRootSignature;

		ComPtr<ID3D12PipelineState> clearRayInfoPSO;
		ComPtr<ID3D12PipelineState> regroupRayInfoPSO;
		ComPtr<ID3D12PipelineState> clearHitCounterPSO;
		ComPtr<ID3D12PipelineState> accumulationPSO;
		ComPtr<ID3D12PipelineState> copyHitCounterPSO;

		ComPtr<ID3D12DescriptorHeap> descriptorHeap;

		intrusive_ptr<x12::ICoreBuffer> rayInfoBuffer; // w * h * sizeof(RayInfo)
		intrusive_ptr<x12::ICoreBuffer> regroupedIndexesBuffer; // w * h * sizeof(uint32_t)
		intrusive_ptr<x12::ICoreBuffer> iterationRGBA32f; // w * h * sizeof(float4). primary + secondary + ... + N-th bounce
		intrusive_ptr<x12::ICoreBuffer> hitCointerBuffer; // 4 byte

		engine::StreamPtr<engine::Mesh> planeMesh;
		ComPtr<ID3D12Resource> outputRGBA32f; // final image accumulation (Texture)
		intrusive_ptr<ICoreTexture> outputRGBA32fcoreWeakPtr;

		engine::StreamPtr<engine::Shader> copyShader;
		intrusive_ptr<IResourceSet> copyResourceSet;
		intrusive_ptr<ICoreVertexBuffer> plane;

		engine::StreamPtr<engine::Shader> clearShader;
		intrusive_ptr<IResourceSet> clearResources;

		engine::StreamPtr<engine::Shader> clearRayInfoBuffer;

		Fence rtToSwapchainFence;
		Fence clearToRTFence;
		Fence frameEndFence;

		intrusive_ptr<x12::ICoreBuffer> cameraBuffer;

		ComPtr<ID3D12StateObject> dxrPrimaryStateObject;
		ComPtr<ID3D12StateObject> dxrSecondaryStateObject;

		struct RTXscene
		{
			// Scene
			ComPtr<ID3D12Resource> topLevelAccelerationStructure;

			intrusive_ptr<x12::ICoreBuffer> indirectBuffer; // sizeof(D3D12_DISPATCH_RAYS_DESC)

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

	private:
		UINT totalInstances = 0; // If 0, render black screen
		engine::Camera* cam;
		std::unique_ptr<RTXscene> rtx;
		HWND hwnd;
		UINT width, height;
		bool needClearBackBuffer = true;
		uint32_t frame;
		size_t CameraBuffer;
		math::mat4 cameraTransform;
		ID3D12Device* device;
		ID3D12Device5* m_dxrDevice;
		UINT descriptorSize;
		UINT descriptorsAllocated;
		ID3D12CommandQueue* rtxQueue;
		UINT backBufferIndex;
		HANDLE event;
		D3D12_GPU_DESCRIPTOR_HANDLE raytracingOutputResourceUAVGpuDescriptor;
		D3D12_GPU_DESCRIPTOR_HANDLE texturesHandle;

		void InitFence(Fence& fence);
		void SignalFence(ID3D12CommandQueue* ToQueue, Fence& fence);
		void Sync(ID3D12CommandQueue* fromQueue, ID3D12CommandQueue* ToQueue, Fence& fence);
		void OnLoadScene();
		void CreateShaderBindingTable(ComPtr<ID3D12StateObject> state, UINT totalInstances, ComPtr<ID3D12Resource>& r, ComPtr<ID3D12Resource>& h, ComPtr<ID3D12Resource>& m);
		auto CreatePSO(ComPtr<IDxcBlob> r, ComPtr<IDxcBlob> h, ComPtr<IDxcBlob> m) -> ComPtr<ID3D12StateObject>;
		void CreateRaytracingOutputResource(UINT width, UINT height);
		void BindRaytracingResources();

		static void OnObjAdded();

	public:
		void Init();
		void Free();
		void Update();
		void RenderFrame(const ViewportData& viewport, const engine::CameraData& camera);

		void WaitForGpu();
		void WaitForGpuAll();
	};
}
