#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* user_param, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
}

static void write_text(const char* path, const std::string& s) {
  std::ofstream ofs(path, std::ios::binary); ofs << s; ofs.close();
}

int main() {
  // 1) Invalid transport
  write_text("invalid_transport.json", R"({"transport":"udp","items":[{"name":"a","unit_id":1,"function":1,"address":0,"type":"bool"}]})");
  assert(CreateIoInstance(nullptr, "invalid_transport.json") == nullptr);

  // 2) Empty items
  write_text("empty_items.json", R"({"transport":"tcp","tcp":{"host":"127.0.0.1","port":1502},"items":[]})");
  assert(CreateIoInstance(nullptr, "empty_items.json") == nullptr);

  // 3) Type/function mismatch: FC1 but type=int16
  write_text("bad_type_fc.json", R"({"transport":"tcp","tcp":{"host":"127.0.0.1","port":1502},"items":[{"name":"x","unit_id":1,"function":1,"address":0,"type":"int16"}]})");
  assert(CreateIoInstance(nullptr, "bad_type_fc.json") == nullptr);

  // 4) Invalid unit_id
  write_text("bad_unit_id.json", R"({"transport":"tcp","tcp":{"host":"127.0.0.1","port":1502},"items":[{"name":"x","unit_id":0,"function":1,"address":0,"type":"bool"}]})");
  assert(CreateIoInstance(nullptr, "bad_unit_id.json") == nullptr);

  // 5) Valid minimal config should succeed
  write_text("ok.json", R"({"transport":"tcp","tcp":{"host":"127.0.0.1","port":1502},"items":[{"name":"ok","unit_id":1,"function":1,"address":0,"type":"bool"}]})");
  IoHandle h = CreateIoInstance(nullptr, "ok.json");
  assert(h != nullptr);
  DestroyIoInstance(h);

  std::puts("unit_config_invalid: ok");
  return 0;
}

