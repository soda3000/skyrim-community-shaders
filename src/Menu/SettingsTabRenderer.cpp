#include "SettingsTabRenderer.h"

#include <imgui.h>
// imgui_internal not needed anymore

#include "Globals.h"
#include "Menu.h"
#include "ShaderCache.h"
#include "Util.h"
#include "Localization.h"
#include "LocalizationMacros.h"
#include <filesystem>
#include <codecvt>
#include <algorithm>
#include <string>

namespace {
    // ---- Manual selection state ----
    enum class GeneralTabSel { Shaders, Keybindings, Interface };
    static GeneralTabSel g_selectedGeneral = GeneralTabSel::Shaders;
    static int g_forceFramesGeneral = 0;

    enum class InterfaceTabSel { UIOptions, Sizes, Colors };
    static InterfaceTabSel g_selectedInner = InterfaceTabSel::UIOptions;
    static int g_forceFramesInner = 0;
    static std::string g_lastLangForcing;
}

void SettingsTabRenderer::RenderGeneralSettings(
	SettingsState& state,
	const std::function<const char*(uint32_t)>& keyIdToString)
{
    const std::string dbgLang = Loc::CurrentLanguage();
    // Detect language change and schedule selection forcing for a few frames
    if (g_lastLangForcing != dbgLang) {
        g_lastLangForcing = dbgLang;
        g_forceFramesGeneral = 3;
        g_forceFramesInner = 3; // inner bar too
    }

    if (ImGui::BeginTabBar("##GeneralTabBar", ImGuiTabBarFlags_None)) {
        RenderShadersTab();
        RenderKeybindingsTab(state, keyIdToString);
        RenderInterfaceTab();
        ImGui::EndTabBar();
        if (g_forceFramesGeneral > 0) --g_forceFramesGeneral;
    }
}

void SettingsTabRenderer::RenderShadersTab()
{
    ImGuiTabItemFlags selFlag = (g_forceFramesGeneral > 0 && g_selectedGeneral == GeneralTabSel::Shaders) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem((TR("menu.tabs.shaders") + "##tab_shaders").c_str(), nullptr, selFlag)) {
        g_selectedGeneral = GeneralTabSel::Shaders;
        auto shaderCache = globals::shaderCache;

        bool useCustomShaders = shaderCache->IsEnabled();
        if (ImGui::Checkbox(TR("menu.lbl.use_custom_shaders").c_str(), &useCustomShaders)) {
            shaderCache->SetEnabled(useCustomShaders);
        }
        if (auto _tt = Util::HoverTooltipWrapper()) {
            ImGui::TextUnformatted(TR("menu.tip.disable_all_features").c_str());
        }

        bool useDiskCache = shaderCache->IsDiskCache();
        if (ImGui::Checkbox(TR("menu.lbl.enable_disk_cache").c_str(), &useDiskCache)) {
            shaderCache->SetDiskCache(useDiskCache);
        }
        if (auto _tt = Util::HoverTooltipWrapper()) {
            ImGui::TextUnformatted(TR("menu.tip.disable_disk_cache").c_str());
        }

        bool useAsync = shaderCache->IsAsync();
        if (ImGui::Checkbox(TR("menu.lbl.enable_async").c_str(), &useAsync)) {
            shaderCache->SetAsync(useAsync);
        }
        if (auto _tt = Util::HoverTooltipWrapper()) {
            ImGui::TextUnformatted(TR("menu.tip.skip_uncompiled").c_str());
        }

        ImGui::EndTabItem();
    }
}

