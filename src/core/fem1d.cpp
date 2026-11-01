#include "fem1d.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace wb {
namespace {
void require(bool ok, const char* msg) {
    if (!ok) throw std::invalid_argument(msg);
}
void validate(const RodProblem& p, int n) {
    require(n >= 1, "Number of elements N must be at least 1.");
    require(n <= 20000000, "Number of elements N exceeds the supported maximum (20,000,000).");
    require(std::isfinite(p.L) && p.L > 0, "Length L must be finite and > 0.");
    require(std::isfinite(p.A) && p.A > 0, "Area A must be finite and > 0.");
    require(std::isfinite(p.k) && p.k > 0, "Conductivity k must be finite and > 0.");
    require(std::isfinite(p.q), "Heat generation q must be finite.");
    require(std::isfinite(p.T0) && std::isfinite(p.TL), "Boundary temperatures must be finite.");
}
}  // namespace

FemResult solveRod(const RodProblem& p, int n) {
    validate(p, n);
    const double h = p.L / n;
    const double kAh = p.k * p.A / h;          // element stiffness factor
    const double fe = p.q * p.A * h / 2.0;     // consistent nodal load per node

    // Global tridiagonal assembly from element contributions
    //   Ke = kA/h [[1,-1],[-1,1]],   fe = qAh/2 [1,1]
    std::vector<double> diag(n + 1, 0.0), off(n, 0.0), F(n + 1, 0.0);
    for (int e = 0; e < n; ++e) {
        diag[e] += kAh;
        diag[e + 1] += kAh;
        off[e] -= kAh;           // K[e][e+1] = K[e+1][e]
        F[e] += fe;
        F[e + 1] += fe;
    }

    FemResult r;
    r.x.resize(n + 1);
    r.T.assign(n + 1, 0.0);
    for (int i = 0; i <= n; ++i) r.x[i] = h * i;
    r.x[n] = p.L;
    // Dirichlet BCs (elimination method): known values, move K*T_known to RHS.
    r.T[0] = p.T0;
    r.T[n] = p.TL;

    const int m = n - 1;  // interior unknowns
    if (m >= 1) {
        std::vector<double> a(m), rhs(m), up(m, 0.0), lo(m, 0.0);
        for (int j = 0; j < m; ++j) {
            const int i = j + 1;
            a[j] = diag[i];
            rhs[j] = F[i];
            if (j + 1 < m) up[j] = off[i];   // K[i][i+1]
            if (j > 0) lo[j] = off[i - 1];   // K[i][i-1]
        }
        rhs[0] -= off[0] * p.T0;             // K[1][0] * T(0)
        rhs[m - 1] -= off[n - 1] * p.TL;     // K[n-1][n] * T(L)
        // Thomas algorithm (K is SPD, so no pivoting needed)
        for (int j = 1; j < m; ++j) {
            const double w = lo[j] / a[j - 1];
            a[j] -= w * up[j - 1];
            rhs[j] -= w * rhs[j - 1];
        }
        rhs[m - 1] /= a[m - 1];
        for (int j = m - 2; j >= 0; --j) rhs[j] = (rhs[j] - up[j] * rhs[j + 1]) / a[j];
        for (int j = 0; j < m; ++j) r.T[j + 1] = rhs[j];
    }
    return r;
}

double exactTemperature(const RodProblem& p, double x) {
    return p.T0 + (p.TL - p.T0) * x / p.L + (p.q / (2.0 * p.k)) * x * (p.L - x);
}

double feInterpolate(const FemResult& r, double x) {
    if (r.x.size() < 2) throw std::invalid_argument("FE result has no elements.");
    x = std::min(std::max(x, r.x.front()), r.x.back());
    auto it = std::upper_bound(r.x.begin(), r.x.end(), x);
    size_t e = (it == r.x.begin()) ? 0 : static_cast<size_t>(it - r.x.begin()) - 1;
    if (e >= r.x.size() - 1) e = r.x.size() - 2;
    const double t = (x - r.x[e]) / (r.x[e + 1] - r.x[e]);
    return r.T[e] * (1.0 - t) + r.T[e + 1] * t;
}

ErrorNorms errorNorms(const RodProblem& p, const FemResult& r) {
    ErrorNorms n{0.0, 0.0};
    for (size_t i = 0; i < r.x.size(); ++i)
        n.maxNodal = std::max(n.maxNodal, std::fabs(r.T[i] - exactTemperature(p, r.x[i])));
    for (size_t e = 0; e + 1 < r.x.size(); ++e) {
        const double xm = 0.5 * (r.x[e] + r.x[e + 1]);
        n.maxMidpoint = std::max(n.maxMidpoint, std::fabs(feInterpolate(r, xm) - exactTemperature(p, xm)));
    }
    return n;
}
}  // namespace wb
