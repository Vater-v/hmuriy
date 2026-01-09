#pragma once
#include <cstdint>
#include <string>
#include <codecvt>
#include <locale>

struct Il2CppObject { 
    void* klass; 
    void* monitor; 
};

struct Il2CppString : public Il2CppObject { 
    int32_t length; 
    char16_t chars[1]; 
};

namespace Il2CppUtils {
    inline std::string Utf16ToUtf8(const char16_t* chars, int32_t len) {
        if (!chars || len <= 0) return "";
        std::u16string u16_str(chars, len);
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        return convert.to_bytes(u16_str);
    }
}