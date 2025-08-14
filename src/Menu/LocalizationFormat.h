#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

namespace SimpleFormat
{
    // Replace all "{key}" with kv.at(key) if present; leaves unknown tokens unchanged.
    inline std::string Apply(std::string_view input,
                             const std::unordered_map<std::string, std::string>& kv)
    {
        std::string out;
        out.reserve(input.size() + 16);
        for (size_t i = 0; i < input.size(); )
        {
            if (input[i] == '{')
            {
                size_t j = input.find('}', i + 1);
                if (j != std::string_view::npos)
                {
                    auto key = std::string(input.substr(i + 1, j - (i + 1)));
                    auto it = kv.find(key);
                    if (it != kv.end())
                        out.append(it->second);
                    else
                        out.append(input.substr(i, j - i + 1)); // keep as-is
                    i = j + 1;
                    continue;
                }
            }
            out.push_back(input[i++]);
        }
        return out;
    }
}