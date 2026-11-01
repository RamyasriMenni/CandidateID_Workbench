// Thin command-line presentation layer over wb_core. All numerics live in the core.
#include "backprop.hpp"
#include "batch.hpp"
#include "devices.hpp"
#include "fem1d.hpp"
#include "nodal_csv.hpp"
#include "stl_parser.hpp"
#include "sysinfo.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <thread>

using namespace wb;
namespace {
using Args = std::map<std::string, std::string>;
Args parseArgs(int argc, char** argv, int from) {
    Args a;
    for (int i = from; i < argc; ++i)
        if (std::strncmp(argv[i], "--", 2) == 0 && i + 1 < argc) { a[argv[i] + 2] = argv[i + 1]; ++i; }
    return a;
}
std::string get(const Args& a, const char* k, const char* d) { auto i = a.find(k); return i == a.end() ? d : i->second; }
long long getInt(const Args& a, const char* k, long long d) {
    auto i = a.find(k);
    if (i == a.end()) return d;
    char* e = nullptr;
    const long long v = std::strtoll(i->second.c_str(), &e, 10);
    if (*e || i->second.empty()) throw std::invalid_argument(std::string("--") + k + " must be an integer.");
    return v;
}
std::ofstream openOut(const std::string& p) {
    std::ofstream f(p);
    if (!f) throw std::runtime_error("Cannot write '" + p + "'. Does the output folder exist?");
    f << std::setprecision(17);
    return f;
}

int cmdFea(const Args& a) {
    const std::string out = get(a, "out", "results");
    RodProblem p;
    auto conv = openOut(out + "/fea_convergence.csv");
    conv << "N,h_m,max_nodal_error_C,max_midpoint_error_C,observed_order_midpoint\n";
    double prev = 0;
    std::cout << "N    h[m]      max|err| nodes [C]   max|err| midpoints [C]   order\n";
    for (int n : {10, 20, 40, 80}) {
        FemResult r = solveRod(p, n);
        ErrorNorms e = errorNorms(p, r);
        const double order = prev > 0 ? std::log2(prev / e.maxMidpoint) : 0.0;
        conv << n << ',' << p.L / n << ',' << e.maxNodal << ',' << e.maxMidpoint << ',' << (prev > 0 ? order : NAN) << '\n';
        std::printf("%-4d %-9.5f %-20.3e %-23.6f %s\n", n, p.L / n, e.maxNodal, e.maxMidpoint,
                    prev > 0 ? std::to_string(order).c_str() : "-");
        prev = e.maxMidpoint;
        auto f = openOut(out + "/fea_solution_N" + std::to_string(n) + ".csv");
        f << "x_m,T_fe_C,T_exact_C,error_C\n";
        for (size_t i = 0; i < r.x.size(); ++i) {
            const double te = exactTemperature(p, r.x[i]);
            f << r.x[i] << ',' << r.T[i] << ',' << te << ',' << r.T[i] - te << '\n';
        }
        auto m = openOut(out + "/fea_midpoints_N" + std::to_string(n) + ".csv");
        m << "x_mid_m,T_fe_interp_C,T_exact_C,abs_error_C\n";
        for (size_t i = 0; i + 1 < r.x.size(); ++i) {
            const double xm = 0.5 * (r.x[i] + r.x[i + 1]);
            m << xm << ',' << feInterpolate(r, xm) << ',' << exactTemperature(p, xm) << ','
              << std::fabs(feInterpolate(r, xm) - exactTemperature(p, xm)) << '\n';
        }
    }
    // dense exact curve for plotting
    auto ex = openOut(out + "/fea_exact_curve.csv");
    ex << "x_m,T_exact_C\n";
    for (int i = 0; i <= 400; ++i) ex << p.L * i / 400.0 << ',' << exactTemperature(p, p.L * i / 400.0) << '\n';
    std::cout << "Wrote CSVs to " << out << "/\n";
    return 0;
}

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 ? v[v.size() / 2] : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
}

int cmdBatch(const Args& a) {
    const auto seed = static_cast<uint64_t>(getInt(a, "seed", 42));
    const auto cases = static_cast<size_t>(getInt(a, "cases", 1000));
    const int n = static_cast<int>(getInt(a, "elements", 2000));
    const auto w = static_cast<unsigned>(getInt(a, "workers", 2));
    if (cases < 1 || cases > 1000000) throw std::invalid_argument("--cases must be in [1, 1000000].");
    if (n < 1 || n > 1000000) throw std::invalid_argument("--elements must be in [1, 1000000].");
    if (w < 1 || w > 256) throw std::invalid_argument("--workers must be in [1, 256].");
    const auto cs = generateCases(seed, cases);
    RodProblem base;
    auto s = runSerial(cs, base, n);
    auto p = runParallel(cs, base, n, w);
    std::cout << "cases=" << cases << " N=" << n << " workers=" << w
              << " max|T_serial - T_parallel| = " << std::scientific << maxAbsDifference(s, p) << " C\n";
    return 0;
}

