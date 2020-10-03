
#define BRDF_MINIMUM_ALPHA 1e-3f
#define BRDF_DOT_EPSILON 1e-5f
#define g_dielectric_F0 0.04f
#define g_inv_pi (1.0f / 3.1415926f)
#define sqr(x) (x * x)

// TODO: move to material.h
struct Material
{
	float3 m_base_color;
	float m_roughness;
	float m_metalness;
};
float4 srgbInv(float4 v)
{
	return float4(pow(v.x, 2.2), pow(v.y, 2.2), pow(v.z, 2.2), pow(v.w, 2.2));
}
float3 srgb(float3 v)
{
	return float3(pow(v.x, 0.45), pow(v.y, 0.45), pow(v.z, 0.45));
}


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
	return max(BRDF_MINIMUM_ALPHA, sqr(roughness));
}

float3 F0_Specular(Material material)
{
	return lerp(g_dielectric_F0, material.m_base_color, material.m_metalness);
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

	return g_inv_pi * alpha2 / sqr(temp1);
}

// alpha
// The alpha value which is equal to the square of the surface
float D_GGX(float n_dot_h, float alpha)
{
	return D_TrowbridgeReitz(n_dot_h, alpha);
}

struct BRDF
{
	float3 m_diffuse;
	float3 m_specular;
};

BRDF CookTorranceBRDF(float3 n, float3 l, float3 v, Material material)
{
	const float  alpha = RoughnessToAlpha(material.m_roughness);
	const float  n_dot_l = SaturateDot(n, l) + BRDF_DOT_EPSILON;
	const float  n_dot_v = SaturateDot(n, v) + BRDF_DOT_EPSILON;
	const float3 h = HalfDirection(l, v);
	const float  n_dot_h = SaturateDot(n, h) + BRDF_DOT_EPSILON;
	const float  v_dot_h = SaturateDot(v, h) + BRDF_DOT_EPSILON;

	const float  D = D_GGX(n_dot_h, alpha);
	const float  V = V_GGX(n_dot_v, n_dot_l, n_dot_h, v_dot_h, alpha);
	const float3 F_specular = F_Schlick(v_dot_h, F0_Specular(material));
	const float3 F_diffuse = (1.0f - F_specular) * (1.0f - material.m_metalness);

	BRDF brdf;
	brdf.m_diffuse = F_diffuse * material.m_base_color * g_inv_pi;
	brdf.m_specular = F_specular * 0.25f * D * V;

	return brdf;
}
