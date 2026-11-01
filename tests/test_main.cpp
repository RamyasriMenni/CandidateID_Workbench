// Minimal self-contained test harness (no external dependencies).
#include "backprop.hpp"
#include "batch.hpp"
#include "devices.hpp"
#include "fem1d.hpp"
#include "nodal_csv.hpp"
#include "stl_parser.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
using namespace wb;

static int g_fail = 0, g_run = 0;
#define CHECK(cond) do { if (!(cond)) { ++g_fail; std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a=(a), _b=(b); if (!(std::fabs(_a-_b) <= (tol))) { ++g_fail; std::printf("    FAIL %s:%d  |%g - %g| > %g\n", __FILE__, __LINE__, _a, _b, (double)(tol)); } } while (0)
#define CHECK_THROWS(expr, T) do { bool _t=false; try { expr; } catch (const T&) { _t=true; } catch (...) {} if(!_t){ ++g_fail; std::printf("    FAIL %s:%d  expected %s from %s\n", __FILE__, __LINE__, #T, #expr);} } while (0)
static std::vector<std::pair<std::string, std::function<void()>>>& reg() { static std::vector<std::pair<std::string, std::function<void()>>> r; return r; }
struct Reg { Reg(const char* n, std::function<void()> f) { reg().push_back({n, f}); } };
#define TEST(name) static void name(); static Reg r_##name(#name, name); static void name()

static bool throwsMsg(const std::function<void()>& f, const std::string& needle) {
    try { f(); } catch (const std::exception& e) { return std::string(e.what()).find(needle) != std::string::npos; }
    return false;
}
static StlMesh stl(const std::string& s) { std::istringstream is(s); return parseStl(is, "t"); }
static NodalField csv(const std::string& s) { std::istringstream is(s); return parseNodalCsv(is, "t"); }
static const char* kTri =
    "solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n  endloop\n endfacet\nendsolid t\n";

// ---------------- Thermal FEA ----------------
TEST(fea_boundary_conditions_exact) {
    RodProblem p;
    for (int n : {1, 2, 3, 10, 80}) { auto r = solveRod(p, n); CHECK_NEAR(r.T.front(), 100.0, 0); CHECK_NEAR(r.T.back(), 20.0, 0); CHECK((int)r.T.size() == n + 1); }
}
TEST(fea_nodal_exactness_1d_linear_elements) {
    RodProblem p;
    for (int n : {10, 20, 40, 80}) CHECK(errorNorms(p, solveRod(p, n)).maxNodal < 1e-10);
}
TEST(fea_midpoint_error_matches_theory_q_h2_over_8k) {
    RodProblem p;
    for (int n : {10, 20, 40, 80}) {
        const double h = p.L / n;
        CHECK_NEAR(errorNorms(p, solveRod(p, n)).maxMidpoint, p.q * h * h / (8 * p.k), 1e-9);
    }
    CHECK_NEAR(errorNorms(p, solveRod(p, 10)).maxMidpoint, 1.25, 1e-9);
}
TEST(fea_second_order_convergence) {
    RodProblem p;
    double prev = errorNorms(p, solveRod(p, 10)).maxMidpoint;
    for (int n : {20, 40, 80}) { double e = errorNorms(p, solveRod(p, n)).maxMidpoint; CHECK_NEAR(prev / e, 4.0, 1e-6); prev = e; }
}
TEST(fea_no_generation_gives_linear_profile) {
    RodProblem p; p.q = 0;
    auto r = solveRod(p, 7);
    for (size_t i = 0; i < r.x.size(); ++i) CHECK_NEAR(r.T[i], 100 - 80 * r.x[i], 1e-10);
}
TEST(fea_other_properties_and_asymmetric_bcs) {
    RodProblem p; p.k = 17.3; p.q = -2500; p.T0 = -5; p.TL = 60; p.L = 2.5; p.A = 3e-3;
    CHECK(errorNorms(p, solveRod(p, 13)).maxNodal < 1e-9);
}
TEST(fea_interpolation_between_nodes) {
    RodProblem p; auto r = solveRod(p, 4);
    CHECK_NEAR(feInterpolate(r, 0.125), 0.5 * (r.T[0] + r.T[1]), 1e-12);
    CHECK_NEAR(feInterpolate(r, -1), r.T.front(), 0); CHECK_NEAR(feInterpolate(r, 9), r.T.back(), 0);
}
TEST(fea_rejects_invalid_input) {
    RodProblem p;
    CHECK_THROWS(solveRod(p, 0), std::invalid_argument);
    CHECK_THROWS(solveRod(p, -4), std::invalid_argument);
    RodProblem b = p; b.k = 0; CHECK_THROWS(solveRod(b, 10), std::invalid_argument);
    b = p; b.k = -1; CHECK_THROWS(solveRod(b, 10), std::invalid_argument);
    b = p; b.L = NAN; CHECK_THROWS(solveRod(b, 10), std::invalid_argument);
    b = p; b.q = INFINITY; CHECK_THROWS(solveRod(b, 10), std::invalid_argument);
    b = p; b.A = 0; CHECK_THROWS(solveRod(b, 10), std::invalid_argument);
    CHECK(throwsMsg([&] { solveRod(p, 0); }, "at least 1"));
}

