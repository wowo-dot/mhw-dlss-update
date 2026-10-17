#pragma once
#include <iomanip>
#include <sstream>
#include <string>

namespace mhw {
// A JSON string: quotes and backslashes escaped, control characters as \uXXXX.
inline std::string quoted(const std::string &value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\')
            out << '\\' << c;
        else if (c < 32)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
        else
            out << c;
    }
    return out.str() + '"';
}
} // namespace mhw