int cmdBench(const Args& a) {
    const std::string out = get(a, "out", "results");
    const auto seed = static_cast<uint64_t>(getInt(a, "seed", 42));
    const auto cases = static_cast<size_t>(getInt(a, "cases", 3000));
    const int n = static_cast<int>(getInt(a, "elements", 4000));
    const int reps = 5;
    const SystemInfo si = querySystemInfo();
    const auto cs = generateCases(seed, cases);
    RodProblem base;

    std::vector<unsigned> counts;
    for (unsigned p : {1u, 2u, 4u}) if (p <= si.logicalCpus) counts.push_back(p);
    bool demoOnly = false;
    if (si.logicalCpus <= 1) { counts = {1u, 2u}; demoOnly = true; }   // demonstrate parallel path

    using clk = std::chrono::steady_clock;
    auto timeIt = [&](auto&& fn) {
        fn();  // warm-up (not recorded)
        std::vector<double> t;
        for (int r = 0; r < reps; ++r) {
            auto t0 = clk::now();
            fn();
            t.push_back(std::chrono::duration<double>(clk::now() - t0).count());
        }
        return t;
    };
    BatchResult ref = runSerial(cs, base, n);
    const auto ts = timeIt([&] { volatile auto c = runSerial(cs, base, n).completed; (void)c; });
    const double T1 = median(ts);

    auto sf = openOut(out + "/system_info.txt");
    sf << "CPU: " << si.cpuModel << "\nLogical CPUs: " << si.logicalCpus << "\nPhysical cores: "
       << (si.physicalCores ? std::to_string(si.physicalCores) : "unknown (not exposed by OS/VM)")
       << "\nRAM (GiB): " << si.ramGiB << "\nOS: " << si.os << "\nCompiler: " << si.compiler
       << "\nOptimisation flags: " << si.buildFlags << "\nWorkload: " << cases << " cases, N=" << n
       << " elements each, seed=" << seed << ", k in [5,20], q in [5e3,2e4]\nProtocol: 1 warm-up + " << reps
       << " measured repetitions per configuration, median reported; file I/O/rendering/compilation excluded.\n";
    if (demoOnly) sf << "NOTE: single logical CPU - parallel path demonstrated with 2 workers; speedup cannot be assessed.\n";

    auto f = openOut(out + "/performance.csv");
    f << "version,workers,rep1_s,rep2_s,rep3_s,rep4_s,rep5_s,median_s,speedup_T1_over_Tp,efficiency,max_abs_diff_vs_serial_C\n";
    f << "serial,1";
    for (double t : ts) f << ',' << t;
    f << ',' << T1 << ",1,1,0\n";
    std::printf("serial            median %.4f s\n", T1);
    for (unsigned p : counts) {
        const auto tp = timeIt([&] { volatile auto c = runParallel(cs, base, n, p).completed; (void)c; });
        const double Tp = median(tp);
        const double diff = maxAbsDifference(ref, runParallel(cs, base, n, p));
        f << "parallel," << p;
        for (double t : tp) f << ',' << t;
        f << ',' << Tp << ',' << T1 / Tp << ',' << (T1 / Tp) / p << ',' << diff << '\n';
        std::printf("parallel p=%u      median %.4f s  speedup %.3f  efficiency %.3f  max diff %.2e C%s\n", p, Tp,
                    T1 / Tp, (T1 / Tp) / p, diff, diff <= 1e-8 ? "" : "  **FAILS 1e-8 CHECK**");
    }
    if (demoOnly) std::cout << "Single logical CPU: speedup cannot be assessed (parallel path demonstrated only).\n";
    return 0;
}