// ---------------- Batch / parallel ----------------
TEST(batch_generator_is_reproducible_and_positive) {
    auto a = generateCases(42, 500), b = generateCases(42, 500), c = generateCases(43, 500);
    bool same = true, differs = false;
    for (size_t i = 0; i < a.size(); ++i) {
        same = same && a[i].k == b[i].k && a[i].q == b[i].q;
        differs = differs || a[i].k != c[i].k;
        CHECK(a[i].k >= 5 && a[i].k <= 20 && a[i].q >= 5e3 && a[i].q <= 2e4);
    }
    CHECK(same); CHECK(differs);
}
TEST(batch_parallel_matches_serial_within_1e8) {
    auto cs = generateCases(7, 200); RodProblem base;
    auto s = runSerial(cs, base, 100);
    for (unsigned w : {1u, 2u, 3u, 4u, 8u}) {
        auto p = runParallel(cs, base, 100, w);
        CHECK(p.completed == cs.size()); CHECK(!p.cancelled);
        CHECK(maxAbsDifference(s, p) <= 1e-8);
    }
}
TEST(batch_results_in_input_order) {
    auto cs = generateCases(9, 64); RodProblem base;
    auto p = runParallel(cs, base, 20, 4);
    for (size_t i = 0; i < cs.size(); ++i) { RodProblem q = base; q.k = cs[i].k; q.q = cs[i].q; CHECK(p.temperatures[i] == solveRod(q, 20).T); }
}
TEST(batch_cancellation_is_safe) {
    auto cs = generateCases(1, 2000); RodProblem base;
    BatchControl ctl; ctl.cancel = true;
    auto r = runParallel(cs, base, 200, 4, &ctl);
    CHECK(r.cancelled); CHECK(r.completed < cs.size());
    for (auto& t : r.temperatures) CHECK(t.empty() || t.size() == 201);   // never half-written
}
TEST(batch_edge_cases) {
    RodProblem base;
    auto e = runParallel({}, base, 10, 4); CHECK(e.completed == 0);
    auto one = runParallel(generateCases(1, 1), base, 10, 64); CHECK(one.completed == 1);
    CHECK_THROWS(runParallel(generateCases(1, 5), base, 0, 2), std::invalid_argument);   // error propagates from worker
}

