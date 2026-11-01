#pragma once
#include <string>
namespace wb {
struct SystemInfo {
    std::string cpuModel, compiler, buildFlags, os;
    unsigned logicalCpus = 0, physicalCores = 0;   // 0 = unknown
    double ramGiB = 0;
};
SystemInfo querySystemInfo();
}  // namespace wb
