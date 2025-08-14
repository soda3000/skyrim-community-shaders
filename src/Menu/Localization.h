#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <shared_mutex>
#include <vector>

namespace Loc
{
    // Initialize with a folder containing locale files (e.g., "resources/locales")
    void Init(std::wstring baseDirectoryUtf16);

    // Set active language ("en", "pt-BR"); triggers reload with fallback chain (lang -> parent -> "en").
    bool SetLanguage(std::string_view languageTag);

    // Get localized string by ID; falls back to English if not found; returns ID if still missing.
    std::string Get(std::string_view id);

    // Format a string by replacing {placeholders} using the provided map.
    std::string Fmt(std::string_view id, const std::unordered_map<std::string, std::string>& kv);

    // Simple plural: baseId+".one"/".other" using count 'n'
    std::string Plural(std::string_view baseId, int n);

    // Optional: expose current language tag
    std::string CurrentLanguage();

    // Optional: diagnostics
    bool HasKey(std::string_view id);

    // Enumerate available languages from the locales directory. Populated on Init.
    void RescanAvailableLanguages();
    const std::vector<std::string>& GetAvailableLanguages();
    bool IsLanguageAvailable(std::string_view languageTag);
}