#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>
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

int main() {
  // Configure two items on the same holding address: one for write (FC16), one for read (FC3)
  // swap_words=true exercises word order handling
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "f.hold",       "unit_id": 1, "function": 16, "address": 50, "count": 2, "type": "float", "swap_words": true },
      { "name": "f.hold.read",  "unit_id": 1, "function": 3,  "address": 50, "count": 2, "type": "float", "swap_words": true }
    ]
  })JSON";
  write_text("unit_api_float_swap.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_float_swap.json");
  assert(h != nullptr);

  // Write negative float and read back
  int rcw = WriteItem(h, "f.hold", "-7.25");
  assert(rcw == 0);

  char buf[128] = {0};
  int rcr = ReadItem(h, "f.hold.read", buf, sizeof(buf));
  assert(rcr == 0);
  double val = std::stod(std::string(buf));
  assert(std::abs(val - (-7.25)) < 1e-6);

  DestroyIoInstance(h);
  std::puts("unit_api_float_swap: ok");
  return 0;
}

