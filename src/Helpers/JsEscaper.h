#pragma once
#include <string>

inline std::string EscapeForJs(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        if (c == '\\') {
            result += "\\\\";
        }
        else if (c == '`') {
            result += "\\`";
        }
        else if (c == '$') {
            result += "\\$";
        }
        else {
            result += c;
        }
    }
    return result;
}
