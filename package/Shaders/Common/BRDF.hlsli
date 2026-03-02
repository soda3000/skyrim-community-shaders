#ifndef __BRDF_DEPENDENCY_HLSL__
#define __BRDF_DEPENDENCY_HLSL__

#include "Common/Math.hlsli"

/**
 * @namespace BRDF
 * @brief Bidirectional Reflectance Distribution Function utilities
 *
 * BRDF (Bidirectional Reflectance Distribution Function) describes how light reflects
 * off a surface. It defines the ratio of reflected radiance to incoming irradiance for
 * a given pair of incoming and outgoing directions.
 *
 * This namespace contains fundamental physically-based rendering (PBR) utilities:
 * - Diffuse BRDFs: Lambert, Burley, Oren-Nayar, Gotanda, Chan
 * - Specular components: Fresnel (F), Distribution (D), Visibility (Vis)
 * - Microfacet models: GGX, Beckmann, Charlie (for sheen/microflakes)
 * - Helper functions: IOR conversions, environment BRDF approximations
 *
 * Naming conventions:
 *   N   = Normal of the macro surface
 *   H   = Normal of the micro surface (halfway vector between L and V)
 *   L   = Light direction from surface point to light
 *   V   = View direction from surface point to camera
 *   D   = Distribution (Microfacet NDF - Normal Distribution Function)
 *   F   = Fresnel (reflectance based on viewing angle)
 *   Vis = Visibility (geometric self-shadowing and masking)
 *
 * Specular BRDF formula: D * Vis * F
 */
namespace BRDF
{

	// Diffuse BRDFs
	float Diffuse_Lambert()
	{
		return 1.0 / Math::PI;
	}

	// [Burley 2012, "Physically-Based Shading at Disney"]
	float3 Diffuse_Burley(float roughness, float NdotV, float NdotL, float VdotH)
	{
		float FD90 = 0.5 + 2.0 * VdotH * VdotH * roughness;
		float FdV = 1.0 + (FD90 - 1.0) * pow(1.0 - NdotV, 5.0);
		float FdL = 1.0 + (FD90 - 1.0) * pow(1.0 - NdotL, 5.0);
		return (1.0 / Math::PI) * (FdV * FdL);
	}

	// [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"]
	float3 Diffuse_OrenNayar(float roughness, float3 N, float3 V, float3 L, float NdotV, float NdotL)
	{
		float a = roughness * roughness * 0.25;
		float A = 1.0 - 0.5 * (a / (a + 0.33));
		float B = 0.45 * (a / (a + 0.09));

		float gamma = dot(V - N * NdotV, L - N * NdotL) / (sqrt(saturate(1.0 - NdotV * NdotV)) * sqrt(saturate(1.0 - NdotL * NdotL)));

		float2 cos_alpha_beta = NdotV < NdotL ? float2(NdotV, NdotL) : float2(NdotL, NdotV);
		float2 sin_alpha_beta = sqrt(saturate(1.0 - cos_alpha_beta * cos_alpha_beta));
		float C = sin_alpha_beta.x * sin_alpha_beta.y / (EPSILON_DIVISION + cos_alpha_beta.y);

		return (1 / Math::PI) * (A + B * max(0.0, gamma) * C);
	}

	// [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
	float3 Diffuse_Gotanda(float roughness, float NdotV, float NdotL, float VdotL)
	{
		float a = roughness * roughness;
		float a2 = a * a;
		float F0 = 0.04;
		float Cosri = VdotL - NdotV * NdotL;
		float Fr = (1 - (0.542026 * a2 + 0.303573 * a) / (a2 + 1.36053)) * (1 - pow(1 - NdotV, 5 - 4 * a2) / (a2 + 1.36053)) * ((-0.733996 * a2 * a + 1.50912 * a2 - 1.16402 * a) * pow(1 - NdotV, 1 + rcp(39 * a2 * a2 + 1)) + 1);
		float Lm = (max(1 - 2 * a, 0) * (1 - pow(1 - NdotL, 5)) + min(2 * a, 1)) * (1 - 0.5 * a * (NdotL - 1)) * NdotL;
		float Vd = (a2 / ((a2 + 0.09) * (1.31072 + 0.995584 * NdotV))) * (1 - pow(1 - NdotL, (1 - 0.3726732 * NdotV * NdotV) / (0.188566 + 0.38841 * NdotV)));
		float Bp = Cosri < 0 ? 1.4 * NdotV * NdotL * Cosri : Cosri;
		float Lr = (21.0 / 20.0) * (1 - F0) * (Fr * Lm + Vd + Bp);
		return (1 / Math::PI) * Lr;
	}

