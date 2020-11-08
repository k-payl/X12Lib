#include "../cpp_hlsl_shared.h"

RWTexture2D<float4>								gOutput			: register(u0);

RaytracingAccelerationStructure					gSceneBVH		: register(t0);
StructuredBuffer<engine::Shaders::Light>		gLights			: register(t1);
StructuredBuffer<engine::Shaders::Vertex>		lVertices		: register(t2); // local
Texture2D										gTxMats[]		: register(t3);

SamplerState									gSampler		: register(s0);

ConstantBuffer<engine::Shaders::Camera>			gCamera			: register(b0);
ConstantBuffer<engine::Shaders::Scene>			gScene			: register(b1);
ConstantBuffer<engine::Shaders::InstanceData>	lInstanceData	: register(b2); // local
