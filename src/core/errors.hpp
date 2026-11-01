#pragma once
#include <cstdlib>
#include <stdexcept>
#include <string>
namespace wb {
// Thrown for any malformed / unreadable input file. Message is user-facing.
class FormatError : public std::runtime_error {
public:
    explicit FormatError(const std::string& m) : std::runtime_error(m) {}
};
// Strict number parse: the whole token must be consumed.
inline bool parseDouble(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size();
}
}  // namespace wb
