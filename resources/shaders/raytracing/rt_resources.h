#pragma once
#include "../cpp_hlsl_shared.h"

RWStructuredBuffer<float4>						gOutput			: register(u0);
RWStructuredBuffer<engine::Shaders::RayInfo>	gRayInfo		: register(u1);

RaytracingAccelerationStructure					gSceneBVH		: register(t0);
StructuredBuffer<engine::Shaders::Light>		gLights			: register(t1);
StructuredBuffer<engine::Shaders::Vertex>		lVertices		: register(t2); // local
StructuredBuffer<engine::Shaders::InstanceData>	gInstances		: register(t3);
StructuredBuffer<engine::Shaders::Material>		gMaterials		: register(t4);
StructuredBuffer<uint>							gRegroupedIndexes : register(t5);
Texture2D										gTxMats[]		: register(t6);

SamplerState									gSampler		: register(s0);

ConstantBuffer<engine::Shaders::Camera>			gCamera			: register(b0);
ConstantBuffer<engine::Shaders::Scene>			gScene			: register(b1);
ConstantBuffer<engine::Shaders::InstancePointer>lInstanceData	: register(b2); // local
ConstantBuffer<engine::Shaders::Frame>			gFrame			: register(b3);
