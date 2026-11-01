# Task F - reference values for checking YOUR handwritten derivation

> The brief requires **your own legible handwriting (max 2 pages)** and that you can defend it. These numbers are the
> C++ output (`wb_cli backprop`) to check against - do not copy them without working through the chain rule yourself.

Setup: x=1, y=2; w=0.5, b=0, v=1, c=0; lr=0.1; h=tanh(wx+b), y_hat=vh+c, J=0.5(y_hat-y)^2.

Forward: z=0.5; h=tanh(0.5)=0.4621171573; y_hat=0.4621171573; e=y_hat-y=-1.5378828427; J=1.182541819.

Chain rule:
- dJ/dy_hat = e
- dJ/dv = e*h = -0.7106820475      dJ/dc = e*1 = -1.5378828427
- dJ/dh = e*v = -1.5378828427;  dh/dz = 1-h^2 = 0.7864477329;  dJ/dz = -1.2094644750
- dJ/dw = dJ/dz * x = -1.2094644750      dJ/db = dJ/dz * 1 = -1.2094644750

Simultaneous update (all gradients from the ORIGINAL parameters): w=0.6209464475, b=0.1209464475, v=1.0710682048, c=0.1537882843; J drops 1.1825 -> 0.6858.

Gradient direction: every gradient is negative (increasing any parameter would *raise* y_hat toward the target 2, lowering J), so
w <- w - lr*grad *increases* each parameter. Central-difference check (eps=1e-6): max |error| = 1.1e-10 <= 1e-5 (`results/backprop_gradcheck.csv`).
Loss plot: `results/backprop_loss.png` - monotone decrease; J = 1.1e-20 after 101 total updates because one sample can be fit exactly (a 4-parameter model, 1 data point: over-parameterised, so this is memorisation, not generalisation).
