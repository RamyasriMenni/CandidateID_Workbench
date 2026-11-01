#include "batch.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>

namespace wb {

std::vector<BatchCase> generateCases(uint64_t seed, size_t count) {
    std::mt19937_64 rng(seed);
    auto u01 = [&rng] { return static_cast<double>(rng() >> 11) * (1.0 / 9007199254740992.0); };
    std::vector<BatchCase> c(count);
    for (auto& x : c) {
        x.k = 5.0 + 15.0 * u01();
        x.q = 5.0e3 + 1.5e4 * u01();
    }
    return c;
}

static std::vector<double> solveOne(const BatchCase& c, RodProblem base, int n) {
    base.k = c.k;
    base.q = c.q;
    return solveRod(base, n).T;
}

BatchResult runSerial(const std::vector<BatchCase>& cases, const RodProblem& base, int n, BatchControl* ctl) {
    BatchResult r;
    r.temperatures.resize(cases.size());
    for (size_t i = 0; i < cases.size(); ++i) {
        if (ctl && ctl->cancel.load()) { r.cancelled = true; break; }
        r.temperatures[i] = solveOne(cases[i], base, n);
        ++r.completed;
        if (ctl) ctl->done.store(r.completed);
    }
    return r;
}

BatchResult runParallel(const std::vector<BatchCase>& cases, const RodProblem& base, int n,
                        unsigned workers, BatchControl* ctl) {
    if (workers < 1) workers = 1;
    workers = static_cast<unsigned>(std::min<size_t>(workers, std::max<size_t>(cases.size(), 1)));
    BatchResult r;
    r.temperatures.resize(cases.size());
    std::atomic<size_t> next{0}, completed{0};
    std::atomic<bool> stop{false};
    std::exception_ptr firstError;
    std::mutex errMutex;
    constexpr size_t kChunk = 16;

    auto work = [&] {
        for (;;) {
            if (stop.load() || (ctl && ctl->cancel.load())) return;
            const size_t begin = next.fetch_add(kChunk);
            if (begin >= cases.size()) return;
            const size_t end = std::min(begin + kChunk, cases.size());
            try {
                for (size_t i = begin; i < end; ++i) {
                    if (ctl && ctl->cancel.load()) return;
                    r.temperatures[i] = solveOne(cases[i], base, n);   // own slot only
                    const size_t d = ++completed;
                    if (ctl) ctl->done.store(d);
                }
            } catch (...) {
                std::lock_guard<std::mutex> lock(errMutex);
                if (!firstError) firstError = std::current_exception();
                stop.store(true);
                return;
            }
        }
    };

    std::vector<std::thread> pool;
    pool.reserve(workers);
    for (unsigned t = 0; t < workers; ++t) pool.emplace_back(work);
    for (auto& t : pool) t.join();
    if (firstError) std::rethrow_exception(firstError);
    r.completed = completed.load();
    r.cancelled = ctl && ctl->cancel.load() && r.completed < cases.size();
    return r;
}

double maxAbsDifference(const BatchResult& a, const BatchResult& b) {
    if (a.temperatures.size() != b.temperatures.size())
        throw std::invalid_argument("Batch results have different case counts.");
    double m = 0.0;
    for (size_t i = 0; i < a.temperatures.size(); ++i) {
        if (a.temperatures[i].size() != b.temperatures[i].size())
            throw std::invalid_argument("Batch results have different mesh sizes.");
        for (size_t j = 0; j < a.temperatures[i].size(); ++j)
            m = std::max(m, std::fabs(a.temperatures[i][j] - b.temperatures[i][j]));
    }
    return m;
}
}  // namespace wb