void SettingsTabRenderer::RenderKeybindingsTab(
    SettingsState& state,
    const std::function<const char*(uint32_t)>& keyIdToString)
{
    ImGuiTabItemFlags selFlag = (g_forceFramesGeneral > 0 && g_selectedGeneral == GeneralTabSel::Keybindings) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem((TR("menu.tabs.keybindings") + "##tab_keybindings").c_str(), nullptr, selFlag)) {
        g_selectedGeneral = GeneralTabSel::Keybindings;
        auto& settings = globals::menu->GetSettings();
        auto& themeSettings = globals::menu->GetSettings().Theme;

        // Toggle Key
        if (state.settingToggleKey) {
            ImGui::TextUnformatted(TR("menu.tip.press_any_key_toggle").c_str());
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TR("menu.lbl.toggle_key").c_str());
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(themeSettings.StatusPalette.CurrentHotkey, "%s", keyIdToString(settings.ToggleKey));

            ImGui::AlignTextToFramePadding();
            ImGui::SameLine();
            if (ImGui::Button(TR("menu.btn.change_toggle").c_str())) {
                state.settingToggleKey = true;
            }
        }

        // Effects Toggle Key
        if (state.settingsEffectsToggle) {
            ImGui::TextUnformatted(TR("menu.tip.press_any_key_effect_toggle").c_str());
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TR("menu.lbl.effect_toggle_key").c_str());
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(themeSettings.StatusPalette.CurrentHotkey, "%s", keyIdToString(settings.EffectToggleKey));

            ImGui::AlignTextToFramePadding();
            ImGui::SameLine();
            if (ImGui::Button(TR("menu.btn.change_effect_toggle").c_str())) {
                state.settingsEffectsToggle = true;
            }
        }

        // Skip Compilation Key
        if (state.settingSkipCompilationKey) {
            ImGui::TextUnformatted(TR("menu.tip.press_any_key_skip").c_str());
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TR("menu.lbl.skip_compilation_key").c_str());
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(themeSettings.StatusPalette.CurrentHotkey, "%s", keyIdToString(settings.SkipCompilationKey));

            ImGui::AlignTextToFramePadding();
            ImGui::SameLine();
            if (ImGui::Button(TR("menu.btn.change_skip").c_str())) {
                state.settingSkipCompilationKey = true;
            }
        }

        // Overlay Toggle Key
        if (state.settingOverlayToggleKey) {
            ImGui::TextUnformatted(TR("menu.tip.press_any_key_overlay").c_str());
        } else {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(TR("menu.lbl.overlay_toggle_key").c_str());
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(themeSettings.StatusPalette.CurrentHotkey, "%s", keyIdToString(settings.OverlayToggleKey));

            ImGui::AlignTextToFramePadding();
            ImGui::SameLine();
            if (ImGui::Button(TR("menu.btn.change_overlay_toggle").c_str())) {
                state.settingOverlayToggleKey = true;
            }
        }

        ImGui::EndTabItem();
    }
}

void SettingsTabRenderer::RenderInterfaceTab()
{
    ImGuiTabItemFlags selFlag = (g_forceFramesGeneral > 0 && g_selectedGeneral == GeneralTabSel::Interface) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem((TR("menu.tabs.interface") + "##tab_interface").c_str(), nullptr, selFlag)) {
        g_selectedGeneral = GeneralTabSel::Interface;
        // Inner Interface tab bar debug logging
        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
            RenderUIOptionsTab();
            RenderSizesTab();
            RenderColorsTab();
            ImGui::EndTabBar();
            if (g_forceFramesInner > 0) --g_forceFramesInner;
        }
        ImGui::EndTabItem();
    }
}

