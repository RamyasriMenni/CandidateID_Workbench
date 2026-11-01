#include "stl_parser.hpp"
#include "errors.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace wb {
namespace {
std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
void tokenize(const std::string& line, std::vector<std::string>& t) {
    t.clear();
    std::istringstream ss(line);
    std::string w;
    while (ss >> w) t.push_back(w);
}
}  // namespace

StlMesh parseStl(std::istream& in, const std::string& label) {
    std::string line;
    size_t ln = 0;
    std::vector<std::string> t;
    auto next = [&]() {
        while (std::getline(in, line)) {
            ++ln;
            tokenize(line, t);
            if (!t.empty()) return true;
        }
        return false;
    };
    auto fail = [&](const std::string& m) -> void {
        throw FormatError(label + ": line " + std::to_string(ln) + ": " + m);
    };
    auto need = [&](const char* what) {
        if (!next()) throw FormatError(label + ": unexpected end of file while reading " + what +
                                       " (file truncated?).");
    };
    auto num = [&](const std::string& s) {
        double d;
        if (!parseDouble(s, d)) fail("'" + s + "' is not a number.");
        if (!std::isfinite(d)) fail("non-finite value '" + s + "' (NaN/Inf) is not allowed.");
        return d;
    };

    if (!next()) throw FormatError(label + ": file is empty (contains no non-blank text).");
    if (lower(t[0]) != "solid") fail("expected 'solid' on the first line; this does not look like an ASCII STL.");
    StlMesh m;
    for (size_t i = 1; i < t.size(); ++i) m.name += (i > 1 ? " " : "") + t[i];

    bool ended = false;
    bool first = true;
    while (next()) {
        if (lower(t[0]) == "endsolid") { ended = true; break; }
        if (!(t.size() == 5 && lower(t[0]) == "facet" && lower(t[1]) == "normal"))
            fail("expected 'facet normal nx ny nz' or 'endsolid'.");
        for (int i = 2; i < 5; ++i) num(t[i]);
        need("'outer loop'");
        if (!(t.size() == 2 && lower(t[0]) == "outer" && lower(t[1]) == "loop")) fail("expected 'outer loop'.");
        Triangle tri;
        for (int v = 0; v < 3; ++v) {
            need("'vertex'");
            if (!(t.size() == 4 && lower(t[0]) == "vertex")) fail("expected 'vertex x y z'.");
            for (int c = 0; c < 3; ++c) tri.v[v * 3 + c] = num(t[c + 1]);
        }
        need("'endloop'");
        if (lower(t[0]) != "endloop") fail("expected 'endloop' (a facet must have exactly 3 vertices).");
        need("'endfacet'");
        if (lower(t[0]) != "endfacet") fail("expected 'endfacet'.");
        // bounds + degenerate check
        for (int v = 0; v < 3; ++v)
            for (int c = 0; c < 3; ++c) {
                const double x = tri.v[v * 3 + c];
                if (first) { m.minB[c] = m.maxB[c] = x; first = false; }
                m.minB[c] = std::min(m.minB[c], x);
                m.maxB[c] = std::max(m.maxB[c], x);
            }
        const double ax = tri.v[3] - tri.v[0], ay = tri.v[4] - tri.v[1], az = tri.v[5] - tri.v[2];
        const double bx = tri.v[6] - tri.v[0], by = tri.v[7] - tri.v[1], bz = tri.v[8] - tri.v[2];
        const double cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
        if (cx * cx + cy * cy + cz * cz == 0.0) ++m.degenerate;
        m.triangles.push_back(tri);
    }
    if (!ended) throw FormatError(label + ": missing 'endsolid' (file truncated?).");
    if (m.triangles.empty()) throw FormatError(label + ": the solid contains no facets (empty geometry).");
    return m;
}

StlMesh parseStlFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw FormatError("Cannot open '" + path + "'. Check the path and permissions.");
    const uint64_t size = static_cast<uint64_t>(f.tellg());
    if (size == 0) throw FormatError("'" + path + "' is empty (0 bytes).");
    if (size > kMaxFileBytes)
        throw FormatError("'" + path + "' is " + std::to_string(size / (1024 * 1024)) +
                          " MB; the limit is " + std::to_string(kMaxFileBytes / (1024 * 1024)) + " MB.");
    f.seekg(0);
    char head[1024];
    f.read(head, sizeof head);
    const auto got = static_cast<size_t>(f.gcount());
    if (std::find(head, head + got, '\0') != head + got)
        throw FormatError("'" + path + "' looks like a BINARY STL. Only ASCII STL is supported; "
                          "re-export as ASCII.");
    f.clear();
    f.seekg(0);
    return parseStl(f, path);
}
}  // namespace wb
