#include "nodal_csv.hpp"
#include "errors.hpp"
#include "stl_parser.hpp"   // kMaxFileBytes
#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_set>

namespace wb {
namespace {
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b;
    return s.substr(a, b - a);
}
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    size_t st = 0;
    for (;;) {
        const size_t p = s.find(',', st);
        out.push_back(trim(s.substr(st, p == std::string::npos ? p : p - st)));
        if (p == std::string::npos) break;
        st = p + 1;
    }
    return out;
}
}  // namespace

NodalField parseNodalCsv(std::istream& in, const std::string& label) {
    std::string line;
    size_t ln = 0;
    auto fail = [&](const std::string& m) -> void {
        throw FormatError(label + ": line " + std::to_string(ln) + ": " + m);
    };
    // header (skip blank lines, tolerate UTF-8 BOM)
    bool haveHeader = false;
    while (std::getline(in, line)) {
        ++ln;
        if (ln == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        if (!trim(line).empty()) { haveHeader = true; break; }
    }
    if (!haveHeader) throw FormatError(label + ": file is empty (contains no non-blank text).");
    const auto h = split(line);
    static const char* kHdr[5] = {"node_id", "x_m", "y_m", "z_m", "temperature_C"};
    if (h.size() != 5 || !std::equal(h.begin(), h.end(), kHdr))
        fail("header must be exactly 'node_id,x_m,y_m,z_m,temperature_C'.");

    NodalField f;
    std::unordered_set<int64_t> seen;
    bool first = true;
    while (std::getline(in, line)) {
        ++ln;
        if (trim(line).empty()) continue;
        const auto c = split(line);
        if (c.size() != 5) fail("expected 5 comma-separated fields, found " + std::to_string(c.size()) + ".");
        char* end = nullptr;
        const long long id = std::strtoll(c[0].c_str(), &end, 10);
        if (c[0].empty() || *end != '\0' || id < 0) fail("node_id '" + c[0] + "' must be a non-negative integer.");
        double v[4];
        for (int i = 0; i < 4; ++i) {
            if (!parseDouble(c[i + 1], v[i])) fail("'" + c[i + 1] + "' is not a number.");
            if (!std::isfinite(v[i])) fail("non-finite value '" + c[i + 1] + "' is not allowed.");
        }
        if (v[3] < -273.15) fail("temperature " + c[4] + " C is below absolute zero.");
        if (!seen.insert(id).second) fail("duplicate node_id " + c[0] + ".");
        f.points.push_back({id, v[0], v[1], v[2], v[3]});
        const double p[3] = {v[0], v[1], v[2]};
        if (first) {
            for (int k = 0; k < 3; ++k) f.minB[k] = f.maxB[k] = p[k];
            f.minT = f.maxT = v[3];
            first = false;
        }
        for (int k = 0; k < 3; ++k) { f.minB[k] = std::min(f.minB[k], p[k]); f.maxB[k] = std::max(f.maxB[k], p[k]); }
        f.minT = std::min(f.minT, v[3]);
        f.maxT = std::max(f.maxT, v[3]);
    }
    if (f.points.empty()) throw FormatError(label + ": header found but there are no data rows.");
    return f;
}

NodalField parseNodalCsvFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw FormatError("Cannot open '" + path + "'. Check the path and permissions.");
    const uint64_t size = static_cast<uint64_t>(f.tellg());
    if (size == 0) throw FormatError("'" + path + "' is empty (0 bytes).");
    if (size > kMaxFileBytes)
        throw FormatError("'" + path + "' exceeds the " + std::to_string(kMaxFileBytes / (1024 * 1024)) + " MB limit.");
    f.seekg(0);
    return parseNodalCsv(f, path);
}
}  // namespace wb
