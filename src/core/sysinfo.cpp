#include "sysinfo.hpp"
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#endif
#ifndef WB_BUILD_FLAGS
#define WB_BUILD_FLAGS "unknown"
#endif
namespace wb {
SystemInfo querySystemInfo() {
    SystemInfo s;
    s.logicalCpus = std::thread::hardware_concurrency();
    s.buildFlags = WB_BUILD_FLAGS;
#if defined(_MSC_VER)
    s.compiler = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    s.compiler = std::string("Clang ") + __clang_version__;
#elif defined(__GNUC__)
    s.compiler = std::string("GCC ") + __VERSION__;
#endif
#ifdef _WIN32
    s.os = "Windows";
    if (const char* c = std::getenv("PROCESSOR_IDENTIFIER")) s.cpuModel = c;
    DWORD len = 0;
    GetLogicalProcessorInformation(nullptr, &len);
    if (len) {
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(buf.data(), &len))
            for (auto& b : buf) if (b.Relationship == RelationProcessorCore) ++s.physicalCores;
    }
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) s.ramGiB = static_cast<double>(ms.ullTotalPhys) / (1024.0 * 1024 * 1024);
#else
    s.os = "Linux";
    std::ifstream ci("/proc/cpuinfo");
    std::string line;
    std::set<std::pair<std::string, std::string>> cores;
    std::string phys = "0";
    while (std::getline(ci, line)) {
        const auto p = line.find(':');
        if (p == std::string::npos) continue;
        std::string key = line.substr(0, p), val = line.substr(p + 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        if (!val.empty() && val[0] == ' ') val.erase(0, 1);
        if (key == "model name" && s.cpuModel.empty()) s.cpuModel = val;
        else if (key == "physical id") phys = val;
        else if (key == "core id") cores.insert({phys, val});
    }
    s.physicalCores = static_cast<unsigned>(cores.size());   // 0 when the kernel/VM hides core ids
    std::ifstream mi("/proc/meminfo");
    while (std::getline(mi, line))
        if (line.rfind("MemTotal:", 0) == 0) { s.ramGiB = std::atof(line.c_str() + 9) / (1024.0 * 1024.0); break; }
#endif
    return s;
}
}  // namespace wb
