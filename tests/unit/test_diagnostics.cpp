#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void*, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
  int      ReadItem(IoHandle h, const char* name, /*out*/char* outJson, int outSize);
  int      WriteItem(IoHandle h, const char* name, const char* valueJson);
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
      },
      {
        {"name", "broadcast.cmd"},
        {"unit_id", 0},
        {"function", 16},
        {"address", 20},
        {"count", 2},
        {"type", "uint16"},
        {"broadcast_allowed", true}
      }
    })}
  };

  std::ofstream ofs(filename);
  ofs << cfg.dump(2);
  ofs.close();
  return filename;
}

static nlohmann::json snapshot(IoHandle h) {
  char buf[512] = {0};
  int rc = CallMethod(h, "diagnostics.snapshot", "{}", buf, sizeof(buf));
  assert(rc == 0);
  return nlohmann::json::parse(buf);
}

int main() {
  const std::string cfgPath = writeConfig("test_diagnostics_config.json");
  IoHandle h = CreateIoInstance(nullptr, cfgPath.c_str());
  assert(h && "CreateIoInstance failed");

  char readBuf[128] = {0};
  int rc = ReadItem(h, "coil.ok", readBuf, sizeof(readBuf));
  assert(rc == 0);

  char errBuf[128] = {0};
  rc = ReadItem(h, "coil.bad", errBuf, sizeof(errBuf));
  assert(rc == -3202);

  rc = WriteItem(h, "broadcast.cmd", "[1,2]");
  assert(rc == 0);

  auto snap = snapshot(h);
  auto counters = snap["counters"];
  assert(counters["operations"].get<std::uint64_t>() >= 3);
  assert(counters["broadcasts_sent"].get<std::uint64_t>() >= 1);
  assert(!snap["exceptions"].empty());

  rc = CallMethod(h, "diagnostics.reset", "{}", nullptr, 0);
  assert(rc == 0);
  snap = snapshot(h);
  auto zero = snap["counters"];
  assert(zero["operations"].get<std::uint64_t>() == 0);
  assert(snap["exceptions"].empty());

  DestroyIoInstance(h);
  std::remove(cfgPath.c_str());
  std::puts("test_diagnostics: ok");
  return 0;
}