	// [ Chan 2018, "Material Advances in Call of Duty: WWII" ]
	float3 Diffuse_Chan(float roughness, float NdotV, float NdotL, float VdotH, float NdotH)
	{
		float a = roughness * roughness;
		float a2 = a * a;
		float g = saturate((1.0 / 18.0) * log2(2 * rcp(a2) - 1));

		float F0 = VdotH + pow(1 - VdotH, 5);
		float FdV = 1 - 0.75 * pow(1 - NdotV, 5);
		float FdL = 1 - 0.75 * pow(1 - NdotL, 5);

		float Fd = lerp(F0, FdV * FdL, saturate(2.2 * g - 0.5));

		float Fb = ((34.5 * g - 59) * g + 24.5) * VdotH * exp2(-max(73.2 * g - 21.2, 8.9) * sqrt(NdotH));

		return (1 / Math::PI) * (Fd + Fb);
	}

	// Energy-preserving Oren-Nayar (EON) diffuse BRDF and importance sampling
	// [Fujii 2024, "A Survey on Physically Based Oren-Nayar Variants"]
	namespace EON
	{
		static const float CONSTANT1 = 0.5f - 2.0f / (3.0f * Math::PI);
		static const float CONSTANT2 = 2.0f / 3.0f - 28.0f / (15.0f * Math::PI);

		// Exact directional albedo of the Fujii Oren-Nayar (FON) model
		float E_FON_Exact(float mu, float r)
		{
			float AF = 1.0f / (1.0f + CONSTANT1 * r);
			float BF = r * AF;
			float Si = sqrt(max(1.0f - mu * mu, 0.0f));
			float G = Si * (acos(clamp(mu, -1.0f, 1.0f)) - Si * mu)
			        + (2.0f / 3.0f) * ((Si / max(mu, EPSILON_DIVISION)) * (1.0f - Si * Si * Si) - Si);
			return AF + (BF / Math::PI) * G;
		}

		// Fast polynomial approximation of E_FON
		float E_FON_Approx(float mu, float r)
		{
			float mucomp = 1.0f - mu;
			static const float g1 = 0.0571085289f;
			static const float g2 = 0.491881867f;
			static const float g3 = -0.332181442f;
			static const float g4 = 0.0714429953f;
			float GoverPi = mucomp * (g1 + mucomp * (g2 + mucomp * (g3 + mucomp * g4)));
			return (1.0f + r * GoverPi) / (1.0f + CONSTANT1 * r);
		}

		// EON BRDF evaluation
		//   rho     = single-scattering albedo
		//   r       = roughness in [0, 1]
		//   wi, wo  = incident and outgoing directions in local tangent space (z = surface normal)
		//   exact   = true for exact E_FON, false for fast approximation
		float3 Diffuse(float3 rho, float r, float3 wi, float3 wo, bool exact)
		{
			float mu_i = wi.z;
			float mu_o = wo.z;
			float s = dot(wi, wo) - mu_i * mu_o;
			float sovertF = s > 0.0f ? s / max(mu_i, mu_o) : s;
			float AF = 1.0f / (1.0f + CONSTANT1 * r);
			float3 f_ss = (rho / Math::PI) * AF * (1.0f + r * sovertF);

			float EFo = exact ? E_FON_Exact(mu_o, r) : E_FON_Approx(mu_o, r);
			float EFi = exact ? E_FON_Exact(mu_i, r) : E_FON_Approx(mu_i, r);
			float avgEF = AF * (1.0f + CONSTANT2 * r);

			float3 rho_ms = (rho * rho) * avgEF / (1.0f - rho * (1.0f - avgEF));
			static const float eps = 1.0e-7f;
			float3 f_ms = (rho_ms / Math::PI) * max(eps, 1.0f - EFo)
			            * max(eps, 1.0f - EFi)
			            / max(eps, 1.0f - avgEF);

			return f_ss + f_ms;
		}