int cmdBackprop(const Args& a) {
    const std::string out = get(a, "out", "results");
    BpParams p{0.5, 0.0, 1.0, 0.0};
    const double x = 1, y = 2, lr = 0.1;
    BpForward f = bpForward(p, x, y);
    BpGrads g = bpGradients(p, x, y), gn = bpNumericGradients(p, x, y, 1e-6);
    BpParams p1 = bpStep(p, g, lr);
    std::cout << std::setprecision(10);
    std::cout << "z=" << f.z << " h=" << f.h << " y_hat=" << f.yhat << " J=" << f.J << "\n";
    const char* nm[4] = {"w", "b", "v", "c"};
    const double an[4] = {g.dw, g.db, g.dv, g.dc}, nu[4] = {gn.dw, gn.db, gn.dv, gn.dc};
    auto gc = openOut(out + "/backprop_gradcheck.csv");
    gc << "param,analytic,numeric_central_eps1e-6,abs_error,pass_le_1e-5\n";
    bool all = true;
    for (int i = 0; i < 4; ++i) {
        const double e = std::fabs(an[i] - nu[i]);
        all = all && e <= 1e-5;
        gc << nm[i] << ',' << an[i] << ',' << nu[i] << ',' << e << ',' << (e <= 1e-5) << '\n';
        std::cout << "dJ/d" << nm[i] << " analytic=" << an[i] << " numeric=" << nu[i] << " |err|=" << e << "\n";
    }
    std::cout << "After one step: w=" << p1.w << " b=" << p1.b << " v=" << p1.v << " c=" << p1.c
              << "  J=" << bpForward(p1, x, y).J << "\nGradient check " << (all ? "PASSED" : "FAILED") << "\n";
    auto loss = bpTrain(p1, x, y, lr, 100);   // 100 steps after the first update
    auto lf = openOut(out + "/backprop_loss.csv");
    lf << "step,loss\n";
    lf << "0," << bpForward(p, x, y).J << "\n";
    for (size_t i = 0; i < loss.size(); ++i) lf << i + 1 << ',' << loss[i] << '\n';
    std::cout << "Loss after update 1: " << loss.front() << "  after 101 updates total: " << loss.back() << "\n";
    return all ? 0 : 1;
}

int cmdStl(const std::string& path) {
    StlMesh m = parseStlFile(path);
    std::cout << "STL '" << m.name << "': " << m.triangles.size() << " triangles, " << m.degenerate
              << " degenerate\nbounds min(" << m.minB[0] << ", " << m.minB[1] << ", " << m.minB[2] << ") max("
              << m.maxB[0] << ", " << m.maxB[1] << ", " << m.maxB[2] << ")  [file units; units are not stored in STL]\n";
    return 0;
}
int cmdCsv(const std::string& path) {
    NodalField f = parseNodalCsvFile(path);
    std::cout << "CSV: " << f.points.size() << " nodes, T range [" << f.minT << ", " << f.maxT << "] C\nbounds min("
              << f.minB[0] << ", " << f.minB[1] << ", " << f.minB[2] << ") max(" << f.maxB[0] << ", " << f.maxB[1]
              << ", " << f.maxB[2] << ") m\n";
    return 0;
}

int cmdDevices() {
    std::cout << "=== SIMULATED DEVICES (no hardware is used) ===\n";
    auto devs = discoverDevices(true);
    for (auto& d : devs) {
        const auto& i = d->info();
        std::cout << i.id << " | " << kindName(i.kind) << " | " << i.name << " | " << i.voltageDomain << " | " << i.hostLink << "\n  capabilities:";
        for (auto& c : i.capabilities) std::cout << ' ' << c << ';';
        std::cout << "\n";
    }
    for (auto& d : devs) {
        std::vector<std::string> log;
        Result r = connectWithRetry(*d, 3, &log);
        for (auto& l : log) std::cout << "  [" << d->info().id << "] " << l << "\n";
        if (!r.ok()) continue;
        Telemetry t;
        d->readTelemetry(t);
        std::cout << "  telemetry (simulated) #" << t.sequence << ":";
        for (auto& v : t.values) std::cout << ' ' << v.name << '=' << v.value << ' ' << v.unit << ';';
        std::cout << "\n";
        d->disconnect();
    }
    return 0;
}

void usage() {
    std::cout << "wb_cli <command>\n  fea [--out DIR]\n  batch [--seed N --cases N --elements N --workers N]\n"
                 "  bench [--out DIR --seed N --cases N --elements N]\n  backprop [--out DIR]\n"
                 "  stl FILE | csv FILE | devices\n";
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    const std::string c = argv[1];
    try {
        const Args a = parseArgs(argc, argv, 2);
        if (c == "fea") return cmdFea(a);
        if (c == "batch") return cmdBatch(a);
        if (c == "bench") return cmdBench(a);
        if (c == "backprop") return cmdBackprop(a);
        if (c == "devices") return cmdDevices();
        if ((c == "stl" || c == "csv") && argc >= 3) return c == "stl" ? cmdStl(argv[2]) : cmdCsv(argv[2]);
        usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}
