#pragma once

#include <cstdint>
#include <memory>
#include "export.hpp"

namespace wiq {

// Negative error codes aligned with spec
enum class ModbusErr : int {
  OK             = 0,
  INVALID_ARG    = -1,
  NOT_FOUND      = -2,
  IO_TIMEOUT     = -3,
  IO_ERROR       = -4,
  NOT_CONNECTED  = -5,
  UNSUPPORTED    = -6,
  PARSE_ERROR    = -7,
  CRC_ERROR      = -8,
  LRC_ERROR      = -9,
};

class IModbusClient {
public:
  virtual ~IModbusClient() {}

  // lifecycle
  virtual int connect() = 0;          // 0 on success
  virtual void close() = 0;
  virtual void set_timeout_ms(int ms) = 0;

  // reads
  virtual int read_coils(int unit, int addr, int count, std::uint8_t* out) = 0;              // FC1
  virtual int read_discrete_inputs(int unit, int addr, int count, std::uint8_t* out) = 0;    // FC2
  virtual int read_holding_regs(int unit, int addr, int count, std::uint16_t* out) = 0;      // FC3
  virtual int read_input_regs(int unit, int addr, int count, std::uint16_t* out) = 0;        // FC4

  // writes
  virtual int write_single_coil(int unit, int addr, bool on) = 0;                            // FC5
  virtual int write_single_reg(int unit, int addr, std::uint16_t value) = 0;                 // FC6
  virtual int write_multiple_coils(int unit, int addr, int count, const std::uint8_t* v) = 0; // FC15
  virtual int write_multiple_regs(int unit, int addr, int count, const std::uint16_t* v) = 0; // FC16

  // optional hooks for transports requiring frame timing/diagnostics
  virtual void set_frame_silence_ms(int /*ms*/) {}
  virtual int read_diagnostics(int /*unit*/, int /*addr*/, std::uint16_t* /*out*/, int /*count*/) {
    return static_cast<int>(ModbusErr::UNSUPPORTED);
  }
};

// Factory for a simple in-memory stub client usable without libmodbus
WIQ_IOH_API std::unique_ptr<IModbusClient> make_stub_client(
  int coils_size = 200,
  int discrete_inputs_size = 200,
  int holding_regs_size = 200,
  int input_regs_size = 200);

} // namespace wiq