		// Pipeline-compatible EON diffuse factor (replaces Diffuse_Lambert)
		// Returns EON BRDF pre-divided by albedo so downstream BaseColor multiply
		// gives the correct energy-preserving result: output * BaseColor = EON(BaseColor).
		//   rho     = albedo (needed for multi-scattering energy compensation)
		//   r       = roughness in [0, 1]
		//   wi, wo  = light and view directions in local tangent space (z = surface normal)
		float3 DiffuseFactor(float3 rho, float r, float3 wi, float3 wo)
		{
			float mu_i = wi.z;
			float mu_o = wo.z;
			float s = dot(wi, wo) - mu_i * mu_o;
			float sovertF = s > 0.0f ? s / max(mu_i, mu_o) : s;
			float AF = 1.0f / (1.0f + CONSTANT1 * r);

			// Single-scattering factor (replaces 1/pi from Lambert)
			float f_ss = (1.0f / Math::PI) * AF * (1.0f + r * sovertF);

			// Multi-scattering energy compensation
			float EFo = E_FON_Approx(mu_o, r);
			float EFi = E_FON_Approx(mu_i, r);
			float avgEF = AF * (1.0f + CONSTANT2 * r);

			// rho_ms / rho = rho * avgEF / (1 - rho * (1 - avgEF))
			static const float eps = 1.0e-7f;
			float3 rho_ms_over_rho = rho * avgEF / max(eps, 1.0f - rho * (1.0f - avgEF));

			float3 f_ms = (rho_ms_over_rho / Math::PI) * max(eps, 1.0f - EFo)
			            * max(eps, 1.0f - EFi)
			            / max(eps, 1.0f - avgEF);

			return f_ss + f_ms;
		}

		// EON directional albedo (for energy conservation / lobe weighting)
		float3 DirectionalAlbedo(float3 rho, float r, float3 wi, bool exact)
		{
			float mu_i = wi.z;
			float AF = 1.0f / (1.0f + CONSTANT1 * r);
			float EF = exact ? E_FON_Exact(mu_i, r) : E_FON_Approx(mu_i, r);
			float avgEF = AF * (1.0f + CONSTANT2 * r);
			float3 rho_ms = (rho * rho) * avgEF / (1.0f - rho * (1.0f - avgEF));
			return rho * EF + rho_ms * (1.0f - EF);
		}

		// LTC lobe coefficients a, b, c, d as a function of mu = cos(theta_o) and r
		void LTC_Coeffs(float mu, float r, out float a, out float b, out float c, out float d)
		{
			a = 1.0f + r * (0.303392f + (-0.518982f + 0.111709f * mu) * mu + (-0.276266f + 0.335918f * mu) * r);
			b = r * (-1.16407f + 1.15859f * mu + (0.150815f - 0.150105f * mu) * r) / (mu * mu * mu - 1.43545f);
			c = 1.0f + r * (0.20013f + (-0.506373f + 0.261777f * mu) * mu);
			d = r * (0.540852f + (-1.01625f + 0.475392f * mu) * mu) / (-1.0743f + (0.0725628f + mu) * mu);
		}

		// Orthonormal basis for LTC transform (z-axis aligned with surface normal)
		// Returns float3x3 with basis vectors as rows.
		// To transform from LTC to local space: mul(v, basis)
		// To transform from local to LTC space: mul(basis, v)
		float3x3 OrthonormalBasis_LTC(float3 w)
		{
			float lenSqr = dot(w.xy, w.xy);
			float3 X = lenSqr > 0.0f ? float3(w.x, w.y, 0.0f) * rsqrt(lenSqr) : float3(1, 0, 0);
			float3 Y = float3(-X.y, X.x, 0.0f);
			return float3x3(X, Y, float3(0, 0, 1));
		}

