#!/usr/bin/env python3
"""Generates the self-created example data in data/. Data-creation helper only
(not part of the application backend). Output is SYNTHETIC, not an ANSYS export."""
import math, os
out = os.path.join(os.path.dirname(__file__), "..", "data")
R = math.sqrt(1e-4 / math.pi)   # radius giving A = 1e-4 m^2 (same rod as Task C)
L, SEG = 1.0, 32

# ---- ASCII STL: cylinder along +x, units = metres ----
def pt(x, a): return (x, R * math.cos(a), R * math.sin(a))
def sub(a, b): return tuple(i - j for i, j in zip(a, b))
def cross(a, b): return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def norm(n):
    m = math.sqrt(sum(c*c for c in n)) or 1.0
    return tuple(c/m for c in n)
tris = []
for i in range(SEG):
    a0, a1 = 2*math.pi*i/SEG, 2*math.pi*(i+1)/SEG
    p00, p01, p10, p11 = pt(0, a0), pt(0, a1), pt(L, a0), pt(L, a1)
    tris += [(p00, p10, p11), (p00, p11, p01)]            # side (outward)
    tris += [((0, 0, 0), p01, p00)]                         # cap x=0 (normal -x)
    tris += [((L, 0, 0), p10, p11)]                         # cap x=L (normal +x)
with open(os.path.join(out, "example_bracket.stl"), "w") as f:
    f.write("solid example_rod_synthetic\n")
    for a, b, c in tris:
        n = norm(cross(sub(b, a), sub(c, a)))
        f.write(f"  facet normal {n[0]:.6e} {n[1]:.6e} {n[2]:.6e}\n    outer loop\n")
        for v in (a, b, c): f.write(f"      vertex {v[0]:.6e} {v[1]:.6e} {v[2]:.6e}\n")
        f.write("    endloop\n  endfacet\n")
    f.write("endsolid example_rod_synthetic\n")

# ---- Synthetic nodal CSV: T(x) from the analytical rod solution + small radial term ----
def T(x, r): return 100 - 80*x/L + (10000/(2*10))*x*(L-x) - 3.0*(r/R)**2   # synthetic radial dip
with open(os.path.join(out, "example_synthetic_nodal_temperature.csv"), "w") as f:
    f.write("node_id,x_m,y_m,z_m,temperature_C\n")
    nid = 1
    for i in range(41):
        x = L*i/40
        f.write(f"{nid},{x:.6f},0.000000,0.000000,{T(x,0):.6f}\n"); nid += 1
        for j in range(8):
            a = 2*math.pi*j/8
            f.write(f"{nid},{x:.6f},{R*math.cos(a):.6f},{R*math.sin(a):.6f},{T(x,R):.6f}\n"); nid += 1
print("wrote example data")
