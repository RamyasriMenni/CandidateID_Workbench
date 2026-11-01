#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace wb {
struct Triangle { std::array<double, 9> v; };   // x0 y0 z0 x1 y1 z1 x2 y2 z2 (file units)
struct StlMesh {
    std::string name;
    std::vector<Triangle> triangles;
    double minB[3]{0, 0, 0}, maxB[3]{0, 0, 0};
    size_t degenerate = 0;   // zero-area triangles (kept, reported)
};
constexpr uint64_t kMaxFileBytes = 256ull * 1024 * 1024;

// ASCII STL only. Throws FormatError with a line number on any problem.
StlMesh parseStl(std::istream& in, const std::string& label = "STL");
StlMesh parseStlFile(const std::string& path);
}  // namespace wb
