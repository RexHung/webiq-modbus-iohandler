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
  // Read 2 input registers; stub defaults to 0 => expect [0,0] (ensure not parsed as float)
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "ir.read2",  "unit_id": 1, "function": 4,  "address": 72, "count": 2, "type": "uint16" }
    ]
  })JSON";
  write_text("unit_api_fc4_array_2.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_fc4_array_2.json");
  assert(h != nullptr);

  char buf[128] = {0};
  int rcr = ReadItem(h, "ir.read2", buf, sizeof(buf));
  assert(rcr == 0);
  assert(std::string(buf) == "[0,0]");

  DestroyIoInstance(h);
  std::puts("unit_api_fc4_array_2: ok");
  return 0;
}