// ---------------- Backprop ----------------
TEST(backprop_known_values) {
    BpParams p{0.5, 0, 1, 0};
    auto f = bpForward(p, 1, 2);
    CHECK_NEAR(f.h, std::tanh(0.5), 1e-15); CHECK_NEAR(f.J, 0.5 * (std::tanh(0.5) - 2) * (std::tanh(0.5) - 2), 1e-15);
}
TEST(backprop_gradient_check_central_differences) {
    BpParams p{0.5, 0, 1, 0};
    auto a = bpGradients(p, 1, 2), n = bpNumericGradients(p, 1, 2, 1e-6);
    CHECK_NEAR(a.dw, n.dw, 1e-5); CHECK_NEAR(a.db, n.db, 1e-5); CHECK_NEAR(a.dv, n.dv, 1e-5); CHECK_NEAR(a.dc, n.dc, 1e-5);
}
TEST(backprop_update_is_simultaneous_and_training_decreases_loss) {
    BpParams p{0.5, 0, 1, 0};
    auto g = bpGradients(p, 1, 2); auto p1 = bpStep(p, g, 0.1);
    CHECK_NEAR(p1.w, 0.5 - 0.1 * g.dw, 1e-15); CHECK_NEAR(p1.v, 1 - 0.1 * g.dv, 1e-15);
    auto L = bpTrain(p, 1, 2, 0.1, 100);
    CHECK(L.size() == 101); CHECK(L.back() < L.front());
    for (size_t i = 1; i < L.size(); ++i) CHECK(L[i] <= L[i - 1] + 1e-15);
}

// ---------------- STL ----------------
TEST(stl_valid_file) {
    auto m = stl(kTri); CHECK(m.triangles.size() == 1); CHECK(m.maxB[0] == 1 && m.maxB[1] == 1); CHECK(m.name == "t"); CHECK(m.degenerate == 0);
}
TEST(stl_rejects_malformed) {
    CHECK(throwsMsg([] { stl(""); }, "empty"));
    CHECK(throwsMsg([] { stl("  \n\n"); }, "empty"));
    CHECK(throwsMsg([] { stl("hello\n"); }, "solid"));
    CHECK(throwsMsg([] { stl("solid t\nendsolid t\n"); }, "no facets"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n"); }, "end of file"));
    CHECK(throwsMsg([] { stl(std::string(kTri).substr(0, std::string(kTri).find("endsolid"))); }, "endsolid"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 nan\n"); }, "non-finite"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 inf\n"); }, "non-finite"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 abc\n"); }, "not a number"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0\n"); }, "vertex x y z"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 0 1 0\n   vertex 5 5 5\n  endloop\n"); }, "endloop"));
    CHECK(throwsMsg([] { stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n"); }, "end of file"));
    CHECK(throwsMsg([] { stl("solid t\n nonsense\n"); }, "line 2"));   // line number reported
}
TEST(stl_file_level_errors) {
    CHECK(throwsMsg([] { parseStlFile("does/not/exist.stl"); }, "Cannot open"));
    { std::ofstream f("tmp_empty.stl", std::ios::binary); }
    CHECK(throwsMsg([] { parseStlFile("tmp_empty.stl"); }, "0 bytes"));
    { std::ofstream f("tmp_bin.stl", std::ios::binary); f << "solid binary header"; for (int i = 0; i < 200; ++i) f.put('\0'); }
    CHECK(throwsMsg([] { parseStlFile("tmp_bin.stl"); }, "BINARY"));
    std::remove("tmp_empty.stl"); std::remove("tmp_bin.stl");
}
TEST(stl_degenerate_triangle_counted) {
    auto m = stl("solid t\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n   vertex 2 0 0\n  endloop\n endfacet\nendsolid t\n");
    CHECK(m.degenerate == 1);
}
TEST(stl_shipped_example_parses) { auto m = parseStlFile("data/example_bracket.stl"); CHECK(m.triangles.size() > 10); }

// ---------------- Nodal CSV ----------------
static const char* kHdr = "node_id,x_m,y_m,z_m,temperature_C\n";
TEST(csv_valid_file_with_bom_and_crlf) {
    auto f = csv(std::string("\xEF\xBB\xBF") + "node_id,x_m,y_m,z_m,temperature_C\r\n1,0,0,0,100\r\n2,1,0,0,20.5\r\n\r\n");
    CHECK(f.points.size() == 2); CHECK_NEAR(f.minT, 20.5, 0); CHECK_NEAR(f.maxT, 100, 0); CHECK_NEAR(f.maxB[0], 1, 0);
}
TEST(csv_rejects_malformed) {
    const std::string H = kHdr;
    CHECK(throwsMsg([] { csv(""); }, "empty"));
    CHECK(throwsMsg([&] { csv(H); }, "no data rows"));
    CHECK(throwsMsg([] { csv("id,x,y,z,t\n1,0,0,0,1\n"); }, "header"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,0\n"); }, "5"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,0,nan\n"); }, "non-finite"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,inf,5\n"); }, "non-finite"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,0,hot\n"); }, "not a number"));
    CHECK(throwsMsg([&] { csv(H + "x,0,0,0,5\n"); }, "node_id"));
    CHECK(throwsMsg([&] { csv(H + "-3,0,0,0,5\n"); }, "node_id"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,0,5\n1,1,1,1,6\n"); }, "duplicate"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,0,-300\n"); }, "absolute zero"));
    CHECK(throwsMsg([&] { csv(H + "1,0,0,0,5\n2,0,0,0,bad\n"); }, "line 3"));
    CHECK(throwsMsg([] { parseNodalCsvFile("nope.csv"); }, "Cannot open"));
}
TEST(csv_shipped_example_parses) { auto f = parseNodalCsvFile("data/example_synthetic_nodal_temperature.csv"); CHECK(f.points.size() == 369); CHECK_NEAR(f.minT, 17.0, 1e-6); CHECK(f.maxT > 188.0 && f.maxT < 188.3); }

