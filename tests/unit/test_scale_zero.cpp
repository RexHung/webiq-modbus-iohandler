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
  // Create a minimal config with a single holding register item and scale=0.0
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "holding.zero", "unit_id": 1, "function": 6, "address": 0, "type": "int16", "scale": 0.0, "offset": 0.0 }
    ]
  })JSON";
  write_text("unit_scale0.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_scale0.json");
  assert(h != nullptr);

  // Attempt to write any value should return PARSE_ERROR (-7) when scale==0
  int rc = WriteItem(h, "holding.zero", "123");
  assert(rc == -7);

  DestroyIoInstance(h);
  std::puts("unit_scale_zero: ok");
  return 0;
}

