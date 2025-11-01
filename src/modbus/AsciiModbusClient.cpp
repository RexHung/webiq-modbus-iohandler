#include "AsciiModbusClient.hpp"
#include "ModbusError.hpp"
#include <vector>
#include <algorithm>

namespace wiq {

namespace {

class AsciiStubClient : public IModbusClient {
public:
  AsciiStubClient(const AsciiConfig& cfg,
                  int coils_size,
                  int discrete_size,
                  int holding_size,
                  int input_size)
  : cfg_(cfg),
    frame_silence_ms_(cfg.frame_silence_ms),
    coils_(coils_size, 0),
    discrete_(discrete_size, 0),
    holding_(holding_size, 0),
    input_(input_size, 0) {}

  int connect() override {
    connected_ = true;
    return 0;
  }

  void close() override { connected_ = false; }

  void set_timeout_ms(int ms) override { cfg_.timeout_ms = ms; }

  void set_frame_silence_ms(int ms) override { frame_silence_ms_ = ms; }

  int read_coils(int /*unit*/, int addr, int count, std::uint8_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(coils_.size())) return make_modbus_exception(2);
    for (int i = 0; i < count; ++i) out[i] = coils_[addr + i] ? 1u : 0u;
    return 0;
  }

  int read_discrete_inputs(int /*unit*/, int addr, int count, std::uint8_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(discrete_.size())) return make_modbus_exception(2);
    for (int i = 0; i < count; ++i) out[i] = discrete_[addr + i] ? 1u : 0u;
    return 0;
  }

  int read_holding_regs(int /*unit*/, int addr, int count, std::uint16_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(holding_.size())) return make_modbus_exception(2);
    for (int i = 0; i < count; ++i) out[i] = holding_[addr + i];
    return 0;
  }

  int read_input_regs(int /*unit*/, int addr, int count, std::uint16_t* out) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!out || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(input_.size())) return make_modbus_exception(2);
    for (int i = 0; i < count; ++i) out[i] = input_[addr + i];
    return 0;
  }

  int write_single_coil(int /*unit*/, int addr, bool on) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (addr < 0 || addr >= static_cast<int>(coils_.size())) return make_modbus_exception(2);
    coils_[addr] = on ? 1 : 0;
    return 0;
  }

  int write_single_reg(int /*unit*/, int addr, std::uint16_t value) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (addr < 0 || addr >= static_cast<int>(holding_.size())) return make_modbus_exception(2);
    holding_[addr] = value;
    return 0;
  }

  int write_multiple_coils(int /*unit*/, int addr, int count, const std::uint8_t* v) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!v || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(coils_.size())) return make_modbus_exception(2);
    for (int i = 0; i < count; ++i) coils_[addr + i] = v[i] ? 1 : 0;
    return 0;
  }

  int write_multiple_regs(int /*unit*/, int addr, int count, const std::uint16_t* v) override {
    if (!connected_) return static_cast<int>(ModbusErr::NOT_CONNECTED);
    if (!v || addr < 0 || count < 0) return static_cast<int>(ModbusErr::INVALID_ARG);
    if (addr + count > static_cast<int>(holding_.size())) return make_modbus_exception(2);
    for (int i = 0; i < count; ++i) holding_[addr + i] = v[i];
    return 0;
  }

private:
  AsciiConfig cfg_;
  int frame_silence_ms_;
  bool connected_{false};
  std::vector<std::uint8_t> coils_;
  std::vector<std::uint8_t> discrete_;
  std::vector<std::uint16_t> holding_;
  std::vector<std::uint16_t> input_;
};

} // namespace

std::unique_ptr<IModbusClient> make_ascii_client(const AsciiConfig& cfg,
                                                 int coils_size,
                                                 int discrete_inputs_size,
                                                 int holding_regs_size,
                                                 int input_regs_size) {
  return std::unique_ptr<IModbusClient>(
      new AsciiStubClient(cfg, coils_size, discrete_inputs_size, holding_regs_size, input_regs_size));
}

} // namespace wiq