// ---------------- Mock devices ----------------
TEST(devices_discovery_and_capabilities) {
    auto d = discoverDevices(false);
    CHECK(d.size() == 3);
    CHECK(d[0]->info().kind == DeviceKind::Microcontroller); CHECK(d[1]->info().kind == DeviceKind::Fpga); CHECK(d[2]->info().kind == DeviceKind::JtagAdapter);
    for (auto& x : d) { CHECK(x->info().simulated); CHECK(!x->info().capabilities.empty()); }
}
TEST(devices_connect_telemetry_disconnect_lifecycle) {
    for (auto& x : discoverDevices(false)) {
        Telemetry t;
        CHECK(x->readTelemetry(t).status == Status::NotConnected);
        CHECK(x->connect().ok()); CHECK(x->connect().status == Status::AlreadyConnected);
        CHECK(x->readTelemetry(t).ok()); CHECK(t.simulated); CHECK(!t.values.empty()); CHECK(t.sequence == 1);
        CHECK(x->readTelemetry(t).ok()); CHECK(t.sequence == 2);
        CHECK(x->disconnect().ok()); CHECK(x->disconnect().status == Status::NotConnected);
    }
}
TEST(devices_timeout_is_recoverable) {
    auto d = discoverDevices(true);
    CHECK(d[1]->connect().status == Status::Timeout); CHECK(!d[1]->connected());
    CHECK(d[1]->connect().ok());
    auto e = discoverDevices(true); std::vector<std::string> log;
    CHECK(connectWithRetry(*e[1], 3, &log).ok()); CHECK(log.size() == 2); CHECK(e[1]->connected());
    auto f = discoverDevices(true); f[1]->injectConnectTimeouts(5);
    CHECK(connectWithRetry(*f[1], 2, nullptr).status == Status::Timeout);   // gives up cleanly
}

int main() {
    for (auto& t : reg()) {
        const int before = g_fail;
        ++g_run;
        t.second();
        std::printf("[%s] %s\n", g_fail == before ? " OK " : "FAIL", t.first.c_str());
    }
    std::printf("\n%d tests, %d failed assertions\n", g_run, g_fail);
    return g_fail ? 1 : 0;
}
