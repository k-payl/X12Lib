#include "../cpp_hlsl_shared.h"

// Output texture
RWTexture2D<float4> gOutput : register(u0);

// TLAS
RaytracingAccelerationStructure gSceneBVH : register(t0);

ConstantBuffer<engine::Shaders::Camera>			gCamera	: register(b0);
ConstantBuffer<engine::Shaders::Scene>			gScene	: register(b2);
StructuredBuffer<engine::Shaders::Light>		gLights	: register(t1);
