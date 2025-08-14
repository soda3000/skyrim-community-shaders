#include "Localization.h"
#include "LocalizationFormat.h"

#include <filesystem>
#include <fstream>
#include <codecvt>
#include <locale>
#include <algorithm>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
    inline std::string ToLowerAscii(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) {
            out.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c));
        }
        return out;
    }
}

namespace Loc
{
    static std::unordered_map<std::string, std::string> g_active;    // resolved (lang -> parent -> en)
    static std::unordered_map<std::string, std::string> g_en;        // base EN
    static std::unordered_map<std::string, std::string> g_lang;      // raw current language
    static std::unordered_map<std::string, std::string> g_parent;    // raw parent (e.g., "pt")
    static std::wstring g_baseDir;
    static std::string g_currentLang = "en";
    static std::shared_mutex g_mutex;
    static std::vector<std::string> g_available; // cached available language codes

    static std::string Narrow(const std::wstring& w)
    {
        // UTF-8 conversion
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.to_bytes(w);
    }

    static bool LoadJsonFile(const fs::path& p, std::unordered_map<std::string, std::string>& out)
    {
        if (!fs::exists(p)) return false;
        std::ifstream f(p, std::ios::binary);
        if (!f) return false;
        json j;
        try { f >> j; } catch (...) { return false; }
        if (!j.is_object()) return false;

        out.clear();
        for (auto it = j.begin(); it != j.end(); ++it)
        {
            if (it.value().is_string())
                out[it.key()] = it.value().get<std::string>();
        }
        return true;
    }

    static std::string ParentOf(std::string_view lang)
    {
        // "pt-BR" -> "pt"; "en" -> ""
        auto pos = lang.find('-');
        if (pos == std::string_view::npos) return {};
        return std::string(lang.substr(0, pos));
    }

    static void BuildActive()
    {
        g_active = g_en;
        for (const auto& kv : g_parent)
            g_active[ToLowerAscii(kv.first)] = kv.second;
        for (const auto& kv : g_lang)
            g_active[ToLowerAscii(kv.first)] = kv.second;
    }

    // Internal helper: assumes caller holds g_mutex
    static void RescanAvailableLanguagesUnlocked()
    {
        g_available.clear();

        const fs::path base = fs::path(g_baseDir);
        std::error_code ec;
        if (fs::exists(base, ec) && fs::is_directory(base, ec)) {
            for (const auto& entry : fs::directory_iterator(base, ec)) {
                if (ec) break;
                if (!entry.is_regular_file()) continue;
                const auto& p = entry.path();
                if (p.extension() == L".json") {
                    auto code = Narrow(p.stem().wstring());
                    if (!code.empty()) g_available.emplace_back(std::move(code));
                }
            }
        }

        if (g_available.empty()) {
            g_available.emplace_back("en");
        } else {
            std::sort(g_available.begin(), g_available.end());
            auto it = std::find(g_available.begin(), g_available.end(), std::string("en"));
            if (it != g_available.end()) {
                std::rotate(g_available.begin(), it, std::next(it));
            }
        }
    }

    void RescanAvailableLanguages()
    {
        std::unique_lock lk(g_mutex);
        RescanAvailableLanguagesUnlocked();
    }

    const std::vector<std::string>& GetAvailableLanguages()
    {
        std::shared_lock lk(g_mutex);
        return g_available;
    }

    bool IsLanguageAvailable(std::string_view languageTag)
    {
        std::shared_lock lk(g_mutex);
        return std::find(g_available.begin(), g_available.end(), std::string(languageTag)) != g_available.end();
    }

    void Init(std::wstring baseDirectoryUtf16)
    {
        std::unique_lock lk(g_mutex);
        g_baseDir = std::move(baseDirectoryUtf16);

        const fs::path base = fs::path(g_baseDir);
        LoadJsonFile(base / L"en.json", g_en);
        g_parent.clear();
        g_lang.clear();
        g_currentLang = "en";
        // Build list of available languages on init without re-locking
        RescanAvailableLanguagesUnlocked();
        BuildActive();
    }

    bool SetLanguage(std::string_view languageTag)
    {
        std::unique_lock lk(g_mutex);
        // Validate availability before attempting to load
        if (std::find(g_available.begin(), g_available.end(), std::string(languageTag)) == g_available.end()) {
            // attempt a rescan once (user may have added files at runtime)
            lk.unlock();
            RescanAvailableLanguages();
            lk.lock();
            if (std::find(g_available.begin(), g_available.end(), std::string(languageTag)) == g_available.end()) {
                return false; // not available
            }
        }

        g_currentLang = std::string(languageTag);

        const fs::path base = fs::path(g_baseDir);
        g_lang.clear();
        g_parent.clear();

        // Load parent first if applicable
        const auto parent = ParentOf(languageTag);
        if (!parent.empty())
        {
            LoadJsonFile(base / fs::path(std::wstring(parent.begin(), parent.end()) + L".json"), g_parent);
        }

        // Load exact language (e.g., "pt-BR.json")
        const std::wstring wlang(languageTag.begin(), languageTag.end());
        bool ok = LoadJsonFile(base / fs::path(wlang + L".json"), g_lang);

        // Ensure EN base is present
        if (g_en.empty())
            LoadJsonFile(base / L"en.json", g_en);

        BuildActive();
        return ok;
    }

    std::string Get(std::string_view id)
    {
        std::shared_lock lk(g_mutex);
        // Ensure key is lowercase
        auto it = g_active.find(ToLowerAscii(id));
        if (it != g_active.end())
            return it->second;
        // fallback: return id itself, helps find missing keys in dev builds
        return std::string(id);
    }

    std::string Fmt(std::string_view id, const std::unordered_map<std::string, std::string>& kv)
    {
        return SimpleFormat::Apply(Get(id), kv);
    }

    std::string Plural(std::string_view baseId, int n)
    {
        std::string key = std::string(baseId);
        key += (n == 1) ? ".one" : ".other";
        return Fmt(key, {{"n", std::to_string(n)}});
    }

    std::string CurrentLanguage()
    {
        std::shared_lock lk(g_mutex);
        return g_currentLang;
    }

    bool HasKey(std::string_view id)
    {
        std::shared_lock lk(g_mutex);
        return g_active.find(ToLowerAscii(id)) != g_active.end();
    }
}