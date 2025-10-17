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
  std::string cfg = R"JSON({
    "transport": "tcp",
    "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
    "items": [
      { "name": "hr.min", "unit_id": 1, "function": 6, "address": 0, "type": "int16", "scale": 1.0, "offset": 0.0 },
      { "name": "hr.min.r", "unit_id": 1, "function": 3, "address": 0, "type": "int16", "scale": 1.0, "offset": 0.0 },
      { "name": "hr.max", "unit_id": 1, "function": 6, "address": 1, "type": "int16", "scale": 1.0, "offset": 0.0 },
      { "name": "hr.max.r", "unit_id": 1, "function": 3, "address": 1, "type": "int16", "scale": 1.0, "offset": 0.0 }
    ]
  })JSON";
  write_text("unit_api_int16_bounds.json", cfg);

  IoHandle h = CreateIoInstance(nullptr, "unit_api_int16_bounds.json");
  assert(h != nullptr);

  // Write min and max scaled values and read back
  int rc1 = WriteItem(h, "hr.min", "-32768");
  int rc2 = WriteItem(h, "hr.max", "32767");
  assert(rc1 == 0 && rc2 == 0);

  char buf[64] = {0};
  int r1 = ReadItem(h, "hr.min.r", buf, sizeof(buf));
  assert(r1 == 0);
  double vmin = std::stod(std::string(buf));
  std::memset(buf, 0, sizeof(buf));
  int r2 = ReadItem(h, "hr.max.r", buf, sizeof(buf));
  assert(r2 == 0);
  double vmax = std::stod(std::string(buf));

  assert((int)vmin == -32768);
  assert((int)vmax == 32767);

  DestroyIoInstance(h);
  std::puts("unit_api_int16_scale_bounds: ok");
  return 0;
}

