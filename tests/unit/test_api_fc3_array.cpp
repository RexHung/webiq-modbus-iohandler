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
  // We will write 3 holding registers via FC16 array and then read via FC3 array
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "hr.write", "unit_id": 1, "function": 16, "address": 30, "count": 3, "type": "uint16" },
      { "name": "hr.read",  "unit_id": 1, "function": 3,  "address": 30, "count": 3, "type": "uint16" }
    ]
  })JSON";
  write_text("unit_api_fc3_array.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_fc3_array.json");
  assert(h != nullptr);

  int rcw = WriteItem(h, "hr.write", "[1,2,3]");
  assert(rcw == 0);

  char buf[128] = {0};
  int rcr = ReadItem(h, "hr.read", buf, sizeof(buf));
  assert(rcr == 0);
  assert(std::string(buf) == "[1,2,3]");

  DestroyIoInstance(h);
  std::puts("unit_api_fc3_array: ok");
  return 0;
}

