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
  // float with wrong count
  write_text("bad_fc3_float_count.json", R"({
    "transport":"tcp", "tcp":{"host":"127.0.0.1","port":1502},
    "items":[{"name":"f","unit_id":1,"function":3,"address":0,"type":"float","count":3}]
  })");
  assert(CreateIoInstance(nullptr, "bad_fc3_float_count.json") == nullptr);

  // float without count should be accepted (normalized to 2)
  write_text("ok_fc3_float_no_count.json", R"({
    "transport":"tcp", "tcp":{"host":"127.0.0.1","port":1502},
    "items":[{"name":"f","unit_id":1,"function":3,"address":0,"type":"float"}]
  })");
  assert(CreateIoInstance(nullptr, "ok_fc3_float_no_count.json") != nullptr);

  std::puts("unit_config_invalid_float_count: ok");
  return 0;
}

