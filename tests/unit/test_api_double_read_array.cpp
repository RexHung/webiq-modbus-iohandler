#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <cmath>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* user_param, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
  int      ReadItem(IoHandle h, const char* name, char* outJson, int outSize);
  int      WriteItem(IoHandle h, const char* name, const char* valueJson);
}

static void write_text(const char* path, const std::string& s) {
  std::ofstream ofs(path, std::ios::binary); ofs << s; ofs.close();
}

static std::string double_to_regs_array(double d) {
  std::uint64_t u = 0; std::memcpy(&u, &d, 8);
  // big-endian words: hi..lo
  std::uint16_t r0 = (u >> 48) & 0xFFFF;
  std::uint16_t r1 = (u >> 32) & 0xFFFF;
  std::uint16_t r2 = (u >> 16) & 0xFFFF;
  std::uint16_t r3 = u & 0xFFFF;
  char buf[128];
  std::snprintf(buf, sizeof(buf), "[%u,%u,%u,%u]", (unsigned)r0,(unsigned)r1,(unsigned)r2,(unsigned)r3);
  return std::string(buf);
}

int main() {
  double val = 123.456;
  std::string regs = double_to_regs_array(val);

  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "dbl.w", "unit_id": 1, "function": 16, "address": 90, "count": 4, "type": "double" },
      { "name": "dbl.r", "unit_id": 1, "function": 3,  "address": 90, "count": 4, "type": "double" }
    ]
  })JSON";
  write_text("unit_api_double_read_array.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_double_read_array.json");
  assert(h != nullptr);

  int rcw = WriteItem(h, "dbl.w", regs.c_str());
  assert(rcw == 0);

  char buf[128] = {0};
  int rcr = ReadItem(h, "dbl.r", buf, sizeof(buf));
  assert(rcr == 0);
  double back = std::stod(std::string(buf));
  assert(std::abs(back - val) < 1e-9);

  DestroyIoInstance(h);
  std::puts("unit_api_double_read_array: ok");
  return 0;
}
