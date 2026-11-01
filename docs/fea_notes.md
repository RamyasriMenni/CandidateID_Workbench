# Task C - Thermal FEA notes
**Model.** -d/dx(kA dT/dx) = qA on 0<x<L, T(0)=100 C, T(L)=20 C. Assumptions: steady state, 1-D conduction, constant k, A, q, insulated lateral surface, no convection/radiation.
**Units.** L [m], A [m^2], k [W/(m K)], q [W/m^3] -> qA is W/m; T [C] (temperature *differences* make C and K equivalent here).
**Elements.** Two-node linear: K_e = (kA/h)[[1,-1],[-1,1]], f_e = (qAh/2)[1,1]. Assembly into a tridiagonal global system (`fem1d.cpp: solveRod`).
**Boundary conditions.** Dirichlet by elimination: unknowns are interior nodes only; K(i,0)T(0) and K(i,N)T(L) are moved to the RHS (node 1 and node N-1; both land on the same row when N=2 - tested for N=2,3).
**Solution.** Thomas algorithm (K is symmetric positive definite -> stable without pivoting).
**Verification.** T_exact = 100 - 80x/L + q/(2k) x(L-x). Errors in `results/fea_convergence.csv`:

| N | max nodal error [C] | max midpoint error (FE interpolation) [C] | observed order |
|---|---|---|---|
| 10 | 5.7e-14 | 1.250000 | - |
| 20 | 2.6e-13 | 0.312500 | 2.00 |
| 40 | 7.7e-13 | 0.078125 | 2.00 |
| 80 | 1.1e-12 | 0.019531 | 2.00 |

Midpoint error equals q h^2/(8k) exactly (tests assert this to 1e-9).
**Debugging insight (real).** Checking only nodal values gives ~1e-13 "perfect" results, which looks like a bug-free but uninformative check: for 1-D constant-coefficient problems linear elements are *nodally exact*, so
the whole O(h^2) error lives *between* nodes. Only the midpoint error with FE interpolation exposes it and shows convergence - which is why the brief insists on it.
