#pragma once
#include <vector>
namespace wb {

// Uniform rod, constant properties, insulated lateral surface.
// Governing equation: -d/dx(kA dT/dx) = qA, T(0)=T0, T(L)=TL.
struct RodProblem {
    double L = 1.0;      // m
    double A = 1e-4;     // m^2
    double k = 10.0;     // W/(m K)
    double q = 1e4;      // W/m^3 volumetric heat generation
    double T0 = 100.0;   // C at x = 0
    double TL = 20.0;    // C at x = L
};

struct FemResult {
    std::vector<double> x;  // node coordinates, m  (size N+1)
    std::vector<double> T;  // nodal temperatures, C (size N+1)
};

struct ErrorNorms {
    double maxNodal;     // max |T_fe - T_exact| at nodes
    double maxMidpoint;  // max |T_fe(interp) - T_exact| at element midpoints
};

// Two-node linear elements, N uniform elements. Throws std::invalid_argument
// on invalid input (N < 1, non-positive/non-finite properties, ...).
FemResult solveRod(const RodProblem& p, int nElements);

// T_exact(x) = T0 + (TL-T0) x/L + q/(2k) x (L-x)   (valid for the given BCs)
double exactTemperature(const RodProblem& p, double x);

// Piecewise-linear FE interpolation at any x in [0, L].
double feInterpolate(const FemResult& r, double x);

ErrorNorms errorNorms(const RodProblem& p, const FemResult& r);

}  // namespace wb