void SettingsTabRenderer::RenderUIOptionsTab()
{
    ImGuiTabItemFlags selFlag = (g_forceFramesInner > 0 && g_selectedInner == InterfaceTabSel::UIOptions) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem((TR("menu.tabs.ui_options") + "##tab_ui_options").c_str(), nullptr, selFlag)) {
        g_selectedInner = InterfaceTabSel::UIOptions;
        auto& themeSettings = globals::menu->GetSettings().Theme;

        ImGui::SeparatorText(TR("menu.ui.language").c_str());
        {
            // Rescan button to refresh available languages at runtime
            if (ImGui::Button(TR("menu.ui.rescan").c_str())) {
                Loc::RescanAvailableLanguages();
            }
            ImGui::SameLine();

            // Retrieve cached list
            const auto& langs = Loc::GetAvailableLanguages();

            // Build current index
            int currentIndex = 0;
            for (int i = 0; i < (int)langs.size(); ++i) {
                if (langs[i] == themeSettings.Language) { currentIndex = i; break; }
            }
            const char* currentLabel = langs.empty() ? "en" : langs[currentIndex].c_str();

            if (ImGui::BeginCombo("##language_combo", currentLabel)) {
                for (int i = 0; i < (int)langs.size(); ++i) {
                    bool selected = (i == currentIndex);
                    if (ImGui::Selectable(langs[i].c_str(), selected)) {
                        if (i != currentIndex) {
                            const std::string& chosen = langs[i];
                            globals::menu->langChanged = true;
                            globals::menu->langNew = chosen;
                        }
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if (ImGui::BeginPopup("LanguageError")) {
                ImGui::TextUnformatted(TR("menu.ui.language_error").c_str());
                if (ImGui::Button(TR("menu.ui.ok").c_str())) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }

        ImGui::SeparatorText(TR("menu.ui.section.ui_elements").c_str());
        ImGui::Checkbox(TR("menu.lbl.use_icon_buttons_header").c_str(), &themeSettings.ShowActionIcons);
        if (auto _tt = Util::HoverTooltipWrapper()) {
            ImGui::TextUnformatted(TR("menu.tip.alignment_button").c_str());
        }

        ImGui::SliderFloat(TR("menu.lbl.tooltip_hover_delay").c_str(), &themeSettings.TooltipHoverDelay, 0.0f, 2.0f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
        if (auto _tt = Util::HoverTooltipWrapper()) {
            ImGui::TextUnformatted(TR("menu.tip.tooltip_hover_delay").c_str());
        }

        ImGui::EndTabItem();
    }
}

void SettingsTabRenderer::RenderSizesTab()
{
    ImGuiTabItemFlags selFlag = (g_forceFramesInner > 0 && g_selectedInner == InterfaceTabSel::Sizes) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem((TR("menu.tabs.sizes") + "##tab_sizes").c_str(), nullptr, selFlag)) {
        g_selectedInner = InterfaceTabSel::Sizes;
        auto& themeSettings = globals::menu->GetSettings().Theme;
        auto& style = themeSettings.Style;

        ImGui::SeparatorText(TR("menu.sections.main").c_str());
        ImGui::SliderFloat(TR("menu.lbl.global_scale").c_str(), &themeSettings.GlobalScale, -1.f, 1.f, "%.2f");
        if (auto _tt = Util::HoverTooltipWrapper()) {
            ImGui::TextUnformatted(TR("menu.tip.global_scale").c_str());
        }

        ImGui::SliderFloat(TR("menu.lbl.font_size").c_str(), &themeSettings.FontSize, ThemeManager::Constants::MIN_FONT_SIZE, ThemeManager::Constants::MAX_FONT_SIZE, "%.0f");
        ImGui::SliderFloat2(TR("menu.lbl.window_padding").c_str(), (float*)&style.WindowPadding, 0.0f, 20.0f, "%.0f");
        ImGui::SliderFloat2(TR("menu.lbl.frame_padding").c_str(), (float*)&style.FramePadding, 0.0f, 20.0f, "%.0f");
        ImGui::SliderFloat2(TR("menu.lbl.item_spacing").c_str(), (float*)&style.ItemSpacing, 0.0f, 20.0f, "%.0f");
        ImGui::SliderFloat2(TR("menu.lbl.item_inner_spacing").c_str(), (float*)&style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.indent_spacing").c_str(), &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.scrollbar_size").c_str(), &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.grab_min_size").c_str(), &style.GrabMinSize, 1.0f, 20.0f, "%.0f");

        ImGui::SeparatorText(TR("menu.sections.borders").c_str());
        ImGui::SliderFloat(TR("menu.lbl.window_border_size").c_str(), &style.WindowBorderSize, 0.0f, 5.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.child_border_size").c_str(), &style.ChildBorderSize, 0.0f, 5.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.popup_border_size").c_str(), &style.PopupBorderSize, 0.0f, 5.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.frame_border_size").c_str(), &style.FrameBorderSize, 0.0f, 5.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.tab_border_size").c_str(), &style.TabBorderSize, 0.0f, 5.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.tab_bar_border_size").c_str(), &style.TabBarBorderSize, 0.0f, 5.0f, "%.0f");

        ImGui::SeparatorText(TR("menu.sections.rounding").c_str());
        ImGui::SliderFloat(TR("menu.lbl.window_rounding").c_str(), &style.WindowRounding, 0.0f, 12.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.child_rounding").c_str(), &style.ChildRounding, 0.0f, 12.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.frame_rounding").c_str(), &style.FrameRounding, 0.0f, 12.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.popup_rounding").c_str(), &style.PopupRounding, 0.0f, 12.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.scrollbar_rounding").c_str(), &style.ScrollbarRounding, 0.0f, 12.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.grab_rounding").c_str(), &style.GrabRounding, 0.0f, 12.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.tab_rounding").c_str(), &style.TabRounding, 0.0f, 12.0f, "%.0f");

        ImGui::SeparatorText(TR("menu.sections.tables").c_str());
        ImGui::SliderFloat2(TR("menu.lbl.cell_padding").c_str(), (float*)&style.CellPadding, 0.0f, 20.0f, "%.0f");
        ImGui::SliderAngle(TR("menu.lbl.table_angled_headers_angle").c_str(), &style.TableAngledHeadersAngle, -50.0f, +50.0f);

        ImGui::SeparatorText(TR("menu.sections.widgets").c_str());
        ImGui::Combo(TR("menu.lbl.color_button_position").c_str(), (int*)&style.ColorButtonPosition, "Left\0Right\0");
        ImGui::SliderFloat2(TR("menu.lbl.button_text_align").c_str(), (float*)&style.ButtonTextAlign, 0.0f, 1.0f, "%.2f");
        if (auto _tt = Util::HoverTooltipWrapper())
            ImGui::TextUnformatted(TR("menu.tip.button_text_align").c_str());
        ImGui::SliderFloat2(TR("menu.lbl.selectable_text_align").c_str(), (float*)&style.SelectableTextAlign, 0.0f, 1.0f, "%.2f");
        if (auto _tt = Util::HoverTooltipWrapper())
            ImGui::TextUnformatted(TR("menu.tip.selectable_text_align").c_str());
        ImGui::SliderFloat(TR("menu.lbl.separator_text_border_size").c_str(), &style.SeparatorTextBorderSize, 0.0f, 10.0f, "%.0f");
        ImGui::SliderFloat2(TR("menu.lbl.separator_text_align").c_str(), (float*)&style.SeparatorTextAlign, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat2(TR("menu.lbl.separator_text_padding").c_str(), (float*)&style.SeparatorTextPadding, 0.0f, 40.0f, "%.0f");
        ImGui::SliderFloat(TR("menu.lbl.log_slider_deadzone").c_str(), &style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f");

        ImGui::SeparatorText(TR("menu.sections.docking").c_str());
        ImGui::SliderFloat(TR("menu.lbl.docking_splitter_size").c_str(), &style.DockingSeparatorSize, 0.0f, 12.0f, "%.0f");

        ImGui::EndTabItem();
    }
}

void SettingsTabRenderer::RenderColorsTab()
{
    ImGuiTabItemFlags selFlag = (g_forceFramesInner > 0 && g_selectedInner == InterfaceTabSel::Colors) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
    if (ImGui::BeginTabItem((TR("menu.tabs.colors") + "##tab_colors").c_str(), nullptr, selFlag)) {
        g_selectedInner = InterfaceTabSel::Colors;
        auto& themeSettings = globals::menu->GetSettings().Theme;
        auto& colors = themeSettings.FullPalette;

        ImGui::SeparatorText(TR("menu.sections.status").c_str());

        ImGui::ColorEdit4(TR("menu.lbl.disabled_text").c_str(), (float*)&themeSettings.StatusPalette.Disable);
        ImGui::ColorEdit4(TR("menu.lbl.error_text").c_str(), (float*)&themeSettings.StatusPalette.Error);
        ImGui::ColorEdit4(TR("menu.lbl.warning_text").c_str(), (float*)&themeSettings.StatusPalette.Warning);
        ImGui::ColorEdit4(TR("menu.lbl.restart_needed_text").c_str(), (float*)&themeSettings.StatusPalette.RestartNeeded);
        ImGui::ColorEdit4(TR("menu.lbl.current_hotkey_text").c_str(), (float*)&themeSettings.StatusPalette.CurrentHotkey);
        ImGui::ColorEdit4(TR("menu.lbl.success_text").c_str(), (float*)&themeSettings.StatusPalette.SuccessColor);
        ImGui::ColorEdit4(TR("menu.lbl.info_text").c_str(), (float*)&themeSettings.StatusPalette.InfoColor);

        ImGui::SeparatorText(TR("menu.sections.feature_headings").c_str());

        ImGui::ColorEdit4(TR("menu.lbl.regular").c_str(), (float*)&themeSettings.FeatureHeading.ColorDefault);
        ImGui::ColorEdit4(TR("menu.lbl.hovered").c_str(), (float*)&themeSettings.FeatureHeading.ColorHovered);
        ImGui::SliderFloat(TR("menu.lbl.minimized_alpha_factor").c_str(), &themeSettings.FeatureHeading.MinimizedFactor, 0.0f, 1.0f, "%.2f");

        ImGui::SeparatorText(TR("menu.sections.palette").c_str());

		if (ImGui::RadioButton(TR("menu.lbl.simple_palette").c_str(), themeSettings.UseSimplePalette))
			themeSettings.UseSimplePalette = true;
		ImGui::SameLine();
		if (ImGui::RadioButton(TR("menu.lbl.full_palette").c_str(), !themeSettings.UseSimplePalette))
			themeSettings.UseSimplePalette = false;

		if (themeSettings.UseSimplePalette) {
			ImGui::ColorEdit4("Background", (float*)&themeSettings.Palette.Background);
			ImGui::ColorEdit4("Text", (float*)&themeSettings.Palette.Text);
			ImGui::ColorEdit4("Border", (float*)&themeSettings.Palette.Border);
		} else {
			static ImGuiTextFilter filter;
			filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

			for (int i = 0; i < ImGuiCol_COUNT; i++) {
				const char* name = ImGui::GetStyleColorName(i);
				if (!filter.PassFilter(name))
					continue;
				ImGui::ColorEdit4(name, (float*)&colors[i], ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf);
			}
		}

		ImGui::EndTabItem();
	}
}