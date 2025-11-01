#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void*, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
  int      ReadItem(IoHandle h, const char* name, /*out*/char* outJson, int outSize);
  int      CallMethod(IoHandle h, const char* method, const char* paramsJson, /*out*/char* outJson, int outSize);
}

static std::string writeConfig(const std::string& filename) {
  nlohmann::json cfg = {
    {"transport", "tcp"},
    {"items", nlohmann::json::array({
      {
        {"name", "coil.ok"},
        {"unit_id", 1},
        {"function", 1},
        {"address", 0},
        {"type", "bool"}
      },
      {
        {"name", "coil.bad"},
        {"unit_id", 1},
        {"function", 1},
        {"address", 400},
        {"type", "bool"}
      }
    })}
  };

  std::ofstream ofs(filename);
  ofs << cfg.dump(2);
  ofs.close();
  return filename;
}

int main() {
  const std::string cfgPath = writeConfig("test_exception_config.json");

  IoHandle h = CreateIoInstance(nullptr, cfgPath.c_str());
  assert(h && "CreateIoInstance failed");

  char buf[256] = {0};
  int rc = ReadItem(h, "coil.bad", buf, sizeof(buf));
  assert(rc == -3202 && "Expected Modbus exception code -3202");

  auto err = nlohmann::json::parse(buf);
  assert(err.contains("error"));
  assert(err["error"]["code"].get<int>() == -3202);
  assert(err["error"]["exception"]["name"].get<std::string>() == "ILLEGAL_DATA_ADDRESS");

  char diagBuf[512] = {0};
  rc = CallMethod(h, "diagnostics.snapshot", "{}", diagBuf, sizeof(diagBuf));
  assert(rc == 0);
  auto snap = nlohmann::json::parse(diagBuf);
  assert(snap["counters"]["operations"].get<std::uint64_t>() >= 1);
  assert(!snap["exceptions"].empty());

  DestroyIoInstance(h);
  std::remove(cfgPath.c_str());
  std::puts("test_exception_map: ok");
  return 0;
}

