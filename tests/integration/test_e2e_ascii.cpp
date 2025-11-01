#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* /*param*/, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
  int      SubscribeItems(IoHandle h, const char** names, int count);
  int      UnsubscribeItems(IoHandle h, const char** names, int count);
  int      ReadItem(IoHandle h, const char* name, /*out*/char* outBuf, int outSize);
  int      WriteItem(IoHandle h, const char* name, const char* valueJson);
  int      CallMethod(IoHandle h, const char* method, const char* paramsJson, /*out*/char* outBuf, int outSize);
}

static std::string writeAsciiConfig(const std::string& filename) {
  const char* json = R"JSON({
    "transport": "ascii",
    "ascii": {
      "port": "/dev/ttyS10",
      "baud": 9600,
      "parity": "E",
      "data_bits": 7,
      "stop_bits": 1,
      "timeout_ms": 1200,
      "lrc_check": true,
      "frame_silence_ms": 5
    },
    "items": [
      { "name": "coil.run", "unit_id": 1, "function": 1, "address": 0, "type": "bool" },
      { "name": "holding.temp", "unit_id": 1, "function": 3, "address": 0, "type": "int16", "scale": 0.1 },
      { "name": "diag.snapshot", "unit_id": 1, "function": 8, "address": 0, "type": "diagnostic" },
      { "name": "broadcast.cmd", "unit_id": 0, "function": 16, "address": 10, "count": 2, "type": "uint16", "broadcast_allowed": true }
    ]
  })JSON";
  std::ofstream ofs(filename);
  ofs << json;
  ofs.close();
  return filename;
}

static bool fileExists(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  std::fclose(f);
  return true;
}

int main() {
  const std::string cfgPath = writeAsciiConfig("test_ascii_config.json");
  if (!fileExists(cfgPath)) {
    std::fprintf(stderr, "Failed to write config %s\n", cfgPath.c_str());
    return 1;
  }

  IoHandle h = CreateIoInstance(nullptr, cfgPath.c_str());
  if (!h) {
    std::fprintf(stderr, "CreateIoInstance failed\n");
    return 2;
  }

  const char* items[] = { "coil.run", "holding.temp" };
  if (SubscribeItems(h, items, 2) != 0) {
    std::fprintf(stderr, "SubscribeItems failed\n");
    DestroyIoInstance(h);
    return 3;
  }

  char buf[128] = {0};
  int rc = ReadItem(h, "coil.run", buf, sizeof(buf));
  if (rc != 0) {
    std::fprintf(stderr, "ReadItem(coil.run) failed rc=%d\n", rc);
    UnsubscribeItems(h, items, 2);
    DestroyIoInstance(h);
    return 4;
  }

  if (WriteItem(h, "coil.run", "true") != 0) {
    std::fprintf(stderr, "WriteItem(coil.run) failed\n");
    UnsubscribeItems(h, items, 2);
    DestroyIoInstance(h);
    return 5;
  }

  std::memset(buf, 0, sizeof(buf));
  rc = ReadItem(h, "holding.temp", buf, sizeof(buf));
  if (rc != 0) {
    std::fprintf(stderr, "ReadItem(holding.temp) failed rc=%d\n", rc);
    UnsubscribeItems(h, items, 2);
    DestroyIoInstance(h);
    return 6;
  }

  if (WriteItem(h, "broadcast.cmd", "[123,456]") != 0) {
    std::fprintf(stderr, "WriteItem(broadcast.cmd) failed\n");
    UnsubscribeItems(h, items, 2);
    DestroyIoInstance(h);
    return 7;
  }

  char diagBuf[512] = {0};
  rc = CallMethod(h, "diagnostics.snapshot", "{}", diagBuf, sizeof(diagBuf));
  if (rc != 0) {
    std::fprintf(stderr, "diagnostics.snapshot failed rc=%d\n", rc);
    UnsubscribeItems(h, items, 2);
    DestroyIoInstance(h);
    return 8;
  }

  std::printf("ASCII diagnostics: %s\n", diagBuf);

  UnsubscribeItems(h, items, 2);
  DestroyIoInstance(h);
  std::remove(cfgPath.c_str());

  std::puts("ASCII E2E test finished successfully.");
  return 0;
}

