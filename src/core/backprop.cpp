#include "backprop.hpp"
#include <cmath>
namespace wb {
BpForward bpForward(const BpParams& p, double x, double y) {
    BpForward f;
    f.z = p.w * x + p.b;
    f.h = std::tanh(f.z);
    f.yhat = p.v * f.h + p.c;
    const double e = f.yhat - y;
    f.J = 0.5 * e * e;
    return f;
}
BpGrads bpGradients(const BpParams& p, double x, double y) {
    const BpForward f = bpForward(p, x, y);
    const double e = f.yhat - y;           // dJ/dyhat
    BpGrads g;
    g.dv = e * f.h;                        // dyhat/dv = h
    g.dc = e;                              // dyhat/dc = 1
    const double dh = e * p.v;             // dyhat/dh = v
    const double dz = dh * (1.0 - f.h * f.h);  // dh/dz = 1 - tanh^2
    g.dw = dz * x;                         // dz/dw = x
    g.db = dz;                             // dz/db = 1
    return g;
}
BpGrads bpNumericGradients(const BpParams& p, double x, double y, double eps) {
    auto J = [&](BpParams q) { return bpForward(q, x, y).J; };
    BpGrads g;
    BpParams a = p, b = p;
    a.w += eps; b.w -= eps; g.dw = (J(a) - J(b)) / (2 * eps);
    a = p; b = p; a.b += eps; b.b -= eps; g.db = (J(a) - J(b)) / (2 * eps);
    a = p; b = p; a.v += eps; b.v -= eps; g.dv = (J(a) - J(b)) / (2 * eps);
    a = p; b = p; a.c += eps; b.c -= eps; g.dc = (J(a) - J(b)) / (2 * eps);
    return g;
}
BpParams bpStep(const BpParams& p, const BpGrads& g, double lr) {
    return {p.w - lr * g.dw, p.b - lr * g.db, p.v - lr * g.dv, p.c - lr * g.dc};
}
std::vector<double> bpTrain(BpParams p, double x, double y, double lr, int steps) {
    std::vector<double> loss;
    loss.reserve(steps + 1);
    loss.push_back(bpForward(p, x, y).J);
    for (int i = 0; i < steps; ++i) {
        p = bpStep(p, bpGradients(p, x, y), lr);
        loss.push_back(bpForward(p, x, y).J);
    }
    return loss;
}
}  // namespace wb
