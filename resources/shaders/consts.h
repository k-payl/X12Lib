#pragma once

#define BRDF_MINIMUM_ALPHA .0001f
#define BRDF_DOT_EPSILON 1e-5f
#define g_dielectric_F0 0.04f
#define INVPI (1.0f / 3.1415926f)
#define PI (3.1415926f)
#define sqr(x) (x * x)
#define _2PI (2.0f * PI)

#define RAY_TYPE_NONE 0
#define RAY_TYPE_ROUGH_SPECULAR 1
#define RAY_TYPE_DIFFUSE 2
#define RAY_TYPE_SPECULAR 3