		// CLTC importance sampling of a direction
		// Returns float4(wi_local.xyz, pdf)
		float4 CLTC_Sample(float3 wo_local, float r, float u1, float u2)
		{
			float a, b, c, d;
			LTC_Coeffs(wo_local.z, r, a, b, c, d);
			float R = sqrt(u1);
			float phi = 2.0f * Math::PI * u2;
			float x, y;
			sincos(phi, y, x);
			x *= R;
			y *= R;
			float vz = 1.0f / sqrt(d * d + 1.0f);
			float s = 0.5f * (1.0f + vz);
			x = -lerp(sqrt(1.0f - y * y), x, s);
			float3 wh = float3(x, y, sqrt(max(1.0f - (x * x + y * y), 0.0f)));
			float pdf_wh = wh.z / (Math::PI * s);
			float3 wi = float3(a * wh.x + b * wh.z, c * wh.y, d * wh.x + wh.z);
			float len = length(wi);
			float detM = c * (a - b * d);
			float pdf_wi = pdf_wh * len * len * len / detM;
			float3x3 basis = OrthonormalBasis_LTC(wo_local);
			wi = normalize(mul(wi, basis));
			return float4(wi, pdf_wi);
		}

		// CLTC PDF evaluation
		float CLTC_PDF(float3 wo_local, float3 wi_local, float r)
		{
			float3x3 basis = OrthonormalBasis_LTC(wo_local);
			float3 wi = mul(basis, wi_local);
			float a, b, c, d;
			LTC_Coeffs(wo_local.z, r, a, b, c, d);
			float detM = c * (a - b * d);
			float3 wh = float3(c * (wi.x - b * wi.z), (a - b * d) * wi.y, -c * (d * wi.x - a * wi.z));
			float lenSqr = dot(wh, wh);
			float vz = 1.0f / sqrt(d * d + 1.0f);
			float s = 0.5f * (1.0f + vz);
			float pdf = detM * detM / (lenSqr * lenSqr) * max(wh.z, 0.0f) / (Math::PI * s);
			return pdf;
		}

		// Uniform hemisphere lobe sampling
		float3 UniformLobeSample(float u1, float u2)
		{
			float sinTheta = sqrt(1.0f - u1 * u1);
			float phi = 2.0f * Math::PI * u2;
			float cp, sp;
			sincos(phi, sp, cp);
			return float3(sinTheta * cp, sinTheta * sp, u1);
		}

		// Importance sampling of the EON BRDF via CLTC
		//   wo_local = outgoing direction in local tangent space (z = normal)
		//   r        = roughness in [0, 1]
		//   u1, u2   = uniform random numbers in [0, 1]
		//   Returns float4(wi_local.xyz, pdf)
		float4 Sample(float3 wo_local, float r, float u1, float u2)
		{
			float mu = wo_local.z;
			float P_u = pow(max(r, 0.0f), 0.1f) * (0.162925f + (-0.372058f + (0.538233f - 0.290822f * mu) * mu) * mu);
			float P_c = 1.0f - P_u;
			float4 wi;
			float pdf_c;
			if (u1 <= P_u) {
				u1 = u1 / P_u;
				wi = float4(UniformLobeSample(u1, u2), 0.0f);
				pdf_c = CLTC_PDF(wo_local, wi.xyz, r);
			} else {
				u1 = (u1 - P_u) / P_c;
				wi = CLTC_Sample(wo_local, r, u1, u2);
				pdf_c = wi.w;
			}
			static const float pdf_u = 1.0f / (2.0f * Math::PI);
			wi.w = P_u * pdf_u + P_c * pdf_c;
			return wi;
		}

		// PDF of the EON importance sampling
		float PDF(float3 wo_local, float3 wi_local, float r)
		{
			float mu = wo_local.z;
			float P_u = pow(max(r, 0.0f), 0.1f) * (0.162925f + (-0.372058f + (0.538233f - 0.290822f * mu) * mu) * mu);
			float P_c = 1.0f - P_u;
			float pdf_c = CLTC_PDF(wo_local, wi_local, r);
			static const float pdf_u = 1.0f / (2.0f * Math::PI);
			return P_u * pdf_u + P_c * pdf_c;
		}
	}

