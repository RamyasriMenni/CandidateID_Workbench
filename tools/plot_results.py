#!/usr/bin/env python3
"""Evidence plots from the CSVs written by wb_cli (matplotlib; not application backend)."""
import csv, os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
R = os.path.join(os.path.dirname(__file__), "..", "results")
def rd(n): return list(csv.DictReader(open(os.path.join(R, n))))
ex = rd("fea_exact_curve.csv")
fig, ax = plt.subplots(1, 2, figsize=(12, 4.6))
ax[0].plot([float(r["x_m"]) for r in ex], [float(r["T_exact_C"]) for r in ex], "k-", lw=2, label="Exact")
for n, m in zip((10, 20, 40, 80), ("o", "s", "^", "v")):
    d = rd(f"fea_solution_N{n}.csv")
    ax[0].plot([float(r["x_m"]) for r in d], [float(r["T_fe_C"]) for r in d], m + "--", ms=4, lw=1, label=f"FE N={n}")
ax[0].set(xlabel="x [m]", ylabel="Temperature [°C]", title="Steady-state rod temperature: FE vs exact")
ax[0].grid(alpha=.3); ax[0].legend()
c = rd("fea_convergence.csv")
h = [float(r["h_m"]) for r in c]; em = [float(r["max_midpoint_error_C"]) for r in c]
ax[1].loglog(h, em, "o-", label="max midpoint error (FE interpolation)")
ax[1].loglog(h, [em[0] * (x / h[0]) ** 2 for x in h], "k:", label="slope 2 reference")
ax[1].set(xlabel="element size h [m]", ylabel="max |T_fe − T_exact| at midpoints [°C]", title="Mesh convergence")
ax[1].grid(alpha=.3, which="both"); ax[1].legend()
fig.suptitle("Task C - 1-D thermal FEA, L=1 m, A=1e-4 m², k=10 W/(m K), q=1e4 W/m³, T(0)=100 °C, T(L)=20 °C", fontsize=9)
fig.tight_layout(); fig.savefig(os.path.join(R, "fea_temperature_and_convergence.png"), dpi=150)
l = rd("backprop_loss.csv")
fig, ax = plt.subplots(figsize=(6.5, 4.2))
ax.semilogy([int(r["step"]) for r in l], [float(r["loss"]) for r in l], "-o", ms=3)
ax.set(xlabel="gradient-descent update (0 = initial parameters)", ylabel="loss J (log scale)", title="Task F - loss vs training step (lr = 0.1)")
ax.grid(alpha=.3, which="both"); fig.tight_layout(); fig.savefig(os.path.join(R, "backprop_loss.png"), dpi=150)
print("plots written")
