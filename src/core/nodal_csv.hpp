#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>
namespace wb {
struct NodalPoint { int64_t id; double x, y, z, T; };   // metres, degrees C
struct NodalField {
    std::vector<NodalPoint> points;
    double minT = 0, maxT = 0;
    double minB[3]{0, 0, 0}, maxB[3]{0, 0, 0};
};
// Required header: node_id,x_m,y_m,z_m,temperature_C
NodalField parseNodalCsv(std::istream& in, const std::string& label = "CSV");
NodalField parseNodalCsvFile(const std::string& path);
}  // namespace wb
