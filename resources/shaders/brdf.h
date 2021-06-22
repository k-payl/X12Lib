#pragma once
#include "consts.h"
#include "cpp_hlsl_shared.h"

struct SurfaceHit
{
	float3 albedo;
	float3 roughness;
	float3 metalness;
};

float SaturateDot(float3 a, float3 b)
{
	return saturate(dot(a, b));
}

float3 HalfDirection(float3 d1, float3 d2)
{
	// h := d1+d2 / ||d1+d2||
	return normalize(d1 + d2);
}

float RoughnessToAlpha(float roughness)
{
	return max(BRDF_MINIMUM_ALPHA, roughness /** roughness*/);
}

float3 F0_Specular(float3 albedo, float metalness)
{
	return lerp(g_dielectric_F0, albedo, metalness);
}

/**
 Calculates the Schlick Fresnel component.
 @param[in]		v_dot_h
				The clamped cosine of the difference angle. The difference
				angle is the angle between the view (hit-to-eye) direction and
				half direction and is the angle between the light
				(hit-to-light) direction and half direction.
 @param[in]		F0
				The reflectance at normal incidence.
 @param[in]		F90
				The reflectance at tangent incidence.
 @return		The Schlick Fresnel component.
 */
float F_Schlick(float v_dot_h, float F0, float F90)
{
	// F := F0 + (F90 - F0) (1 - v_dot_h)^5

	const float m = (1.0f - v_dot_h);
	const float m2 = sqr(m);
	const float m5 = sqr(m2) * m;

	return lerp(F0, F90, m5);
}

/**
 Calculates the Schlick Fresnel component.
 @param[in]		v_dot_h
				The clamped cosine of the difference angle. The difference
				angle is the angle between the view (hit-to-eye) direction and
				half direction and is the angle between the light
				(hit-to-light) direction and half direction.
 @param[in]		F0
				The reflectance at normal incidence.
 @param[in]		F90
				The reflectance at tangent incidence.
 @return		The Schlick Fresnel component.
 */
float3 F_Schlick(float v_dot_h, float3 F0, float F90)
{
	// F := F0 + (F90 - F0) (1 - v_dot_h)^5

	const float m = (1.0f - v_dot_h);
	const float m2 = sqr(m);
	const float m5 = sqr(m2) * m;

	return lerp(F0, F90, m5);
}
/**
 Calculates the Schlick Fresnel component.
 @param[in]		v_dot_h
				The clamped cosine of the difference angle. The difference
				angle is the angle between the view (hit-to-eye) direction and
				half direction and is the angle between the light
				(hit-to-light) direction and half direction.
 @param[in]		F0
				The reflectance at normal incidence.
 @return		The Schlick Fresnel component.
 */
float F_Schlick(float v_dot_h, float F0) {
	// F := F0 + (1 - F0) (1 - v_dot_h)^5

	return F_Schlick(v_dot_h, F0, 1.0f);
}
/**
 Calculates the Schlick Fresnel component.
 @param[in]		v_dot_h
				The clamped cosine of the difference angle. The difference
				angle is the angle between the view (hit-to-eye) direction and
				half direction and is the angle between the light
				(hit-to-light) direction and half direction.
 @param[in]		F0
				The reflectance at normal incidence.
 @return		The Schlick Fresnel component.
 */
float3 F_Schlick(float v_dot_h, float3 F0) {
	// F := F0 + (1 - F0) (1 - v_dot_h)^5

	return F_Schlick(v_dot_h, F0, 1.0f);
}