	// Specular BRDFs
	// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
	float3 F_Schlick(float3 specularColor, float VdotH)
	{
		float Fc = pow(1 - VdotH, 5);
		return Fc + (1 - Fc) * specularColor;
	}

	float3 F_Schlick(float3 F0, float3 F90, float VdotH)
	{
		float Fc = pow(1 - VdotH, 5);
		return F0 + (F90 - F0) * Fc;
	}

	// [Kutz et al. 2021, "Novel aspects of the Adobe Standard Material" ]
	float3 F_AdobeF82(float3 F0, float3 F82, float VdotH)
	{
		const float Fc = pow(1 - VdotH, 5);
		const float K = 49.0 / 46656.0;
		float3 b = (K - K * F82) * (7776.0 + 9031.0 * F0);
		return saturate(F0 + Fc * ((1 - F0) - b * (VdotH - VdotH * VdotH)));
	}

	// [Beckmann 1963, "The scattering of electromagnetic waves from rough surfaces"]
	float D_Beckmann(float roughness, float NdotH)
	{
		float NdotH2 = NdotH * NdotH;
		float a = roughness * roughness;
		float a2 = a * a;
		return exp((NdotH2 - 1.0) / (a2 * NdotH2)) / (Math::PI * a2 * NdotH2 * NdotH2);
	}

	// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
	float D_GGX(float roughness, float NdotH)
	{
		float NdotH2 = NdotH * NdotH;
		float a = roughness * roughness;
		float a2 = a * a;
		float d = NdotH2 * (a2 - 1.0) + 1.0;
		return (a2 / (Math::PI * d * d));
	}

	// [Burley 2012, "Physically-Based Shading at Disney"]
	float D_AnisoGGX(float alphaX, float alphaY, float NdotH, float XdotH, float YdotH)
	{
		float d = XdotH * XdotH / (alphaX * alphaX) + YdotH * YdotH / (alphaY * alphaY) + NdotH * NdotH;
		return rcp(Math::PI * alphaX * alphaY * d * d);
	}

	// [Estevez et al. 2017, "Production Friendly Microfacet Sheen BRDF"]
	float D_Charlie(float roughness, float NdotH)
	{
		float invAlpha = pow(abs(roughness), -4);
		float cos2h = NdotH * NdotH;
		float sin2h = 1.0 - cos2h;
		return (2.0 + invAlpha) * pow(abs(sin2h), invAlpha * 0.5) / Math::TAU;
	}

	// Smith term for GGX
	// [Smith 1967, "Geometrical shadowing of a random rough surface"]
	float Vis_Smith(float roughness, float NdotV, float NdotL)
	{
		float a = roughness * roughness;
		float a2 = a * a;
		float Vis_SmithV = NdotV + sqrt(a2 + (1.0 - a2) * NdotV * NdotV);
		float Vis_SmithL = NdotL + sqrt(a2 + (1.0 - a2) * NdotL * NdotL);
		return rcp(max(Vis_SmithV * Vis_SmithL, EPSILON_DIVISION));
	}

	// Appoximation of joint Smith term for GGX
	// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
	float Vis_SmithJointApprox(float roughness, float NdotV, float NdotL)
	{
		float a = roughness * roughness;
		float Vis_SmithV = NdotL * (NdotV * (1.0 + a) + a);
		float Vis_SmithL = NdotV * (NdotL * (1.0 + a) + a);
		return rcp(max(Vis_SmithV + Vis_SmithL, EPSILON_DIVISION)) * 0.5;
	}

	float Vis_SmithJoint(float roughness, float NdotV, float NdotL)
	{
		float a = roughness * roughness;
		float a2 = a * a;
		float Vis_SmithV = NdotL * sqrt(a2 + (1.0 - a2) * NdotV * NdotV);
		float Vis_SmithL = NdotV * sqrt(a2 + (1.0 - a2) * NdotL * NdotL);
		return rcp(max(Vis_SmithV + Vis_SmithL, EPSILON_DIVISION)) * 0.5;
	}

