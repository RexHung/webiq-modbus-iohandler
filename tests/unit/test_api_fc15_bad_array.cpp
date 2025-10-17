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
      { "name": "coils.bad", "unit_id": 1, "function": 15, "address": 40, "count": 3, "type": "bool" }
    ]
  })JSON";
  write_text("unit_api_fc15_bad.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_fc15_bad.json");
  assert(h != nullptr);

  // wrong element type (string)
  int rc1 = WriteItem(h, "coils.bad", "[1,\"x\",1]");
  assert(rc1 == -7); // PARSE_ERROR

  // wrong length (2 vs count=3)
  int rc2 = WriteItem(h, "coils.bad", "[1,0]");
  assert(rc2 == -1); // INVALID_ARG

  DestroyIoInstance(h);
  std::puts("unit_api_fc15_bad_array: ok");
  return 0;
}

