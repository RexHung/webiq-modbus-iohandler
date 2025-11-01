#pragma once

#include <cstdint>

namespace wiq {

constexpr int kModbusExceptionBase = -3200;

inline int make_modbus_exception(std::uint8_t code) {
  return kModbusExceptionBase - static_cast<int>(code);
}

inline bool is_modbus_exception(int rc) {
  return rc <= kModbusExceptionBase - 1 && rc >= kModbusExceptionBase - 255;
}

inline std::uint8_t decode_modbus_exception(int rc) {
  return static_cast<std::uint8_t>(kModbusExceptionBase - rc);
}

inline const char* modbus_exception_to_string(std::uint8_t code) {
  switch (code) {
    case 1: return "ILLEGAL_FUNCTION";
    case 2: return "ILLEGAL_DATA_ADDRESS";
    case 3: return "ILLEGAL_DATA_VALUE";
    case 4: return "SLAVE_DEVICE_FAILURE";
    case 5: return "ACKNOWLEDGE";
    case 6: return "SLAVE_DEVICE_BUSY";
    case 8: return "MEMORY_PARITY_ERROR";
    case 10: return "GATEWAY_PATH_UNAVAILABLE";
    case 11: return "GATEWAY_TARGET_FAILED";
    default: return "UNKNOWN";
  }
}

} // namespace wiq

