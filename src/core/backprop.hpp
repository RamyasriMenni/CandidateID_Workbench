#pragma once
#include <vector>
namespace wb {
// Single neuron-pair network: h = tanh(w x + b), y_hat = v h + c, J = 0.5 (y_hat - y)^2
// Gradients are hand-derived (no automatic differentiation).
struct BpParams { double w, b, v, c; };
struct BpGrads { double dw, db, dv, dc; };
struct BpForward { double z, h, yhat, J; };

BpForward bpForward(const BpParams& p, double x, double y);
BpGrads bpGradients(const BpParams& p, double x, double y);                 // chain rule
BpGrads bpNumericGradients(const BpParams& p, double x, double y, double eps); // central differences
// Simultaneous update: every parameter uses the gradient at the ORIGINAL parameters.
BpParams bpStep(const BpParams& p, const BpGrads& g, double lr);
// Loss before step 0 .. after step `steps` (size steps+1).
std::vector<double> bpTrain(BpParams p, double x, double y, double lr, int steps);
}  // namespace wb
