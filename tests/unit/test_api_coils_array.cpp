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
  // Define a 3-coil window starting at address 20; write with FC15, read with FC1 count=3
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "coils.win.write", "unit_id": 1, "function": 15, "address": 20, "count": 3, "type": "bool" },
      { "name": "coils.win.read",  "unit_id": 1, "function": 1,  "address": 20, "count": 3, "type": "bool" }
    ]
  })JSON";
  write_text("unit_api_coils_array.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_coils_array.json");
  assert(h != nullptr);

  // Write array [1,0,1] and read back as booleans
  int rcw = WriteItem(h, "coils.win.write", "[1,0,1]");
  assert(rcw == 0);

  char buf[128] = {0};
  int rcr = ReadItem(h, "coils.win.read", buf, sizeof(buf));
  assert(rcr == 0);
  // Expect strict JSON array of booleans without spaces (nlohmann::json::dump default)
  assert(std::string(buf) == "[true,false,true]");

  DestroyIoInstance(h);
  std::puts("unit_api_coils_array: ok");
  return 0;
}

