#pragma once
#include <string>
#include <algorithm>

namespace JsonUtils {

    inline std::string SmartMinify(const std::string& input) {
        if (input.empty()) return "";
        std::string output;
        output.reserve(input.length()); 
        bool insideQuotes = false;
        for (size_t i = 0; i < input.length(); ++i) {
            char c = input[i];
            if (c == '"' && (i == 0 || input[i - 1] != '\\')) insideQuotes = !insideQuotes;
            if (insideQuotes) {
                if (c == '\n') output += "\\n";
                else if (c == '\r') output += "\\r"; 
                else if (c == '\t') output += "\\t";
                else output += c;
            } else {
                if (!std::isspace(static_cast<unsigned char>(c))) output += c;
            }
        }
        return output;
    }

    inline bool IsSpam(const std::string& jsonContent) {
        return (jsonContent.find("\"verbose_localised_name\"") != std::string::npos) || 
               (jsonContent.find("\"keys\":{\"APIError") != std::string::npos);
    }
}