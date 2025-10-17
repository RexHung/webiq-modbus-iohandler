#include "IModbusClient.hpp"
#include <vector>
#include <algorithm>

namespace wiq {

class StubModbusClient : public IModbusClient {
public:
  StubModbusClient(int co, int di, int hr, int ir)
  : connected_(false), timeout_ms_(1000),
    coils_(co, 0), discrete_(di, 0), holding_(hr, 0), input_(ir, 0) {}

  int connect() override {
    connected_ = true;
    return 0;
  }
  void close() override { connected_ = false; }
  void set_timeout_ms(int ms) override { timeout_ms_ = ms; (void)timeout_ms_; }

  int read_coils(int /*unit*/, int addr, int count, std::uint8_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(coils_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    for (int i = 0; i < count; ++i) out[i] = coils_[addr + i] ? 1u : 0u;
    return 0;
  }

  int read_discrete_inputs(int /*unit*/, int addr, int count, std::uint8_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(discrete_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    for (int i = 0; i < count; ++i) out[i] = discrete_[addr + i] ? 1u : 0u;
    return 0;
  }

  int read_holding_regs(int /*unit*/, int addr, int count, std::uint16_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(holding_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    for (int i = 0; i < count; ++i) out[i] = holding_[addr + i];
    return 0;
  }

  int read_input_regs(int /*unit*/, int addr, int count, std::uint16_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(input_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    for (int i = 0; i < count; ++i) out[i] = input_[addr + i];
    return 0;
  }

  int write_single_coil(int /*unit*/, int addr, bool on) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (addr < 0 || addr >= static_cast<int>(coils_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    coils_[addr] = on ? 1 : 0;
    return 0;
  }

  int write_single_reg(int /*unit*/, int addr, std::uint16_t value) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (addr < 0 || addr >= static_cast<int>(holding_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    holding_[addr] = value;
    return 0;
  }

  int write_multiple_coils(int /*unit*/, int addr, int count, const std::uint8_t* v) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!v || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(coils_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    for (int i = 0; i < count; ++i) coils_[addr + i] = v[i] ? 1 : 0;
    return 0;
  }

  int write_multiple_regs(int /*unit*/, int addr, int count, const std::uint16_t* v) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!v || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(holding_.size())) return static_cast<int>(ModbusErr::INVALID_ARG);
    for (int i = 0; i < count; ++i) holding_[addr + i] = v[i];
    return 0;
  }

  // Helpers to seed stub state (optional)
  void seed_coil(int addr, bool on) { if (addr >= 0 && addr < (int)coils_.size()) coils_[addr] = on ? 1 : 0; }
  void seed_discrete(int addr, bool on) { if (addr >= 0 && addr < (int)discrete_.size()) discrete_[addr] = on ? 1 : 0; }
  void seed_hr(int addr, std::uint16_t v) { if (addr >= 0 && addr < (int)holding_.size()) holding_[addr] = v; }
  void seed_ir(int addr, std::uint16_t v) { if (addr >= 0 && addr < (int)input_.size()) input_[addr] = v; }

private:
  bool connected_;
  int timeout_ms_;
  std::vector<std::uint8_t>  coils_;
  std::vector<std::uint8_t>  discrete_;
  std::vector<std::uint16_t> holding_;
  std::vector<std::uint16_t> input_;
};

std::unique_ptr<IModbusClient> make_stub_client(
  int coils_size,
  int discrete_inputs_size,
  int holding_regs_size,
  int input_regs_size) {
  return std::unique_ptr<IModbusClient>(
    new StubModbusClient(coils_size, discrete_inputs_size, holding_regs_size, input_regs_size));
}

} // namespace wiq
