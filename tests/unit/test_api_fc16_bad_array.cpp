#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <fstream>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* user_param, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
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
      { "name": "hr.bad", "unit_id": 1, "function": 16, "address": 60, "count": 2, "type": "uint16" }
    ]
  })JSON";
  write_text("unit_api_fc16_bad.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_fc16_bad.json");
  assert(h != nullptr);

  // wrong element type (string)
  int rc1 = WriteItem(h, "hr.bad", "[1,\"x\"]");
  assert(rc1 == -7); // PARSE_ERROR

  // out-of-range element (>65535)
  int rc2 = WriteItem(h, "hr.bad", "[1,70000]");
  assert(rc2 == -7); // PARSE_ERROR

  // wrong length (1 vs count=2)
  int rc3 = WriteItem(h, "hr.bad", "[1]");
  assert(rc3 == -1); // INVALID_ARG

  DestroyIoInstance(h);
  std::puts("unit_api_fc16_bad_array: ok");
  return 0;
}

