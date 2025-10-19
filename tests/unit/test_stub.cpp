#include "IModbusClient.hpp"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

int main() {
  auto cli = wiq::make_stub_client(16,16,256,256);
  assert(cli->connect() == 0);

  // coil 10 default false
  std::uint8_t c = 1; assert(cli->read_coils(1, 10, 1, &c) == 0); assert(c == 0);
  assert(cli->write_single_coil(1, 10, true) == 0);
  c = 0; assert(cli->read_coils(1, 10, 1, &c) == 0); assert(c == 1);

  // holding[0] = 123
  std::uint16_t r = 0; assert(cli->write_single_reg(1, 0, 123) == 0);
  r = 0; assert(cli->read_holding_regs(1, 0, 1, &r) == 0); assert(r == 123);

  // write float 1.5 into input regs 100..101 via write_multiple_regs (simulating device)
  float f = 1.5f; std::uint32_t u; std::memcpy(&u, &f, 4);
  std::uint16_t hi = (u >> 16) & 0xFFFF, lo = u & 0xFFFF;
  std::uint16_t rr[2] = {hi, lo};
  assert(cli->write_multiple_regs(1, 100, 2, rr) == 0);
  std::uint16_t rr2[2] = {0,0};
  assert(cli->read_input_regs(1, 100, 2, rr2) == 0);
  assert(rr2[0] == hi && rr2[1] == lo);

  std::puts("unit_stub: ok");
  return 0;
}
