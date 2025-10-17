#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* user_param, const char* jsonConfigPath);
}

static void write_text(const char* path, const std::string& s) {
  std::ofstream ofs(path, std::ios::binary); ofs << s; ofs.close();
}

int main() {
  // FC6 with type double should be invalid (single register not allowed)
  write_text("bad_fc6_double.json", R"({
    "transport":"tcp", "tcp":{"host":"127.0.0.1","port":1502},
    "items":[{"name":"x","unit_id":1,"function":6,"address":0,"type":"double"}]
  })");
  assert(CreateIoInstance(nullptr, "bad_fc6_double.json") == nullptr);

  // FC3 with type double and no count is acceptable (normalized to count=4)
  write_text("ok_fc3_double.json", R"({
    "transport":"tcp", "tcp":{"host":"127.0.0.1","port":1502},
    "items":[{"name":"y","unit_id":1,"function":3,"address":10,"type":"double"}]
  })");
  assert(CreateIoInstance(nullptr, "ok_fc3_double.json") != nullptr);

  std::puts("unit_config_invalid_double: ok");
  return 0;
}

