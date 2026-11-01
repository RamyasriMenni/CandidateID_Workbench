# Task E - Embedded integration design (design only; the code is SIMULATED)

## Block diagram
```
                         +----------------------- Host PC (Windows 11) -----------------------+
                         |  Workbench UI  <->  C++ core (device adapters: MCU / FPGA / JTAG)   |
                         +---------+--------------------+------------------------+------------+
                                   | USB-C/A (5 V, data)| USB (5 V, data)        | USB (5 V, data)
                          +--------v-------+   +--------v---------+   +----------v-----------+
                          | MCU board      |   | FPGA board       |   | JTAG debug adapter   |
                          | 3.3 V domain   |   | 3.3 V IO/1.0 V   |   | VTref sense (1.8-3.3)|
                          | UART telemetry |   | core/1.8 V aux   |   | TCK TMS TDI TDO nTRST|
                          +--------^-------+   | UART bridge      |   +----+-------------+---+
                                   |           +--------^---------+        |             |
                                   |  SWD/JTAG header   |  JTAG header     |             |
                                   +--------------------+------------------+             |
                         (adapter attaches to ONE target at a time via its own cable; target powers itself;)
                         (adapter only senses VTref - it never powers the target. Common GND is mandatory.)
```
A labelled graphical version belongs in the Word report (`docs/` + report); this ASCII version is the source.

## Interface / voltage table (assumptions - verify against the *actual* boards' datasheets)
| Item | MCU board | FPGA board | JTAG adapter |
|---|---|---|---|
| Host power | USB 5 V bus power -> on-board LDO to 3.3 V | USB 5 V or barrel; multiple rails (1.0/1.8/3.3 V) | USB 5 V bus power |
| Logic domain(s) | 3.3 V | 3.3 V bank I/O, 1.0 V core, 1.8 V aux | Output level tracks target VTref (1.8-3.3 V) |
| Host link | USB-CDC/UART (115200-921600 baud) | USB-UART bridge + JTAG config port | USB debug-probe protocol |
| Debug/config connector | 10-pin 1.27 mm SWD/JTAG (assumed) | 14-pin 2 mm JTAG (assumed) | Adapter-side connector differs: **needs a board-specific adaptor cable** |
| Ground | Common GND with host via USB shield/return and the debug cable GND pins | same | same - single star ground, no ground loops via multiple PSUs |
| Level shifting | none needed if all at 3.3 V | needed only if connected to a 1.8 V bank (use auto-direction translator, e.g. TXS-type; not assumed) | adapter does its own VTref-referenced buffering |
| JTAG signal ownership | target: TDO output, TDI/TMS/TCK/nTRST inputs | same | adapter: TCK, TMS, TDI, nTRST outputs; TDO input |
Pinouts are *assumptions*: no board is named in the brief, so no real pin numbers are claimed.

## Why a JTAG adapter is not a compute board
It exposes the Test Access Port (TCK/TMS/TDI/TDO) to drive a target's boundary-scan / debug TAP state machine; it has no application logic for the host to run
and no general I/O to compute with. It is a *probe*: its value is access to a *target*. MCU and FPGA boards have different connectors, voltages (3.3 V vs multi-rail),
and protocols (UART vs JTAG configuration), so they are **not** interchangeable and the code models them as separate adapters behind one interface.

## Mock implementation
`src/core/devices.*`: discovery (3 devices), capability report, connect/disconnect, one telemetry message each, an injected FPGA timeout that the retry wrapper recovers from.
Every message is prefixed `[SIMULATED]` and `DeviceInfo.simulated == true`.

## Safe integration plan
1. Power sequencing: connect each board on its own USB port, with no cross-wiring, and verify the rails with a multimeter before connecting any debug cable.
2. Connect the adapter to the target with the target powered; read VTref first (reject if outside 1.8-3.3 V).
3. Start TCK at 1 MHz, scan the chain, compare IDCODEs against the datasheet before any write.
4. Add read-only operations first; gate any programming behind an explicit confirmation.
Limitations: no real hardware tested; connectors/pinouts are assumed; ESD, hot-plug and power-fault behaviour are not modelled.