/**
 Calculates the (correlated) GGX Visibility component.
 @param[in]		n_dot_v
				The clamped cosine of the view angle. The view angle is the
				angle between the surface normal and the view (hit-to-eye)
				direction.
 @param[in]		n_dot_l
				The clamped cosine of the light angle. The light angle is the
				angle between the surface normal and the light (hit-to-light)
				direction.
 @param[in]		n_dot_h
				The clamped cosine of the half angle. The half angle is the
				angle between the surface normal and the half direction between
				the view (hit-to-eye) and light (hit-to-light) direction.
 @param[in]		v_dot_h
				The clamped cosine of the difference angle. The difference
				angle is the angle between the view (hit-to-eye) direction and
				half direction and is the angle between the light
				(hit-to-light) direction and half direction.
 @param[in]		alpha
				The alpha value which is equal to the square of the surface
				roughness.
 @return		The (correlated) GGX Visibility component.
 */
float V_GGX(float n_dot_v, float n_dot_l, float n_dot_h, float v_dot_h, float alpha)
{
	//                                                      2
	// V := -------------------------------------------------------------------------------------------------
	//      n_dot_v sqrt(alpha^2 + (1 - alpha^2) n_dot_l^2) + n_dot_l sqrt(alpha^2 + (1 - alpha^2) n_dot_v^2)

	const float alpha2 = sqr(alpha);
	const float lambda_v = sqrt(alpha2 + (1.0f - alpha2) * sqr(n_dot_v));
	const float lambda_l = sqrt(alpha2 + (1.0f - alpha2) * sqr(n_dot_l));

	return 2.0f / (n_dot_v * lambda_l + n_dot_l * lambda_v);
}

/**
 Calculates the Trowbridge-Reitz Normal Distribution Function component.
 @param[in]		n_dot_h
				The clamped cosine of the half angle. The half angle is the
				angle between the surface normal and the half direction between
				the view (hit-to-eye) and light (hit-to-light) direction.
 @param[in]		alpha
				The alpha value which is equal to the square of the surface
				roughness.
 @return		The Trowbridge-Reitz Normal Distribution Function component.
 */
float D_TrowbridgeReitz(float n_dot_h, float alpha) {

	//                  alpha^2                                      c
	// D:= ---------------------------------- = ---------------------------------------------
	//     pi (n_dot_h^2 (alpha^2 - 1) + 1)^2   (alpha^2 * cos(theta_h)^2 + sin(theta_h)^2)^2

	const float alpha2 = sqr(alpha);
	const float n_dot_h2 = sqr(n_dot_h);
	const float temp1 = n_dot_h2 * (alpha2 - 1.0f) + 1.0f;

	return INVPI * alpha2 / sqr(temp1);
}

// alpha
// The alpha value which is equal to the square of the surface
float D_GGX(float n_dot_h, float alpha)
{
	return D_TrowbridgeReitz(n_dot_h, alpha);
}

float D_GTR2(float n_dot_h, float alpha) {
	return D_TrowbridgeReitz(n_dot_h, alpha);
}

float3 ToTangentSpace(float3 N, float3 vec)
{
	float3 UpVector = abs(N.z) < 0.9999 ? float3(0, 0, 1) : float3(1, 0, 0);
	float3 TangentX = normalize(cross(UpVector, N));
	float3 TangentY = cross(N, TangentX);
	return normalize(TangentX * vec.x + TangentY * vec.y + N * vec.z);
}

float3 rayCosine(float3 N, float u1, float u2)
{
	float3 dir;
	float r = sqrt(u1);
	float phi = 2.0 * PI * u2;
	dir.x = r * cos(phi);
	dir.y = r * sin(phi);
	dir.z = sqrt(max(0.0, 1.0 - dir.x * dir.x - dir.y * dir.y));

	return ToTangentSpace(N, dir);
}

#if 0
float3 CookTorranceBRDF(float3 n, float3 l, float3 v, SurfaceHit surface, bool specularBounce)
{
	float3 brdf = surface.albedo * INVPI;
	return brdf;
}

float CookTorranceBRDFPdf(float3 N, float3 L, float3 V, SurfaceHit surface, bool specularBounce)
{
	float pdfDiff = abs(dot(L, N)) * INVPI;
	return pdfDiff;
}

