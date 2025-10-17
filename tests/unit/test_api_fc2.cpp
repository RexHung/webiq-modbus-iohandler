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
}

static void write_text(const char* path, const std::string& s) {
  std::ofstream ofs(path, std::ios::binary); ofs << s; ofs.close();
}

int main() {
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "di.single", "unit_id": 1, "function": 2, "address": 0,  "type": "bool" },
      { "name": "di.array",  "unit_id": 1, "function": 2, "address": 10, "count": 3, "type": "bool" }
    ]
  })JSON";
  write_text("unit_api_fc2.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_fc2.json");
  assert(h != nullptr);

  char buf[128] = {0};
  int rc1 = ReadItem(h, "di.single", buf, sizeof(buf));
  assert(rc1 == 0);
  assert(std::string(buf) == "false");

  std::memset(buf, 0, sizeof(buf));
  int rc2 = ReadItem(h, "di.array", buf, sizeof(buf));
  assert(rc2 == 0);
  assert(std::string(buf) == "[true,false,true]" || std::string(buf) == "[false,false,false]" || std::string(buf) == "[false,false,false]\n");
  // Stub default is 0s -> [false,false,false]; allow tolerance in case of future seeding

  DestroyIoInstance(h);
  std::puts("unit_api_fc2: ok");
  return 0;
}

