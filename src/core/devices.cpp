#include "devices.hpp"
#include <cmath>

namespace wb {
const char* kindName(DeviceKind k) {
    switch (k) {
        case DeviceKind::Microcontroller: return "Microcontroller";
        case DeviceKind::Fpga: return "FPGA";
        case DeviceKind::JtagAdapter: return "JTAG adapter";
    }
    return "?";
}
const char* statusName(Status s) {
    switch (s) {
        case Status::Ok: return "OK";
        case Status::Timeout: return "TIMEOUT";
        case Status::NotConnected: return "NOT_CONNECTED";
        case Status::AlreadyConnected: return "ALREADY_CONNECTED";
    }
    return "?";
}

Result IDeviceAdapter::connect() {
    if (connected_) return {Status::AlreadyConnected, info_.id + " is already connected."};
    if (pendingTimeouts_ > 0) {
        --pendingTimeouts_;
        return {Status::Timeout, "[SIMULATED] " + info_.id + ": no response within 500 ms."};
    }
    connected_ = true;
    return {Status::Ok, "[SIMULATED] " + info_.id + " connected."};
}
Result IDeviceAdapter::disconnect() {
    if (!connected_) return {Status::NotConnected, info_.id + " is not connected."};
    connected_ = false;
    return {Status::Ok, "[SIMULATED] " + info_.id + " disconnected."};
}
Result IDeviceAdapter::readTelemetry(Telemetry& out) {
    if (!connected_) return {Status::NotConnected, "Connect " + info_.id + " before requesting telemetry."};
    out = Telemetry{};
    out.deviceId = info_.id;
    out.sequence = ++seq_;
    fillTelemetry(out);
    return {Status::Ok, "[SIMULATED] telemetry frame " + std::to_string(out.sequence)};
}

namespace {
double wobble(uint64_t s) { return std::sin(static_cast<double>(s)); }

class McuAdapter : public IDeviceAdapter {
public:
    McuAdapter() : IDeviceAdapter({"mcu-0", "Simulated 3.3 V Cortex-M MCU board", "3.3 V", "USB-CDC serial (UART)",
                                   DeviceKind::Microcontroller,
                                   {"uart-telemetry", "gpio", "adc", "swd/jtag-debug-target"}, true}) {}
protected:
    void fillTelemetry(Telemetry& t) const override {
        t.values = {{"supply", 3.30 + 0.01 * wobble(t.sequence), "V"},
                    {"die_temperature", 31.0 + 0.5 * wobble(t.sequence), "C"},
                    {"uptime", static_cast<double>(t.sequence), "s"}};
    }
};
class FpgaAdapter : public IDeviceAdapter {
public:
    FpgaAdapter() : IDeviceAdapter({"fpga-0", "Simulated FPGA dev board (Artix-class)", "3.3 V I/O, 1.0 V core, 1.8 V aux",
                                    "USB-UART bridge + JTAG configuration port", DeviceKind::Fpga,
                                    {"jtag-configure (simulated)", "status-registers", "temperature-xadc"}, true}) {}
protected:
    void fillTelemetry(Telemetry& t) const override {
        t.values = {{"core_voltage", 1.00 + 0.005 * wobble(t.sequence), "V"},
                    {"die_temperature", 42.0 + 1.0 * wobble(t.sequence), "C"},
                    {"config_done", 1.0, "flag"}};
    }
};
class JtagAdapter : public IDeviceAdapter {
public:
    JtagAdapter() : IDeviceAdapter({"jtag-0", "Simulated JTAG debug adapter", "VTref follows target (1.8-3.3 V)",
                                    "USB (debug probe protocol)", DeviceKind::JtagAdapter,
                                    {"jtag-chain-scan", "swd", "tck-frequency-set", "vtref-sense"}, true}) {}
protected:
    // A JTAG adapter is a debug INTERFACE: it reports probe/chain status, not compute results.
    void fillTelemetry(Telemetry& t) const override {
        t.values = {{"vtref", 3.30, "V"}, {"tck", 4.0, "MHz"}, {"devices_on_chain", 2.0, "count"}};
    }
};
}  // namespace

std::vector<std::unique_ptr<IDeviceAdapter>> discoverDevices(bool injectFault) {
    std::vector<std::unique_ptr<IDeviceAdapter>> v;
    v.push_back(std::make_unique<McuAdapter>());
    auto fpga = std::make_unique<FpgaAdapter>();
    if (injectFault) fpga->injectConnectTimeouts(1);
    v.push_back(std::move(fpga));
    v.push_back(std::make_unique<JtagAdapter>());
    return v;
}

Result connectWithRetry(IDeviceAdapter& d, int maxAttempts, std::vector<std::string>* log) {
    Result r{Status::Timeout, "no attempts made"};
    for (int a = 1; a <= maxAttempts; ++a) {
        r = d.connect();
        if (log) log->push_back("attempt " + std::to_string(a) + "/" + std::to_string(maxAttempts) + ": " +
                                statusName(r.status) + " - " + r.message);
        if (r.status != Status::Timeout) break;   // only timeouts are retried
    }
    return r;
}
}  // namespace wb
