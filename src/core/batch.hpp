#pragma once
#include "fem1d.hpp"
#include <atomic>
#include <cstdint>
#include <vector>
namespace wb {

struct BatchCase { double k; double q; };

// Fixed-seed, platform-independent generator (raw mt19937_64 output mapped to
// doubles; no std::*_distribution, whose output differs between standard libs).
// k in [5, 20] W/(m K), q in [5e3, 2e4] W/m^3.
std::vector<BatchCase> generateCases(uint64_t seed, size_t count);

struct BatchControl {
    std::atomic<bool> cancel{false};   // set from UI thread to request stop
    std::atomic<size_t> done{0};       // progress counter
};

struct BatchResult {
    std::vector<std::vector<double>> temperatures;  // input order; empty entry = not computed
    size_t completed = 0;
    bool cancelled = false;
};

BatchResult runSerial(const std::vector<BatchCase>& cases, const RodProblem& base, int nElements,
                      BatchControl* ctl = nullptr);

// Bounded pool of `workers` std::threads pulling chunks from an atomic counter.
// Each case writes only its own output slot -> independent, deterministic order.
BatchResult runParallel(const std::vector<BatchCase>& cases, const RodProblem& base, int nElements,
                        unsigned workers, BatchControl* ctl = nullptr);

// Max |a-b| over all temperatures. Throws if shapes differ.
double maxAbsDifference(const BatchResult& a, const BatchResult& b);
}  // namespace wb
