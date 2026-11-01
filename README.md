# Unified Engineering Workbench (assessment prototype)

C++17 core for geometry/result import, 1-D thermal FEA, multithreaded batch solves, simulated embedded devices and a hand-derived backprop example.
**Status: core + tests + evidence complete; UI, Windows packaging, report, video and handwritten pages are NOT yet done (see table).**

## Status vs brief
| Area | State |
|---|---|
| Task C FEA, convergence, CSV, plots | Done (core + tests + `results/`) |
| Task D batch generator, serial/parallel, benchmark | Done; **measured on 1 logical CPU only** - rerun `wb_cli bench` on a multicore PC for real speedup |
| Task B STL + nodal CSV parsing/validation (C++) | Done; 3D viewer (rotate/pan/zoom, legend, inspection) **not yet built** |
| Task E mock adapters + recoverable timeout | Done in C++/CLI; UI view **not yet built** |
| Task F C++ gradient check + loss plot | Done; **handwritten pages are yours to write** |
| Task A GUI, `Launch.exe`, bundling, editor run config | **Not done** (needs Windows build + frontend) |
| Report (<=6 pp), demo video, git history, AI log | Partial: docs + AI log scaffolds; report/video pending |

## Prerequisites (build from source)
CMake >= 3.16, a C++17 compiler (GCC/Clang/MSVC 2019+). No third-party libraries.

## Build, test, run
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure      # 27 tests
build/wb_cli fea | backprop | devices | bench | batch --cases 500 --workers 4
build/wb_cli stl data/example_bracket.stl
build/wb_cli csv data/example_synthetic_nodal_temperature.csv
python tools/plot_results.py                               # regenerates results/*.png (matplotlib; evidence only)
```
## Architecture
`src/core` (wb_core, no UI/transport): `fem1d`, `batch` (bounded std::thread pool, atomic chunking, cancel/progress), `stl_parser`, `nodal_csv`, `devices` (simulated), `backprop`, `sysinfo`.
`src/cli` = thin presentation layer. Planned: a local transport (stdin/stdout JSON or loopback HTTP) + web/desktop frontend that only calls the CLI/core.

## Supported formats
ASCII STL (metres assumed; binary STL rejected with a message); CSV `node_id,x_m,y_m,z_m,temperature_C`. Details, limits: `data/README_PROVENANCE.md`.

## Known limitations
Single-CPU benchmark only; Windows code path in `sysinfo.cpp` is untested; STL units are assumed; no GUI yet; device behaviour is simulated; no hardware tested.
