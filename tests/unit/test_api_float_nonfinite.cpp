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
      { "name": "f.hold.w", "unit_id": 1, "function": 16, "address": 80, "count": 2, "type": "float", "swap_words": false }
    ]
  })JSON";
  write_text("unit_api_float_nonfinite.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_float_nonfinite.json");
  assert(h != nullptr);

  // NaN not a JSON number: expect parse error
  int rc1 = WriteItem(h, "f.hold.w", "NaN");
  assert(rc1 == -7);

  // Infinity as very large number -> expect PARSE_ERROR due to non-finite guard
  int rc2 = WriteItem(h, "f.hold.w", "1e309");
  assert(rc2 == -7);

  DestroyIoInstance(h);
  std::puts("unit_api_float_nonfinite: ok");
  return 0;
}