	float Vis_SmithJointAniso(float alphaX, float alphaY, float NdotL, float NdotV, float XdotL, float YdotL, float XdotV, float YdotV)
	{
		float Vis_SmithV = NdotL * length(float3(alphaX * XdotV, alphaY * YdotV, NdotV));
		float Vis_SmithL = NdotV * length(float3(alphaX * XdotL, alphaY * YdotL, NdotL));
		return rcp(max(Vis_SmithV + Vis_SmithL, EPSILON_DIVISION)) * 0.5;
	}

	// [Estevez and Kulla 2017, "Production Friendly Microfacet Sheen BRDF"]
	float Vis_Charlie_L(float x, float r)
	{
		r = saturate(r);
		r = 1.0 - (1.0 - r) * (1.0 - r);
		float a = lerp(25.3245, 21.5473, r);
		float b = lerp(3.32435, 3.82987, r);
		float c = lerp(0.16801, 0.19823, r);
		float d = lerp(-1.27393, -1.97760, r);
		float e = lerp(-4.85967, -4.32054, r);

		return a * rcp((1 + b * pow(x, c)) + d * x + e);
	}

	float Vis_Charlie(float roughness, float NdotV, float NdotL)
	{
		float visV = NdotV < 0.5 ? exp(Vis_Charlie_L(NdotV, roughness)) : exp(2.0 * Vis_Charlie_L(0.5, roughness) - Vis_Charlie_L(1.0 - NdotV, roughness));
		float visL = NdotL < 0.5 ? exp(Vis_Charlie_L(NdotL, roughness)) : exp(2.0 * Vis_Charlie_L(0.5, roughness) - Vis_Charlie_L(1.0 - NdotL, roughness));
		return rcp(((1.0 + visV + visL) * max(4.0 * NdotL * NdotV, EPSILON_DIVISION)));
	}

	// [Neubelt et al. 2013, "Crafting a Next-gen Material Pipeline for The Order: 1886"]
	float Vis_Neubelt(float NdotV, float NdotL)
	{
		return rcp(4.0 * max(NdotL + NdotV - NdotL * NdotV, EPSILON_DIVISION));
	}

	// [Lazarov 2013, "Getting More Physical in Call of Duty: Black Ops II"]
	float2 EnvBRDFApproxLazarov(float roughness, float NdotV)
	{
		const float4 c0 = { -1, -0.0275, -0.572, 0.022 };
		const float4 c1 = { 1, 0.0425, 1.04, -0.04 };
		float4 r = roughness * c0 + c1;
		float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
		float2 AB = float2(-1.04, 1.04) * a004 + r.zw;
		return AB;
	}

	// [Hirvonen et al. 2019 "Accurate Real-Time Specular Reflections with Radiance Caching"]
	float2 EnvBRDFApproxHirvonen(float roughness, float NdotV)
	{
		const float2x2 m0 = float2x2(0.99044, -1.28514, 1.29678, -0.755907);
		const float3x3 m1 = float3x3(1, 2.92338, 59.4188, 20.3225, -27.0302, 222.592, 121.563, 626.13, 316.627);
		const float2x2 m2 = float2x2(0.0365463, 3.32707, 9.0632, -9.04756);
		const float3x3 m3 = float3x3(1, 3.59685, -1.36772, 9.04401, -16.3174, 9.22949, 5.56589, 19.7886, -20.2123);

		float a = roughness * roughness;
		float a2 = a * a;
		float a3 = a * a2;
		float c = NdotV;
		float c2 = c * c;
		float c3 = c * c2;

		float k0 = dot(float2(1.0, a), mul(m0, float2(1.0, c)));
		k0 /= dot(float3(1.0, a, a3), mul(m1, float3(1.0, c, c3)));
		float k1 = dot(float2(1.0, a), mul(m2, float2(1.0, c)));
		k1 /= dot(float3(1.0, a, a3), mul(m3, float3(1.0, c2, c3)));

		return float2(k1, k0);
	}

	float2 EnvBRDF(float roughness, float NdotV)
	{
#if defined(ENV_BRDF_HIRVONEN)
		return EnvBRDFApproxHirvonen(roughness, NdotV);
#else
		return EnvBRDFApproxLazarov(roughness, NdotV);
#endif
	}
}

#endif  // __BRDF_DEPENDENCY_HLSL__