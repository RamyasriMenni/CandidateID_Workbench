#pragma once
// ALL BEHAVIOUR IN THIS FILE IS SIMULATED. No hardware is touched.
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace wb {
enum class DeviceKind { Microcontroller, Fpga, JtagAdapter };
const char* kindName(DeviceKind k);

enum class Status { Ok, Timeout, NotConnected, AlreadyConnected };
const char* statusName(Status s);
struct Result { Status status; std::string message; bool ok() const { return status == Status::Ok; } };

struct TelemetryValue { std::string name; double value; std::string unit; };
struct Telemetry {
    std::string deviceId;
    uint64_t sequence = 0;
    std::vector<TelemetryValue> values;
    bool simulated = true;
};
struct DeviceInfo {
    std::string id, name, voltageDomain, hostLink;
    DeviceKind kind;
    std::vector<std::string> capabilities;
    bool simulated = true;
};

class IDeviceAdapter {
public:
    virtual ~IDeviceAdapter() = default;
    const DeviceInfo& info() const { return info_; }
    bool connected() const { return connected_; }
    Result connect();       // may return Timeout if a fault was injected
    Result disconnect();
    Result readTelemetry(Telemetry& out);
    void injectConnectTimeouts(int n) { pendingTimeouts_ = n; }
protected:
    explicit IDeviceAdapter(DeviceInfo i) : info_(std::move(i)) {}
    virtual void fillTelemetry(Telemetry& t) const = 0;
    DeviceInfo info_;
    bool connected_ = false;
    int pendingTimeouts_ = 0;
    uint64_t seq_ = 0;
};

// Discovery returns one MCU, one FPGA, one JTAG adapter (simulated).
// If injectFault, the FPGA's first connect() times out (recoverable by retry).
std::vector<std::unique_ptr<IDeviceAdapter>> discoverDevices(bool injectFault);

// Retry wrapper used by the UI/CLI for recoverable failures. Appends a log.
Result connectWithRetry(IDeviceAdapter& d, int maxAttempts, std::vector<std::string>* log);
}  // namespace wb
