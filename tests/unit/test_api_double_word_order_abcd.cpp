#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>

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
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "dbl.w", "unit_id": 1, "function": 16, "address": 120, "count": 4, "type": "double", "word_order": "ABCD" },
      { "name": "dbl.r", "unit_id": 1, "function": 3,  "address": 120, "count": 4, "type": "double", "word_order": "ABCD" }
    ]
  })JSON";
  write_text("unit_api_double_word_order_abcd.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_double_word_order_abcd.json");
  assert(h != nullptr);

  int rcw = WriteItem(h, "dbl.w", "3.1415926535");
  assert(rcw == 0);

  char buf[128] = {0};
  int rcr = ReadItem(h, "dbl.r", buf, sizeof(buf));
  assert(rcr == 0);
  double v = std::stod(std::string(buf));
  assert(std::abs(v - 3.1415926535) < 1e-9);

  DestroyIoInstance(h);
  std::puts("unit_api_double_word_order_abcd: ok");
  return 0;
}