float3 CookTorranceBRDFSample(float3 N, float3 V, SurfaceHit surface, float r1, float r2, out bool specular)
{
	float3 dir;
	dir = rayCosine(N, r1, r2);
	specular = false;

	return dir;
}
#else

float3 CookTorranceBRDF(float3 n, float3 l, float3 v, SurfaceHit surface, uint raytype)
{
	const float  alpha = RoughnessToAlpha(surface.roughness.x);
	const float  n_dot_l = SaturateDot(n, l) + BRDF_DOT_EPSILON;
	const float  n_dot_v = SaturateDot(n, v) + BRDF_DOT_EPSILON;
	const float3 h = HalfDirection(l, v);
	const float  n_dot_h = SaturateDot(n, h) + BRDF_DOT_EPSILON;
	const float  v_dot_h = SaturateDot(v, h) + BRDF_DOT_EPSILON;

	const float  D = D_GGX(n_dot_h, alpha);
	const float  V = V_GGX(n_dot_v, n_dot_l, n_dot_h, v_dot_h, alpha);
	const float3 F_specular = F_Schlick(v_dot_h, F0_Specular(surface.albedo, surface.metalness.x));
	const float3 F_diffuse = /*(1.0f - F_specular) **/ (1.0f - surface.metalness);

	float3 s = F_specular * 0.25f * D * V;
	float3 d = F_diffuse * surface.albedo * INVPI;

	if (raytype == RAY_TYPE_DIFFUSE)
		return d;
	else if (raytype == RAY_TYPE_ROUGH_SPECULAR)
		return s;
	else if (raytype == RAY_TYPE_SPECULAR)
		return F_specular;

	return d + s;
}

float CookTorranceBRDFPdf(float3 N, float3 L, float3 V, SurfaceHit surface, uint raytype)
{
	float specularAlpha = RoughnessToAlpha(surface.roughness.x);

	float diffuseRatio = 0.5 * (1.0 - surface.metalness.x);
	float specularRatio = 1.0 - diffuseRatio;

	float3 halfVec = normalize(L + V);

	float cosTheta = abs(dot(halfVec, N));
	float pdfGTR2 = D_GTR2(cosTheta, specularAlpha) * cosTheta;

	// calculate diffuse and specular pdfs and mix ratio
	float pdfSpec = pdfGTR2 / (4.0 * abs(dot(L, halfVec)));
	float pdfDiff = abs(dot(L, N)) * INVPI;

	float3 s = specularRatio * pdfSpec;
	float3 d = diffuseRatio * pdfDiff;

	if (raytype == RAY_TYPE_DIFFUSE)
		return d;
	else if (raytype == RAY_TYPE_ROUGH_SPECULAR)
		return s;
	else if (raytype == RAY_TYPE_SPECULAR)
		return specularRatio;

	return d + s;
}

float3 CookTorranceBRDFSample(float3 N, float3 V, SurfaceHit surface, float r1, float r2, out uint raytype)
{
	float3 dir;

	float diffuseRatio = 0.5 * (1.0 - surface.metalness.x);

	if (r1 < diffuseRatio) // sample diffuse
	{
		dir = rayCosine(N, r1, r2);
		raytype = RAY_TYPE_DIFFUSE;
	}
	else
	{
		float a = RoughnessToAlpha(surface.roughness.x);
		float phi = r1 * _2PI;

		float cosTheta = sqrt((1.0 - r2) / (1.0 + (a*a - 1.0) *r2));
		float sinTheta = clamp(sqrt(1.0 - (cosTheta * cosTheta)), 0.0, 1.0);
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);

		float3 h = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);

		h = ToTangentSpace(N, h);

		dir = reflect(-V, h);

		if (surface.roughness.x == 0)
			raytype = RAY_TYPE_SPECULAR;
		else
			raytype = RAY_TYPE_ROUGH_SPECULAR;
	}

	return dir;
}
#endif
