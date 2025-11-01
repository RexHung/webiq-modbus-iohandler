#pragma once

#include <memory>
#include <string>
#include "IModbusClient.hpp"

namespace wiq {

struct AsciiConfig {
  std::string port;
  int baud{9600};
  char parity{'E'};      // 'N', 'E', 'O'
  int data_bits{7};
  int stop_bits{1};
  int timeout_ms{1500};
  bool lrc_check{true};
  int frame_silence_ms{5};
};

std::unique_ptr<IModbusClient> make_ascii_client(const AsciiConfig& cfg,
                                                 int coils_size = 200,
                                                 int discrete_inputs_size = 200,
                                                 int holding_regs_size = 200,
                                                 int input_regs_size = 200);

} // namespace wiq

