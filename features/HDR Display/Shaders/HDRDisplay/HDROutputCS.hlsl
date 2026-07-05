/**
 * @file HDROutputCS.hlsl
 * @brief HDR: gamma decode, paper-white × (nits/203), BT.2020, PQ. SDR: passthrough + UI.
 */

#include "Common/Color.hlsli"
#include "Common/SharedData.hlsli"

Texture2D<float4> SceneTex : register(t0);
Texture2D<float4> UITex : register(t1);
RWTexture2D<float4> HDROutput : register(u0);

cbuffer PerFrame : register(b0)
{
	float enableHDR : packoffset(c0.x);
	float paperWhite : packoffset(c0.y);
	float peakNits : packoffset(c0.z);
	float skipUIComposite : packoffset(c0.w);
	float uiBrightness : packoffset(c1.x);
	float isMainOrLoadingMenu : packoffset(c1.y);
	float fgTweenMenuMidAlphaBoost : packoffset(c1.z);  ///< TweenMenu: soften AA band when compositing here (UIBrightnessCS skips while paused)
	float previewSDR : packoffset(c1.w);                ///< 1.0 = emit sRGB SDR (crop preview) instead of PQ HDR10
}

[numthreads(8, 8, 1)] void main(uint3 dispatchID : SV_DispatchThreadID) {
	uint width, height;
	HDROutput.GetDimensions(width, height);
	if (dispatchID.x >= width || dispatchID.y >= height)
		return;

	float4 scene = SceneTex[dispatchID.xy];
	float4 ui = UITex[dispatchID.xy];

	bool hdrEnabled = enableHDR > 0.5;
	bool skipUI = skipUIComposite > 0.5;

	float3 finalColor;

	if (hdrEnabled) {
		float3 compositedColorLinear;

		// Scene from ISHDR is gamma-encoded. Composite UI+scene in gamma space.
		float3 sceneGamma = scene.rgb;
		float3 compositedColorGamma;
		if (skipUI) {
			compositedColorGamma = sceneGamma;
		} else {
			float3 uiGamma = ui.rgb;
			if (!(isMainOrLoadingMenu > 0.5)) {  // UI and scene can't be separated in main menu or loading screen
				// scale UI brightness (multiplier based on paperWhite)
				float3 uiLinear = Color::SrgbToLinear(max(0, uiGamma));
				uiLinear *= uiBrightness;
				uiGamma = Color::LinearToSrgb(uiLinear);
			}

			compositedColorGamma = uiGamma + sceneGamma * (1.0 - ui.a);
		}

		compositedColorLinear = Color::GammaToLinearSafe(compositedColorGamma);

		if (previewSDR > 0.5) {
			// Crop preview lives in the SDR menu buffer: emit sRGB instead of PQ.
			finalColor = saturate(Color::LinearToSrgb(max(0.0, compositedColorLinear)));
		} else {
			compositedColorLinear = Color::BT709ToBT2020(compositedColorLinear);
			finalColor = Color::pq::Encode(max(0.0, compositedColorLinear), paperWhite);

			finalColor = saturate(finalColor);
		}
	} else {
		// SDR: scene is already gamma from ISHDR. Passthrough.
		float3 sceneGamma = scene.rgb;

		if (skipUI) {
			finalColor = sceneGamma;
		} else {
			finalColor = ui.rgb + sceneGamma * (1.0 - ui.a);
		}

		finalColor = saturate(finalColor);
	}

	HDROutput[dispatchID.xy] = float4(finalColor, 1.0);
}